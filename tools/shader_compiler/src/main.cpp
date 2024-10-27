#include "core/kraft_asserts.h"
#include "core/kraft_main.h"
#include "core/kraft_memory.h"
#include "core/kraft_events.h"
#include "core/kraft_input.h"
#include "core/kraft_string.h"
#include "core/kraft_time.h"
#include "math/kraft_math.h"
#include "platform/kraft_platform.h"
#include "platform/kraft_filesystem.h"
#include "renderer/shaderfx/kraft_shaderfx.h"
#include "core/kraft_application.h"

kraft::Application* App = nullptr;
bool Init()
{
    if (App->CommandLineArgs.Count < 2)
    {
        KWARN("No command line arguments provided. At least one parameter is required!");
        return false;
    }

    kraft::String BasePath = "Z:/Dev/kraft/res/shaders";
    kraft::Array<kraft::filesystem::FileInfo> Files;
    kraft::filesystem::ReadDir(BasePath, Files);

    for (int i = 0; i < Files.Length; i++)
    {
        if (Files[i].Name.EndsWith(".kfx"))
        {
            kraft::String ShaderFXFilePath = BasePath + "/" + Files[i].Name;
            kraft::String OutFilePath = ShaderFXFilePath + ".bkfx";
            bool Result = kraft::renderer::CompileShaderFX(ShaderFXFilePath, OutFilePath);
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

    return true;
}

bool Shutdown()
{
    return true;
}

bool CreateApplication(kraft::Application* app, int argc, char *argv[])
{
    app->Config.ApplicationName = "Kraft Shader Compiler";
    app->Config.WindowTitle     = "Kraft Shader Compiler";
    app->Config.WindowWidth     = 1280;
    app->Config.WindowHeight    = 800;
    app->Config.RendererBackend = kraft::renderer::RendererBackendType::RENDERER_BACKEND_TYPE_VULKAN;
    app->Config.ConsoleApp      = true;

    app->Init                   = Init;
    app->Shutdown               = Shutdown;

    App = app;

    return true;
}

