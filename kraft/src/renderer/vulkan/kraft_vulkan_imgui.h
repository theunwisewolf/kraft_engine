#pragma once

struct ImDrawData;

namespace kraft {
struct Texture;
}

namespace kraft::renderer {

template<typename>
struct Handle;

namespace VulkanImgui {

bool Init();
bool BeginFrame();
bool EndFrame(ImDrawData* DrawData);
bool Destroy();

//
// API
//
void* AddTexture(Handle<Texture> Texture);
void  RemoveTexture(void* TextureID);
void  PostFrameCleanup();

}
}