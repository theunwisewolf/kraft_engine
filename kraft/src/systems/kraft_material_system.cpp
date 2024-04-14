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
    bool     AutoRelease;
    Material Material;
};

struct MaterialSystemState
{
    uint32             MaxMaterialCount;
    MaterialReference *MaterialReferences;
};

static MaterialSystemState* State = 0;

static void ReleaseMaterialInternal(uint32 Index);
static void CreateDefaultMaterialsInternal();
static bool LoadMaterialFromFileInternal(const String& FilePath, MaterialData* Data);

void MaterialSystem::Init(MaterialSystemConfig config)
{
    uint32 totalSize = sizeof(MaterialSystemState) + sizeof(MaterialReference) * config.MaxMaterialsCount;

    State = (MaterialSystemState*)kraft::Malloc(totalSize, MEMORY_TAG_MATERIAL_SYSTEM, true);
    State->MaxMaterialCount = config.MaxMaterialsCount;
    State->MaterialReferences = (MaterialReference*)(State + sizeof(MaterialSystemState));

    CreateDefaultMaterialsInternal();
}

void MaterialSystem::Shutdown()
{
    // Free the default material
    ReleaseMaterialInternal(0);

    for (int i = 1; i < State->MaxMaterialCount; ++i)
    {
        MaterialReference* ref = &State->MaterialReferences[i];
        if (ref->RefCount > 0)
        {
            MaterialSystem::DestroyMaterial(&ref->Material);
        }
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

Material* MaterialSystem::AcquireMaterial(const String& Name)
{
    return AcquireMaterial(*Name);
}

Material* MaterialSystem::AcquireMaterial(const char* Name)
{
    int index = -1;

    // Look for an already acquired material
    for (uint32 i = 0; i < State->MaxMaterialCount; ++i)
    {
        if (StringEqual(State->MaterialReferences[i].Material.Name, Name))
        {
            index = i;
            break;
        }
    }

    // Material already exists in the cache
    // Just increase the ref-count
    if (index > -1)
    {
        KINFO("[MaterialSystem::AcquireMaterial]: Material already acquired; Reusing!");
        State->MaterialReferences[index].RefCount++;
    }
    else
    {
        String FilePath = Name;
        if (!FilePath.EndsWith(".kmt"))
        {
            FilePath = FilePath + ".kmt";
        }

        MaterialData Data;
        if (!LoadMaterialFromFileInternal(FilePath, &Data))
        {
            KINFO("[MaterialSystem::AcquireMaterial]: Failed to parse material %s", *FilePath);
            return nullptr;
        }

        return AcquireMaterialWithData(Data);
    }

    return &State->MaterialReferences[index].Material;
}

Material* MaterialSystem::AcquireMaterialWithData(MaterialData Data)
{
    int freeIndex = -1;
    int index = -1;

    // Look for an already acquired material
    for (uint32 i = 0; i < State->MaxMaterialCount; ++i)
    {
        if (Data.Name == State->MaterialReferences[i].Material.Name)
        {
            index = i;
            break;
        }

        if (freeIndex == -1 && State->MaterialReferences[i].RefCount == 0)
        {
            freeIndex = i;
        }
    }

    // Material already exists in the cache
    // Just increase the ref-count
    if (index > -1)
    {
        KINFO("[MaterialSystem::AcquireMaterialWithData]: Material already acquired; Reusing!");
        // Default material's refcount always stays at 1
        if (index > 0)
        {
            State->MaterialReferences[index].RefCount++;
        }
    }
    else
    {
        if (freeIndex == -1)
        {
            KERROR("[MaterialSystem::AcquireMaterialWithData]: Failed to find a free slot; Out-of-memory!");
            return NULL;
        }

        MaterialReference* Reference = &State->MaterialReferences[freeIndex];
        Reference->RefCount++;
        Reference->AutoRelease = Data.AutoRelease;
        Texture* Texture = TextureSystem::AcquireTexture(Data.DiffuseTextureMapName, Data.AutoRelease);
        if (!Texture)
        {
            Texture = TextureSystem::GetDefaultDiffuseTexture();
        }

        Reference->Material.ID = freeIndex;

        // Load the shader
        Shader* Shader = ShaderSystem::AcquireShader(Data.ShaderAsset);
        if (!Shader)
        {
            Shader = ShaderSystem::GetDefaultShader();
        }

        Reference->Material.Shader = Shader;
        Reference->Material.Dirty = false;
        Reference->Material.DiffuseColor = Data.DiffuseColor;
        Reference->Material.DiffuseMap = TextureMap
        {
            Texture,
            TEXTURE_USE_MAP_DIFFUSE,
        };

        StringNCopy(Reference->Material.Name, Data.Name.Data(), sizeof(Reference->Material.Name));
        index = freeIndex;
    }

    KDEBUG("[MaterialSystem::AcquireMaterialWithData]: Acquired material %s (index = %d)", *Data.Name, index);
    return &State->MaterialReferences[index].Material;
}

void MaterialSystem::ReleaseMaterial(const char* name)
{
    KDEBUG("[MaterialSystem::ReleaseMaterial]: Releasing material %s", name);

    int index = -1;
    for (uint32 i = 0; i < State->MaxMaterialCount; ++i)
    {
        if (StringEqual(State->MaterialReferences[i].Material.Name, name))
        {
            index = i;
            break;
        }
    }

    if (index == -1)
    {
        KERROR("[MaterialSystem::ReleaseMaterial]: Unknown material %s", name);
        return;
    }

    if (index == 0)
    {
        KWARN("[MaterialSystem::ReleaseMaterial]: Default material cannot be released!");
        return;
    }

    ReleaseMaterialInternal(index);
}

void MaterialSystem::ReleaseMaterial(Material *material)
{
    KASSERTM(material, "[MaterialSystem::ReleaseMaterial]: Material is null");

    ReleaseMaterial(material->Name);
}

void MaterialSystem::DestroyMaterial(Material *material)
{
    KDEBUG("[MaterialSystem::DestroyMaterial]: Destroying material %s", material->Name);

    if (material->DiffuseMap.Texture)
    {
        TextureSystem::ReleaseTexture(material->DiffuseMap.Texture->Name);
    }

    if (material->Shader)
    {
        ShaderSystem::ReleaseShader(material->Shader);
        material->Shader = 0;
    }

    MemZero(material, sizeof(Material));
}

void MaterialSystem::ApplyGlobalProperties(Material* Material, const Mat4f& Projection, const Mat4f& View)
{
    ShaderSystem::SetUniformByIndex(0, Projection);
    ShaderSystem::SetUniformByIndex(1, View);

    ShaderSystem::ApplyGlobalProperties();
}

void MaterialSystem::ApplyInstanceProperties(Material* Material)
{
    ShaderSystem::SetUniform("DiffuseColor", Material->DiffuseColor, Material->Dirty);
    ShaderSystem::SetUniform("DiffuseSampler", Material->DiffuseMap.Texture, Material->Dirty);

    ShaderSystem::ApplyInstanceProperties();
    Material->Dirty = false;
}

void MaterialSystem::ApplyLocalProperties(Material* Material, const Mat4f& Model)
{
    ShaderSystem::SetUniform("Model", Model);
}

bool MaterialSystem::SetTexture(Material* Material, const String& Key, const String& TexturePath)
{
    Texture* Texture = TextureSystem::AcquireTexture(TexturePath, true);
    if (!Texture)
    {
        return false;
    }

    // Release the old texture
    TextureSystem::ReleaseTexture(Material->DiffuseMap.Texture);
    
    return SetTexture(Material, Key, Texture);
}

bool MaterialSystem::SetTexture(Material* Material, const String& Key, Texture* Texture)
{
    KASSERT(Texture);
    return SetProperty(Material, Key, Texture);
}

template<>
bool MaterialSystem::SetProperty(Material* Material, const String& Name, BufferView Value)
{
    return SetProperty(Material, Name, (void*)Value.Memory, Value.Size);
}

bool MaterialSystem::SetProperty(Material* Material, const String& Name, void* Value, uint64 Size)
{
    if (Name == "DiffuseSampler")
    {
        Material->DiffuseMap.Texture = (Texture*)Value;
    }
    else if (Name == "DiffuseColor")
    {
        Vec4f* Color = (Vec4f*)Value;
        Material->DiffuseColor = *Color;
    }
    else
    {
        return false;
    }

    Material->Dirty = true;
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
        KWARN("[MaterialSystem::ReleaseMaterial]: Material %s already released!", Reference.Material.Name);
        return;
    }

    Reference.RefCount--;
    if (Reference.RefCount == 0 && Reference.AutoRelease)
    {
        MaterialSystem::DestroyMaterial(&Reference.Material);
    }
}

static void CreateDefaultMaterialsInternal()
{
    MaterialReference* ref = &State->MaterialReferences[0];
    ref->RefCount = 1;
    
    Material* material = &ref->Material;
    material->ID = 0;
    material->DiffuseColor = Vec4fOne;
    material->DiffuseMap = TextureMap
    {
        TextureSystem::GetDefaultDiffuseTexture(),
        TEXTURE_USE_MAP_DIFFUSE,
    };

    StringNCopy(material->Name, "default-material", sizeof(material->Name));
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

                            Data->DiffuseColor = Vec4f(DiffuseColor[0], DiffuseColor[1], DiffuseColor[2], DiffuseColor[3]);
                        }
                    }
                    else if (Token.MatchesKeyword("DiffuseMapName"))
                    {
                        if (!Lexer.ExpectToken(Token, TokenType::TOKEN_TYPE_STRING))
                        {
                            return false;
                        }

                        Data->DiffuseTextureMapName = Token.String();
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