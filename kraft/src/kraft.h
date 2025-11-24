#pragma once

#include <core/kraft_core.h>

namespace kraft {

struct Window;
struct EngineConfig;

bool CreateEngine(const EngineConfig* config);
void DestroyEngine();

} // namespace kraft

// --------------
// GUI Only Code
// --------------

#if defined(KRAFT_GUI_APP)
namespace kraft {

struct WindowOptions;
struct RendererOptions;

Window* CreateWindow(const WindowOptions& Opts);
Window* CreateWindow(String Title, uint32 Width, uint32 Height);
void    DestroyWindow(Window* Window);

namespace r {
struct RendererFrontend;
}

r::RendererFrontend* CreateRenderer(const RendererOptions& Opts);
void                        DestroyRenderer(r::RendererFrontend* Instance);

} // namespace kraft
#endif