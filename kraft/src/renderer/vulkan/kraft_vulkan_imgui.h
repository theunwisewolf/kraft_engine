#pragma once

#include <GLFW/glfw3.h>

#include "renderer/vulkan/kraft_vulkan_types.h"
#include "resources/kraft_resource_types.h"

namespace kraft::renderer
{
namespace VulkanImgui
{

bool Init();
bool BeginFrame(float64 deltaTime);
bool EndFrame(ImDrawData* DrawData);
void* AddTexture(Texture* Texture);
bool Destroy();

}
}