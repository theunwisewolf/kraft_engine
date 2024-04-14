#pragma once

#include "math/kraft_math.h"
#include "core/kraft_core.h"
#include "resources/kraft_resource_types.h"
#include "renderer/shaderfx/kraft_shaderfx.h"

namespace kraft
{

struct MaterialSystemConfig
{
    uint32 MaxMaterialsCount;
};

struct MaterialData
{
    bool   AutoRelease;
    String Name;
    String DiffuseTextureMapName;
    String ShaderAsset;
    Vec4f  DiffuseColor;
};

struct MaterialSystem
{
    static void Init(MaterialSystemConfig Config);
    static void Shutdown();

    static Material* GetDefaultMaterial();
    static Material* AcquireMaterial(const String& Name);
    static Material* AcquireMaterial(const char* Name);
    static Material* AcquireMaterialWithData(MaterialData Data);
    static void ReleaseMaterial(const char* name);
    static void ReleaseMaterial(Material *material);
    static void DestroyMaterial(Material *material);
    static void ApplyGlobalProperties(Material* Material, const Mat4f& Projection, const Mat4f& View);
    static void ApplyInstanceProperties(Material* Material);
    static void ApplyLocalProperties(Material* Material, const Mat4f& Model);

    static bool SetTexture(Material* Material, const String& Key, const String& TexturePath);
    static bool SetTexture(Material* Material, const String& Key, Texture* Texture);
    
    template<typename T>
    static bool SetProperty(Material* Material, const String& Name, T Value)
    {
        return SetProperty(Material, Name, (void*)Value, sizeof(T));
    }

    static bool SetProperty(Material* Material, const String& Name, void* Value, uint64 Size);
};

}