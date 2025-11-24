#include "kraft_shader_system.h"

#include <containers/kraft_array.h>
#include <containers/kraft_hashmap.h>
#include <core/kraft_asserts.h>
#include <core/kraft_log.h>
#include <renderer/kraft_renderer_frontend.h>

// TODO: REMOVE
#include <core/kraft_base_includes.h>
#include <renderer/kraft_renderer_types.h>
#include <shaderfx/kraft_shaderfx_includes.h>

#include <resources/kraft_resource_types.h>

namespace kraft {

static void ReleaseShaderInternal(u32 Index);
static u32  AddUniform(Shader* shader, String8 name, u32 location, u32 offset, u32 size, r::ShaderDataType data_type, r::ShaderUniformScope::Enum scope);

struct ShaderReference
{
    u32    RefCount;
    bool   AutoRelease;
    Shader Shader;
};

struct ShaderSystemState
{
    ArenaAllocator* arena;

    u32              MaxShaderCount;
    ShaderReference* Shaders;
    // HashMap<String, u32> IndexMapping;
    u32     CurrentShaderID;
    Shader* CurrentShader;
};

static ShaderSystemState* State = nullptr;

void ShaderSystem::Init(u32 MaxShaderCount)
{
    ArenaAllocator* arena = CreateArena({ .ChunkSize = KRAFT_SIZE_MB(64), .Alignment = 64, .Tag = MEMORY_TAG_SHADER_SYSTEM });

    // u32 SizeRequirement = sizeof(ShaderSystemState) + sizeof(ShaderReference) * MaxShaderCount;
    // char*  RawMemory = (char*)kraft::Malloc(SizeRequirement, MEMORY_TAG_SHADER_SYSTEM, true);
    State = ArenaPush(arena, ShaderSystemState);
    State->arena = arena;
    State->Shaders = ArenaPushArray(arena, ShaderReference, MaxShaderCount);
    State->MaxShaderCount = MaxShaderCount;
    // State->IndexMapping = HashMap<String, u32>();
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

    DestroyArena(State->arena);

    KINFO("[ShaderSystem::Shutdown]: Shutting down shader system");
}

Shader* ShaderSystem::AcquireShader(String8 shader_path, r::Handle<r::RenderPass> render_pass_handle, bool auto_release)
{
    for (i32 i = 0; i < State->MaxShaderCount; i++)
    {
        if (StringEqual(State->Shaders[i].Shader.Path, shader_path))
        {
            State->Shaders[i].RefCount++;
            return &State->Shaders[i].Shader;
        }
    }

    int FreeIndex = -1;
    for (u32 i = 0; i < State->MaxShaderCount; i++)
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
    if (!shaderfx::LoadShaderFX(State->arena, shader_path, &Reference->Shader.ShaderEffect))
    {
        KWARN("[ShaderSystem::AcquireShader]: Failed to load %S", shader_path);
        return nullptr;
    }

    // State->IndexMapping[shader_path] = FreeIndex;
    Reference->Shader.ID = FreeIndex; // Cache the index
    Reference->Shader.Path = shader_path;
    Reference->RefCount = 1;
    Reference->AutoRelease = auto_release;

    u32 GlobalResourceBindingsCount = 2; // TODO (amn): Don't hardcode
    Reference->Shader.UniformCacheMapping = HashMap<u64, u32>();
    Reference->Shader.UniformCache = Array<ShaderUniform>();

    const shaderfx::ShaderEffect&         Effect = Reference->Shader.ShaderEffect;
    const shaderfx::RenderPassDefinition& Pass = Effect.render_pass_def;
    Reference->Shader.UniformCache.Reserve(GlobalResourceBindingsCount + (Pass.resources != nullptr ? Pass.resources->binding_count : 0) + Pass.contant_buffers->field_count);
    g_Renderer->CreateRenderPipeline(&Reference->Shader, render_pass_handle);

    // Cache the uniforms
    // The instance uniforms begin after the global data

    // Collect global resources
    for (int i = 0; i < Effect.global_resource_count; i++)
    {
        const auto* resource_bindings = Effect.global_resources[i].bindings;
        for (int j = 0; j < Effect.global_resources[i].binding_count; j++)
        {
            const auto& binding = resource_bindings[j];
            if (binding.type == r::ResourceType::UniformBuffer)
            {
                u32         cursor = 0;
                const auto& buffer_definition = Effect.uniform_buffers[binding.parent_index];
                for (int field_idx = 0; field_idx < buffer_definition.field_count; field_idx++)
                {
                    const auto& field = buffer_definition.fields[field_idx];
                    cursor = math::AlignUp(cursor, r::ShaderDataType::GetAlignment(field.type));
                    AddUniform(&Reference->Shader, field.name, field_idx, cursor, r::ShaderDataType::SizeOf(field.type), field.type, r::ShaderUniformScope::Global);

                    cursor += r::ShaderDataType::SizeOf(field.type);
                }
            }
            else if (binding.type == r::ResourceType::StorageBuffer)
            {
                u32         cursor = 0;
                const auto& buffer_definition = Effect.storage_buffers[binding.parent_index];
                for (int field_idx = 0; field_idx < buffer_definition.field_count; field_idx++)
                {
                    const auto& field = buffer_definition.fields[field_idx];
                    cursor = math::AlignUp(cursor, r::ShaderDataType::GetAlignment(field.type));
                    AddUniform(&Reference->Shader, field.name, field_idx, cursor, r::ShaderDataType::SizeOf(field.type), field.type, r::ShaderUniformScope::Global);

                    cursor += r::ShaderDataType::SizeOf(field.type);
                }
            }
            else if (binding.type == r::ResourceType::Sampler)
            {
                // todo (amn)
            }
        }
    }

    // Collect all the local resources
    for (int i = 0; i < Effect.local_resource_count; i++)
    {
        const auto& resource_bindings = Effect.local_resources[i].bindings;
        for (int j = 0; j < Effect.local_resources[i].binding_count; j++)
        {
            const auto& binding = resource_bindings[j];
            if (binding.type == r::ResourceType::UniformBuffer)
            {
                u32         cursor = 0;
                const auto& buffer_definition = Effect.uniform_buffers[binding.parent_index];
                for (int field_idx = 0; field_idx < buffer_definition.field_count; field_idx++)
                {
                    const auto& field = buffer_definition.fields[field_idx];
                    cursor = math::AlignUp(cursor, r::ShaderDataType::GetAlignment(field.type));
                    AddUniform(&Reference->Shader, field.name, field_idx, cursor, r::ShaderDataType::SizeOf(field.type), field.type, r::ShaderUniformScope::Instance);

                    cursor += r::ShaderDataType::SizeOf(field.type);
                }
            }
            else if (binding.type == r::ResourceType::StorageBuffer)
            {
                u32         cursor = 0;
                const auto& buffer_definition = Effect.storage_buffers[binding.parent_index];
                for (int field_idx = 0; field_idx < buffer_definition.field_count; field_idx++)
                {
                    const auto& field = buffer_definition.fields[field_idx];
                    cursor = math::AlignUp(cursor, r::ShaderDataType::GetAlignment(field.type));
                    AddUniform(&Reference->Shader, field.name, field_idx, cursor, r::ShaderDataType::SizeOf(field.type), field.type, r::ShaderUniformScope::Instance);

                    cursor += r::ShaderDataType::SizeOf(field.type);
                }
            }
            else if (binding.type == r::ResourceType::Sampler)
            {
                // todo (amn)
            }
        }
    }

    for (auto it = Reference->Shader.UniformCacheMapping.ibegin(); it != Reference->Shader.UniformCacheMapping.iend(); it++)
    {
        const auto& uniform = Reference->Shader.UniformCache[it.value()];
        KDEBUG("%lld = %s Binding %d Offset %d", it.key(), r::ShaderDataType::String(uniform.DataType.UnderlyingType), uniform.Location, uniform.Offset);
    }

    // Cache constant buffers as local uniforms
    u32 offset = 0;
    for (u64 i = 0; i < Pass.contant_buffers->field_count; i++)
    {
        auto& buffer_def = Pass.contant_buffers->fields[i];

        // Push constants must be aligned to 4 bytes
        u32 aligned_size = (u32)math::AlignUp(r::ShaderDataType::SizeOf(buffer_def.type), 4);
        AddUniform(&Reference->Shader, buffer_def.name, 0, offset, aligned_size, r::ShaderDataType::Invalid(), r::ShaderUniformScope::Local);

        offset += aligned_size;
    }

    return &State->Shaders[FreeIndex].Shader;
}

static u32 AddUniform(Shader* shader, String8 name, u32 location, u32 offset, u32 size, r::ShaderDataType data_type, r::ShaderUniformScope::Enum scope)
{
    ShaderUniform Uniform = {};
    Uniform.Location = location;
    Uniform.Offset = offset;
    Uniform.Stride = size;
    Uniform.Scope = scope;
    Uniform.DataType = data_type;

    u32 index = (u32)shader->UniformCache.Size();
    shader->UniformCacheMapping[FNV1AHashBytes(name)] = index;
    shader->UniformCache.Push(Uniform);

    return index;
}

static void ReleaseShaderInternal(u32 Index)
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

bool ShaderSystem::ReleaseShader(Shader* shader)
{
    for (i32 i = 0; i < State->MaxShaderCount; i++)
    {
        if (StringEqual(State->Shaders[i].Shader.Path, shader->Path))
        {
            if (i == 0)
            {
                KWARN("[ShaderSystem::ReleaseShader]: Default shader cannot be released");
                return false;
            }

            ReleaseShaderInternal(i);
            return true;
        }
    }

    return false;
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

Shader* ShaderSystem::BindByID(u32 ShaderID)
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

bool ShaderSystem::GetUniform(const Shader* shader, String8 name, ShaderUniform* uniform)
{
    auto It = shader->UniformCacheMapping.find(FNV1AHashBytes(name));
    if (It == shader->UniformCacheMapping.iend())
        return false;

    return GetUniformByIndex(shader, It->second.get(), uniform);
}

bool ShaderSystem::GetUniformByIndex(const Shader* Shader, u32 Index, ShaderUniform* Uniform)
{
    KASSERT(Index < Shader->UniformCache.Length);
    if (Index >= Shader->UniformCache.Length)
        return false;

    *Uniform = Shader->UniformCache[Index];
    return true;
}

template<>
bool ShaderSystem::SetUniform(String8 name, kraft::BufferView Value, bool Invalidate)
{
    return ShaderSystem::SetUniform(name, (void*)Value.Memory, Invalidate);
}

template<>
bool ShaderSystem::SetUniformByIndex(u32 Index, kraft::BufferView Value, bool Invalidate)
{
    return ShaderSystem::SetUniformByIndex(Index, (void*)Value.Memory, Invalidate);
}

bool ShaderSystem::SetUniform(String8 name, void* Value, bool Invalidate)
{
    if (State->CurrentShaderID == KRAFT_INVALID_ID)
        return false;

    auto It = State->CurrentShader->UniformCacheMapping.find(FNV1AHashBytes(name));
    if (It == State->CurrentShader->UniformCacheMapping.iend())
        return false;

    return SetUniformByIndex(It->second, Value, Invalidate);
}

bool ShaderSystem::SetUniformByIndex(u32 Index, void* Value, bool Invalidate)
{
    KASSERT(Index < State->CurrentShader->UniformCache.Length);

    return true;
}

} // namespace kraft