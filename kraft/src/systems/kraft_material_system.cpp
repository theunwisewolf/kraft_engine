#include "kraft_material_system.h"

#include "core/kraft_memory.h"
#include "core/kraft_string.h"
#include "core/kraft_asserts.h"
#include "platform/kraft_filesystem.h"
#include "math/kraft_math.h"
#include "systems/kraft_texture_system.h"
#include "systems/kraft_shader_system.h"
#include "renderer/kraft_renderer_frontend.h"

namespace kraft
{

struct MaterialReference
{
    uint32   RefCount;
    Material Material;
};

struct MaterialSystemState
{
    uint32               MaxMaterialCount;
    HashMap<String, int> IndexMapping;
    MaterialReference*   MaterialReferences;
};

static MaterialSystemState* State = 0;

static void ReleaseMaterialInternal(uint32 Index);
static void CreateDefaultMaterialsInternal();
static bool LoadMaterialFromFileInternal(const String& FilePath, MaterialData* Data);

KRAFT_INLINE Material* GetMaterialByName(const String& Name)
{
    auto It = State->IndexMapping.find(Name);
    if (It == State->IndexMapping.iend())
    {
        return nullptr;
    }
    
    return &State->MaterialReferences[It->second.get()].Material;
}

void MaterialSystem::Init(MaterialSystemConfig Config)
{
    uint32 StateSize = sizeof(MaterialSystemState);
    uint32 ArraySize = sizeof(MaterialReference) * Config.MaxMaterialsCount;
    uint32 TotalSize = StateSize + ArraySize;

    char* RawMemory = (char*)kraft::Malloc(TotalSize, MEMORY_TAG_MATERIAL_SYSTEM, true);

    State = (MaterialSystemState*)RawMemory;
    State->MaxMaterialCount = Config.MaxMaterialsCount;
    State->MaterialReferences = (MaterialReference*)(RawMemory + StateSize);
    State->IndexMapping = HashMap<String, int>();
    State->IndexMapping.reserve(Config.MaxMaterialsCount);

    // CreateDefaultMaterialsInternal();
}

void MaterialSystem::Shutdown()
{
    // Free the default material
    ReleaseMaterialInternal(0);

    for (int i = 1; i < State->MaxMaterialCount; ++i)
    {
        MaterialReference* Ref = &State->MaterialReferences[i];
        if (Ref->RefCount)
            MaterialSystem::DestroyMaterial(&Ref->Material);
    }

    uint32 totalSize = sizeof(MaterialSystemState) + sizeof(MaterialReference) * State->MaxMaterialCount;
    kraft::Free(State, totalSize, MEMORY_TAG_MATERIAL_SYSTEM);
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

Material* MaterialSystem::CreateMaterialFromFile(const String& Path)
{
    String FilePath = Path;
    if (!FilePath.EndsWith(".kmt"))
    {
        FilePath = FilePath + ".kmt";
    }

    MaterialData Data;
    Data.FilePath = FilePath;
    if (!LoadMaterialFromFileInternal(FilePath, &Data))
    {
        KINFO("[MaterialSystem::AcquireMaterial]: Failed to parse material %s", *FilePath);
        return nullptr;
    }

    return CreateMaterialWithData(Data);
}

Material* MaterialSystem::CreateMaterialWithData(MaterialData Data)
{
    int FreeIndex = -1; 
    for (uint32 i = 0; i < State->MaxMaterialCount; ++i)
    {
        if (State->MaterialReferences[i].RefCount == 0)
        {
            FreeIndex = i;
            break;
        }
    }

    if (FreeIndex == -1)
    {
        KERROR("[MaterialSystem::CreateMaterialWithData]: Max number of materials reached!");
        return nullptr;
    }

    State->IndexMapping[Data.Name] = FreeIndex;
    MaterialReference* Reference = &State->MaterialReferences[FreeIndex];
    Reference->RefCount = 1;

    // Load the shader
    Shader* Shader = ShaderSystem::AcquireShader(Data.ShaderAsset);
    if (!Shader)
    {
        KERROR("[MaterialSystem::CreateMaterialWithData]: Failed to load shader %s reference by the material %s", *Data.ShaderAsset, *Data.Name);
        Shader = ShaderSystem::GetDefaultShader();
    }

    Material* Instance = &Reference->Material;
    Instance->ID = FreeIndex;
    Instance->Name = Data.Name;
    Instance->AssetPath = Data.FilePath;
    Instance->Shader = Shader;
    Instance->Textures = Array<Texture*>(Shader->TextureCount);
    Instance->Properties = HashMap<String, MaterialProperty>();
    Instance->Properties.reserve(Shader->InstanceUniformsCount);
    Instance->Dirty = true;

    // Create backend data such has descriptor sets
    Renderer->CreateMaterial(Instance);

    // Cache the uniforms for this material
    for (auto It = Shader->UniformCacheMapping.ibegin(); It != Shader->UniformCacheMapping.iend(); It++)
    {
        ShaderUniform Uniform = Shader->UniformCache[It->second];
        if (Uniform.Scope == ShaderUniformScope::Instance)
        {
            const String& UniformName = It->first;
            
            Instance->Properties[UniformName] = MaterialProperty();

            // Look to see if this property is present in the material
            auto _MaterialIt = Data.Properties.find(UniformName);
            if (_MaterialIt == Data.Properties.iend())
            {
                KWARN("[MaterialSystem::CreateMaterialWithData]: Material %s does not contain shader uniform %s", *Data.Name, *UniformName);
            }
            else
            {
                // Copy over the value
                if (Uniform.Type == ResourceType::Sampler)
                {
                    MemCpy(Instance->Properties[UniformName].Memory, Data.Properties[UniformName].Memory, sizeof(Texture));
                }
                else
                {
                    MemCpy(Instance->Properties[UniformName].Memory, Data.Properties[UniformName].Memory, Uniform.Stride);
                }

                Instance->Properties[UniformName].UniformIndex = It->second;
            }
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
    KASSERT(Instance->ID < State->MaxMaterialCount);
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

void MaterialSystem::ApplyInstanceProperties(Material* Instance)
{
    // Copy over all the properties from the material to the shader's uniform buffer
    if (Instance->Dirty)
    {
        for (auto It = Instance->Properties.vbegin(); It != Instance->Properties.vend(); It++)
        {
            MaterialProperty& Property = *It;
            ShaderSystem::SetUniformByIndex(It->UniformIndex, (void*)(&Property.Memory[0]), Instance->Dirty);
        }

        Instance->Dirty = false;
    }
    ShaderSystem::ApplyInstanceProperties();
}

void MaterialSystem::ApplyLocalProperties(Material* Material, const Mat4f& Model)
{
    ShaderSystem::SetUniform("Model", Model);
}

bool MaterialSystem::SetTexture(Material* Instance, const String& Key, const String& TexturePath)
{
    Texture* NewTexture = TextureSystem::AcquireTexture(TexturePath, true);
    if (!NewTexture)
    {
        return false;
    }

    return SetTexture(Instance, Key, NewTexture);
}

bool MaterialSystem::SetTexture(Material* Instance, const String& Key, Texture* NewTexture)
{
    KASSERT(NewTexture);
    if (!NewTexture)
    {
        return false;
    }

    auto It = Instance->Properties.find(Key);
    if (It == Instance->Properties.iend())
    {
        KERROR("[MaterialSystem::SetTexture]: Unknown key %s", *Key);
        return false;
    }

    MaterialProperty& Property = It->second;

    // Validate the Uniform
    Shader* Shader = Instance->Shader;
    ShaderUniform Uniform;
    if (!ShaderSystem::GetUniform(Shader, Key, Uniform)) return false;

    KDEBUG("Uniform offset %d", Uniform.Offset);

    // Release the old texture
    Texture* OldTexture = Instance->Textures[Uniform.Offset];
    TextureSystem::ReleaseTexture(OldTexture);

    Property.Set(NewTexture);
    // Instance->Textures[Uniform.Offset] = NewTexture;
    Instance->Dirty = true;

    return true;
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
    MaterialData Data;
    Data.Name = "Materials.Default";
    Data.FilePath = "Material.Default.kmt";
    Data.ShaderAsset = ShaderSystem::GetDefaultShader()->Path;
    Data.Properties["DiffuseColor"] = MaterialProperty(0, Vec4fOne);
    Data.Properties["DiffuseSampler"] = MaterialProperty(1, TextureSystem::GetDefaultDiffuseTexture());

    KASSERT(MaterialSystem::CreateMaterialWithData(Data));
}

static bool LoadMaterialFromFileInternal(const String& FilePath, MaterialData* Data)
{
    FileHandle Handle;
    bool Result = filesystem::OpenFile(FilePath, kraft::FILE_OPEN_MODE_READ, true, &Handle);
    if (!Result)
    {
        return false;
    }

    uint64 BufferSize = kraft::filesystem::GetFileSize(&Handle) + 1;
    uint8* FileDataBuffer = (uint8*)Malloc(BufferSize, kraft::MemoryTag::MEMORY_TAG_FILE_BUF, true);
    filesystem::ReadAllBytes(&Handle, &FileDataBuffer);
    filesystem::CloseFile(&Handle);

    Lexer Lexer;
    Lexer.Create((char*)FileDataBuffer);

    while (true)
    {
        Token Token = Lexer.NextToken();
        if (Token.Type == TokenType::TOKEN_TYPE_IDENTIFIER)
        {
            if (Token.MatchesKeyword("Material"))
            {
                if (!Lexer.ExpectToken(Token, TokenType::TOKEN_TYPE_IDENTIFIER))
                {
                    return false;
                }

                Data->Name = Token.String();
                if (!Lexer.ExpectToken(Token, TokenType::TOKEN_TYPE_OPEN_BRACE))
                {
                    return false;
                }

                // Parse material block
                while (!Lexer.EqualsToken(Token, TokenType::TOKEN_TYPE_CLOSE_BRACE))
                {
                    if (Token.MatchesKeyword("DiffuseColor"))
                    {
                        if (!Lexer.ExpectToken(Token, TokenType::TOKEN_TYPE_IDENTIFIER))
                        {
                            return false;
                        }

                        if (Token.MatchesKeyword("vec4"))
                        {
                            if (!Lexer.ExpectToken(Token, TokenType::TOKEN_TYPE_OPEN_PARENTHESIS))
                            {
                                return false;
                            }

                            float DiffuseColor[4];
                            int Index = 0;
                            while (!Lexer.EqualsToken(Token, TokenType::TOKEN_TYPE_CLOSE_PARENTHESIS))
                            {
                                if (Token.Type == TokenType::TOKEN_TYPE_COMMA)
                                {
                                    continue;
                                }
                                else if (Token.Type == TokenType::TOKEN_TYPE_NUMBER)
                                {
                                    DiffuseColor[Index++] = Token.FloatValue;
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
                        if (!Lexer.ExpectToken(Token, TokenType::TOKEN_TYPE_STRING))
                        {
                            return false;
                        }

                        const String& TexturePath = Token.String();
                        Texture* Texture = TextureSystem::AcquireTexture(TexturePath);
                        if (!Texture)
                        {
                            KERROR("[MaterialSystem::CreateMaterial]: Failed to load texture %s for material %s. Using default texture.", *TexturePath, *FilePath);
                            Texture = TextureSystem::GetDefaultDiffuseTexture();
                        }

                        Data->Properties["DiffuseSampler"] = Texture;
                    }
                    else if (Token.MatchesKeyword("Shader"))
                    {
                        if (!Lexer.ExpectToken(Token, TokenType::TOKEN_TYPE_STRING))
                        {
                            return false;
                        }

                        Data->ShaderAsset = Token.String();
                    }
                    else
                    {
                        KERROR("Material has an invalid field %s", *Token.String());
                    }
                }
            }
        }
        else if (Token.Type == TokenType::TOKEN_TYPE_END_OF_STREAM)
        {
            break;
        }
    }

    return true;
}

}