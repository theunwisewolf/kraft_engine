#pragma once

#include "core/kraft_core.h"

namespace kraft {

namespace r {
template<typename>
struct Handle;

struct GlobalShaderData;
struct Buffer;
} // namespace r

struct Shader;
struct Material;
struct ShaderUniform;

struct ShaderReference
{
    u32    ref_count;
    bool   auto_release;
    u64    source_last_modified_time;
    Shader shader;
};

struct ShaderSystemState
{
    ArenaAllocator* arena;

    u32              max_shader_count;
    ShaderReference* shaders;
    // HashMap<String, u32> IndexMapping;
    u32     current_shader_id;
    Shader* current_shader;

    // Active variant for surface-driven variant selection
    String8 active_variant_name;
    i32     active_variant_index; // Cached index for the current shader, -1 if not resolved

    // Hot-reload
    String8 shader_compiler_path;
    i32     watch_index;
};

struct ShaderSystem
{
    static void Init(u32 max_shader_count);
    static void Shutdown();

    static Shader* AcquireShader(String8 shader_path, bool auto_release = true);
    static bool    ReleaseShader(Shader* shader);
    static bool    ReloadShader(Shader* shader);
    static void    ReloadAllShaders();
    static Shader* GetDefaultShader();
    static Shader* GetActiveShader();

    // Shader hot-reloading
    // shader_compiler_path: path to the KraftShaderCompiler executable
    // watch_directory: directory containing .kfx shader sources
    static void EnableHotReload(ArenaAllocator* arena, String8 shader_compiler_path, String8 watch_directory);
    static void ProcessHotReload();

    // Set the active variant name (called by BeginRenderSurface)
    // When set, Bind/BindByID will look up this variant in each shader.
    // If a shader doesn't have the variant, Bind returns nullptr (caller should skip draw).
    static void SetActiveVariant(String8 variant_name);

    // Find a variant index by name in the given shader. Returns -1 if not found.
    static i32 FindVariantIndex(const Shader* shader, String8 variant_name);

    // Bind returns nullptr if the shader doesn't have the active variant
    static Shader* Bind(Shader* shader);
    static Shader* BindByID(u32 id);
    static void    Unbind();

    // Finds a uniform by name in the given shader
    static bool GetUniform(const Shader* shader, String8 name, ShaderUniform* uniform);
    static bool GetUniformByIndex(const Shader* shader, u32 Index, ShaderUniform* uniform);

    template<typename T>
    static bool SetUniformByIndex(u32 index, T value, bool invalidate = false)
    {
        return ShaderSystem::SetUniformByIndex(index, (void*)value, invalidate);
    }

    static bool SetUniformByIndex(u32 index, void* value, bool invalidate = false);

    template<typename T>
    static bool SetUniform(String8 name, T value, bool invalidate = false)
    {
        return ShaderSystem::SetUniform(name, (void*)value, invalidate);
    }

    static bool SetUniform(String8 name, void* value, bool invalidate = false);
};

} // namespace kraft