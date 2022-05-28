#include "texture.h"

#include "stb_image.h"

#include <glad/gl.h>

Texture::Texture(const char *filename)
{
    stbi_set_flip_vertically_on_load(1);
    unsigned char *data = stbi_load(filename, &width, &height, &channels, 0);
    if (!data)
    {
        printf("Failed to load image %s", filename);
        return;
    }

    glGenTextures(1, &TextureID);
    glBindTexture(GL_TEXTURE_2D, TextureID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    uint32_t format;
    if (channels == 4)
    {
        format = GL_RGBA;
    }
    else
    {
        format = GL_RGB;
    }

    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    stbi_image_free(data);
}

void Texture::Bind(int slot)
{
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D, TextureID);
}