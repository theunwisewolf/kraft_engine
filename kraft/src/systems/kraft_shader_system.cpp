#include "kraft_shader_system.h"

#include <containers/kraft_array.h>
#include <containers/kraft_hashmap.h>
#include <core/kraft_asserts.h>
#include <core/kraft_log.h>
#include <renderer/kraft_renderer_frontend.h>
#include <shaderfx/kraft_shaderfx.h>

#include <resources/kraft_resource_types.h>
#include <shaderfx/kraft_shaderfx_types.h>

namespace kraft {

using namespace renderer;

static void   ReleaseShaderInternal(uint32 Index);
static uint32 AddUniform(Shader* Shader, String Name, uint32 Location, uint32 Offset, uint32 Size, ShaderDataType DataType, ShaderUniformScope::Enum Scope);

struct ShaderReference
{
    uint32 RefCount;
    bool   AutoRelease;
    Shader Shader;
};

struct ShaderSystemState
{
    uint32                  MaxShaderCount;
    ShaderReference*        Shaders;
    HashMap<String, uint32> IndexMapping;
    uint32                  CurrentShaderID;
    Shader*                 CurrentShader;
};

static ShaderSystemState* State = nullptr;

void ShaderSystem::Init(uint32 MaxShaderCount)
{
    uint32 SizeRequirement = sizeof(ShaderSystemState) + sizeof(ShaderReference) * MaxShaderCount;
    char*  RawMemory = (char*)kraft::Malloc(SizeRequirement, MEMORY_TAG_SHADER_SYSTEM, true);

    State = (ShaderSystemState*)RawMemory;
    State->Shaders = (ShaderReference*)(RawMemory + sizeof(ShaderSystemState));
    State->MaxShaderCount = MaxShaderCount;
    State->IndexMapping = HashMap<String, uint32>();
    State->CurrentShaderID = KRAFT_INVALID_ID;
    State->CurrentShader = nullptr;

    // Load the default shader
    // KASSERT(AcquireShader("res/shaders/basic.kfx.bkfx", Handle<RenderPass>::Invalid()));

    KINFO("[ShaderSystem::Init]: Initialized shader system");
}

void ShaderSystem::Shutdown()
{
    // Free the default shader
    ReleaseShaderInternal(0);

    uint32 SizeRequirement = sizeof(ShaderSystemState) + sizeof(ShaderReference) * State->MaxShaderCount;
    kraft::Free(State, SizeRequirement, MEMORY_TAG_SHADER_SYSTEM);

    KINFO("[ShaderSystem::Shutdown]: Shutting down shader system");
}

Shader* ShaderSystem::AcquireShader(const String& ShaderPath, Handle<RenderPass> RenderPassHandle, bool AutoRelease)
{
    auto It = State->IndexMapping.find(ShaderPath);
    if (It != State->IndexMapping.iend())
    {
        uint32 Index = It->second;
        if (Index > 0)
        {
            State->Shaders[Index].RefCount++;
        }

        return &State->Shaders[Index].Shader;
    }

    int FreeIndex = -1;
    for (uint32 i = 0; i < State->MaxShaderCount; i++)
    {
        if (State->Shaders[i].RefCount == 0)
        {
            FreeIndex = i;
            break;
        }
    }

    if (FreeIndex == -1)
    {
        KWARN("[ShaderSystem::AcquireShader]: Out of memory");
        return nullptr;
    }

    ShaderReference* Reference = &State->Shaders[FreeIndex];
    if (!shaderfx::LoadShaderFX(ShaderPath, &Reference->Shader.ShaderEffect))
    {
        KWARN("[ShaderSystem::AcquireShader]: Failed to load %s", *ShaderPath);
        return nullptr;
    }

    State->IndexMapping[ShaderPath] = FreeIndex;
    Reference->Shader.ID = FreeIndex; // Cache the index
    Reference->Shader.Path = ShaderPath;
    Reference->RefCount = 1;
    Reference->AutoRelease = AutoRelease;

    // TODO (amn): What to do with other passes?
    const uint32 PassIndex = 0;
    uint32       GlobalResourceBindingsCount = 2; // TODO (amn): Don't hardcode
    Reference->Shader.UniformCacheMapping = HashMap<String, uint32>();
    Reference->Shader.UniformCache = Array<ShaderUniform>();

    const shaderfx::ShaderEffect&         Effect = Reference->Shader.ShaderEffect;
    const shaderfx::RenderPassDefinition& Pass = Effect.RenderPasses[PassIndex];
    Reference->Shader.UniformCache.Reserve(GlobalResourceBindingsCount + (Pass.Resources != nullptr ? Pass.Resources->ResourceBindings.Length : 0) + Pass.ConstantBuffers->Fields.Length);
    g_Renderer->CreateRenderPipeline(&Reference->Shader, PassIndex, RenderPassHandle);

    // Cache the uniforms
    // The instance uniforms begin after the global data

    // Collect global resources
    for (int i = 0; i < Effect.GlobalResources.Length; i++)
    {
        const auto& resource_bindings = Effect.GlobalResources[i].ResourceBindings;
        for (int j = 0; j < resource_bindings.Length; j++)
        {
            const auto& binding = resource_bindings[j];
            if (binding.Type == ResourceType::UniformBuffer)
            {
                uint32      cursor = 0;
                const auto& buffer_definition = Effect.UniformBuffers[binding.ParentIndex];
                for (int field_idx = 0; field_idx < buffer_definition.Fields.Length; field_idx++)
                {
                    const auto& field = buffer_definition.Fields[field_idx];
                    cursor = math::AlignUp(cursor, ShaderDataType::AlignOf(field.Type));
                    AddUniform(&Reference->Shader, field.Name, field_idx, cursor, ShaderDataType::SizeOf(field.Type), field.Type, ShaderUniformScope::Global);

                    cursor += ShaderDataType::SizeOf(field.Type);
                }
            }
            else if (binding.Type == ResourceType::StorageBuffer)
            {
                uint32      cursor = 0;
                const auto& buffer_definition = Effect.StorageBuffers[binding.ParentIndex];
                for (int field_idx = 0; field_idx < buffer_definition.Fields.Length; field_idx++)
                {
                    const auto& field = buffer_definition.Fields[field_idx];
                    cursor = math::AlignUp(cursor, ShaderDataType::AlignOf(field.Type));
                    AddUniform(&Reference->Shader, field.Name, field_idx, cursor, ShaderDataType::SizeOf(field.Type), field.Type, ShaderUniformScope::Global);

                    cursor += ShaderDataType::SizeOf(field.Type);
                }
            }
            else if (binding.Type == ResourceType::Sampler)
            {
                // todo (amn)
            }
        }
    }

    // Collect all the local resources
    for (int i = 0; i < Effect.LocalResources.Length; i++)
    {
        const auto& resource_bindings = Effect.LocalResources[i].ResourceBindings;
        for (int j = 0; j < resource_bindings.Length; j++)
        {
            const auto& binding = resource_bindings[j];
            if (binding.Type == ResourceType::UniformBuffer)
            {
                uint32      cursor = 0;
                const auto& buffer_definition = Effect.UniformBuffers[binding.ParentIndex];
                for (int field_idx = 0; field_idx < buffer_definition.Fields.Length; field_idx++)
                {
                    const auto& field = buffer_definition.Fields[field_idx];
                    cursor = math::AlignUp(cursor, ShaderDataType::AlignOf(field.Type));
                    AddUniform(&Reference->Shader, field.Name, field_idx, cursor, ShaderDataType::SizeOf(field.Type), field.Type, ShaderUniformScope::Instance);

                    cursor += ShaderDataType::SizeOf(field.Type);
                }
            }
            else if (binding.Type == ResourceType::StorageBuffer)
            {
                uint32      cursor = 0;
                const auto& buffer_definition = Effect.StorageBuffers[binding.ParentIndex];
                for (int field_idx = 0; field_idx < buffer_definition.Fields.Length; field_idx++)
                {
                    const auto& field = buffer_definition.Fields[field_idx];
                    cursor = math::AlignUp(cursor, ShaderDataType::AlignOf(field.Type));
                    AddUniform(&Reference->Shader, field.Name, field_idx, cursor, ShaderDataType::SizeOf(field.Type), field.Type, ShaderUniformScope::Instance);

                    cursor += ShaderDataType::SizeOf(field.Type);
                }
            }
            else if (binding.Type == ResourceType::Sampler)
            {
                // todo (amn)
            }
        }
    }

    for (auto it = Reference->Shader.UniformCacheMapping.ibegin(); it != Reference->Shader.UniformCacheMapping.iend(); it++)
    {
        const auto& uniform = Reference->Shader.UniformCache[it.value()];
        KDEBUG("%s = %s Binding %d Offset %d", *it.key(), ShaderDataType::String(uniform.DataType.UnderlyingType), uniform.Location, uniform.Offset);
    }

    // Cache constant buffers as local uniforms
    uint32 Offset = 0;
    for (uint64 i = 0; i < Pass.ConstantBuffers->Fields.Length; i++)
    {
        auto& ConstantBuffer = Pass.ConstantBuffers->Fields[i];

        // Push constants must be aligned to 4 bytes
        uint32 AlignedSize = (uint32)math::AlignUp(ShaderDataType::SizeOf(ConstantBuffer.Type), 4);
        AddUniform(&Reference->Shader, ConstantBuffer.Name, 0, Offset, AlignedSize, ShaderDataType::Invalid(), ShaderUniformScope::Local);

        Offset += AlignedSize;
    }

    return &State->Shaders[FreeIndex].Shader;
}

static uint32 AddUniform(Shader* Shader, String Name, uint32 Location, uint32 Offset, uint32 Size, ShaderDataType DataType, ShaderUniformScope::Enum Scope)
{
    ShaderUniform Uniform = {};
    Uniform.Location = Location;
    Uniform.Offset = Offset;
    Uniform.Stride = Size;
    Uniform.Scope = Scope;
    Uniform.DataType = DataType;

    uint32 Index = (uint32)Shader->UniformCache.Size();
    Shader->UniformCacheMapping[Name] = Index;
    Shader->UniformCache.Push(Uniform);

    return Index;
}

static void ReleaseShaderInternal(uint32 Index)
{
    ShaderReference* Reference = &State->Shaders[Index];
    if (Reference->RefCount > 0)
    {
        Reference->RefCount--;
    }

    if (Reference->AutoRelease && Reference->RefCount == 0)
    {
        g_Renderer->DestroyRenderPipeline(&Reference->Shader);

        Reference->AutoRelease = false;
        Reference->Shader = {};
    }
}

void ShaderSystem::ReleaseShader(Shader* Shader)
{
    auto It = State->IndexMapping.find(Shader->Path);
    if (It == State->IndexMapping.iend())
    {
        KWARN("[ShaderSystem::ReleaseShader]: Shader not found %s", *Shader->Path);
        return;
    }

    uint32 Index = It->second;
    if (Index == 0)
    {
        KWARN("[ShaderSystem::ReleaseShader]: Default shader cannot be released");
        return;
    }

    ReleaseShaderInternal(Index);
}

Shader* ShaderSystem::GetDefaultShader()
{
    return &State->Shaders[0].Shader;
}

Shader* ShaderSystem::GetActiveShader()
{
    return State->CurrentShader;
}

Shader* ShaderSystem::Bind(Shader* Shader)
{
    if (State->CurrentShaderID != Shader->ID)
    {
        g_Renderer->UseShader(Shader);

        State->CurrentShaderID = Shader->ID;
        State->CurrentShader = Shader;
    }

    return Shader;
}

Shader* ShaderSystem::BindByID(uint32 ShaderID)
{
    KASSERT(ShaderID < State->MaxShaderCount);
    ShaderReference* Reference = &State->Shaders[ShaderID];

    ShaderSystem::Bind(&Reference->Shader);

    return &Reference->Shader;
}

void ShaderSystem::Unbind()
{
    if (State->CurrentShader)
    {
        State->CurrentShaderID = KRAFT_INVALID_ID;
        State->CurrentShader = nullptr;
    }
}

bool ShaderSystem::GetUniform(const Shader* Shader, const String& Name, ShaderUniform* Uniform)
{
    auto It = Shader->UniformCacheMapping.find(Name);
    if (It == Shader->UniformCacheMapping.iend())
        return false;

    return GetUniformByIndex(Shader, It->second.get(), Uniform);
}

bool ShaderSystem::GetUniformByIndex(const Shader* Shader, uint32 Index, ShaderUniform* Uniform)
{
    KASSERT(Index < Shader->UniformCache.Length);
    if (Index >= Shader->UniformCache.Length)
        return false;

    *Uniform = Shader->UniformCache[Index];
    return true;
}

template<>
bool ShaderSystem::SetUniform(const String& Name, kraft::BufferView Value, bool Invalidate)
{
    return ShaderSystem::SetUniform(Name, (void*)Value.Memory, Invalidate);
}

template<>
bool ShaderSystem::SetUniformByIndex(uint32 Index, kraft::BufferView Value, bool Invalidate)
{
    return ShaderSystem::SetUniformByIndex(Index, (void*)Value.Memory, Invalidate);
}

bool ShaderSystem::SetUniform(const String& Name, void* Value, bool Invalidate)
{
    if (State->CurrentShaderID == KRAFT_INVALID_ID)
        return false;

    auto It = State->CurrentShader->UniformCacheMapping.find(Name);
    if (It == State->CurrentShader->UniformCacheMapping.iend())
        return false;

    return SetUniformByIndex(It->second, Value, Invalidate);
}

bool ShaderSystem::SetUniformByIndex(uint32 Index, void* Value, bool Invalidate)
{
    KASSERT(Index < State->CurrentShader->UniformCache.Length);

    return true;
}

} // namespace kraft