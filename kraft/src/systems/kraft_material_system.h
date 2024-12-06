#pragma once

#include "core/kraft_core.h"
#include "math/kraft_math.h"
#include "renderer/shaderfx/kraft_shaderfx.h"
#include "resources/kraft_resource_types.h"

namespace kraft {

struct MaterialSystemConfig
{
    uint32 MaxMaterialsCount;
};

struct MaterialData
{
    bool                              AutoRelease;
    String                            Name;
    String                            FilePath;
    String                            ShaderAsset;
    HashMap<String, MaterialProperty> Properties;

    MaterialData()
    {
        Properties = HashMap<String, MaterialProperty>();
    }
};

struct MaterialSystem
{
    static void Init(MaterialSystemConfig Config);
    static void Shutdown();

    static Material* GetDefaultMaterial();
    static Material* CreateMaterialFromFile(const String& Path);
    static Material* CreateMaterialWithData(MaterialData Data);
    static void      DestroyMaterial(const String& Name);
    static void      DestroyMaterial(Material* material);
    static void      ApplyGlobalProperties(Material* Material, const Mat4f& Projection, const Mat4f& View);
    static void      ApplyInstanceProperties(Material* Material);
    static void      ApplyLocalProperties(Material* Material, const Mat4f& Model);

    static bool SetTexture(Material* Instance, const String& Key, const String& TexturePath);
    static bool SetTexture(Material* Instance, const String& Key, Handle<Texture> Texture);

    template<typename T>
    static bool SetProperty(Material* Instance, const String& Name, T Value)
    {
        MaterialProperty& Property = Instance->Properties[Name];
        Property.Set(Value);
        Instance->Dirty = true;

        return true;
    }

    static bool SetProperty(Material* Instance, const String& Name, void* Value);
};

}