#pragma once

#include "core/kraft_core.h"
#include "renderer/kraft_renderer_types.h"

namespace kraft::renderer
{

bool CreateBackend(RendererBackendType type, RendererBackend* out);
void DestroyBackend(RendererBackend* backend);

}