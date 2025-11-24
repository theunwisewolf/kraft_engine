#include "kraft.h"

#include <core/kraft_base_includes.h>
#include <platform/kraft_platform_includes.h>

#include <core/kraft_base_includes.cpp>

#include <renderer/kraft_renderer_types.h>

// HACK: for now
#include <shaderfx/kraft_shaderfx_includes.h>

#include <platform/kraft_platform_includes.cpp>
#include <shaderfx/kraft_shaderfx_includes.cpp>

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

bool CreateEngine(const EngineConfig* config)
{
    return Engine::Init(config);
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

#undef CreateWindow

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

r::RendererFrontend* CreateRenderer(const RendererOptions& Opts)
{
    auto RendererInstance = r::CreateRendererFrontend(&Opts);
    KASSERT(RendererInstance);

    AssetDatabase::Init();
    TextureSystem::Init(Opts.TextureCacheSize);
    MaterialSystem::Init(Opts);
    GeometrySystem::Init({ .MaxGeometriesCount = 256 });
    ShaderSystem::Init(256);

    return RendererInstance;
}

void DestroyRenderer(r::RendererFrontend* Instance)
{
    ShaderSystem::Shutdown();
    GeometrySystem::Shutdown();
    MaterialSystem::Shutdown();
    TextureSystem::Shutdown();
    AssetDatabase::Shutdown();

    r::DestroyRendererFrontend(Instance);
}
} // namespace kraft
#endif
