#pragma once

#include <core/kraft_core.h>

namespace kraft::r {
template<typename>
struct Handle;
} // namespace kraft::r

namespace kraft {

struct MaterialDataIntermediateFormat;
struct Material;
struct Texture;
struct RendererOptions;

struct MaterialSystem
{
    static void Init(const RendererOptions& Opts);
    static void Shutdown();

    static Material* GetDefaultMaterial();
    static Material* CreateMaterialFromFile(String8 path);
    static Material* CreateMaterialWithData(const MaterialDataIntermediateFormat& Data);
    static void      DestroyMaterial(String8 name);
    static void      DestroyMaterial(Material* material);

    static bool SetTexture(Material* Instance, String8 Key, const String& TexturePath);
    static bool SetTexture(Material* Instance, String8 Key, r::Handle<Texture> Texture);

    template<typename T>
    static bool SetProperty(Material* Instance, String8 Name, T Value);

    static uint8* GetMaterialsBuffer();
};

} // namespace kraft