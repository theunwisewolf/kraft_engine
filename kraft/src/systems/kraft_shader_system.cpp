#include "kraft_shader_system.h"

#include "kraft_includes.h"

namespace kraft {

static void ReleaseShaderInternal(u32 Index);
static u32 AddUniform(Shader* shader, String8 name, u32 location, u32 offset, u32 size, r::ShaderDataType data_type, r::ShaderUniformScope::Enum scope);
static void BuildUniformCache(Shader* shader);

static ShaderSystemState* shader_system_state = nullptr;

void ShaderSystem::Init(u32 max_shader_count) {
    ArenaAllocator* arena = CreateArena({.ChunkSize = KRAFT_SIZE_MB(64), .Alignment = 64, .Tag = MEMORY_TAG_SHADER_SYSTEM});

    // u32 SizeRequirement = sizeof(ShaderSystemState) + sizeof(ShaderReference) * max_shader_count;
    // char*  RawMemory = (char*)kraft::Malloc(SizeRequirement, MEMORY_TAG_SHADER_SYSTEM, true);
    shader_system_state = ArenaPush(arena, ShaderSystemState);
    shader_system_state->arena = arena;
    shader_system_state->shaders = ArenaPushArray(arena, ShaderReference, max_shader_count);
    shader_system_state->max_shader_count = max_shader_count;
    // shader_system_state->IndexMapping = HashMap<String, u32>();
    shader_system_state->current_shader_id = KRAFT_INVALID_ID;
    shader_system_state->current_shader = nullptr;
    shader_system_state->active_variant_name = {};
    shader_system_state->active_variant_index = -1;
    shader_system_state->watch = nullptr;

    // Load the default shader
    // KASSERT(AcquireShader("res/shaders/basic.kfx.bkfx", Handle<RenderPass>::Invalid()));

    KINFO("[ShaderSystem::Init]: Initialized shader system");
}

void ShaderSystem::Shutdown() {
    fs::DirectoryWatchStop(shader_system_state->watch);

    // Free the default shader
    ReleaseShaderInternal(0);

    DestroyArena(shader_system_state->arena);

    KINFO("[ShaderSystem::Shutdown]: Shutting down shader system");
}

Shader* ShaderSystem::AcquireShader(String8 shader_path, bool auto_release) {
    for (i32 i = 0; i < shader_system_state->max_shader_count; i++) {
        if (StringEqual(shader_system_state->shaders[i].shader.Path, shader_path)) {
            shader_system_state->shaders[i].ref_count++;
            return &shader_system_state->shaders[i].shader;
        }
    }

    int free_index = -1;
    for (u32 i = 0; i < shader_system_state->max_shader_count; i++) {
        if (shader_system_state->shaders[i].ref_count == 0) {
            free_index = i;
            break;
        }
    }

    if (free_index == -1) {
        KWARN("[ShaderSystem::AcquireShader]: Out of memory");
        return nullptr;
    }

    ShaderReference* reference = &shader_system_state->shaders[free_index];
    if (!shaderfx::LoadShaderFX(shader_system_state->arena, shader_path, &reference->shader.ShaderEffect)) {
        KWARN("[ShaderSystem::AcquireShader]: Failed to load %S", shader_path);
        return nullptr;
    }

    // shader_system_state->IndexMapping[shader_path] = FreeIndex;
    reference->shader.ID = free_index; // Cache the index
    reference->shader.Path = shader_path;
    reference->ref_count = 1;
    reference->auto_release = auto_release;

    BuildUniformCache(&reference->shader);
    g_Renderer->CreateRenderPipeline(&reference->shader);

    KINFO("Acquired shader %S in slot %d", shader_path, free_index);

    return &shader_system_state->shaders[free_index].shader;
}

static u32 AddUniform(Shader* shader, String8 name, u32 location, u32 offset, u32 size, r::ShaderDataType data_type, r::ShaderUniformScope::Enum scope) {
    ShaderUniform uniform = {};
    uniform.Location = location;
    uniform.Offset = offset;
    uniform.Stride = size;
    uniform.Scope = scope;
    uniform.DataType = data_type;

    u32 index = (u32)shader->UniformCache.Size();
    shader->UniformCacheMapping[FNV1AHashBytes(name)] = index;
    shader->UniformCache.Push(uniform);

    return index;
}

static void BuildUniformCache(Shader* shader) {
    u32 global_resource_bindings_count = 2; // TODO (amn): Don't hardcode
    shader->UniformCacheMapping = FlatHashMap<u64, u32>();
    shader->UniformCache = Array<ShaderUniform>();

    const shaderfx::ShaderEffect& Effect = shader->ShaderEffect;
    const shaderfx::VariantDefinition& variant0 = Effect.variants[0];
    shader->UniformCache.Reserve(global_resource_bindings_count + (variant0.resources != nullptr ? variant0.resources->binding_count : 0) + variant0.contant_buffers->field_count);

    // Collect global resources
    for (int i = 0; i < Effect.global_resource_count; i++) {
        const auto* resource_bindings = Effect.global_resources[i].bindings;
        for (int j = 0; j < Effect.global_resources[i].binding_count; j++) {
            const auto& binding = resource_bindings[j];
            if (binding.type == r::ResourceType::UniformBuffer) {
                u32 cursor = 0;
                const auto& buffer_definition = Effect.uniform_buffers[binding.parent_index];
                for (int field_idx = 0; field_idx < buffer_definition.field_count; field_idx++) {
                    const auto& field = buffer_definition.fields[field_idx];
                    cursor = math::AlignUp(cursor, r::ShaderDataType::GetAlignment(field.type));
                    AddUniform(shader, field.name, field_idx, cursor, r::ShaderDataType::SizeOf(field.type), field.type, r::ShaderUniformScope::Global);
                    cursor += r::ShaderDataType::SizeOf(field.type);
                }
            } else if (binding.type == r::ResourceType::StorageBuffer) {
                u32 cursor = 0;
                const auto& buffer_definition = Effect.storage_buffers[binding.parent_index];
                for (int field_idx = 0; field_idx < buffer_definition.field_count; field_idx++) {
                    const auto& field = buffer_definition.fields[field_idx];
                    cursor = math::AlignUp(cursor, r::ShaderDataType::GetAlignment(field.type));
                    AddUniform(shader, field.name, field_idx, cursor, r::ShaderDataType::SizeOf(field.type), field.type, r::ShaderUniformScope::Global);
                    cursor += r::ShaderDataType::SizeOf(field.type);
                }
            }
        }
    }

    // Collect local resources
    for (int i = 0; i < Effect.local_resource_count; i++) {
        const auto& resource_bindings = Effect.local_resources[i].bindings;
        for (int j = 0; j < Effect.local_resources[i].binding_count; j++) {
            const auto& binding = resource_bindings[j];
            if (binding.type == r::ResourceType::UniformBuffer) {
                u32 cursor = 0;
                const auto& buffer_definition = Effect.uniform_buffers[binding.parent_index];
                for (int field_idx = 0; field_idx < buffer_definition.field_count; field_idx++) {
                    const auto& field = buffer_definition.fields[field_idx];
                    cursor = math::AlignUp(cursor, r::ShaderDataType::GetAlignment(field.type));
                    AddUniform(shader, field.name, field_idx, cursor, r::ShaderDataType::SizeOf(field.type), field.type, r::ShaderUniformScope::Instance);
                    cursor += r::ShaderDataType::SizeOf(field.type);
                }
            } else if (binding.type == r::ResourceType::StorageBuffer) {
                u32 cursor = 0;
                const auto& buffer_definition = Effect.storage_buffers[binding.parent_index];
                for (int field_idx = 0; field_idx < buffer_definition.field_count; field_idx++) {
                    const auto& field = buffer_definition.fields[field_idx];
                    cursor = math::AlignUp(cursor, r::ShaderDataType::GetAlignment(field.type));
                    AddUniform(shader, field.name, field_idx, cursor, r::ShaderDataType::SizeOf(field.type), field.type, r::ShaderUniformScope::Instance);
                    cursor += r::ShaderDataType::SizeOf(field.type);
                }
            }
        }
    }

    // Cache constant buffers as local uniforms
    u32 offset = 0;
    for (u64 i = 0; i < variant0.contant_buffers->field_count; i++) {
        auto& buffer_def = variant0.contant_buffers->fields[i];
        u32 aligned_size = (u32)math::AlignUp(r::ShaderDataType::SizeOf(buffer_def.type), 4);
        AddUniform(shader, buffer_def.name, 0, offset, aligned_size, r::ShaderDataType::Invalid(), r::ShaderUniformScope::Local);
        offset += aligned_size;
    }
}

bool ShaderSystem::ReloadShader(Shader* shader) {
    if (!shader)
        return false;

    KINFO("[ShaderSystem::ReloadShader]: Reloading shader '%S'", shader->Path);

    g_Renderer->DestroyRenderPipeline(shader);
    if (!shaderfx::LoadShaderFX(shader_system_state->arena, shader->Path, &shader->ShaderEffect)) {
        KERROR("[ShaderSystem::ReloadShader]: Failed to reload %S", shader->Path);
        return false;
    }

    // Rebuild the uniform cache
    BuildUniformCache(shader);

    g_Renderer->CreateRenderPipeline(shader);

    KSUCCESS("[ShaderSystem::ReloadShader]: Successfully reloaded '%S'", shader->Path);
    return true;
}

void ShaderSystem::ReloadAllShaders() {
    for (u32 i = 0; i < shader_system_state->max_shader_count; i++) {
        if (shader_system_state->shaders[i].ref_count > 0) {
            ReloadShader(&shader_system_state->shaders[i].shader);
        }
    }
}

static void RecompileShaderFile(String8 file_path) {
    KINFO("[ShaderSystem]: Detected change in '%S', recompiling...", file_path);

    TempArena scratch = ScratchBegin(&shader_system_state->arena, 1);

    ProcessOptions options = {};
    options.program = shader_system_state->shader_compiler_path;
    options.arguments = &file_path;
    options.argument_count = 1;
    Process* compiler = ProcessStart(&options);
    if (!compiler) {
        KERROR("[ShaderSystem]: Failed to launch shader compiler at path: %S", shader_system_state->shader_compiler_path);
        ScratchEnd(scratch);
        return;
    }

    ProcessWait(compiler, KRAFT_WAIT_FOREVER);
    i32 exit_code = ProcessExitCode(compiler);
    ProcessFree(compiler);
    if (exit_code != 0) {
        KERROR("[ShaderSystem]: Shader compilation failed for '%S' (exit code %d)", file_path, exit_code);
        ScratchEnd(scratch);
        return;
    }

    KSUCCESS("[ShaderSystem]: Recompiled '%S'", file_path);

    String8 clean_path = fs::CleanPath(scratch.arena, file_path);
    for (u32 i = 0; i < shader_system_state->max_shader_count; i++) {
        ShaderReference* ref = &shader_system_state->shaders[i];
        if (ref->ref_count > 0 && StringEndsWith(ref->shader.ShaderEffect.resource_path, clean_path)) {
            ShaderSystem::ReloadShader(&ref->shader);
        }
    }

    ScratchEnd(scratch);
}

void ShaderSystem::EnableHotReload(ArenaAllocator* arena, String8 shader_compiler_path, String8 watch_directory) {
    String8 absolute_path = fs::AbsolutePath(arena, shader_compiler_path);
    shader_system_state->shader_compiler_path = absolute_path;

    KINFO("Shader Hot-Reload is enabled\nShader compiler: %S", absolute_path);

    shader_system_state->watch_directory = ArenaPushString8Copy(shader_system_state->arena, watch_directory);
    shader_system_state->watch = fs::DirectoryWatchStart(watch_directory, true, nullptr, nullptr);
}

void ShaderSystem::ProcessHotReload() {
    // Editors often write a file more than once when saving, so wait for the changes to settle
    f64 seconds_since_last = 0.0;
    f64 seconds_since_first = 0.0;
    if (!fs::DirectoryWatchHasChanges(shader_system_state->watch, &seconds_since_last, &seconds_since_first) || seconds_since_last < 0.1) {
        return;
    }

    TempArena scratch = ScratchBegin(&shader_system_state->arena, 1);
    fs::DirectoryWatchChanges changes = fs::DirectoryWatchTakeChanges(shader_system_state->watch, scratch.arena);
    for (u32 i = 0; i < changes.path_count; i++) {
        if (!StringEndsWith(changes.paths[i], String8Raw(".kfx"))) {
            continue;
        }

        bool repeated = false;
        for (u32 j = 0; j < i && !repeated; j++) {
            repeated = StringEqual(changes.paths[j], changes.paths[i]);
        }

        String8 file_path = fs::PathJoin(scratch.arena, shader_system_state->watch_directory, changes.paths[i]);
        if (!repeated && fs::FileExists(file_path)) {
            RecompileShaderFile(file_path);
        }
    }

    ScratchEnd(scratch);
}

static void ReleaseShaderInternal(u32 index) {
    ShaderReference* reference = &shader_system_state->shaders[index];
    if (reference->ref_count > 0) {
        reference->ref_count--;
    }

    if (reference->auto_release && reference->ref_count == 0) {
        g_Renderer->DestroyRenderPipeline(&reference->shader);

        reference->auto_release = false;
        reference->shader = {};
    }
}

bool ShaderSystem::ReleaseShader(Shader* shader) {
    for (i32 i = 0; i < shader_system_state->max_shader_count; i++) {
        if (StringEqual(shader_system_state->shaders[i].shader.Path, shader->Path)) {
            if (i == 0) {
                KWARN("[ShaderSystem::ReleaseShader]: Default shader cannot be released");
                return false;
            }

            ReleaseShaderInternal(i);
            return true;
        }
    }

    return false;
}

Shader* ShaderSystem::GetDefaultShader() {
    return &shader_system_state->shaders[0].shader;
}

Shader* ShaderSystem::GetActiveShader() {
    return shader_system_state->current_shader;
}

void ShaderSystem::SetActiveVariant(String8 variant_name) {
    shader_system_state->active_variant_name = variant_name;
    shader_system_state->active_variant_index = -1; // Reset cached index
}

i32 ShaderSystem::FindVariantIndex(const Shader* shader, String8 variant_name) {
    const shaderfx::ShaderEffect& effect = shader->ShaderEffect;
    for (u32 i = 0; i < effect.variant_count; i++) {
        if (StringEqual(effect.variants[i].name, variant_name)) {
            return (i32)i;
        }
    }

    return -1;
}

Shader* ShaderSystem::Bind(Shader* Shader) {
    // Resolve variant index
    u32 variant_index = 0;
    if (shader_system_state->active_variant_name.count > 0) {
        i32 idx = FindVariantIndex(Shader, shader_system_state->active_variant_name);
        if (idx < 0) {
            // Shader doesn't have the requested variant - skip
            return nullptr;
        }

        variant_index = (u32)idx;
    }

    g_Renderer->UseShader(Shader, variant_index);

    shader_system_state->current_shader_id = Shader->ID;
    shader_system_state->current_shader = Shader;

    return Shader;
}

Shader* ShaderSystem::BindByID(u32 id) {
    KASSERT(id < shader_system_state->max_shader_count);
    ShaderReference* reference = &shader_system_state->shaders[id];

    return ShaderSystem::Bind(&reference->shader);
}

void ShaderSystem::Unbind() {
    if (shader_system_state->current_shader) {
        shader_system_state->current_shader_id = KRAFT_INVALID_ID;
        shader_system_state->current_shader = nullptr;
    }
}

bool ShaderSystem::GetUniform(const Shader* shader, String8 name, ShaderUniform* uniform) {
    auto it = shader->UniformCacheMapping.find(FNV1AHashBytes(name));
    if (it == shader->UniformCacheMapping.end())
        return false;

    return GetUniformByIndex(shader, it->second, uniform);
}

bool ShaderSystem::GetUniformByIndex(const Shader* Shader, u32 Index, ShaderUniform* Uniform) {
    KASSERT(Index < Shader->UniformCache.Length);
    if (Index >= Shader->UniformCache.Length)
        return false;

    *Uniform = Shader->UniformCache[Index];
    return true;
}

template <> bool ShaderSystem::SetUniform(String8 name, kraft::BufferView Value, bool Invalidate) {
    return ShaderSystem::SetUniform(name, (void*)Value.Memory, Invalidate);
}

template <> bool ShaderSystem::SetUniformByIndex(u32 Index, kraft::BufferView Value, bool Invalidate) {
    return ShaderSystem::SetUniformByIndex(Index, (void*)Value.Memory, Invalidate);
}

bool ShaderSystem::SetUniform(String8 name, void* Value, bool Invalidate) {
    if (shader_system_state->current_shader_id == KRAFT_INVALID_ID) {
        KWARN("[SetUniform]: No shader is currently bound");
        return false;
    }

    auto it = shader_system_state->current_shader->UniformCacheMapping.find(FNV1AHashBytes(name));
    if (it == shader_system_state->current_shader->UniformCacheMapping.end()) {
        KWARN("[SetUniform]: Unknown uniform '%S' on shader '%S'", name, shader_system_state->current_shader->Path);
        return false;
    }

    return SetUniformByIndex(it->second, Value, Invalidate);
}

bool ShaderSystem::SetUniformByIndex(u32 Index, void* Value, bool Invalidate) {
    KASSERT(Index < shader_system_state->current_shader->UniformCache.Length);

    return true;
}

} // namespace kraft