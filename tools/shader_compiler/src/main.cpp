#include "containers/kraft_array.h"
#include "core/kraft_asserts.h"
#include "core/kraft_engine.h"
#include "core/kraft_events.h"
#include "core/kraft_input.h"
#include "core/kraft_lexer.h"
#include "core/kraft_log.h"
#include "core/kraft_memory.h"
#include "core/kraft_string.h"
#include "core/kraft_time.h"
#include "math/kraft_math.h"
#include "platform/kraft_filesystem.h"
#include "platform/kraft_platform.h"
#include "shaderfx/kraft_shaderfx.h"
#include <kraft.h>

#include <kraft_types.h>
#include <platform/kraft_filesystem_types.h>
#include <shaderfx/kraft_shaderfx_types.h>

#include <shaderc/shaderc.h>
#include <vendor/spirv-reflect/spirv_reflect.h>

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

bool CompileShaderFX(const String& InputPath, const String& OutputPath, bool Verbose = false);
bool CompileShaderFX(ArenaAllocator* Arena, shaderfx::ShaderEffect& Shader, const String& OutputPath, bool Verbose = false);

void SpirvErrorCallback(void* userdata, const char* error)
{}

bool CompileShaderFX(const String& InputPath, const String& OutputPath, bool Verbose)
{
    KINFO("Compiling shaderfx %s", *InputPath);

    filesystem::FileHandle File;
    bool                   Result = filesystem::OpenFile(InputPath, filesystem::FILE_OPEN_MODE_READ, true, &File);
    if (!Result)
    {
        return false;
    }

    ArenaAllocator* TempArena = CreateArena({ .ChunkSize = KRAFT_SIZE_MB(16), .Alignment = 64 });

    uint64 BufferSize = kraft::filesystem::GetFileSize(&File) + 1;
    uint8* FileDataBuffer = ArenaPush(TempArena, BufferSize);
    filesystem::ReadAllBytes(&File, &FileDataBuffer);
    filesystem::CloseFile(&File);

    Lexer Lexer;
    Lexer.Create((char*)FileDataBuffer, BufferSize - 1);
    shaderfx::ShaderFXParser Parser;
    Parser.Verbose = Verbose;

    shaderfx::ShaderEffect Effect;
    if (!Parser.Parse(InputPath, &Lexer, &Effect))
    {
        KERROR("Parsing failed\nLine: %d\nColumn: %d\nError: %s", Parser.ErrorLine, Parser.ErrorColumn, Parser.ErrorString);
        return false;
    }

    Result = CompileShaderFX(TempArena, Effect, OutputPath);

    DestroyArena(TempArena);
    return Result;
}

uint64 GetUniformBufferSize(const shaderfx::UniformBufferDefinition& UniformBuffer)
{
    uint64 SizeInBytes = 0;
    for (int FieldIdx = 0; FieldIdx < UniformBuffer.Fields.Length; FieldIdx++)
    {
        const shaderfx::UniformBufferEntry& Field = UniformBuffer.Fields[FieldIdx];
        SizeInBytes += renderer::ShaderDataType::SizeOf(Field.Type);
    }

    return SizeInBytes;
}

static bool CheckBufferDefinition(shaderfx::ShaderEffect& Shader, const Array<shaderfx::UniformBufferDefinition>* Buffers, shaderfx::ResourceBinding* BindingDescription)
{
    for (int Idx = 0; Idx < Buffers->Length; Idx++)
    {
        if ((*Buffers)[Idx].Name == BindingDescription->Name)
        {
            // Check if the size is incorrect
            uint64 ExpectedSize = GetUniformBufferSize((*Buffers)[Idx]);
            BindingDescription->Size = ExpectedSize;

            // Update the parent index
            BindingDescription->ParentIndex = Idx;

            return true;
        }
    }

    KERROR("'%s' shader compilation failed with error: Missing BufferDefinition for '%s'.", *Shader.ResourcePath, *BindingDescription->Name);

    return false;
}

struct ShaderIncludeUserData
{
    ArenaAllocator*         Arena;
    shaderfx::ShaderEffect* Shader;
};

