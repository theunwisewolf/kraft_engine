#pragma once

#include <core/kraft_core.h>

namespace kraft {

struct Window;
struct EngineConfig;
struct CreateWindowOptions;
struct CreateRendererOptions;

bool CreateEngine(const EngineConfig& Config);
void DestroyEngine();

} // namespace kraft

// --------------
// GUI Only Code
// --------------

#if defined(KRAFT_GUI_APP)
namespace kraft {

Window* CreateWindow(const CreateWindowOptions& Opts);
Window* CreateWindow(String Title, uint32 Width, uint32 Height);
void    DestroyWindow(Window* Window);

namespace renderer {
struct RendererFrontend;
}

renderer::RendererFrontend* CreateRenderer(const CreateRendererOptions& Opts);
void                        DestroyRenderer(renderer::RendererFrontend* Instance);

} // namespace kraft
#endif