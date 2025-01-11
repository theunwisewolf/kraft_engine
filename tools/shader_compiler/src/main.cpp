#include "containers/kraft_array.h"
#include "core/kraft_asserts.h"
#include "core/kraft_engine.h"
#include "core/kraft_events.h"
#include "core/kraft_input.h"
#include "core/kraft_log.h"
#include "core/kraft_memory.h"
#include "core/kraft_string.h"
#include "core/kraft_time.h"
#include "math/kraft_math.h"
#include "platform/kraft_filesystem.h"
#include "platform/kraft_platform.h"
#include "renderer/shaderfx/kraft_shaderfx.h"
#include <kraft.h>

#include "platform/kraft_filesystem_types.h"
#include <kraft_types.h>

struct ApplicationState
{};

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
            bool          Result = kraft::renderer::CompileShaderFX(ShaderFXFilePath, OutFilePath, Verbose);
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
