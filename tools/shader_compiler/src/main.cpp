#include "containers/kraft_array.h"
#include "platform/kraft_filesystem.h"
#include "platform/kraft_platform.h"
#include <kraft.h>

#include <kraft_types.h>

#include <shaderc/shaderc.h>
#include <vendor/spirv-reflect/spirv_reflect.h>


#include <core/kraft_base_includes.h>
#include <platform/kraft_platform_includes.h>
#include <renderer/kraft_renderer_types.h>
#include <shaderfx/kraft_shaderfx_includes.h>

// #include <core/kraft_base_includes.cpp>
// #include <platform/kraft_platform_includes.cpp>
// #include <shaderfx/kraft_shaderfx_includes.cpp>

#define SPIRV_REFLECT_CHECK(x)                                                                                                                                                                         \
    do                                                                                                                                                                                                 \
    {                                                                                                                                                                                                  \
        if ((x) != SPV_REFLECT_RESULT_SUCCESS)                                                                                                                                                         \
        {                                                                                                                                                                                              \
            fprintf(stderr, "Failed at line %d.\n", __LINE__);                                                                                                                                         \
            exit(1);                                                                                                                                                                                   \
        }                                                                                                                                                                                              \
    } while (0)

struct ApplicationState
{};

#define KRAFT_VERTEX_DEFINE       "VERTEX"
#define KRAFT_GEOMETRY_DEFINE     "GEOMETRY"
#define KRAFT_FRAGMENT_DEFINE     "FRAGMENT"
#define KRAFT_COMPUTE_DEFINE      "COMPUTE"
#define KRAFT_SHADERFX_ENABLE_VAL "1"

using namespace kraft;

struct ShaderFxCompilerOpts
{
    bool verbose = false;
    bool write_spirv_for_shaders = false;
    bool validate = true;
};

bool CompileShaderFX(ArenaAllocator* arena, String8 InputPath, String8 OutputPath, ShaderFxCompilerOpts compiler_opts);
bool CompileShaderFX(ArenaAllocator* arena, shaderfx::ShaderEffect* Shader, String8 OutputPath, ShaderFxCompilerOpts compiler_opts);

void SpirvErrorCallback(void* userdata, const char* error)
{
    KERROR("Encountered an error %s", error);
}

bool CompileShaderFX(ArenaAllocator* arena, String8 input_path, String8 output_path, ShaderFxCompilerOpts compiler_opts)
{
    KINFO("Compiling shaderfx %S", input_path);
    if (!fs::FileExists(input_path))
    {
        return false;
    }

    TempArena scratch = ScratchBegin(&arena, 1);
    String8   file_buf = fs::ReadAllBytes(scratch.arena, input_path);

    Lexer lexer;
    lexer.Create(file_buf);
    shaderfx::ShaderFXParser Parser;
    Parser.Verbose = compiler_opts.verbose;

    shaderfx::ShaderEffect effect;
    if (!Parser.Parse(arena, input_path, &lexer, &effect))
    {
        KERROR("Parsing failed\nLine: %d\nColumn: %d\nError: %S", Parser.ErrorLine, Parser.ErrorColumn, Parser.error_str);
        ScratchEnd(scratch);
        return false;
    }

    bool result = CompileShaderFX(arena, &effect, output_path, compiler_opts);

    // Validate the effect
    if (compiler_opts.validate)
    {
        KINFO("Validating shaderfx %S", input_path);
        shaderfx::ShaderEffect written_effect;
        shaderfx::LoadShaderFX(scratch.arena, output_path, &written_effect);
        if (!shaderfx::ValidateShaderFX(&written_effect, &effect))
        {
            KERROR("Validation failed!");
        }
    }

    ScratchEnd(scratch);

    return result;
}

u64 GetUniformBufferSize(shaderfx::UniformBufferDefinition* uniform_buffer)
{
    u64 size_in_bytes = 0;
    for (i32 i = 0; i < uniform_buffer->field_count; i++)
    {
        size_in_bytes += r::ShaderDataType::SizeOf(uniform_buffer->fields[i].type);
    }

    return size_in_bytes;
}

