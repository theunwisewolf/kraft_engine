#pragma once

#include <core/kraft_core.h>

namespace kraft::r {
template <typename> struct Handle;
} // namespace kraft::r

namespace kraft {

struct MaterialDataIntermediateFormat;
struct Material;
struct Texture;
struct RendererOptions;

struct MaterialReference {
    Material material;
    u32 ref_count;
};

struct MaterialSystemState {
    ArenaAllocator* arena;

    u16 material_buffer_size;
    u16 max_materials_count;
    bool dirty = false;

    MaterialReference* material_references;
    CArray(u8) materials_buffer;
};

struct MaterialSystem {
    static void Init(const RendererOptions& options);
    static void Shutdown();

    static Material* GetDefaultMaterial();
    static Material* CreateMaterialFromFile(String8 path);
    static Material* CreateMaterialWithData(const MaterialDataIntermediateFormat& data);
    static void DestroyMaterial(String8 name);
    static void DestroyMaterial(Material* instance);

    static bool SetTexture(Material* instance, String8 key, String8 texture_path);
    static bool SetTexture(Material* instance, String8 key, r::Handle<Texture> texture);

    template <typename T> static bool SetProperty(Material* instance, String8 name, T value);

    static bool IsDirty();
    static void SetDirty(bool value);
    static u8* GetMaterialsBuffer();
};

} // namespace kraft