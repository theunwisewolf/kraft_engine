#include "kraft_material_system.h"

#include "core/kraft_memory.h"
#include "core/kraft_string.h"
#include "core/kraft_asserts.h"
#include "platform/kraft_filesystem.h"
#include "math/kraft_math.h"
#include "systems/kraft_texture_system.h"
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

static void _releaseMaterial(MaterialReference ref);
static void _createDefaultMaterials();
static bool _loadMaterialFromFile(const String& FilePath, MaterialData* Data);

void MaterialSystem::Init(MaterialSystemConfig config)
{
    uint32 totalSize = sizeof(MaterialSystemState) + sizeof(MaterialReference) * config.MaxMaterialsCount;

    State = (MaterialSystemState*)kraft::Malloc(totalSize, MEMORY_TAG_MATERIAL_SYSTEM, true);
    State->MaxMaterialCount = config.MaxMaterialsCount;
    State->MaterialReferences = (MaterialReference*)(State + sizeof(MaterialSystemState));

    _createDefaultMaterials();
}

void MaterialSystem::Shutdown()
{
    for (int i = 0; i < State->MaxMaterialCount; ++i)
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
        if (!_loadMaterialFromFile(FilePath, &Data))
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
        if (StringEqual(State->MaterialReferences[i].Material.Name, Data.Name))
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
        State->MaterialReferences[index].RefCount++;
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
        Reference->Material.DiffuseColor = Data.DiffuseColor;
        Reference->Material.DiffuseMap = TextureMap
        {
            Texture,
            TEXTURE_USE_MAP_DIFFUSE,
        };

        StringNCopy(Reference->Material.Name, Data.Name, sizeof(Reference->Material.Name));
        index = freeIndex;
    }

    KDEBUG("[MaterialSystem::AcquireMaterialWithData]: Acquired material %s (index = %d)", Data.Name, index);
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

    _releaseMaterial(State->MaterialReferences[index]);
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

    MemZero(material, sizeof(Material));
}

//
// Internal private methods
//

static void _releaseMaterial(MaterialReference ref)
{
    if (ref.RefCount == 0)
    {
        KWARN("[MaterialSystem::ReleaseMaterial]: Material %s already released!", ref.Material.Name);
        return;
    }

    ref.RefCount--;
    if (ref.RefCount == 0 && ref.AutoRelease)
    {
        MaterialSystem::DestroyMaterial(&ref.Material);
    }
}

static void _createDefaultMaterials()
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

static bool _loadMaterialFromFile(const String& FilePath, MaterialData* Data)
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
        if (Token.Type == TOKEN_TYPE_IDENTIFIER)
        {
            if (Token.MatchesKeyword("Material"))
            {
                if (!Lexer.ExpectToken(Token, TOKEN_TYPE_IDENTIFIER))
                {
                    return false;
                }

                StringNCopy(Data->Name, Token.Text, Token.Length);

                if (!Lexer.ExpectToken(Token, TOKEN_TYPE_OPEN_BRACE))
                {
                    return false;
                }

                // Parse material block
                while (!Lexer.EqualsToken(Token, TOKEN_TYPE_CLOSE_BRACE))
                {
                    if (Token.MatchesKeyword("DiffuseColor"))
                    {
                        if (!Lexer.ExpectToken(Token, TOKEN_TYPE_IDENTIFIER))
                        {
                            return false;
                        }

                        if (Token.MatchesKeyword("vec4"))
                        {
                            if (!Lexer.ExpectToken(Token, TOKEN_TYPE_OPEN_PARENTHESIS))
                            {
                                return false;
                            }

                            float DiffuseColor[4];
                            int Index = 0;
                            while (!Lexer.EqualsToken(Token, TOKEN_TYPE_CLOSE_PARENTHESIS))
                            {
                                if (Token.Type == TOKEN_TYPE_COMMA)
                                {
                                    continue;
                                }
                                else if (Token.Type == TOKEN_TYPE_NUMBER)
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
                        if (!Lexer.ExpectToken(Token, TOKEN_TYPE_STRING))
                        {
                            return false;
                        }

                        StringNCopy(Data->DiffuseTextureMapName, Token.Text, Token.Length);
                    }
                    else
                    {
                        KERROR("Material has an invalid field %s", *Token.String());
                    }
                }
            }
        }
        else if (Token.Type == TOKEN_TYPE_END_OF_STREAM)
        {
            break;
        }
    }

    return true;
}

}