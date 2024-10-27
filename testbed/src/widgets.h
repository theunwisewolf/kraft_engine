#pragma once

#include "core/kraft_core.h"
#include "core/kraft_string.h"

#include <renderer/kraft_renderer_types.h>

void InitImguiWidgets(kraft::renderer::Handle<kraft::Texture> DiffuseTexture);
void DrawImGuiWidgets(bool refresh);
void PipelineDebugger();
