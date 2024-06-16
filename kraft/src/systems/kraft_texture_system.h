#pragma once

#include "core/kraft_core.h"
#include "core/kraft_string.h"
#include "resources/kraft_resource_types.h"

namespace kraft
{

namespace renderer
{
template <typename>
struct Handle;
}

namespace TextureSystem
{
    void Init(uint32 maxTextureCount);
    void Shutdown();

    Handle<Texture> AcquireTexture(const String& name, bool autoRelease = true);
    void ReleaseTexture(const String& name);
    void ReleaseTexture(Handle<Texture> Resource);
    Handle<Texture> CreateTextureWithData(TextureDescription Description, uint8* Data);
    uint8* CreateEmptyTexture(uint32 width, uint32 height, uint8 channels);
    Handle<Texture> GetDefaultDiffuseTexture();
};

}