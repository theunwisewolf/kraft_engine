#pragma once

#include "core/kraft_core.h"
#include "renderer/kraft_renderer_types.h"

namespace kraft
{

bool RendererCreateBackend(RendererBackendType type, RendererBackend* out);
void RendererDestroyBackend(RendererBackend* backend);

}