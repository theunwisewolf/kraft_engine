#pragma once

#include "core/kraft_core.h"
#include "resources/kraft_resource_types.h"

namespace kraft
{

namespace TextureSystem
{
    void Init(uint32 maxTextureCount);
    void Shutdown();

    Texture* AcquireTexture(const char* name, bool autoRelease = true);
    void ReleaseTexture(const char* name);
    void ReleaseTexture(Texture* texture);
    bool LoadTexture(const char* name, Texture* texture);
    void CreateEmptyTexture(uint32 width, uint32 height, uint8 channels, Texture* out);
    Texture* GetDefaultDiffuseTexture();
};

}