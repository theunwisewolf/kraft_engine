#include "kraft_material_system.h"

#include "core/kraft_memory.h"
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
    MaterialReference *Materials;
};

static MaterialSystemState* State = 0;

static void _releaseMaterial(MaterialReference ref);

void MaterialSystem::Init(MaterialSystemConfig config)
{
    uint32 totalSize = sizeof(MaterialSystemConfig) + sizeof(MaterialReference) * config.MaxMaterialsCount;

    State = (MaterialSystemState*)kraft::Malloc(totalSize, MEMORY_TAG_MATERIAL_SYSTEM, true);
    State->MaxMaterialCount = config.MaxMaterialsCount;
    State->Materials = (MaterialReference*)(State + sizeof(MaterialSystemState));
}

void MaterialSystem::Shutdown()
{
    uint32 totalSize = sizeof(MaterialSystemConfig) + sizeof(MaterialReference) * State->MaxMaterialCount;
    kraft::Free(State, totalSize, MEMORY_TAG_MATERIAL_SYSTEM);
}

Material* MaterialSystem::AcquireMaterial(const char* name)
{

}

Material* MaterialSystem::AcquireMaterialWithData(MaterialData data)
{
    int index = -1;

    // Look for an already acquired material
    for (int i = 0; i < State->MaxMaterialCount; ++i)
    {
        if (State->Materials[i].Material.Name == data.Name)
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
            if (State->Materials[i].RefCount == 0)
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

    Material *material = &State->Materials[index].Material;
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
        if (StringEqual(State->Materials[i].Material.Name, name))
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

    _releaseMaterial(State->Materials[index]);
}

void ReleaseMaterial(Material *material)
{
    KDEBUG("[MaterialSystem::ReleaseMaterial]: Releasing material %s", material->Name);

    int index = -1;
    for (int i = 0; i < State->MaxMaterialCount; ++i)
    {
        if (State->Materials[i].Material.ID == material->ID)
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

    _releaseMaterial(State->Materials[index]);
}

void DestroyMaterial(Material *material)
{
    KDEBUG("[MaterialSystem::DestroyMaterial]: Destroying material %s", material->Name);

    if (material->DiffuseMap.Texture)
    {

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

}