static shaderc_include_result* ShaderIncludeResolverFunction(void* UserData, const char* RequestedSource, int Type, const char* RequestingSource, size_t IncludeDepth)
{
    ShaderIncludeUserData*  IncludeData = (ShaderIncludeUserData*)UserData;
    ArenaAllocator*         Arena = IncludeData->Arena;
    shaderfx::ShaderEffect* Shader = IncludeData->Shader;

    char* ShaderDirectory = filesystem::Dirname(Arena, Shader->ResourcePath);
    char* IncludedFilePath = filesystem::PathJoin(Arena, ShaderDirectory, RequestedSource);

    filesystem::FileHandle File;
    KASSERT(filesystem::OpenFile(IncludedFilePath, kraft::filesystem::FILE_OPEN_MODE_READ, true, &File));
    uint64 FileSize = filesystem::GetFileSize(&File);
    uint8* FileBuffer = kraft::ArenaPush(Arena, FileSize);
    filesystem::ReadAllBytes(&File, &FileBuffer);

    shaderc_include_result* Result = (shaderc_include_result*)ArenaPush(Arena, sizeof(shaderc_include_result));
    Result->content = (const char*)FileBuffer;
    Result->content_length = FileSize;

    uint64 SourceNameLength = StringLength(IncludedFilePath);
    char*  SourceName = ArenaPushString(Arena, IncludedFilePath, SourceNameLength);
    Result->source_name = SourceName;
    Result->source_name_length = SourceNameLength;

    return Result;
}

static void ShaderIncludeResultReleaseFunction(void* user_data, shaderc_include_result* include_result)
{}

bool VerifyResources(shaderfx::ShaderEffect& Shader, Array<shaderfx::ResourceBindingsDefinition>& Resources)
{
    for (int i = 0; i < Resources.Length; i++)
    {
        shaderfx::ResourceBindingsDefinition* Resource = &Resources[i];
        for (int j = 0; j < Resource->ResourceBindings.Length; j++)
        {
            if (Resource->ResourceBindings[j].Type == renderer::ResourceType::UniformBuffer)
            {
                if (false == CheckBufferDefinition(Shader, &Shader.UniformBuffers, &Resource->ResourceBindings[j]))
                {
                    return false;
                }
            }
            else if (Resource->ResourceBindings[j].Type == renderer::ResourceType::StorageBuffer)
            {
                if (false == CheckBufferDefinition(Shader, &Shader.StorageBuffers, &Resource->ResourceBindings[j]))
                {
                    return false;
                }
            }
        }
    }

    return true;
}

