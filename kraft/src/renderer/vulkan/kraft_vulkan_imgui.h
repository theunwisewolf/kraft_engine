#pragma once

#include <GLFW/glfw3.h>

#include "renderer/vulkan/kraft_vulkan_types.h"
#include "resources/kraft_resource_types.h"

namespace kraft::renderer
{
namespace VulkanImgui
{

bool Init();
bool BeginFrame();
bool EndFrame(ImDrawData* DrawData);
bool Destroy();

// 
// API
//
ImTextureID AddTexture(Handle<Texture> Texture);
void RemoveTexture(ImTextureID TextureID);
void PostFrameCleanup();

}
}