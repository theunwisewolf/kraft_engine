#pragma once

#include <cstdint>

struct Texture
{
    // Texture Data
    int width;
    int height;
    int channels;

    uint32_t TextureID;

    Texture(const char* filename);

    void Bind(int slot);
};