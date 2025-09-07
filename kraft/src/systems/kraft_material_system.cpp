#include "kraft_material_system.h"

#include <platform/kraft_filesystem.h>
#include <platform/kraft_filesystem_types.h>
#include <renderer/kraft_renderer_frontend.h>
#include <systems/kraft_shader_system.h>
#include <systems/kraft_texture_system.h>

#include <core/kraft_string.h>
#include <containers/kraft_hashmap.h>
#include <containers/kraft_array.h>
#include <core/kraft_lexer_types.h>
#include <kraft_types.h>
#include <renderer/kraft_renderer_types.h>
#include <resources/kraft_resource_types.h>
#include <systems/kraft_material_system_types.h>
#include <core/kraft_lexer.h>

// TODO (amn): Remove
#include <core/kraft_base_includes.h>

namespace kraft {

struct MaterialReference
{
    Material material;
    u32      ref_count;
};

struct MaterialSystemState
{
    ArenaAllocator* arena;

    uint16 MaterialBufferSize;
    uint16 MaxMaterialsCount;

    MaterialReference* material_references;
    CArray(uint8) MaterialsBuffer;
};

static MaterialSystemState* State = 0;

static void ReleaseMaterialInternal(uint32 Index);
// static void CreateDefaultMaterialsInternal();
static bool LoadMaterialFromFileInternal(ArenaAllocator* arena, String8 file_path, MaterialDataIntermediateFormat* data);

void MaterialSystem::Init(const RendererOptions& Opts)
{
    ArenaAllocator* arena = CreateArena({ .ChunkSize = KRAFT_SIZE_MB(64), .Alignment = 64, .Tag = MEMORY_TAG_MATERIAL_SYSTEM });

    State = ArenaPush(arena, MaterialSystemState);
    State->arena = arena;
    State->MaterialBufferSize = Opts.MaterialBufferSize;
    State->MaxMaterialsCount = Opts.MaxMaterials;
    State->MaterialsBuffer = ArenaPushArray(arena, u8, State->MaterialBufferSize * State->MaxMaterialsCount);
    State->material_references = ArenaPushArray(arena, MaterialReference, State->MaxMaterialsCount);

    // new (&State->IndexMapping) FlatHashMap<String, int>();
    // State->IndexMapping.reserve(State->MaxMaterialsCount);

    // CreateDefaultMaterialsInternal();
}

void MaterialSystem::Shutdown()
{
    DestroyArena(State->arena);
}

Material* MaterialSystem::GetDefaultMaterial()
{
    if (!State)
    {
        KFATAL("MaterialSystem not yet initialized!");
        return nullptr;
    }

    return &State->material_references[0].material;
}

Material* MaterialSystem::CreateMaterialFromFile(String8 file_path, Handle<RenderPass> RenderPassHandle)
{
    TempArena scratch = ScratchBegin(0, 0);
    String8   _file_path = file_path;
    if (!StringEndsWith(file_path, String8Raw(".kmt")))
    {
        _file_path = StringCat(scratch.arena, file_path, String8Raw(".kmt"));
    }

    // String file_path = file_path;
    // if (!file_path.EndsWith(".kmt"))
    // {
    //     file_path = file_path + ".kmt";
    // }

    MaterialDataIntermediateFormat data;
    data.filepath = ArenaPushString8Copy(State->arena, _file_path);
    if (!LoadMaterialFromFileInternal(State->arena, _file_path, &data))
    {
        KINFO("[CreateMaterialFromFile]: Failed to parse material %S", file_path);
        return nullptr;
    }

    ScratchEnd(scratch);
    return CreateMaterialWithData(data, RenderPassHandle);
}

Material* MaterialSystem::CreateMaterialWithData(const MaterialDataIntermediateFormat& Data, Handle<RenderPass> RenderPassHandle)
{
    uint32 FreeIndex = uint32(-1);
    for (uint32 i = 0; i < State->MaxMaterialsCount; ++i)
    {
        if (State->material_references[i].ref_count == 0)
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

    // State->IndexMapping[Data.Name] = FreeIndex;
    MaterialReference* Reference = &State->material_references[FreeIndex];
    Reference->ref_count = 1;

    // Load the shader
    Shader* Shader = ShaderSystem::AcquireShader(Data.shader_asset, RenderPassHandle);
    if (!Shader)
    {
        KERROR("[CreateMaterialWithData]: Failed to load shader %S reference by the material %S", Data.shader_asset, Data.name);
        return nullptr;
    }

    Material* Instance = &Reference->material;
    Instance->ID = FreeIndex;
    Instance->Name = Data.name;
    Instance->AssetPath = Data.filepath;
    Instance->Shader = Shader;

    // Offset into the material buffer
    uint8* MaterialBuffer = State->MaterialsBuffer + FreeIndex * State->MaterialBufferSize;
    MemZero(MaterialBuffer, State->MaterialBufferSize);

    // Copy over the material properties from the material data
    for (auto It = Shader->UniformCacheMapping.ibegin(); It != Shader->UniformCacheMapping.iend(); It++)
    {
        ShaderUniform* Uniform = &Shader->UniformCache[It->second];
        if (Uniform->Scope != ShaderUniformScope::Instance)
        {
            continue;
        }

        const String& UniformName = It->first;

        // Look to see if this property is present in the material
        auto MaterialIt = Data.Properties.find(UniformName);
        if (MaterialIt == Data.Properties.end())
        {
            KWARN("[CreateMaterialWithData]: Material %S does not contain shader uniform %s", Data.name, *UniformName);
            continue;
        }

        const MaterialProperty* Property = &MaterialIt->second;
        if (Property->Size != Uniform->Stride)
        {
            KERROR("[CreateMaterialWithData]: Material %S data type mismatch for uniform %s. Expected size: %d, got %d.", Data.name, *UniformName, Uniform->Stride, Property->Size);
            return nullptr;
        }

        // KDEBUG("Setting uniform '%s' to '%d' (Stride = %d, Offset = %d)", *UniformName, MaterialIt->second.UInt32Value, Uniform->Stride, Uniform->Offset);

        if (Uniform->DataType.UnderlyingType == ShaderDataType::TextureID)
        {
            uint32 Index = (uint32)MaterialIt->second.TextureValue.GetIndex();
            MemCpy(MaterialBuffer + Uniform->Offset, &Index, Uniform->Stride);
        }
        else
        {
            MemCpy(MaterialBuffer + Uniform->Offset, MaterialIt->second.Memory, Uniform->Stride);
        }
    }

    Instance->Properties = Data.Properties;
    return Instance;
}

void MaterialSystem::DestroyMaterial(String8 name)
{
    Material* instance = 0;
    for (i32 i = 0; i < State->MaxMaterialsCount; i++)
    {
        if (StringEqual(State->material_references[i].material.Name, name))
        {
            instance = &State->material_references[i].material;
            break;
        }
    }

    if (!instance)
    {
        KERROR("[MaterialSystem::DestroyMaterial]: Unknown material %S", name);
        return;
    }

    KDEBUG("[MaterialSystem::DestroyMaterial]: Releasing material %S", name);
    if (instance->ID == 0)
    {
        KWARN("[MaterialSystem::DestroyMaterial]: Default material cannot be released!");
        return;
    }

    ReleaseMaterialInternal(instance->ID);
}

void MaterialSystem::DestroyMaterial(Material* instance)
{
    KASSERT(instance->ID < State->MaxMaterialsCount);
    KDEBUG("[MaterialSystem::DestroyMaterial]: Destroying material %S", instance->Name);

    MaterialReference* Reference = &State->material_references[instance->ID];
    if (!Reference)
    {
        KERROR("Invalid material %S", instance->Name);
    }

    ShaderSystem::ReleaseShader(instance->Shader);
    MemZero(instance, sizeof(Material));
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

    uint8*        MaterialBuffer = State->MaterialsBuffer + Instance->ID * State->MaterialBufferSize;
    Shader*       Shader = Instance->Shader;
    ShaderUniform Uniform;
    if (!ShaderSystem::GetUniform(Shader, Key, &Uniform))
    {
        KERROR("GetUniform() failed for '%s'", *Key);
        return false;
    }

    uint32 Index = NewTexture.GetIndex();
    MemCpy(MaterialBuffer + Uniform.Offset, &Index, Uniform.Stride);
    Instance->Properties[Key].TextureValue = NewTexture;

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
    MaterialReference ref = State->material_references[Index];
    if (ref.ref_count == 0)
    {
        KWARN("[MaterialSystem::ReleaseMaterial]: Material %S already released!", ref.material.Name);
        return;
    }

    ref.ref_count--;
    if (ref.ref_count == 0)
    {
        MaterialSystem::DestroyMaterial(&ref.material);
    }
}

// static void CreateDefaultMaterialsInternal()
// {
//     MaterialDataIntermediateFormat Data;
//     Data.Name = "Materials.Default";
//     Data.file_path = "Material.Default.kmt";
//     Data.ShaderAsset = ShaderSystem::GetDefaultShader()->Path;
//     Data.Properties["DiffuseColor"] = MaterialProperty(0, Vec4fOne);
//     Data.Properties["DiffuseTexture"] = MaterialProperty(1, TextureSystem::GetDefaultDiffuseTexture());

//     KASSERT(MaterialSystem::CreateMaterialWithData(Data, Handle<RenderPass>::Invalid()));
// }

static bool LoadMaterialFromFileInternal(ArenaAllocator* arena, String8 file_path, MaterialDataIntermediateFormat* Data)
{
    filesystem::FileHandle File;
    bool                   Result = filesystem::OpenFile(file_path, filesystem::FILE_OPEN_MODE_READ, true, &File);
    if (!Result)
    {
        return false;
    }

    uint64 BufferSize = filesystem::GetFileSize(&File) + 1;
    uint8* FileDataBuffer = (uint8*)Malloc(BufferSize, MEMORY_TAG_FILE_BUF, true);
    filesystem::ReadAllBytes(&File, &FileDataBuffer);
    filesystem::CloseFile(&File);

    Lexer lexer;
    lexer.Create((char*)FileDataBuffer, BufferSize);

    while (true)
    {
        LexerToken token;
        if (LexerError ErrorCode = lexer.NextToken(&token))
        {
            KERROR("Encountered an error while trying to get the next token from the material file! ErrorCode = %d", ErrorCode);
            return false;
        }

        if (token.Type == TokenType::TOKEN_TYPE_IDENTIFIER)
        {
            if (token.MatchesKeyword("Material"))
            {
                if (!lexer.ExpectToken(&token, TokenType::TOKEN_TYPE_IDENTIFIER))
                {
                    return false;
                }

                Data->name = ArenaPushString8Copy(arena, String8FromPtrAndLength((u8*)token.Text, token.Length));
                if (!lexer.ExpectToken(&token, TokenType::TOKEN_TYPE_OPEN_BRACE))
                {
                    return false;
                }

                // Parse material block
                while (!lexer.EqualsToken(&token, TokenType::TOKEN_TYPE_CLOSE_BRACE))
                {
                    if (token.MatchesKeyword("DiffuseTexture"))
                    {
                        if (!lexer.ExpectToken(&token, TokenType::TOKEN_TYPE_STRING))
                        {
                            return false;
                        }

                        const String&   TexturePath = token.ToString();
                        Handle<Texture> Resource = TextureSystem::AcquireTexture(TexturePath);
                        if (Resource.IsInvalid())
                        {
                            KERROR("[MaterialSystem::CreateMaterial]: Failed to load texture %s for material %S. Using default texture.", *TexturePath, file_path);
                            Resource = TextureSystem::GetDefaultDiffuseTexture();
                        }

                        Data->Properties["DiffuseTexture"] = Resource;
                        Data->Properties["DiffuseTexture"].Size = ShaderDataType::SizeOf(ShaderDataType{ .UnderlyingType = ShaderDataType::TextureID });
                    }
                    else if (token.MatchesKeyword("Shader"))
                    {
                        if (!lexer.ExpectToken(&token, TokenType::TOKEN_TYPE_STRING))
                        {
                            return false;
                        }

                        Data->shader_asset = ArenaPushString8Copy(arena, String8FromPtrAndLength((u8*)token.Text, token.Length));
                    }
                    else
                    {
                        String Key = token.ToString();
                        if (!lexer.ExpectToken(&token, TokenType::TOKEN_TYPE_IDENTIFIER))
                        {
                            return false;
                        }

                        bool IsVector = false;
                        int  ExpectedComponentCount = 0;
                        if (token.MatchesKeyword("vec4"))
                        {
                            IsVector = true;
                            ExpectedComponentCount = 4;
                        }
                        else if (token.MatchesKeyword("vec3"))
                        {
                            IsVector = true;
                            ExpectedComponentCount = 3;
                        }
                        else if (token.MatchesKeyword("vec2"))
                        {
                            IsVector = true;
                            ExpectedComponentCount = 2;
                        }

                        if (IsVector)
                        {
                            if (!lexer.ExpectToken(&token, TokenType::TOKEN_TYPE_OPEN_PARENTHESIS))
                            {
                                return false;
                            }

                            float32 Components[4];
                            int     ComponentCount = 0;
                            while (!lexer.EqualsToken(&token, TokenType::TOKEN_TYPE_CLOSE_PARENTHESIS))
                            {
                                if (token.Type == TokenType::TOKEN_TYPE_COMMA)
                                {
                                    continue;
                                }
                                else if (token.Type == TokenType::TOKEN_TYPE_NUMBER)
                                {
                                    Components[ComponentCount++] = (float32)token.FloatValue;
                                }
                                else
                                {
                                    return false;
                                }
                            }

                            if (ExpectedComponentCount != ComponentCount)
                            {
                                KERROR(
                                    "Failed to parse %S. Expected %d components for field '%s' but got only %d. Make sure your vec%d() has %d components.",
                                    file_path,
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
                        else if (token.MatchesKeyword("float32"))
                        {
                            if (!lexer.ExpectToken(&token, TokenType::TOKEN_TYPE_OPEN_PARENTHESIS))
                            {
                                return false;
                            }

                            if (!lexer.ExpectToken(&token, TokenType::TOKEN_TYPE_NUMBER))
                            {
                                return false;
                            }

                            Data->Properties[Key] = float32(token.FloatValue);
                            Data->Properties[Key].Size = sizeof(float32);

                            if (!lexer.ExpectToken(&token, TokenType::TOKEN_TYPE_CLOSE_PARENTHESIS))
                            {
                                return false;
                            }
                        }
                        else if (token.MatchesKeyword("float64"))
                        {
                            if (!lexer.ExpectToken(&token, TokenType::TOKEN_TYPE_OPEN_PARENTHESIS))
                            {
                                return false;
                            }

                            if (!lexer.ExpectToken(&token, TokenType::TOKEN_TYPE_NUMBER))
                            {
                                return false;
                            }

                            Data->Properties[Key] = float64(token.FloatValue);
                            Data->Properties[Key].Size = sizeof(float64);

                            if (!lexer.ExpectToken(&token, TokenType::TOKEN_TYPE_CLOSE_PARENTHESIS))
                            {
                                return false;
                            }
                        }
                        else
                        {
                            KERROR("Material has an invalid field %s", *token.ToString());
                            return false;
                        }
                    }
                }
            }
        }
        else if (token.Type == TokenType::TOKEN_TYPE_END_OF_STREAM)
        {
            break;
        }
    }

    kraft::Free(FileDataBuffer, BufferSize, MEMORY_TAG_FILE_BUF);

    return true;
}

#define MATERIAL_SYSTEM_SET_PROPERTY(Type)                                                                                                                                                             \
    template<>                                                                                                                                                                                         \
    bool MaterialSystem::SetProperty(Material* instance, const String& key, Type value)                                                                                                                \
    {                                                                                                                                                                                                  \
        auto it = instance->Shader->UniformCacheMapping.find(key);                                                                                                                                     \
        if (it == instance->Shader->UniformCacheMapping.iend())                                                                                                                                        \
        {                                                                                                                                                                                              \
            KERROR("[SetTexture]: Unknown key %s", *key);                                                                                                                                              \
            return false;                                                                                                                                                                              \
        }                                                                                                                                                                                              \
                                                                                                                                                                                                       \
        uint8*        material_buffer = State->MaterialsBuffer + instance->ID * State->MaterialBufferSize;                                                                                             \
        Shader*       shader = instance->Shader;                                                                                                                                                       \
        ShaderUniform uniform;                                                                                                                                                                         \
        if (!ShaderSystem::GetUniform(shader, key, &uniform))                                                                                                                                          \
        {                                                                                                                                                                                              \
            KERROR("GetUniform() failed for '%s'", *key);                                                                                                                                              \
            return false;                                                                                                                                                                              \
        }                                                                                                                                                                                              \
                                                                                                                                                                                                       \
        if (sizeof(value) != uniform.Stride)                                                                                                                                                           \
        {                                                                                                                                                                                              \
            KERROR("You are trying to set a value of size '%d' when the uniform's stride is set to '%d'", sizeof(value), uniform.Stride);                                                              \
            return false;                                                                                                                                                                              \
        }                                                                                                                                                                                              \
                                                                                                                                                                                                       \
        MemCpy(material_buffer + uniform.Offset, &value, uniform.Stride);                                                                                                                              \
        return true;                                                                                                                                                                                   \
    }

MATERIAL_SYSTEM_SET_PROPERTY(Mat4f);
MATERIAL_SYSTEM_SET_PROPERTY(Vec4f);
MATERIAL_SYSTEM_SET_PROPERTY(Vec3f);
MATERIAL_SYSTEM_SET_PROPERTY(Vec2f);
MATERIAL_SYSTEM_SET_PROPERTY(float32);
MATERIAL_SYSTEM_SET_PROPERTY(float64);
MATERIAL_SYSTEM_SET_PROPERTY(u8);
MATERIAL_SYSTEM_SET_PROPERTY(u16);
MATERIAL_SYSTEM_SET_PROPERTY(u32);
MATERIAL_SYSTEM_SET_PROPERTY(u64);

} // namespace kraft