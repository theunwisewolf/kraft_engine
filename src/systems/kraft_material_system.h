#pragma once

#include "math/kraft_math.h"
#include "core/kraft_core.h"
#include "resources/kraft_resource_types.h"

namespace kraft
{

struct MaterialSystemConfig
{
    uint32 MaxMaterialsCount;
};

struct MaterialData
{
    bool AutoRelease;
    TCHAR Name[KRAFT_MATERIAL_NAME_MAX_LENGTH];
    TCHAR DiffuseTextureMapName[KRAFT_TEXTURE_NAME_MAX_LENGTH];
    Vec4f DiffuseColor;
};

struct MaterialSystem
{
    static void Init(MaterialSystemConfig config);
    static void Shutdown();

    static Material* GetDefaultMaterial();
    static Material* AcquireMaterial(const TCHAR* name);
    static Material* AcquireMaterialWithData(MaterialData data);
    static void ReleaseMaterial(const TCHAR* name);
    static void ReleaseMaterial(Material *material);
    static void DestroyMaterial(Material *material);
};

}