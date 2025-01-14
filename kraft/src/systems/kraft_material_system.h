#pragma once

#include <core/kraft_core.h>

namespace kraft::renderer {
template<typename>
struct Handle;

struct RenderPass;
}

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
    static Material* CreateMaterialFromFile(const String& Path, renderer::Handle<renderer::RenderPass> RenderPassHandle); // TODO: :cry: TEMP Renderpass handle
    static Material* CreateMaterialWithData(const MaterialDataIntermediateFormat& Data, renderer::Handle<renderer::RenderPass> RenderPassHandle);
    static void      DestroyMaterial(const String& Name);
    static void      DestroyMaterial(Material* material);

    static bool SetTexture(Material* Instance, const String& Key, const String& TexturePath);
    static bool SetTexture(Material* Instance, const String& Key, renderer::Handle<Texture> Texture);

    template<typename T>
    static bool SetProperty(Material* Instance, const String& Name, T Value);

    static uint8* GetMaterialsBuffer();
};

}