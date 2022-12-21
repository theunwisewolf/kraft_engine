#include "kraft_texture_system.h"

#include "core/kraft_memory.h"
#include "core/kraft_asserts.h"
#include "core/kraft_string.h"
#include "renderer/kraft_renderer_frontend.h"

#define STBI_FAILURE_USERMSG
#include "stb_image.h"

namespace kraft
{

struct TextureReference
{
    uint32      RefCount;
    bool        AutoRelease;
    Texture     Texture;
    char        Name[255];
};

// Private state
struct TextureSystemState
{
    uint32            MaxTextureCount;
    TextureReference* Textures;
};

static TextureSystemState* state = 0;
void createDefaultTextures();

void TextureSystem::Init(uint32 maxTextureCount)
{
    uint32 totalSize = sizeof(TextureSystemState) + sizeof(TextureReference) * maxTextureCount;
    
    state = (TextureSystemState*)kraft::Malloc(totalSize, MEMORY_TAG_TEXTURE_SYSTEM, true);
    state->Textures = (TextureReference*)(state + sizeof(TextureSystemState));
    state->MaxTextureCount = maxTextureCount;

    createDefaultTextures();
}

void TextureSystem::Shutdown()
{
    kraft::Free(state, MEMORY_TAG_TEXTURE_SYSTEM);
}

Texture* TextureSystem::AcquireTexture(const char* name, bool autoRelease)
{
    int freeIndex = -1;
    int index = -1;

    // TODO: (TheUnwiseWolf) Use a hashmap instead of this
    for (int i = 0; i < state->MaxTextureCount; ++i)
    {
        if (state->Textures[i].Name == name)
        {
            index = i;
            break;
        }

        if (freeIndex == -1 && state->Textures[i].RefCount == 0)
        {
            freeIndex = i;
        }        
    }

    // Texture already exists in the cache
    // Just increase the ref-count
    if (index > -1)
    {
        state->Textures[index].RefCount++;
    }
    else
    {
        if (freeIndex > -1)
        {
            TextureReference* reference = &state->Textures[freeIndex];
            reference->RefCount++;
            reference->AutoRelease = autoRelease;

            uint64 length = StringLengthClamped(name, sizeof(TextureReference::Name));
            MemCpy(&(reference->Name[0]), (void*)name, length);
            if (!LoadTexture(name, &reference->Texture))
            {
                KERROR("[TextureSystem::AcquireTexture]: Failed to acquire texture; Texture loading failed");
                return NULL;
            }

            index = freeIndex;
        }
        else
        {
            KERROR("[TextureSystem::AcquireTexture]: Failed to acquire texture; Out-of-memory!");
            return NULL;
        }
    }

    return &state->Textures[index].Texture;
}

void TextureSystem::ReleaseTexture(const char* name)
{
    int index = -1;
    for (int i = 0; i < state->MaxTextureCount; ++i)
    {
        if (state->Textures[i].Name == name)
        {
            index = i;
            break;
        }
    }

    if (index == -1)
    {
        KERROR("[TextureSystem::ReleaseTexture]: Called for unknown texture %s", name);
        return;
    }

    TextureReference* ref = &state->Textures[index];
    ref->RefCount--;
    if (ref->RefCount == 0 && ref->AutoRelease)
    {
        Renderer->DestroyTexture(&ref->Texture);
    }
}

bool TextureSystem::LoadTexture(const char* name, Texture* texture)
{
    // Load textures
    stbi_set_flip_vertically_on_load(1);

    int width, height, channels, desiredChannels = 4;
    unsigned char *data = stbi_load(name, &width, &height, &channels, desiredChannels);

    if (data)
    {
        texture->Width = width;
        texture->Height = height;
        texture->Channels = desiredChannels > 0 ? desiredChannels : channels;

        Renderer->CreateTexture(data, texture);
        stbi_image_free(data);

        return true;
    }

    if (const char* failureReason = stbi_failure_reason())
    {
        KERROR("[TextureSystem::LoadTexture]: Failed to load image %s", name);
        KERROR("[TextureSystem::LoadTexture]: Error %s", failureReason);
    }

    return false;
}

void createDefaultTextures()
{
    TextureReference* ref = &state->Textures[0];
    StringCopy(ref->Name, sizeof(ref->Name), "default-diffuse");
    ref->RefCount = 1;
    ref->AutoRelease = false;
    TextureSystem::CreateEmptyTexture(256, 256, 4, &ref->Texture);
}

void TextureSystem::CreateEmptyTexture(uint32 width, uint32 height, uint8 channels, Texture* out)
{
    if (!out)
        out = (Texture*)Malloc(sizeof(Texture), MEMORY_TAG_TEXTURE_SYSTEM);

    out->Width = width;
    out->Height = height;
    out->Channels = channels;

    uint8* pixels = (uint8*)Malloc(width * height * channels, MEMORY_TAG_TEXTURE);
    MemSet(pixels, 255, width * height * channels);

    const int segments = 8, boxSize = width / segments;
    unsigned char white[4] = {255, 255, 255, 255};
    unsigned char black[4] = {0, 0, 0, 255};
    unsigned char* color = NULL;
    int swap = 0;
    int fill = 0;
    for(int i = 0; i < width * height; i++)
    {
        if (i > 0)
        {
            if (i % (width * boxSize) == 0)
                swap = !swap;

            if (i % boxSize == 0)
                fill = !fill;
        }

        if (fill)
        {
            if (swap)
                color = black;
            else
                color = white;
        }
        else
        {
            if (swap)
                color = white;
            else
                color = black;
        }

        for(int j = 0; j < channels; j++)
        {
            pixels[i * channels + j] = color[j];
        }
    }

    // for (int i = 0; i < width; i++)
    // {
    //     for (int j = 0; j < height; j++)
    //     {
    //         uint8 color = (((i & 0x8) == 0) ^ ((j & 0x8)  == 0)) * 255;
    //         int index = i * width + j;
    //         int indexChannel = index * channels;

    //         if (i % 2)
    //         {
    //             if (j % 2)
    //             {
    //                 pixels[indexChannel + 0] = 0;
    //                 pixels[indexChannel + 1] = 0;
    //                 pixels[indexChannel + 2] = 0;
    //             }
    //         }
    //         else if (!(j % 2))
    //         {
    //             pixels[indexChannel + 0] = 0;
    //             pixels[indexChannel + 1] = 0;
    //             pixels[indexChannel + 2] = 0;
    //         }
    //     }
    // }

    Renderer->CreateTexture(pixels, out);
    Free(pixels);
}

Texture* TextureSystem::GetDefaultDiffuseTexture()
{
    return &state->Textures[0].Texture;
}

}