static bool CheckBufferDefinition(shaderfx::ShaderEffect* shader, shaderfx::UniformBufferDefinition* buffers, u32 buffer_count, shaderfx::ResourceBinding* binding_def)
{
    for (i32 i = 0; i < buffer_count; i++)
    {
        if (StringEqual(buffers[i].name, binding_def->name))
        {
            // Check if the size is incorrect
            u64 expected_size = GetUniformBufferSize(&buffers[i]);
            binding_def->size = expected_size;

            // Update the parent index
            binding_def->parent_index = i;

            return true;
        }
    }

    KERROR("'%S' shader compilation failed with error: Missing BufferDefinition for '%S'.", shader->resource_path, binding_def->name);

    return false;
}

struct ShaderIncludeUserData
{
    ArenaAllocator*         Arena;
    shaderfx::ShaderEffect* Shader;
};

static shaderc_include_result* ShaderIncludeResolverFunction(void* userdata, const char* requested_src, int Type, const char* requesting_src, size_t include_depth)
{
    ShaderIncludeUserData*  include_data = (ShaderIncludeUserData*)userdata;
    ArenaAllocator*         arena = include_data->Arena;
    shaderfx::ShaderEffect* shader = include_data->Shader;

    String8 shader_dir = fs::Dirname(arena, shader->resource_path);
    String8 included_filepath = fs::PathJoin(arena, shader_dir, String8FromCString((char*)requested_src));

    fs::FileHandle file;
    KASSERT(fs::OpenFile(included_filepath, fs::FILE_OPEN_MODE_READ, true, &file));
    u64 filesize = fs::GetFileSize(&file);
    u8* file_buf = ArenaPushArray(arena, u8, filesize);
    fs::ReadAllBytes(&file, &file_buf);

    shaderc_include_result* result = ArenaPush(arena, shaderc_include_result);
    result->content = (const char*)file_buf;
    result->content_length = filesize;

    String8 src_name = ArenaPushString8Copy(arena, included_filepath);
    result->source_name = (char*)src_name.ptr;
    result->source_name_length = src_name.count;

    return result;
}

static void ShaderIncludeResultReleaseFunction(void* user_data, shaderc_include_result* include_result)
{}

bool VerifyResources(shaderfx::ShaderEffect* shader, shaderfx::ResourceBindingsDefinition* resources, u32 resource_count)
{
    for (int i = 0; i < resource_count; i++)
    {
        shaderfx::ResourceBindingsDefinition* resource = &resources[i];
        for (int j = 0; j < resource->binding_count; j++)
        {
            if (resource->bindings[j].type == r::ResourceType::UniformBuffer)
            {
                if (false == CheckBufferDefinition(shader, shader->uniform_buffers, shader->uniform_buffer_count, &resource->bindings[j]))
                {
                    return false;
                }
            }
            else if (resource->bindings[j].type == r::ResourceType::StorageBuffer)
            {
                if (false == CheckBufferDefinition(shader, shader->storage_buffers, shader->storage_buffer_count, &resource->bindings[j]))
                {
                    return false;
                }
            }
        }
    }

    return true;
}

