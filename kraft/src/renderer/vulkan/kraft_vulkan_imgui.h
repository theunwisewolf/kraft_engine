#pragma once

struct ImDrawData;

namespace kraft {
struct Texture;
struct TextureSampler;
} // namespace kraft

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
void* AddTexture(Handle<Texture> Texture, Handle<TextureSampler> Sampler);
void  RemoveTexture(void* TextureID);
void  PostFrameCleanup();

} // namespace VulkanImgui
} // namespace kraft::renderer