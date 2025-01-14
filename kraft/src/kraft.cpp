#include "kraft.h"

#include <containers/kraft_array.h>
#include <core/kraft_asserts.h>
#include <core/kraft_engine.h>
#include <core/kraft_events.h>
#include <core/kraft_log.h>
#include <core/kraft_string.h>
#include <platform/kraft_platform.h>
#include <platform/kraft_window.h>

#include <kraft_types.h>

namespace kraft {

bool CreateEngine(const EngineConfig& Config)
{
    return Engine::Init(Config);
}

void DestroyEngine()
{
    kraft::Engine::Destroy();
}

} // namespace kraft

#if defined(KRAFT_GUI_APP)

#include <renderer/kraft_renderer_frontend.h>
#include <systems/kraft_asset_database.h>
#include <systems/kraft_geometry_system.h>
#include <systems/kraft_material_system.h>
#include <systems/kraft_shader_system.h>
#include <systems/kraft_texture_system.h>

namespace kraft {

Window* CreateWindow(const WindowOptions& Opts)
{
    return Platform::CreatePlatformWindow(&Opts);
}

kraft::Window* CreateWindow(String Title, uint32 Width, uint32 Height)
{
    return CreateWindow({ .Title = Title, .Width = Width, .Height = Height });
}

void DestroyWindow(Window* Window)
{}

renderer::RendererFrontend* CreateRenderer(const RendererOptions& Opts)
{
    auto RendererInstance = renderer::CreateRendererFrontend(&Opts);
    KASSERT(RendererInstance);

    AssetDatabase::Init();
    TextureSystem::Init(Opts.TextureCacheSize);
    MaterialSystem::Init(Opts);
    GeometrySystem::Init({ .MaxGeometriesCount = 256 });
    ShaderSystem::Init(256);

    return RendererInstance;
}

void DestroyRenderer(renderer::RendererFrontend* Instance)
{
    ShaderSystem::Shutdown();
    GeometrySystem::Shutdown();
    MaterialSystem::Shutdown();
    TextureSystem::Shutdown();
    AssetDatabase::Shutdown();

    renderer::DestroyRendererFrontend(Instance);
}
} // namespace kraft
#endif
