#include "kraft_texture_system.h"

#include <containers/kraft_array.h>
#include <containers/kraft_hashmap.h>
#include <core/kraft_string.h>
#include <renderer/kraft_renderer_frontend.h>
#include <renderer/kraft_resource_manager.h>

// TODO (amn): REMOVE
#include <core/kraft_base_includes.h>

#include <resources/kraft_resource_types.h>

#define STBI_FAILURE_USERMSG
#ifdef UNICODE
#define STBI_WINDOWS_UTF8
#endif
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define KRAFT_DEFAULT_DIFFUSE_TEXTURE_NAME "default-diffuse"

namespace kraft {

struct TextureReference
{
    u32                RefCount;
    bool               AutoRelease;
    r::Handle<Texture> Resource;

    TextureReference() : RefCount(0), AutoRelease(true), Resource(r::Handle<Texture>::Invalid())
    {}

    TextureReference(r::Handle<Texture> Resource, bool AutoRelease = true) : RefCount(0), AutoRelease(AutoRelease), Resource(Resource)
    {}
};

// Private state
struct TextureSystemState
{
    u32                               MaxTextureCount;
    HashMap<String, TextureReference> TextureCache;
    Array<r::Handle<Texture>>         DirtyTextures;

    TextureSystemState(u32 MaxTextures)
    {
        this->MaxTextureCount = MaxTextures;
    }
};

static TextureSystemState* State = 0;
static void                _createDefaultTextures();

static r::Format::Enum FormatFromChannels(int Channels)
{
    if (Channels == 1)
    {
        return r::Format::RED;
    }
    if (Channels == 3)
    {
        return r::Format::RGB8_UNORM;
    }
    if (Channels == 4)
    {
        return r::Format::RGBA8_UNORM;
    }

    return r::Format::RGBA8_UNORM;
}

void TextureSystem::Init(u32 maxTextureCount)
{
    void* RawMemory = kraft::Malloc(sizeof(TextureSystemState), MEMORY_TAG_TEXTURE_SYSTEM, true);

    State = new (RawMemory) TextureSystemState(maxTextureCount);
    State = (TextureSystemState*)RawMemory;

    _createDefaultTextures();
}

void TextureSystem::Shutdown()
{
    u32 totalSize = sizeof(TextureSystemState);
    kraft::Free(State, totalSize, MEMORY_TAG_TEXTURE_SYSTEM);

    KINFO("[TextureSystem::Shutdown]: Shutting down texture system");
}

r::Handle<Texture> TextureSystem::AcquireTexture(const String& Name, bool AutoRelease)
{
    auto ExistingTexture = State->TextureCache.find(Name);
    if (ExistingTexture != State->TextureCache.end())
    {
        ExistingTexture.value().RefCount++;
        return ExistingTexture.value().Resource;
    }

    if (State->TextureCache.size() == State->MaxTextureCount)
    {
        KERROR("[TextureSystem::AcquireTexture]: Failed to acquire texture; Out-of-memory!");
        return r::Handle<Texture>::Invalid();
    }

    // Read the file
    // TODO (amn): File reading should be done by an asset database
    stbi_set_flip_vertically_on_load(1);

    int Width, Height, Channels, DesiredChannels = 0;
    if (!stbi_info(*Name, &Width, &Height, &Channels))
    {
        KERROR("[TextureSystem::LoadTexture]: Failed to load metadata for image '%s' with error '%s'", Name.Data(), stbi_failure_reason());

        return r::Handle<Texture>::Invalid();
    }

    // GPUs dont usually support 3 channel images, so we load with 4
    DesiredChannels = (Channels == 3) ? 4 : Channels;

    u8* TextureData = stbi_load(*Name, &Width, &Height, &Channels, DesiredChannels);
    if (!TextureData)
    {
        KERROR("[TextureSystem::LoadTexture]: Failed to load image %s", Name.Data());
        if (const char* FailureReason = stbi_failure_reason())
        {
            KERROR("[TextureSystem::LoadTexture]: Error %s", FailureReason);
        }

        return r::Handle<Texture>::Invalid();
    }

    char DebugName[512] = { 0 };
    MemCpy(DebugName, *Name, Name.GetLengthInBytes());

    r::Handle<Texture> TextureResource = TextureSystem::CreateTextureWithData(
        {
            .DebugName = DebugName,
            .Dimensions = { (float32)Width, (float32)Height, 1, (float32)DesiredChannels },
            .Format = FormatFromChannels(DesiredChannels),
            .Usage = r::TextureUsageFlags::TEXTURE_USAGE_FLAGS_TRANSFER_SRC | r::TextureUsageFlags::TEXTURE_USAGE_FLAGS_TRANSFER_DST | r::TextureUsageFlags::TEXTURE_USAGE_FLAGS_SAMPLED,
        },
        TextureData
    );

    stbi_image_free(TextureData);

    State->TextureCache[Name] = TextureReference(TextureResource, AutoRelease);

    if (!TextureResource.IsInvalid())
        KDEBUG("[TextureSystem::AcquireTexture]: Acquired texture %s", *Name);

    return TextureResource;
}

r::Handle<Texture> TextureSystem::AcquireTextureWithData(String8 name, u8* data, u32 width, u32 height, u32 channels)
{
    char debug_name[512] = { 0 };
    MemCpy(debug_name, name.ptr, name.count);

    r::Handle<Texture> resource = TextureSystem::CreateTextureWithData(
        {
            .DebugName = debug_name,
            .Dimensions = { (f32)width, (f32)height, 1, (f32)channels },
            .Format = FormatFromChannels(channels),
            .Usage = r::TextureUsageFlags::TEXTURE_USAGE_FLAGS_TRANSFER_SRC | r::TextureUsageFlags::TEXTURE_USAGE_FLAGS_TRANSFER_DST | r::TextureUsageFlags::TEXTURE_USAGE_FLAGS_SAMPLED,
        },
        data
    );

    if (!resource.IsInvalid())
        KDEBUG("[TextureSystem::AcquireTextureWithData]: Acquired texture '%S'", name);

    return resource;
}

void TextureSystem::ReleaseTexture(const String& TextureName)
{
    if (TextureName == KRAFT_DEFAULT_DIFFUSE_TEXTURE_NAME)
    {
        KDEBUG("[TextureSystem::ReleaseTexture]: %s is a default texture; Cannot be released!", TextureName.Data());
        return;
    }

    KDEBUG("[TextureSystem::ReleaseTexture]: Releasing texture %s", TextureName.Data());
    auto It = State->TextureCache.find(TextureName);
    if (It == State->TextureCache.end())
    {
        KERROR("[TextureSystem::ReleaseTexture]: Called for unknown texture %s", *TextureName);
        return;
    }

    TextureReference& Ref = It.value();
    if (Ref.RefCount == 0)
    {
        KWARN("[TextureSystem::ReleaseTexture]: Texture %s already released!", *TextureName);
        return;
    }

    Ref.RefCount--;
    if (Ref.RefCount == 0 && Ref.AutoRelease)
    {
        r::ResourceManager->DestroyTexture(Ref.Resource);
        State->TextureCache.erase(It);
    }
}

void TextureSystem::ReleaseTexture(r::Handle<Texture> Resource)
{
    KWARN("Not implemented");
    // TODO (amn): Figure this out
    // State->TextureCache.values()
    // if (Texture)
    // {
    //     TextureSystem::ReleaseTexture(Texture->Name);
    // }
}

r::Handle<Texture> TextureSystem::CreateTextureWithData(r::TextureDescription&& Description, const u8* Data)
{
    Description.Usage |= r::TextureUsageFlags::TEXTURE_USAGE_FLAGS_TRANSFER_DST;
    r::Handle<Texture> TextureResource = r::ResourceManager->CreateTexture(Description);

    KASSERT(!TextureResource.IsInvalid());

    u64           TextureSize = (u64)(Description.Dimensions.x * Description.Dimensions.y * Description.Dimensions.z * Description.Dimensions.w);
    r::BufferView StagingBuffer = r::ResourceManager->CreateTempBuffer(TextureSize);
    MemCpy(StagingBuffer.Ptr, Data, TextureSize);

    if (!r::ResourceManager->UploadTexture(TextureResource, StagingBuffer.GPUBuffer, StagingBuffer.Offset))
    {
        KERROR("Texture upload failed");
        // TODO (amn): Delete texture handle
    }
    else
    {
        State->DirtyTextures.Push(TextureResource);
    }

    return TextureResource;
}

kraft::BufferView TextureSystem::CreateEmptyTexture(u32 Width, u32 Height, u8 Channels)
{
    u32 TextureSize = Width * Height * Channels;
    u8* Pixels = (u8*)Malloc(TextureSize, MEMORY_TAG_TEXTURE);
    MemSet(Pixels, 255, TextureSize);

    const int      Segments = 8;
    const int      BoxSize = Width / Segments;
    unsigned char  White[4] = { 255, 255, 255, 255 };
    unsigned char  Black[4] = { 0, 0, 0, 255 };
    unsigned char* Color = NULL;
    bool           Swap = 0;
    bool           Fill = 0;
    for (u32 i = 0; i < Width * Height; i++)
    {
        if (i > 0)
        {
            if (i % (Width * BoxSize) == 0)
                Swap = !Swap;

            if (i % BoxSize == 0)
                Fill = !Fill;
        }

        if (Fill)
        {
            if (Swap)
                Color = Black;
            else
                Color = White;
        }
        else
        {
            if (Swap)
                Color = White;
            else
                Color = Black;
        }

        for (u32 j = 0; j < Channels; j++)
        {
            Pixels[i * Channels + j] = Color[j];
        }
    }

    return { .Memory = Pixels, .Size = TextureSize };

    // for (int i = 0; i < width; i++)
    // {
    //     for (int j = 0; j < height; j++)
    //     {
    //         u8 color = (((i & 0x8) == 0) ^ ((j & 0x8)  == 0)) * 255;
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

    // Renderer->CreateTexture(pixels, out);
    // Free(pixels, TextureSize, MEMORY_TAG_TEXTURE);
}

r::Handle<Texture> TextureSystem::GetDefaultDiffuseTexture()
{
    return State->TextureCache[KRAFT_DEFAULT_DIFFUSE_TEXTURE_NAME].Resource;
}

Array<r::Handle<Texture>> TextureSystem::GetDirtyTextures()
{
    return State->DirtyTextures;
}

void TextureSystem::ClearDirtyTextures()
{
    State->DirtyTextures.Clear();
}

//
// Internal methods
//

static void _createDefaultTextures()
{
    const int         Width = 256;
    const int         Height = 256;
    const int         Channels = 4;
    kraft::BufferView TextureData = TextureSystem::CreateEmptyTexture(Width, Height, Channels);

    r::Handle<Texture> TextureResource = TextureSystem::CreateTextureWithData(
        {
            .DebugName = "Default-Diffuse-Texture",
            .Dimensions = { (float32)Width, (float32)Height, 1, (float32)Channels },
            .Format = r::Format::RGBA8_UNORM,
            .Usage = r::TextureUsageFlags::TEXTURE_USAGE_FLAGS_TRANSFER_SRC | r::TextureUsageFlags::TEXTURE_USAGE_FLAGS_TRANSFER_DST | r::TextureUsageFlags::TEXTURE_USAGE_FLAGS_SAMPLED,
        },
        TextureData.Memory
    );

    State->TextureCache[KRAFT_DEFAULT_DIFFUSE_TEXTURE_NAME] = TextureReference(TextureResource, false);
    Free((void*)TextureData.Memory, TextureData.Size, MEMORY_TAG_TEXTURE);
}

} // namespace kraft
