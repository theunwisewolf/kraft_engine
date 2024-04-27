#pragma once

#include "core/kraft_core.h"
#include "renderer/kraft_camera.h"

void InitGizmos();
void EditTransform(const kraft::Camera& Camera, kraft::Mat4f& Matrix);