bool CompileShaderFX(ArenaAllocator* arena, shaderfx::ShaderEffect* shader, String8 output_path, ShaderFxCompilerOpts compiler_opts)
{
    // Basic verification
    if (!VerifyResources(shader, shader->local_resources, shader->local_resource_count))
        return false;

    if (!VerifyResources(shader, shader->global_resources, shader->global_resource_count))
        return false;

    shaderc_compiler_t           compiler = shaderc_compiler_initialize();
    shaderc_compilation_result_t result;
    shaderc_compile_options_t    shaderc_compile_opts;

    kraft::Buffer binary_output;
    binary_output.WriteString(shader->name);
    binary_output.WriteString(shader->resource_path);

    binary_output.Writeu32(shader->vertex_layout_count);
    for (u32 i = 0; i < shader->vertex_layout_count; i++)
    {
        binary_output.WriteString(shader->vertex_layouts[i].name);

        binary_output.Writeu32(shader->vertex_layouts[i].attribute_count);
        for (u32 j = 0; j < shader->vertex_layouts[i].attribute_count; j++)
        {
            binary_output.Writeu16(shader->vertex_layouts[i].attributes[j].location);
            binary_output.Writeu16(shader->vertex_layouts[i].attributes[j].binding);
            binary_output.Writeu16(shader->vertex_layouts[i].attributes[j].offset);
            // binary_output.WriteRaw(&shader->vertex_layouts[i].attributes[j].format, sizeof(r::ShaderDataType));
            binary_output.Writeu8(shader->vertex_layouts[i].attributes[j].format.UnderlyingType);
            binary_output.Writeu16(shader->vertex_layouts[i].attributes[j].format.ArraySize);
        }

        binary_output.Writeu32(shader->vertex_layouts[i].input_binding_count);
        for (u32 j = 0; j < shader->vertex_layouts[i].input_binding_count; j++)
        {
            binary_output.Writeu16(shader->vertex_layouts[i].input_bindings[j].binding);
            binary_output.Writeu16(shader->vertex_layouts[i].input_bindings[j].stride);
            binary_output.Writei32(shader->vertex_layouts[i].input_bindings[j].input_rate);
        }
    }

    binary_output.Writeu32(shader->local_resource_count);
    for (u32 i = 0; i < shader->local_resource_count; i++)
    {
        binary_output.WriteString(shader->local_resources[i].name);
        binary_output.Writeu32(shader->local_resources[i].binding_count);
        for (u32 j = 0; j < shader->local_resources[i].binding_count; j++)
        {
            binary_output.WriteString(shader->local_resources[i].bindings[j].name);
            binary_output.Writeu16(shader->local_resources[i].bindings[j].set);
            binary_output.Writeu16(shader->local_resources[i].bindings[j].binding);
            binary_output.Writeu16(shader->local_resources[i].bindings[j].size);
            binary_output.Writei16(shader->local_resources[i].bindings[j].parent_index);
            binary_output.Writei32(shader->local_resources[i].bindings[j].type);
            binary_output.Writei32(shader->local_resources[i].bindings[j].stage);
        }
    }

    binary_output.Writeu32(shader->global_resource_count);
    for (u32 i = 0; i < shader->global_resource_count; i++)
    {
        binary_output.WriteString(shader->global_resources[i].name);
        binary_output.Writeu32(shader->global_resources[i].binding_count);
        for (u32 j = 0; j < shader->global_resources[i].binding_count; j++)
        {
            binary_output.WriteString(shader->global_resources[i].bindings[j].name);
            binary_output.Writeu16(shader->global_resources[i].bindings[j].set);
            binary_output.Writeu16(shader->global_resources[i].bindings[j].binding);
            binary_output.Writeu16(shader->global_resources[i].bindings[j].size);
            binary_output.Writei16(shader->global_resources[i].bindings[j].parent_index);
            binary_output.Writei32(shader->global_resources[i].bindings[j].type);
            binary_output.Writei32(shader->global_resources[i].bindings[j].stage);
        }
    }

    binary_output.Writeu32(shader->constant_buffer_count);
    for (u32 i = 0; i < shader->constant_buffer_count; i++)
    {
        binary_output.WriteString(shader->constant_buffers[i].name);
        binary_output.Writeu32(shader->constant_buffers[i].field_count);
        for (u32 j = 0; j < shader->constant_buffers[i].field_count; j++)
        {
            binary_output.WriteString(shader->constant_buffers[i].fields[j].name);
            binary_output.Writei32(shader->constant_buffers[i].fields[j].stage);
            binary_output.WriteRaw(&shader->constant_buffers[i].fields[j].type, sizeof(r::ShaderDataType));
        }
    }

    binary_output.Writeu32(shader->uniform_buffer_count);
    for (u32 i = 0; i < shader->uniform_buffer_count; i++)
    {
        binary_output.WriteString(shader->uniform_buffers[i].name);
        binary_output.Writeu32(shader->uniform_buffers[i].field_count);
        for (u32 j = 0; j < shader->uniform_buffers[i].field_count; j++)
        {
            binary_output.WriteString(shader->uniform_buffers[i].fields[j].name);
            binary_output.WriteRaw(&shader->uniform_buffers[i].fields[j].type, sizeof(r::ShaderDataType));
        }
    }

    binary_output.Writeu32(shader->storage_buffer_count);
    for (u32 i = 0; i < shader->storage_buffer_count; i++)
    {
        binary_output.WriteString(shader->storage_buffers[i].name);
        binary_output.Writeu32(shader->storage_buffers[i].field_count);
        for (u32 j = 0; j < shader->storage_buffers[i].field_count; j++)
        {
            binary_output.WriteString(shader->storage_buffers[i].fields[j].name);
            binary_output.WriteRaw(&shader->storage_buffers[i].fields[j].type, sizeof(r::ShaderDataType));
        }
    }

    // RenderState
    binary_output.WriteString(shader->render_state.name);
    binary_output.Writei32(shader->render_state.cull_mode);
    binary_output.Writei32(shader->render_state.z_test_op);
    binary_output.Writebool(shader->render_state.z_write_enable);
    binary_output.Writebool(shader->render_state.blend_enable);
    binary_output.WriteRaw(&shader->render_state.blend_mode, sizeof(r::BlendState));
    binary_output.Writei32(shader->render_state.polygon_mode);
    binary_output.Writef32(shader->render_state.line_width);

    // RenderPass
    // binary_output.Write(shader->render_pass_def);

    // We don't have to write the vertex layouts or render states
    // They are written as part of the renderpass definition
    binary_output.WriteString(shader->render_pass_def.name);

    // Write the vertex layout & render state offsets
    binary_output.Writei64((i64)(shader->render_pass_def.vertex_layout - &shader->vertex_layouts[0]));
    binary_output.Writei64((i64)(shader->local_resource_count > 0 ? shader->render_pass_def.resources - &shader->local_resources[0] : -1));
    binary_output.Writei64((i64)(shader->render_pass_def.contant_buffers - &shader->constant_buffers[0]));
    binary_output.Writei64((i64)(shader->render_pass_def.render_state - &shader->render_state));

    // Manually write the shader code fragments to the buffer
    binary_output.Writeu32(shader->render_pass_def.shader_stage_count);
    for (u32 j = 0; j < shader->render_pass_def.shader_stage_count; j++)
    {
        const shaderfx::RenderPassDefinition::ShaderDefinition& shader_def = shader->render_pass_def.shader_stages[j];
        if (compiler_opts.verbose)
        {
            KDEBUG("Compiling shaderstage %d for '%S'", shader_def.stage, shader->resource_path);
        }

        shaderc_compile_opts = shaderc_compile_options_initialize();
        ShaderIncludeUserData user_data = {
            .Arena = arena,
            .Shader = shader,
        };
        shaderc_compile_options_set_include_callbacks(shaderc_compile_opts, ShaderIncludeResolverFunction, ShaderIncludeResultReleaseFunction, &user_data);

        shaderc_shader_kind shader_kind;
        switch (shader_def.stage)
        {
            case r::ShaderStageFlags::SHADER_STAGE_FLAGS_VERTEX:
            {
                shader_kind = shaderc_vertex_shader;
                shaderc_compile_options_add_macro_definition(
                    shaderc_compile_opts, KRAFT_VERTEX_DEFINE, sizeof(KRAFT_VERTEX_DEFINE) - 1, KRAFT_SHADERFX_ENABLE_VAL, sizeof(KRAFT_SHADERFX_ENABLE_VAL) - 1
                );
            }
            break;

            case r::ShaderStageFlags::SHADER_STAGE_FLAGS_GEOMETRY:
            {
                shader_kind = shaderc_geometry_shader;
                shaderc_compile_options_add_macro_definition(
                    shaderc_compile_opts, KRAFT_GEOMETRY_DEFINE, sizeof(KRAFT_GEOMETRY_DEFINE) - 1, KRAFT_SHADERFX_ENABLE_VAL, sizeof(KRAFT_SHADERFX_ENABLE_VAL) - 1
                );
            }
            break;

            case r::ShaderStageFlags::SHADER_STAGE_FLAGS_FRAGMENT:
            {
                shader_kind = shaderc_fragment_shader;
                shaderc_compile_options_add_macro_definition(
                    shaderc_compile_opts, KRAFT_FRAGMENT_DEFINE, sizeof(KRAFT_FRAGMENT_DEFINE) - 1, KRAFT_SHADERFX_ENABLE_VAL, sizeof(KRAFT_SHADERFX_ENABLE_VAL) - 1
                );
            }
            break;

            case r::ShaderStageFlags::SHADER_STAGE_FLAGS_COMPUTE:
            {
                shader_kind = shaderc_compute_shader;
                shaderc_compile_options_add_macro_definition(
                    shaderc_compile_opts, KRAFT_COMPUTE_DEFINE, sizeof(KRAFT_COMPUTE_DEFINE) - 1, KRAFT_SHADERFX_ENABLE_VAL, sizeof(KRAFT_SHADERFX_ENABLE_VAL) - 1
                );
            }
            break;
        }

        result = shaderc_compile_into_spv(compiler, (char*)shader_def.code_fragment.code.ptr, shader_def.code_fragment.code.count, shader_kind, "shader", "main", shaderc_compile_opts);
        if (shaderc_result_get_compilation_status(result) == shaderc_compilation_status_success)
        {
            String8 spirv_binary = String8FromPtrAndLength((u8*)shaderc_result_get_bytes(result), shaderc_result_get_length(result));
            binary_output.Writei32(shader_def.stage);
            binary_output.WriteString(shader_def.code_fragment.name);
            binary_output.WriteString(spirv_binary);

            SpvReflectShaderModule module;
            SpvReflectResult       spv_reflect_result = spvReflectCreateShaderModule((size_t)shaderc_result_get_length(result), (const void*)shaderc_result_get_bytes(result), &module);
            KASSERT(spv_reflect_result == SPV_REFLECT_RESULT_SUCCESS);

            // Enumerate and extract shader's input variables
            uint32_t var_count = 0;
            SPIRV_REFLECT_CHECK(spvReflectEnumerateInputVariables(&module, &var_count, NULL));

            SpvReflectInterfaceVariable** input_vars = ArenaPushArray(arena, SpvReflectInterfaceVariable*, var_count);
            SPIRV_REFLECT_CHECK(spvReflectEnumerateInputVariables(&module, &var_count, input_vars));

            uint32 descriptor_set_count = 0;
            SPIRV_REFLECT_CHECK(spvReflectEnumerateDescriptorSets(&module, &descriptor_set_count, NULL));

            SpvReflectDescriptorSet** descriptor_sets = ArenaPushArray(arena, SpvReflectDescriptorSet*, descriptor_set_count);
            SPIRV_REFLECT_CHECK(spvReflectEnumerateDescriptorSets(&module, &descriptor_set_count, descriptor_sets));

            // shader->Resources.Reserve(descriptor_set_count);
            for (uint32 set_idx = 0; set_idx < descriptor_set_count; set_idx++)
            {
                SpvReflectDescriptorSet* descriptor_set = descriptor_sets[set_idx];
                printf("Set = %d Bindings = %d\n", descriptor_set->set, descriptor_set->binding_count);

                for (uint32 binding_idx = 0; binding_idx < descriptor_set->binding_count; binding_idx++)
                {
                    SpvReflectDescriptorBinding* binding = descriptor_set->bindings[binding_idx];
                    printf("\tBinding %d '%s' %d Type name: '%s'\n", binding->binding, binding->name, binding->descriptor_type, binding->type_description->type_name);

                    // shaderfx::ShaderResource Resource = {
                    //     .Name = String(binding->name),
                    //     .Set = descriptor_set->set,

                    // };
                }
            }

            // Output variables, descriptor bindings, descriptor sets, and push constants
            // can be enumerated and extracted using a similar mechanism.

            // Destroy the reflection data when no longer required.
            spvReflectDestroyShaderModule(&module);

            // spvc_context                   context = NULL;
            // spvc_parsed_ir                 ir = NULL;
            // spvc_resources                 resources = NULL;
            // spvc_compiler                  compiler_glsl = NULL;
            // const spvc_reflected_resource* list = NULL;
            // size_t                         count;

            // SPVC_CHECKED_CALL(spvc_context_create(&context));
            // spvc_context_set_error_callback(context, SpirvErrorCallback, (void*)&Shader);
            // SPVC_CHECKED_CALL(spvc_context_parse_spirv(context, (SpvId*)shaderc_result_get_bytes(Result), shaderc_result_get_length(Result) / sizeof(SpvId), &ir));

            // spvc_context_create_compiler(context, SPVC_BACKEND_GLSL, ir, SPVC_CAPTURE_MODE_TAKE_OWNERSHIP, &compiler_glsl);
            // spvc_compiler_create_shader_resources(compiler_glsl, &resources);
            // spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_UNIFORM_BUFFER, &list, &count);

            // for (i = 0; i < count; i++)
            // {
            //     printf("ID: %u, BaseTypeID: %u, TypeID: %u, Name: %s\n", list[i].id, list[i].base_type_id, list[i].type_id, list[i].name);
            //     printf(
            //         "  Set: %u, Binding: %u\n",
            //         spvc_compiler_get_decoration(compiler_glsl, list[i].id, SpvDecorationDescriptorSet),
            //         spvc_compiler_get_decoration(compiler_glsl, list[i].id, SpvDecorationBinding)
            //     );
            // }

            // Also write spirv file
            if (compiler_opts.write_spirv_for_shaders)
            {
                fs::FileHandle file_handle;
                String8        base_dir = fs::Dirname(arena, output_path);
                String8        extension = (shader_kind == shaderc_vertex_shader ? String8Raw(".vert.spv") : String8Raw(".frag.spv"));

                String8 spirv_output_path = fs::PathJoin(arena, base_dir, shader->name);
                spirv_output_path = fs::PathJoin(arena, spirv_output_path, extension);

                // BaseDir + "/" + shader.Name + (shader_kind == shaderc_vertex_shader ? ".vert" : ".frag") + ".spv";
                if (fs::OpenFile(spirv_output_path, fs::FILE_OPEN_MODE_WRITE, true, &file_handle))
                {
                    fs::WriteFile(&file_handle, spirv_binary.ptr, spirv_binary.count);
                    fs::CloseFile(&file_handle);

                    KDEBUG("Wrote %S", spirv_output_path);
                }
                else
                {
                    KERROR("[CompileKFX]: Failed to write spv output to file %S", output_path);
                    return false;
                }
            }

            shaderc_result_release(result);
        }
        else
        {
            KERROR("%d shader compilation failed with error:\n%s", shader_def.stage, shaderc_result_get_error_message(result));

            shaderc_result_release(result);
            shaderc_compiler_release(compiler);

            return false;
        }
    }

    shaderc_compiler_release(compiler);

    fs::FileHandle file;
    if (fs::OpenFile(output_path, fs::FILE_OPEN_MODE_WRITE, true, &file))
    {
        fs::WriteFile(&file, binary_output);
        fs::CloseFile(&file);
    }
    else
    {
        KERROR("[CompileKFX]: Failed to write binary output to file '%S'", output_path);
        return false;
    }

    return true;
}

