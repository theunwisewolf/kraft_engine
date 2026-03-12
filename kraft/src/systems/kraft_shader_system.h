#pragma once

#include "core/kraft_core.h"

namespace kraft {

namespace r {
template<typename>
struct Handle;

struct GlobalShaderData;
struct Buffer;
} // namespace renderer

struct Shader;
struct Material;
struct ShaderUniform;

struct ShaderSystem
{
    static void Init(u32 MaxShadersCount);
    static void Shutdown();

    static Shader* AcquireShader(String8 shader_path, bool auto_release = true);
    static bool    ReleaseShader(Shader* Shader);
    static Shader* GetDefaultShader();
    static Shader* GetActiveShader();

    // Set the active variant name (called by BeginRenderSurface)
    // When set, Bind/BindByID will look up this variant in each shader.
    // If a shader doesn't have the variant, Bind returns nullptr (caller should skip draw).
    static void SetActiveVariant(String8 variant_name);

    // Find a variant index by name in the given shader. Returns -1 if not found.
    static i32 FindVariantIndex(const Shader* shader, String8 variant_name);

    // Bind returns nullptr if the shader doesn't have the active variant
    static Shader* Bind(Shader* Shader);
    static Shader* BindByID(u32 ID);
    static void    Unbind();

    // Finds a uniform by name in the given shader
    static bool GetUniform(const Shader* shader, String8 name, ShaderUniform* uniform);
    static bool GetUniformByIndex(const Shader* shader, u32 Index, ShaderUniform* uniform);

    template<typename T>
    static bool SetUniformByIndex(u32 Index, T Value, bool Invalidate = false)
    {
        return ShaderSystem::SetUniformByIndex(Index, (void*)Value, Invalidate);
    }

    static bool SetUniformByIndex(u32 Index, void* Value, bool Invalidate = false);

    template<typename T>
    static bool SetUniform(String8 name, T Value, bool Invalidate = false)
    {
        return ShaderSystem::SetUniform(name, (void*)Value, Invalidate);
    }

    static bool SetUniform(String8 name, void* Value, bool Invalidate = false);
};

} // namespace kraft