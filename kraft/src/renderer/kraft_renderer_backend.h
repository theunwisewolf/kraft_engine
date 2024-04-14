#pragma once

#include "core/kraft_core.h"
#include "renderer/kraft_renderer_types.h"
#include "resources/kraft_resource_types.h"

namespace kraft
{

namespace renderer
{

bool CreateBackend(RendererBackendType type, RendererBackend* out);
void DestroyBackend(RendererBackend* backend);

}
}