using namespace kraft;

int Init()
{
    auto& args = kraft::Engine::GetCommandLineArgs();
    if (args.count < 2)
    {
        KWARN("No command line arguments provided. At least one parameter is required.");
        return false;
    }

    String8              base_path = args.ptr[1];
    ShaderFxCompilerOpts compiler_opts;
    for (int i = 0; i < args.count; i++)
    {
        if (StringEqual(args.ptr[i], String8Raw("--verbose")))
        {
            compiler_opts.verbose = true;
        }
        else if (StringEqual(args.ptr[i], String8Raw("--write-spv")))
        {
            compiler_opts.write_spirv_for_shaders = true;
        }
    }

    int             failed_tasks = 0;
    ArenaAllocator* arena = CreateArena({ .ChunkSize = KRAFT_SIZE_MB(16), .Alignment = 64 });

    if (fs::FileExists(base_path))
    {
        String8 output_filepath = StringCat(arena, base_path, String8Raw(".bkfx"));
        bool    result = CompileShaderFX(arena, base_path, output_filepath, compiler_opts);
        if (true == result)
        {
            KSUCCESS("Compiled %S to %S", base_path, output_filepath);
        }
        else
        {
            KERROR("Failed to compile %S", base_path);
            failed_tasks++;
        }
    }
    else
    {
        fs::Directory directory = fs::ReadDir(arena, base_path);
        for (int i = 0; i < directory.entry_count; i++)
        {
            if (StringEndsWith(directory.entries[i].name, String8Raw(".kfx")))
            {
                String8 shaderfx_filepath = fs::PathJoin(arena, base_path, directory.entries[i].name);
                String8 output_filepath = StringCat(arena, shaderfx_filepath, String8Raw(".bkfx"));
                bool    result = CompileShaderFX(arena, shaderfx_filepath, output_filepath, compiler_opts);
                if (true == result)
                {
                    KSUCCESS("Compiled %S to %S", shaderfx_filepath, output_filepath);
                }
                else
                {
                    KERROR("Failed to compile %S", shaderfx_filepath);
                    failed_tasks++;
                }
            }
        }
    }

    DestroyArena(arena);
    return failed_tasks;
}

int main(int argc, char** argv)
{
    ThreadContext* thread_context = CreateThreadContext();
    SetCurrentThreadContext(thread_context);

    kraft::EngineConfig config = {
        .argc = argc,
        .argv = argv,
        .application_name = String8Raw("KraftShaderCompiler"),
        .console_app = true,
    };

    kraft::CreateEngine(&config);

    int error_count = Init();

    kraft::DestroyEngine();

    return error_count;
}
