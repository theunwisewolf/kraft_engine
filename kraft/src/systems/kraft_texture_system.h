#pragma once

#include "core/kraft_core.h"

namespace kraft {

struct Texture;
struct BufferView;

namespace r {
struct TextureDescription;

template<typename>
struct Handle;
} // namespace r

namespace TextureSystem {
void Init(u32 max_texture_count);
void Shutdown();

r::Handle<Texture>        AcquireTexture(String8 name, bool auto_release = true);
r::Handle<Texture>        AcquireTextureWithData(String8 name, u8* data, u32 width, u32 height, u32 channels);
void                      ReleaseTexture(String8 name);
void                      ReleaseTexture(r::Handle<Texture> handle);
r::Handle<Texture>        CreateTextureWithData(r::TextureDescription description, const u8* data);
kraft::BufferView         CreateEmptyTexture(u32 width, u32 height, u8 channels);
r::Handle<Texture>        GetDefaultDiffuseTexture();
Array<r::Handle<Texture>> GetDirtyTextures();
void                      ClearDirtyTextures();
}; // namespace TextureSystem

} // namespace kraft