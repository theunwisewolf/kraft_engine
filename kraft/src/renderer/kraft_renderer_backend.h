#pragma once

#include "core/kraft_core.h"

namespace kraft {

namespace renderer {

struct RendererBackend;
enum RendererBackendType : int;

bool CreateBackend(RendererBackendType type, RendererBackend* out);
void DestroyBackend(RendererBackend* backend);

}
}