bool CompileShaderFX(ArenaAllocator* Arena, shaderfx::ShaderEffect& Shader, const String& OutputPath, bool Verbose)
{
    // Basic verification
    if (!VerifyResources(Shader, Shader.LocalResources))
        return false;
    if (!VerifyResources(Shader, Shader.GlobalResources))
        return false;

    shaderc_compiler_t           Compiler = shaderc_compiler_initialize();
    shaderc_compilation_result_t Result;
    shaderc_compile_options_t    CompileOptions;

    kraft::Buffer BinaryOutput;
    BinaryOutput.Write(Shader.Name);
    BinaryOutput.Write(Shader.ResourcePath);
    BinaryOutput.Write(Shader.VertexLayouts);
    BinaryOutput.Write(Shader.LocalResources);
    BinaryOutput.Write(Shader.GlobalResources);
    BinaryOutput.Write(Shader.ConstantBuffers);
    BinaryOutput.Write(Shader.UniformBuffers);
    BinaryOutput.Write(Shader.StorageBuffers);
    BinaryOutput.Write(Shader.RenderStates);
    BinaryOutput.Write(Shader.RenderPasses.Length);

    // We don't have to write the vertex layouts or render states
    // They are written as part of the renderpass definition
    for (int i = 0; i < Shader.RenderPasses.Length; i++)
    {
        const shaderfx::RenderPassDefinition& Pass = Shader.RenderPasses[i];
        BinaryOutput.Write(Pass.Name);

        // Write the vertex layout & render state offsets
        BinaryOutput.Write((int64)(Pass.VertexLayout - &Shader.VertexLayouts[0]));
        BinaryOutput.Write((int64)(Shader.LocalResources.Length > 0 ? Pass.Resources - &Shader.LocalResources[0] : -1));
        BinaryOutput.Write((int64)(Pass.ConstantBuffers - &Shader.ConstantBuffers[0]));
        BinaryOutput.Write((int64)(Pass.RenderState - &Shader.RenderStates[0]));

        // Manually write the shader code fragments to the buffer
        BinaryOutput.Write(Pass.ShaderStages.Length);

        for (int j = 0; j < Pass.ShaderStages.Length; j++)
        {
            const shaderfx::RenderPassDefinition::ShaderDefinition& Stage = Pass.ShaderStages[j];
            if (Verbose)
            {
                KDEBUG("Compiling shaderstage %d for %s", Stage.Stage, *Shader.ResourcePath);
            }

            CompileOptions = shaderc_compile_options_initialize();
            ShaderIncludeUserData UserData = {
                .Arena = Arena,
                .Shader = &Shader,
            };
            shaderc_compile_options_set_include_callbacks(CompileOptions, ShaderIncludeResolverFunction, ShaderIncludeResultReleaseFunction, &UserData);

            shaderc_shader_kind ShaderKind;
            switch (Stage.Stage)
            {
                case renderer::ShaderStageFlags::SHADER_STAGE_FLAGS_VERTEX:
                {
                    ShaderKind = shaderc_vertex_shader;
                    shaderc_compile_options_add_macro_definition(
                        CompileOptions, KRAFT_VERTEX_DEFINE, sizeof(KRAFT_VERTEX_DEFINE) - 1, KRAFT_SHADERFX_ENABLE_VAL, sizeof(KRAFT_SHADERFX_ENABLE_VAL) - 1
                    );
                }
                break;

                case renderer::ShaderStageFlags::SHADER_STAGE_FLAGS_GEOMETRY:
                {
                    ShaderKind = shaderc_geometry_shader;
                    shaderc_compile_options_add_macro_definition(
                        CompileOptions, KRAFT_GEOMETRY_DEFINE, sizeof(KRAFT_GEOMETRY_DEFINE) - 1, KRAFT_SHADERFX_ENABLE_VAL, sizeof(KRAFT_SHADERFX_ENABLE_VAL) - 1
                    );
                }
                break;

                case renderer::ShaderStageFlags::SHADER_STAGE_FLAGS_FRAGMENT:
                {
                    ShaderKind = shaderc_fragment_shader;
                    shaderc_compile_options_add_macro_definition(
                        CompileOptions, KRAFT_FRAGMENT_DEFINE, sizeof(KRAFT_FRAGMENT_DEFINE) - 1, KRAFT_SHADERFX_ENABLE_VAL, sizeof(KRAFT_SHADERFX_ENABLE_VAL) - 1
                    );
                }
                break;

                case renderer::ShaderStageFlags::SHADER_STAGE_FLAGS_COMPUTE:
                {
                    ShaderKind = shaderc_compute_shader;
                    shaderc_compile_options_add_macro_definition(
                        CompileOptions, KRAFT_COMPUTE_DEFINE, sizeof(KRAFT_COMPUTE_DEFINE) - 1, KRAFT_SHADERFX_ENABLE_VAL, sizeof(KRAFT_SHADERFX_ENABLE_VAL) - 1
                    );
                }
                break;
            }

            Result = shaderc_compile_into_spv(Compiler, *Stage.CodeFragment.Code, Stage.CodeFragment.Code.Length, ShaderKind, "shader", "main", CompileOptions);
            if (shaderc_result_get_compilation_status(Result) == shaderc_compilation_status_success)
            {
                shaderfx::ShaderCodeFragment SpirvBinary;
                SpirvBinary.Name = Stage.CodeFragment.Name;
                SpirvBinary.Code = String(shaderc_result_get_bytes(Result), shaderc_result_get_length(Result));
                BinaryOutput.Write(Stage.Stage);
                BinaryOutput.Write(SpirvBinary);

                SpvReflectShaderModule module;
                SpvReflectResult       result = spvReflectCreateShaderModule((size_t)shaderc_result_get_length(Result), (const void*)shaderc_result_get_bytes(Result), &module);
                KASSERT(result == SPV_REFLECT_RESULT_SUCCESS);

                // Enumerate and extract shader's input variables
                uint32_t var_count = 0;
                SPIRV_REFLECT_CHECK(spvReflectEnumerateInputVariables(&module, &var_count, NULL));

                SpvReflectInterfaceVariable** input_vars = (SpvReflectInterfaceVariable**)Arena->Push(var_count * sizeof(SpvReflectInterfaceVariable*));
                SPIRV_REFLECT_CHECK(spvReflectEnumerateInputVariables(&module, &var_count, input_vars));

                uint32 descriptor_set_count = 0;
                SPIRV_REFLECT_CHECK(spvReflectEnumerateDescriptorSets(&module, &descriptor_set_count, NULL));

                SpvReflectDescriptorSet** descriptor_sets = (SpvReflectDescriptorSet**)Arena->Push(descriptor_set_count * sizeof(SpvReflectDescriptorSet*));
                SPIRV_REFLECT_CHECK(spvReflectEnumerateDescriptorSets(&module, &descriptor_set_count, descriptor_sets));

                Shader.Resources.Reserve(descriptor_set_count);
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
                filesystem::FileHandle File;
                String                 BaseDir = filesystem::Dirname(OutputPath);
                String                 SpvOutputFilePath = BaseDir + "/" + Shader.Name + (ShaderKind == shaderc_vertex_shader ? ".vert" : ".frag") + ".spv";
                if (filesystem::OpenFile(SpvOutputFilePath, filesystem::FILE_OPEN_MODE_WRITE, true, &File))
                {
                    filesystem::WriteFile(&File, (uint8*)*SpirvBinary.Code, SpirvBinary.Code.Length);
                    filesystem::CloseFile(&File);

                    KDEBUG("Wrote %s", *SpvOutputFilePath);
                }
                else
                {
                    KERROR("[CompileKFX]: Failed to write spv output to file %s", *OutputPath);
                    return false;
                }

                shaderc_result_release(Result);
            }
            else
            {
                KERROR("%d shader compilation failed with error:\n%s", Stage.Stage, shaderc_result_get_error_message(Result));

                shaderc_result_release(Result);
                shaderc_compiler_release(Compiler);

                return false;
            }
        }
    }

    shaderc_compiler_release(Compiler);

    filesystem::FileHandle File;
    if (filesystem::OpenFile(OutputPath, filesystem::FILE_OPEN_MODE_WRITE, true, &File))
    {
        filesystem::WriteFile(&File, BinaryOutput);
        filesystem::CloseFile(&File);
    }
    else
    {
        KERROR("[CompileKFX]: Failed to write binary output to file %s", *OutputPath);
        return false;
    }

    return true;
}

