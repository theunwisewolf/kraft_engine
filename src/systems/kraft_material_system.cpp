#include "kraft_material_system.h"

#include "core/kraft_memory.h"
#include "math/kraft_math.h"
#include "core/kraft_string.h"
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

Material* MaterialSystem::AcquireMaterial(const char* name)
{
    // TODO: Read from file
    return nullptr;
}

Material* MaterialSystem::AcquireMaterialWithData(MaterialData data)
{
    int index = -1;

    // Look for an already acquired material
    for (int i = 0; i < State->MaxMaterialCount; ++i)
    {
        if (StringEqual(State->MaterialReferences[i].Material.Name, data.Name))
        {
            index = i;
            break;
        }
    }

    // Otherwise find a free slot
    if (index == -1)
    {
        for (int i = 0; i < State->MaxMaterialCount; ++i)
        {
            if (State->MaterialReferences[i].RefCount == 0)
            {
                index = i;
                break;
            }
        }
    }

    if (index == -1)
    {
        KERROR("[MaterialSystem::AcquireMaterialWithData]: Failed to find a free slot!");
        return NULL;
    }

    Material *material = &State->MaterialReferences[index].Material;
    Texture* texture = TextureSystem::AcquireTexture(data.DiffuseTextureMapName);
    if (!texture)
    {
        texture = TextureSystem::GetDefaultDiffuseTexture();
    }

    material->ID = index;
    material->DiffuseColor = data.DiffuseColor;
    material->DiffuseMap = TextureMap
    {
        .Texture = texture,
        .Usage = TEXTURE_USE_MAP_DIFFUSE,
    };

    StringNCopy(material->Name, data.Name, sizeof(material->Name));

    return material;
}

void ReleaseMaterial(const char* name)
{
    KDEBUG("[MaterialSystem::ReleaseMaterial]: Releasing material %s", name);

    int index = -1;
    for (int i = 0; i < State->MaxMaterialCount; ++i)
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

void ReleaseMaterial(Material *material)
{
    KDEBUG("[MaterialSystem::ReleaseMaterial]: Releasing material %s", material->Name);

    int index = -1;
    for (int i = 0; i < State->MaxMaterialCount; ++i)
    {
        if (State->MaterialReferences[i].Material.ID == material->ID)
        {
            index = i;
            break;
        }
    }

    if (index == -1)
    {
        KERROR("[MaterialSystem::ReleaseMaterial]: Unknown material %s", material->Name);
        return;
    }

    _releaseMaterial(State->MaterialReferences[index]);
}

void DestroyMaterial(Material *material)
{
    KDEBUG("[MaterialSystem::DestroyMaterial]: Destroying material %s", material->Name);

    if (material->DiffuseMap.Texture)
    {
        TextureSystem::ReleaseTexture(material->DiffuseMap.Texture->Name);
    }
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
        DestroyMaterial(&ref.Material);
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
        .Texture = TextureSystem::GetDefaultDiffuseTexture(),
        .Usage = TEXTURE_USE_MAP_DIFFUSE,
    };

    StringNCopy(material->Name, "default-material", sizeof(material->Name));
}

}