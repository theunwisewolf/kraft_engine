#pragma once

#include "core/kraft_core.h"

namespace kraft {

namespace renderer {
template<typename>
struct Handle;

struct RenderPass;
struct GlobalShaderData;
struct Buffer;
}

struct Shader;
struct Material;
struct ShaderUniform;

struct ShaderSystem
{
    static void Init(uint32 MaxShadersCount);
    static void Shutdown();

    static Shader* AcquireShader(const String& ShaderPath, renderer::Handle<renderer::RenderPass> RenderPassHandle, bool AutoRelease = true);
    static void    ReleaseShader(Shader* Shader);
    static Shader* GetDefaultShader();
	static Shader* GetActiveShader();

    static Shader* Bind(Shader* Shader);
    static Shader* BindByID(uint32 ID);
    static void Unbind();

    // Sets the material instance to read the instance level uniform values from
    static void SetMaterialInstance(Material* Instance);

    // Finds a uniform by name in the given shader
    static bool GetUniform(const Shader* Shader, const String& Name, ShaderUniform& Uniform);
    static bool GetUniformByIndex(const Shader* Shader, uint32 Index, ShaderUniform& Uniform);

    template<typename T>
    static bool SetUniformByIndex(uint32 Index, T Value, bool Invalidate = false)
    {
        return ShaderSystem::SetUniformByIndex(Index, (void*)Value, Invalidate);
    }

    static bool SetUniformByIndex(uint32 Index, void* Value, bool Invalidate = false);

    template<typename T>
    static bool SetUniform(const String& Name, T Value, bool Invalidate = false)
    {
        return ShaderSystem::SetUniform(Name, (void*)Value, Invalidate);
    }

    static bool SetUniform(const String& Name, void* Value, bool Invalidate = false);

    static void ApplyGlobalProperties(renderer::Handle<renderer::Buffer> DstDataBuffer);
    static void ApplyInstanceProperties();
};

}