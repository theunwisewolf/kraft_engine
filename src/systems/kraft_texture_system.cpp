#include "kraft_texture_system.h"

#include "core/kraft_memory.h"
#include "core/kraft_asserts.h"
#include "core/kraft_string.h"
#include "renderer/kraft_renderer_frontend.h"

#define STBI_FAILURE_USERMSG
#ifdef UNICODE
    #define STBI_WINDOWS_UTF8
#endif
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define KRAFT_DEFAULT_DIFFUSE_TEXTURE_NAME TEXT("default-diffuse")

namespace kraft
{

struct TextureReference
{
    uint32      RefCount;
    bool        AutoRelease;
    Texture     Texture;
};

// Private state
struct TextureSystemState
{
    uint32            MaxTextureCount;
    TextureReference* Textures;
};

static TextureSystemState* State = 0;
static void _createDefaultTextures();

void TextureSystem::Init(uint32 maxTextureCount)
{
    uint32 totalSize = sizeof(TextureSystemState) + sizeof(TextureReference) * maxTextureCount;
    
    State = (TextureSystemState*)kraft::Malloc(totalSize, MEMORY_TAG_TEXTURE_SYSTEM, true);
    State->Textures = (TextureReference*)(State + sizeof(TextureSystemState));
    State->MaxTextureCount = maxTextureCount;

    _createDefaultTextures();
}

void TextureSystem::Shutdown()
{
    for (int i = 0; i < State->MaxTextureCount; ++i)
    {
        TextureReference* ref = &State->Textures[i];
        if (ref->RefCount > 0)
        {
            Renderer->DestroyTexture(&ref->Texture);
        }
    }

    uint32 totalSize = sizeof(TextureSystemState) + sizeof(TextureReference) * State->MaxTextureCount;
    kraft::Free(State, totalSize, MEMORY_TAG_TEXTURE_SYSTEM);

    KINFO(TEXT("[TextureSystem::Shutdown]: Shutting down texture system"));
}

Texture* TextureSystem::AcquireTexture(const TCHAR* _name, bool autoRelease)
{
    const TCHAR* name = StringTrim(_name);
    if (StringLength(name) == 0)
    {
        return NULL;
    }

    int freeIndex = -1;
    int index = -1;

    // TODO: (TheUnwiseWolf) Use a hashmap instead of this
    for (int i = 0; i < State->MaxTextureCount; ++i)
    {
        if (StringEqual(State->Textures[i].Texture.Name, name))
        {
            index = i;
            break;
        }

        if (freeIndex == -1 && State->Textures[i].RefCount == 0)
        {
            freeIndex = i;
        }        
    }

    // Texture already exists in the cache
    // Just increase the ref-count
    if (index > -1)
    {
        KINFO(TEXT("[TextureSystem::AcquireTexture]: Texture already acquired; Reusing!"));
        State->Textures[index].RefCount++;
    }
    else
    {
        if (freeIndex == -1)
        {
            KERROR(TEXT("[TextureSystem::AcquireTexture]: Failed to acquire texture; Out-of-memory!"));
            return NULL;
        }

        TextureReference* reference = &State->Textures[freeIndex];
        reference->RefCount++;
        reference->AutoRelease = autoRelease;

        uint64 length = StringLengthClamped(name, KRAFT_TEXTURE_NAME_MAX_LENGTH);
        StringCopy(&(reference->Texture.Name[0]), (TCHAR*)name);
        if (!LoadTexture(name, &reference->Texture))
        {
            KERROR(TEXT("[TextureSystem::AcquireTexture]: Failed to acquire texture; Texture loading failed"));
            return NULL;
        }

        index = freeIndex;
    }

    KDEBUG(TEXT("[TextureSystem::AcquireTexture]: Acquired texture %s (index = %d)"), name, index);
    kraft::Free((void*)name);
    return &State->Textures[index].Texture;
}

void TextureSystem::ReleaseTexture(const TCHAR* name)
{
    if (StringEqual(name, KRAFT_DEFAULT_DIFFUSE_TEXTURE_NAME))
    {
        KDEBUG(TEXT("[TextureSystem::ReleaseTexture]: %s is a default texture; Cannot be released!"), name);
        return;
    }

    KDEBUG(TEXT("[TextureSystem::ReleaseTexture]: Releasing texture %s"), name);

    int index = -1;
    for (int i = 0; i < State->MaxTextureCount; ++i)
    {
        if (StringEqual(State->Textures[i].Texture.Name, name))
        {
            index = i;
            break;
        }
    }

    if (index == -1)
    {
        KERROR(TEXT("[TextureSystem::ReleaseTexture]: Called for unknown texture %s"), name);
        return;
    }

    TextureReference* ref = &State->Textures[index];
    if (ref->RefCount == 0)
    {
        KWARN(TEXT("[TextureSystem::ReleaseTexture]: Texture %s already released!"), ref->Texture.Name);
        return;
    }

    ref->RefCount--;
    if (ref->RefCount == 0 && ref->AutoRelease)
    {
        Renderer->DestroyTexture(&ref->Texture);
        MemZero(&ref->Texture, sizeof(Texture));
    }
}

void TextureSystem::ReleaseTexture(Texture* texture)
{
    TextureSystem::ReleaseTexture(texture->Name);
}

bool TextureSystem::LoadTexture(const TCHAR* name, Texture* texture)
{
    // Load textures
    stbi_set_flip_vertically_on_load(1);

    int width, height, channels, desiredChannels = 4;
    const char* path = (char*)name;

#if UNICODE
    char utf8Path[256];
    stbi_convert_wchar_to_utf8(utf8Path, sizeof(utf8Path), name);
    path = utf8Path;
#endif

    unsigned char *data = stbi_load(path, &width, &height, &channels, desiredChannels);

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
        KERROR(TEXT("[TextureSystem::LoadTexture]: Failed to load image %s"), name);
        KERROR(TEXT("[TextureSystem::LoadTexture]: Error %s"), failureReason);
    }

    return false;
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
    return &State->Textures[0].Texture;
}

//
// Internal methods
//

static void _createDefaultTextures()
{
    TextureReference* ref = &State->Textures[0];
    StringNCopy(ref->Texture.Name, KRAFT_DEFAULT_DIFFUSE_TEXTURE_NAME, sizeof(ref->Texture.Name));

    ref->RefCount = 1;
    ref->AutoRelease = false;
    TextureSystem::CreateEmptyTexture(256, 256, 4, &ref->Texture);
}

}
