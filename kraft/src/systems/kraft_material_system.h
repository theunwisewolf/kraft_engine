#pragma once

#include <core/kraft_core.h>

namespace kraft::renderer {
template<typename>
struct Handle;
}

namespace kraft {

struct MaterialData;
struct Material;
struct Texture;

struct MaterialSystemConfig
{
    uint32 MaxMaterialsCount;
};

struct MaterialSystem
{
    static void Init(MaterialSystemConfig Config);
    static void Shutdown();

    static Material* GetDefaultMaterial();
    static Material* CreateMaterialFromFile(const String& Path);
    static Material* CreateMaterialWithData(const MaterialData& Data);
    static void      DestroyMaterial(const String& Name);
    static void      DestroyMaterial(Material* material);
    static void      ApplyGlobalProperties(Material* Material, const Matrix<float32, 4, 4>& Projection, const Matrix<float32, 4, 4>& View);
    static void      ApplyInstanceProperties(Material* Material);
    static void      ApplyLocalProperties(Material* Material, const Matrix<float32, 4, 4>& Model);

    static bool SetTexture(Material* Instance, const String& Key, const String& TexturePath);
    static bool SetTexture(Material* Instance, const String& Key, renderer::Handle<Texture> Texture);

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