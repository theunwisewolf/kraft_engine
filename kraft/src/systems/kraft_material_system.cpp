#include "kraft_material_system.h"

#include <containers/kraft_array.h>
#include <containers/kraft_hashmap.h>
#include <core/kraft_allocators.h>
#include <core/kraft_asserts.h>
#include <core/kraft_lexer.h>
#include <core/kraft_log.h>
#include <core/kraft_memory.h>
#include <core/kraft_string.h>
#include <math/kraft_math.h>
#include <platform/kraft_filesystem.h>
#include <platform/kraft_filesystem_types.h>
#include <renderer/kraft_renderer_frontend.h>
#include <systems/kraft_shader_system.h>
#include <systems/kraft_texture_system.h>

#include <core/kraft_lexer_types.h>
#include <kraft_types.h>
#include <renderer/kraft_renderer_types.h>
#include <resources/kraft_resource_types.h>
#include <systems/kraft_material_system_types.h>

namespace kraft {

struct MaterialReference
{
    uint32   RefCount;
    Material Material;
};

struct MaterialSystemState
{
    ArenaAllocator*          Arena;
    FlatHashMap<String, int> IndexMapping;

    uint16 MaterialBufferSize;
    uint16 MaxMaterialsCount;

    CArray(MaterialReference) MaterialReferences;
    CArray(uint8) MaterialsBuffer;
};

static MaterialSystemState* State = 0;

static void ReleaseMaterialInternal(uint32 Index);
static void CreateDefaultMaterialsInternal();
static bool LoadMaterialFromFileInternal(const String& FilePath, MaterialDataIntermediateFormat* Data);

KRAFT_INLINE Material* GetMaterialByName(const String& Name)
{
    auto It = State->IndexMapping.find(Name);
    if (It == State->IndexMapping.end())
    {
        return nullptr;
    }

    return &State->MaterialReferences[It->second].Material;
}

void MaterialSystem::Init(const RendererOptions& Opts)
{
    uint64          SizeRequirement = Opts.MaxMaterials * Opts.MaterialBufferSize + Opts.MaxMaterials * sizeof(MaterialReference) + sizeof(MaterialSystemState);
    ArenaAllocator* Arena = CreateArena({ .ChunkSize = SizeRequirement, .Alignment = 64, .Tag = MEMORY_TAG_MATERIAL_SYSTEM });

    State = (MaterialSystemState*)Arena->Push(sizeof(MaterialSystemState));
    State->Arena = Arena;
    State->MaterialBufferSize = Opts.MaterialBufferSize;
    State->MaxMaterialsCount = Opts.MaxMaterials;
    State->MaterialsBuffer = Arena->Push(State->MaterialBufferSize * State->MaxMaterialsCount);
    State->MaterialReferences = (MaterialReference*)Arena->Push(sizeof(MaterialReference) * State->MaxMaterialsCount);

    new (&State->IndexMapping) FlatHashMap<String, int>();
    State->IndexMapping.reserve(State->MaxMaterialsCount);

    // CreateDefaultMaterialsInternal();
}

void MaterialSystem::Shutdown()
{
    DestroyArena(State->Arena);
}

Material* MaterialSystem::GetDefaultMaterial()
{
    if (!State)
    {
        KFATAL("MaterialSystem not yet initialized!");
        return nullptr;
    }

    return &State->MaterialReferences[0].Material;
}

Material* MaterialSystem::CreateMaterialFromFile(const String& Path, Handle<RenderPass> RenderPassHandle)
{
    String FilePath = Path;
    if (!FilePath.EndsWith(".kmt"))
    {
        FilePath = FilePath + ".kmt";
    }

    MaterialDataIntermediateFormat Data;
    Data.FilePath = FilePath;
    if (!LoadMaterialFromFileInternal(FilePath, &Data))
    {
        KINFO("[CreateMaterialFromFile]: Failed to parse material %s", *FilePath);
        return nullptr;
    }

    return CreateMaterialWithData(Data, RenderPassHandle);
}

Material* MaterialSystem::CreateMaterialWithData(const MaterialDataIntermediateFormat& Data, Handle<RenderPass> RenderPassHandle)
{
    uint32 FreeIndex = uint32(-1);
    for (uint32 i = 0; i < State->MaxMaterialsCount; ++i)
    {
        if (State->MaterialReferences[i].RefCount == 0)
        {
            FreeIndex = i;
            break;
        }
    }

    if (FreeIndex == uint32(-1))
    {
        KERROR("[CreateMaterialWithData]: Max number of materials reached!");
        return nullptr;
    }

    State->IndexMapping[Data.Name] = FreeIndex;
    MaterialReference* Reference = &State->MaterialReferences[FreeIndex];
    Reference->RefCount = 1;

    // Load the shader
    Shader* Shader = ShaderSystem::AcquireShader(Data.ShaderAsset, RenderPassHandle);
    if (!Shader)
    {
        KERROR("[CreateMaterialWithData]: Failed to load shader %s reference by the material %s", *Data.ShaderAsset, *Data.Name);
        return nullptr;
    }

    Material* Instance = &Reference->Material;
    Instance->ID = FreeIndex;
    Instance->Name = Data.Name;
    Instance->AssetPath = Data.FilePath;
    Instance->Shader = Shader;
    Instance->Textures = Array<Handle<Texture>>(Shader->TextureCount);
    Instance->Dirty = true;

    // Offset into the material buffer
    uint8* MaterialBuffer = State->MaterialsBuffer + FreeIndex * State->MaterialBufferSize;
    MemZero(MaterialBuffer, State->MaterialBufferSize);

    // Create backend data such has descriptor sets
    g_Renderer->CreateMaterial(Instance);

    // Copy over the material properties from the material data
    for (auto It = Shader->UniformCacheMapping.ibegin(); It != Shader->UniformCacheMapping.iend(); It++)
    {
        ShaderUniform* Uniform = &Shader->UniformCache[It->second];
        if (Uniform->Scope != ShaderUniformScope::Instance)
        {
            continue;
        }

        const String& UniformName = It->first;
        // MaterialProperty Property = MaterialProperty();

        // Look to see if this property is present in the material
        auto MaterialIt = Data.Properties.find(UniformName);
        if (MaterialIt == Data.Properties.end())
        {
            KWARN("[CreateMaterialWithData]: Material %s does not contain shader uniform %s", *Data.Name, *UniformName);
            continue;
        }

        const MaterialProperty* Property = &MaterialIt->second;
        if (Property->Size != Uniform->Stride)
        {
            KERROR("[CreateMaterialWithData]: Material %s data type mismatch for uniform %s. Expected size: %d, got %d.", *Data.Name, *UniformName, Uniform->Stride, Property->Size);
            return nullptr;
        }

        // Copy over the value
        if (Uniform->Type == ResourceType::Sampler)
        {
            // TODO (amn): Handle texture loading properly
            uint32 Index = (uint32)MaterialIt->second.TextureValue.GetIndex();
            MemCpy(MaterialBuffer + 52, &Index, sizeof(uint32));
        }
        else
        {
            MemCpy(MaterialBuffer + Uniform->Offset, MaterialIt->second.Memory, Uniform->Stride);
        }
    }

    return Instance;
}

void MaterialSystem::DestroyMaterial(const String& Name)
{
    Material* Instance = GetMaterialByName(Name);
    KDEBUG("[MaterialSystem::DestroyMaterial]: Releasing material %s", *Name);

    if (Instance == nullptr)
    {
        KERROR("[MaterialSystem::DestroyMaterial]: Unknown material %s", *Name);
        return;
    }

    if (Instance->ID == 0)
    {
        KWARN("[MaterialSystem::DestroyMaterial]: Default material cannot be released!");
        return;
    }

    ReleaseMaterialInternal(Instance->ID);
}

void MaterialSystem::DestroyMaterial(Material* Instance)
{
    KASSERT(Instance->ID < State->MaxMaterialsCount);
    KDEBUG("[MaterialSystem::DestroyMaterial]: Destroying material %s", *Instance->Name);
    MaterialReference* Reference = &State->MaterialReferences[Instance->ID];

    if (!Reference)
    {
        KERROR("Invalid material %s", *Instance->Name);
    }

    for (uint32 i = 0; i < Instance->Textures.Length; i++)
    {
        TextureSystem::ReleaseTexture(Instance->Textures[i]);
    }

    ShaderSystem::ReleaseShader(Instance->Shader);

    MemZero(Instance, sizeof(Material));
}

bool MaterialSystem::SetTexture(Material* Instance, const String& Key, const String& TexturePath)
{
    Handle<Texture> NewTexture = TextureSystem::AcquireTexture(TexturePath, true);
    if (NewTexture.IsInvalid())
    {
        return false;
    }

    return SetTexture(Instance, Key, NewTexture);
}

bool MaterialSystem::SetTexture(Material* Instance, const String& Key, Handle<Texture> NewTexture)
{
    auto It = Instance->Shader->UniformCacheMapping.find(Key);
    if (It == Instance->Shader->UniformCacheMapping.iend())
    {
        KERROR("[SetTexture]: Unknown key %s", *Key);
        return false;
    }

    uint8* MaterialBuffer = State->MaterialsBuffer + Instance->ID * State->MaterialBufferSize;
    // TODO (amn): Handle texture loading properly
    uint32 Index = NewTexture.GetIndex();
    MemCpy(MaterialBuffer + 52, &Index, sizeof(uint32));

    // MaterialProperty* Property = Instance->Shader->;

    // // Validate the Uniform
    // Shader*       Shader = Instance->Shader;
    // ShaderUniform Uniform;
    // if (!ShaderSystem::GetUniform(Shader, Key, Uniform))
    //     return false;

    // // Release the old texture
    // Handle<Texture> OldTexture = Instance->Textures[Uniform.Offset];
    // TextureSystem::ReleaseTexture(OldTexture);

    // Property.Set(NewTexture);
    // // Instance->Textures[Uniform.Offset] = NewTexture;
    // Instance->Dirty = true;

    return true;
}

uint8* MaterialSystem::GetMaterialsBuffer()
{
    return State->MaterialsBuffer;
}

//
// Internal private methods
//

static void ReleaseMaterialInternal(uint32 Index)
{
    MaterialReference Reference = State->MaterialReferences[Index];
    if (Reference.RefCount == 0)
    {
        KWARN("[MaterialSystem::ReleaseMaterial]: Material %s already released!", *Reference.Material.Name);
        return;
    }

    Reference.RefCount--;
    if (Reference.RefCount == 0)
    {
        MaterialSystem::DestroyMaterial(&Reference.Material);
    }
}

static void CreateDefaultMaterialsInternal()
{
    MaterialDataIntermediateFormat Data;
    Data.Name = "Materials.Default";
    Data.FilePath = "Material.Default.kmt";
    Data.ShaderAsset = ShaderSystem::GetDefaultShader()->Path;
    Data.Properties["DiffuseColor"] = MaterialProperty(0, Vec4fOne);
    Data.Properties["DiffuseSampler"] = MaterialProperty(1, TextureSystem::GetDefaultDiffuseTexture());

    KASSERT(MaterialSystem::CreateMaterialWithData(Data, Handle<RenderPass>::Invalid()));
}

static bool LoadMaterialFromFileInternal(const String& FilePath, MaterialDataIntermediateFormat* Data)
{
    filesystem::FileHandle File;
    bool                   Result = filesystem::OpenFile(FilePath, filesystem::FILE_OPEN_MODE_READ, true, &File);
    if (!Result)
    {
        return false;
    }

    uint64 BufferSize = filesystem::GetFileSize(&File) + 1;
    uint8* FileDataBuffer = (uint8*)Malloc(BufferSize, MEMORY_TAG_FILE_BUF, true);
    filesystem::ReadAllBytes(&File, &FileDataBuffer);
    filesystem::CloseFile(&File);

    Lexer Lexer;
    Lexer.Create((char*)FileDataBuffer);

    while (true)
    {
        Token Token = Lexer.NextToken();
        if (Token.Type == TokenType::TOKEN_TYPE_IDENTIFIER)
        {
            if (Token.MatchesKeyword("Material"))
            {
                if (!Lexer.ExpectToken(&Token, TokenType::TOKEN_TYPE_IDENTIFIER))
                {
                    return false;
                }

                Data->Name = Token.ToString();
                if (!Lexer.ExpectToken(&Token, TokenType::TOKEN_TYPE_OPEN_BRACE))
                {
                    return false;
                }

                // Parse material block
                while (!Lexer.EqualsToken(&Token, TokenType::TOKEN_TYPE_CLOSE_BRACE))
                {
                    if (Token.MatchesKeyword("DiffuseColor"))
                    {
                        if (!Lexer.ExpectToken(&Token, TokenType::TOKEN_TYPE_IDENTIFIER))
                        {
                            return false;
                        }

                        if (Token.MatchesKeyword("vec4"))
                        {
                            if (!Lexer.ExpectToken(&Token, TokenType::TOKEN_TYPE_OPEN_PARENTHESIS))
                            {
                                return false;
                            }

                            float32 DiffuseColor[4];
                            int     Index = 0;
                            while (!Lexer.EqualsToken(&Token, TokenType::TOKEN_TYPE_CLOSE_PARENTHESIS))
                            {
                                if (Token.Type == TokenType::TOKEN_TYPE_COMMA)
                                {
                                    continue;
                                }
                                else if (Token.Type == TokenType::TOKEN_TYPE_NUMBER)
                                {
                                    DiffuseColor[Index++] = (float32)Token.FloatValue;
                                }
                                else
                                {
                                    return false;
                                }
                            }

                            Data->Properties["DiffuseColor"] = Vec4f(DiffuseColor[0], DiffuseColor[1], DiffuseColor[2], DiffuseColor[3]);
                        }
                    }
                    else if (Token.MatchesKeyword("DiffuseMapName"))
                    {
                        if (!Lexer.ExpectToken(&Token, TokenType::TOKEN_TYPE_STRING))
                        {
                            return false;
                        }

                        const String&   TexturePath = Token.ToString();
                        Handle<Texture> Resource = TextureSystem::AcquireTexture(TexturePath);
                        if (Resource.IsInvalid())
                        {
                            KERROR("[MaterialSystem::CreateMaterial]: Failed to load texture %s for material %s. Using default texture.", *TexturePath, *FilePath);
                            Resource = TextureSystem::GetDefaultDiffuseTexture();
                        }

                        Data->Properties["DiffuseSampler"] = Resource;
                    }
                    else if (Token.MatchesKeyword("Shader"))
                    {
                        if (!Lexer.ExpectToken(&Token, TokenType::TOKEN_TYPE_STRING))
                        {
                            return false;
                        }

                        Data->ShaderAsset = Token.ToString();
                    }
                    else
                    {
                        String Key = Token.ToString();
                        if (!Lexer.ExpectToken(&Token, TokenType::TOKEN_TYPE_IDENTIFIER))
                        {
                            return false;
                        }

                        bool IsVector = false;
                        int  ExpectedComponentCount = 0;
                        if (Token.MatchesKeyword("vec4"))
                        {
                            IsVector = true;
                            ExpectedComponentCount = 4;
                        }
                        else if (Token.MatchesKeyword("vec3"))
                        {
                            IsVector = true;
                            ExpectedComponentCount = 3;
                        }
                        else if (Token.MatchesKeyword("vec2"))
                        {
                            IsVector = true;
                            ExpectedComponentCount = 2;
                        }

                        if (IsVector)
                        {
                            if (!Lexer.ExpectToken(&Token, TokenType::TOKEN_TYPE_OPEN_PARENTHESIS))
                            {
                                return false;
                            }

                            float32 Components[4];
                            int     ComponentCount = 0;
                            while (!Lexer.EqualsToken(&Token, TokenType::TOKEN_TYPE_CLOSE_PARENTHESIS))
                            {
                                if (Token.Type == TokenType::TOKEN_TYPE_COMMA)
                                {
                                    continue;
                                }
                                else if (Token.Type == TokenType::TOKEN_TYPE_NUMBER)
                                {
                                    Components[ComponentCount++] = (float32)Token.FloatValue;
                                }
                                else
                                {
                                    return false;
                                }
                            }

                            if (ExpectedComponentCount != ComponentCount)
                            {
                                KERROR(
                                    "Failed to parse %s. Expected %d components for field '%s' but got only %d. Make sure your vec%d() has %d components.",
                                    *FilePath,
                                    ExpectedComponentCount,
                                    *Key,
                                    ComponentCount,
                                    ComponentCount,
                                    ExpectedComponentCount
                                );
                                return false;
                            }

                            if (ComponentCount == 4)
                                Data->Properties[Key] = Vec4f(Components[0], Components[1], Components[2], Components[3]);
                            if (ComponentCount == 3)
                                Data->Properties[Key] = Vec3f(Components[0], Components[1], Components[2]);
                            if (ComponentCount == 2)
                                Data->Properties[Key] = Vec2f(Components[0], Components[1]);

                            Data->Properties[Key].Size = sizeof(float32) * ComponentCount;
                        }
                        else if (Token.MatchesKeyword("float32"))
                        {
                            if (!Lexer.ExpectToken(&Token, TokenType::TOKEN_TYPE_OPEN_PARENTHESIS))
                            {
                                return false;
                            }

                            if (!Lexer.ExpectToken(&Token, TokenType::TOKEN_TYPE_NUMBER))
                            {
                                return false;
                            }

                            Data->Properties[Key] = float32(Token.FloatValue);
                            Data->Properties[Key].Size = sizeof(float32);

                            if (!Lexer.ExpectToken(&Token, TokenType::TOKEN_TYPE_CLOSE_PARENTHESIS))
                            {
                                return false;
                            }
                        }
                        else if (Token.MatchesKeyword("float64"))
                        {
                            if (!Lexer.ExpectToken(&Token, TokenType::TOKEN_TYPE_OPEN_PARENTHESIS))
                            {
                                return false;
                            }

                            if (!Lexer.ExpectToken(&Token, TokenType::TOKEN_TYPE_NUMBER))
                            {
                                return false;
                            }

                            Data->Properties[Key] = float64(Token.FloatValue);
                            Data->Properties[Key].Size = sizeof(float64);

                            if (!Lexer.ExpectToken(&Token, TokenType::TOKEN_TYPE_CLOSE_PARENTHESIS))
                            {
                                return false;
                            }
                        }
                        else
                        {
                            KERROR("Material has an invalid field %s", *Token.ToString());
                            return false;
                        }
                    }
                }
            }
        }
        else if (Token.Type == TokenType::TOKEN_TYPE_END_OF_STREAM)
        {
            break;
        }
    }

    kraft::Free(FileDataBuffer, BufferSize, MEMORY_TAG_FILE_BUF);

    return true;
}

#if 0
#define MATERIAL_SYSTEM_SET_PROPERTY(Type)                                                                                                                                                             \
    template<>                                                                                                                                                                                         \
    bool MaterialSystem::SetProperty(Material* Instance, const String& Name, Type Value)                                                                                                               \
    {                                                                                                                                                                                                  \
        MaterialProperty& Property = Instance->Properties[Name];                                                                                                                                       \
        Property.Set(Value);                                                                                                                                                                           \
        Instance->Dirty = true;                                                                                                                                                                        \
        return true;                                                                                                                                                                                   \
    }

MATERIAL_SYSTEM_SET_PROPERTY(Mat4f);
MATERIAL_SYSTEM_SET_PROPERTY(Vec4f);
MATERIAL_SYSTEM_SET_PROPERTY(Vec3f);
MATERIAL_SYSTEM_SET_PROPERTY(Vec2f);
MATERIAL_SYSTEM_SET_PROPERTY(Float32);
MATERIAL_SYSTEM_SET_PROPERTY(Float64);
MATERIAL_SYSTEM_SET_PROPERTY(UInt8);
MATERIAL_SYSTEM_SET_PROPERTY(UInt16);
MATERIAL_SYSTEM_SET_PROPERTY(UInt32);
MATERIAL_SYSTEM_SET_PROPERTY(UInt64);
MATERIAL_SYSTEM_SET_PROPERTY(Handle<Texture>);
#endif

} // namespace kraft