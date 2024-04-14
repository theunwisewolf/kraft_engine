#pragma once

#include "core/kraft_core.h"
#include "core/kraft_string.h"

namespace kraft
{

struct Shader;

struct ShaderSystem
{
    static void Init(uint32 MaxShadersCount);
    static void Shutdown();

    static Shader* AcquireShader(const String& ShaderPath, bool AutoRelease = true);
    static void ReleaseShader(Shader* Shader);
    static Shader* GetDefaultShader();

    static void Bind(Shader* Shader);
    static void BindByID(uint32 ID);
    static void Unbind();

    template<typename T>
    static bool SetUniformByIndex(uint32 Index, T Value, bool Invalidate = false)
    {
        return ShaderSystem::SetUniformByIndex(Index, (void*)Value, sizeof(T), Invalidate);
    }

    static bool SetUniformByIndex(uint32 Index, void* Value, uint64 Size, bool Invalidate = false);

    template<typename T>
    static bool SetUniform(const String& Name, T Value, bool Invalidate = false)
    {
        return ShaderSystem::SetUniform(Name, (void*)Value, sizeof(T), Invalidate);
    }

    static bool SetUniform(const String& Name, void* Value, uint64 Size, bool Invalidate = false);

    static void ApplyGlobalProperties();
    static void ApplyInstanceProperties();
};

}