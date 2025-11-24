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
void Init(uint32 maxTextureCount);
void Shutdown();

r::Handle<Texture>        AcquireTexture(const String& name, bool autoRelease = true);
r::Handle<Texture>        AcquireTextureWithData(String8 name, u8* data, u32 width, u32 height, u32 channels);
void                      ReleaseTexture(const String& name);
void                      ReleaseTexture(r::Handle<Texture> Resource);
r::Handle<Texture>        CreateTextureWithData(r::TextureDescription&& Description, const uint8* Data);
kraft::BufferView         CreateEmptyTexture(uint32 width, uint32 height, uint8 channels);
r::Handle<Texture>        GetDefaultDiffuseTexture();
Array<r::Handle<Texture>> GetDirtyTextures();
void                      ClearDirtyTextures();
}; // namespace TextureSystem

} // namespace kraft