#include "core/kraft_asserts.h"
#include "core/kraft_engine.h"
#include "core/kraft_events.h"
#include "core/kraft_input.h"
#include "core/kraft_memory.h"
#include "core/kraft_string.h"
#include "core/kraft_time.h"
#include "core/kraft_log.h"
#include "containers/kraft_array.h"
#include "math/kraft_math.h"
#include "platform/kraft_filesystem.h"
#include "platform/kraft_platform.h"
#include "renderer/shaderfx/kraft_shaderfx.h"

#include "platform/kraft_filesystem_types.h"

struct ApplicationState
{};

bool Init()
{
    auto& CliArgs = kraft::Engine::GetCommandLineArgs();
    if (CliArgs.Length < 2)
    {
        KWARN("No command line arguments provided. At least one parameter is required!");
        return false;
    }

    kraft::String                             BasePath = "Z:/Dev/kraft/res/shaders";
    kraft::Array<kraft::filesystem::FileInfo> Files;
    kraft::filesystem::ReadDir(BasePath, Files);

    for (int i = 0; i < Files.Length; i++)
    {
        if (Files[i].Name.EndsWith(".kfx"))
        {
            kraft::String ShaderFXFilePath = BasePath + "/" + Files[i].Name;
            kraft::String OutFilePath = ShaderFXFilePath + ".bkfx";
            bool          Result = kraft::renderer::CompileShaderFX(ShaderFXFilePath, OutFilePath);
            if (true == Result)
            {
                KSUCCESS("Compiled shader %s successfully", *ShaderFXFilePath);
                KSUCCESS("Output file: %s", *OutFilePath);
            }
            else
            {
                KERROR("Failed to compile %s", *ShaderFXFilePath);
            }
        }
    }

    return true;

    // kraft::String ShaderFXFilePath = App->CommandLineArgs.Arguments[1];
    // kraft::String OutFilePath = ShaderFXFilePath + ".bkfx";
    // bool Result = kraft::renderer::CompileShaderFX(ShaderFXFilePath, OutFilePath);
    // if (true == Result)
    // {
    //     KSUCCESS("Compiled shader %s successfully", *ShaderFXFilePath);
    //     KSUCCESS("Output file: %s", *OutFilePath);
    // }
    // else
    // {
    //     KERROR("Failed to compile %s", *ShaderFXFilePath);
    // }

    // kraft::renderer::ShaderEffect Output;
    // KASSERT(kraft::renderer::LoadShaderFX(kraft::Application::Get()->BasePath + "/res/shaders/basic.kfx.bkfx", Output));
}

bool Shutdown()
{
    return true;
}

int main(int argc, char* argv[])
{
    kraft::Engine::Init(
        argc,
        argv,
        {
            .ConsoleApp = true,
        }
    );

    Init();

    kraft::Engine::Destroy();

    return 0;
}
