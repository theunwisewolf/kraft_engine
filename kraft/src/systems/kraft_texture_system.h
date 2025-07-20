#pragma once

#include "core/kraft_core.h"

namespace kraft {

struct Texture;
struct BufferView;

namespace renderer {
struct TextureDescription;

template<typename>
struct Handle;
} // namespace renderer

namespace TextureSystem {
void Init(uint32 maxTextureCount);
void Shutdown();

renderer::Handle<Texture>        AcquireTexture(const String& name, bool autoRelease = true);
void                             ReleaseTexture(const String& name);
void                             ReleaseTexture(renderer::Handle<Texture> Resource);
renderer::Handle<Texture>        CreateTextureWithData(renderer::TextureDescription&& Description, const uint8* Data);
kraft::BufferView                CreateEmptyTexture(uint32 width, uint32 height, uint8 channels);
renderer::Handle<Texture>        GetDefaultDiffuseTexture();
Array<renderer::Handle<Texture>> GetDirtyTextures();
void                             ClearDirtyTextures();
}; // namespace TextureSystem

} // namespace kraft