int Init()
{
    auto& CliArgs = kraft::Engine::GetCommandLineArgs();
    if (CliArgs.Length < 2)
    {
        KWARN("No command line arguments provided. At least one parameter is required!");
        return false;
    }

    kraft::String BasePath = CliArgs[1];
    bool          Verbose = false;
    if (CliArgs.Length > 2 && CliArgs[2] == "--verbose")
    {
        Verbose = true;
    }

    kraft::Array<kraft::filesystem::FileInfo> Files;
    kraft::filesystem::ReadDir(BasePath, Files);

    int ErrorCount = 0;
    for (int i = 0; i < Files.Length; i++)
    {
        if (Files[i].Name.EndsWith(".kfx"))
        {
            kraft::String ShaderFXFilePath = BasePath + "/" + Files[i].Name;
            kraft::String OutFilePath = ShaderFXFilePath + ".bkfx";
            bool          Result = CompileShaderFX(ShaderFXFilePath, OutFilePath, Verbose);
            if (true == Result)
            {
                KSUCCESS("Compiled %s to %s", *ShaderFXFilePath, *OutFilePath);
            }
            else
            {
                KERROR("Failed to compile %s", *ShaderFXFilePath);
                ErrorCount++;
            }
        }
    }

    return ErrorCount;
}

int main(int argc, char** argv)
{
    kraft::CreateEngine({
        .Argc = argc,
        .Argv = argv,
        .ApplicationName = "KraftShaderCompiler",
        .ConsoleApp = true,
    });

    int ErrorCount = Init();

    kraft::DestroyEngine();

    return ErrorCount;
}
