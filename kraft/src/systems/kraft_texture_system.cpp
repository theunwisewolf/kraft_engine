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

#define KRAFT_DEFAULT_DIFFUSE_TEXTURE_NAME S("default-diffuse")

namespace kraft {

struct TextureReference {
    u32 ref_count;
    bool auto_release;
    r::Handle<Texture> handle;

    TextureReference() : ref_count(0), auto_release(true), handle(r::Handle<Texture>::Invalid()) {}

    TextureReference(r::Handle<Texture> handle, bool auto_release = true) : ref_count(0), auto_release(auto_release), handle(handle) {}
};

// Private state
struct TextureSystemState {
    u32 max_texture_count;
    FlatHashMap<String8, TextureReference> cache;
    Array<r::Handle<Texture>> dirty_textures;
};

static TextureSystemState* texture_system_state = 0;
static void _createDefaultTextures();

static r::Format::Enum FormatFromChannels(u8 channels) {
    if (channels == 1) {
        return r::Format::RED;
    }

    if (channels == 3) {
        return r::Format::RGB8_UNORM;
    }

    if (channels == 4) {
        return r::Format::RGBA8_UNORM;
    }

    return r::Format::RGBA8_UNORM;
}

void TextureSystem::Init(u32 max_texture_count) {
    void* raw_memory = Malloc(sizeof(TextureSystemState), MEMORY_TAG_TEXTURE_SYSTEM, true);

    texture_system_state = new (raw_memory) TextureSystemState{max_texture_count};
    texture_system_state = (TextureSystemState*)raw_memory;

    _createDefaultTextures();
}

void TextureSystem::Shutdown() {
    u32 total_size = sizeof(TextureSystemState);
    kraft::Free(texture_system_state, total_size, MEMORY_TAG_TEXTURE_SYSTEM);

    KINFO("[TextureSystem::Shutdown]: Shutting down texture system");
}

r::Handle<Texture> TextureSystem::AcquireTexture(String8 name, bool auto_release) {
    auto existing_texture = texture_system_state->cache.find(name);
    if (existing_texture != texture_system_state->cache.end()) {
        existing_texture->second.ref_count++;
        return existing_texture->second.handle;
    }

    if (texture_system_state->cache.size() == texture_system_state->max_texture_count) {
        KERROR("[TextureSystem::AcquireTexture]: Failed to acquire texture; Out-of-memory!");
        return r::Handle<Texture>::Invalid();
    }

    // Read the file
    // TODO (amn): File reading should be done by an asset database
    stbi_set_flip_vertically_on_load(1);

    i32 width, height, channels, desired_channels = 0;
    if (!stbi_info(name.str, &width, &height, &channels)) {
        KERROR("[TextureSystem::LoadTexture]: Failed to load metadata for image '%S' with error '%s'", name, stbi_failure_reason());
        return r::Handle<Texture>::Invalid();
    }

    // GPUs dont usually support 3 channel images, so we load with 4
    desired_channels = (channels == 3) ? 4 : channels;

    TempArena scratch = ScratchBegin(0, 0);

    // Stb expects a null-terminated string
    // While most string8 strings will be null terminated, it's better to be safe than sorry
    String8 texture_path_null_terminated = ArenaPushString8Copy(scratch.arena, name);
    u8* texture_data = stbi_load(texture_path_null_terminated.str, &width, &height, &channels, desired_channels);
    if (!texture_data) {
        KERROR("[TextureSystem::LoadTexture]: Failed to load image '%S'", name);
        if (const char* reason = stbi_failure_reason()) {
            KERROR("[TextureSystem::LoadTexture]: Error %s", reason);
        }

        ScratchEnd(scratch);
        return r::Handle<Texture>::Invalid();
    }

    r::Handle<Texture> handle = TextureSystem::CreateTextureWithData(
        {
            .DebugName = texture_path_null_terminated.str,
            .Dimensions = {(f32)width, (f32)height, 1, (f32)desired_channels},
            .Format = FormatFromChannels(desired_channels),
            .Usage = r::TextureUsageFlags::TEXTURE_USAGE_FLAGS_TRANSFER_SRC | r::TextureUsageFlags::TEXTURE_USAGE_FLAGS_TRANSFER_DST | r::TextureUsageFlags::TEXTURE_USAGE_FLAGS_SAMPLED,
        },
        texture_data
    );

    stbi_image_free(texture_data);

    texture_system_state->cache[name] = TextureReference(handle, auto_release);

    if (!handle.IsInvalid())
        KDEBUG("[TextureSystem::AcquireTexture]: Acquired texture %S", name);

    ScratchEnd(scratch);
    return handle;
}

r::Handle<Texture> TextureSystem::AcquireTextureWithData(String8 name, u8* data, u32 width, u32 height, u32 channels) {
    char debug_name[512] = {0};
    MemCpy(debug_name, name.ptr, name.count);

    r::Handle<Texture> resource = TextureSystem::CreateTextureWithData(
        {
            .DebugName = debug_name,
            .Dimensions = {(f32)width, (f32)height, 1, (f32)channels},
            .Format = FormatFromChannels(channels),
            .Usage = r::TextureUsageFlags::TEXTURE_USAGE_FLAGS_TRANSFER_SRC | r::TextureUsageFlags::TEXTURE_USAGE_FLAGS_TRANSFER_DST | r::TextureUsageFlags::TEXTURE_USAGE_FLAGS_SAMPLED,
        },
        data
    );

    if (!resource.IsInvalid())
        KDEBUG("[TextureSystem::AcquireTextureWithData]: Acquired texture '%S'", name);

    return resource;
}

void TextureSystem::ReleaseTexture(String8 texture_name) {
    if (StringEqual(texture_name, KRAFT_DEFAULT_DIFFUSE_TEXTURE_NAME)) {
        KDEBUG("[TextureSystem::ReleaseTexture]: '%S' is a default texture; Cannot be released!", texture_name);
        return;
    }

    KDEBUG("[TextureSystem::ReleaseTexture]: Releasing texture '%S'", texture_name);
    auto it = texture_system_state->cache.find(texture_name);
    if (it == texture_system_state->cache.end()) {
        KERROR("[TextureSystem::ReleaseTexture]: Called for unknown texture '%S'", texture_name);
        return;
    }

    TextureReference& ref = it->second;
    if (ref.ref_count == 0) {
        KWARN("[TextureSystem::ReleaseTexture]: Texture '%S' already released!", texture_name);
        return;
    }

    ref.ref_count--;
    if (ref.ref_count == 0 && ref.auto_release) {
        r::ResourceManager->DestroyTexture(ref.handle);
        texture_system_state->cache.erase(it);
    }
}

void TextureSystem::ReleaseTexture(r::Handle<Texture> handle) {
    KWARN("Not implemented");
    // TODO (amn): Figure this out
    // texture_system_state->cache.values()
    // if (Texture)
    // {
    //     TextureSystem::ReleaseTexture(Texture->Name);
    // }
}

r::Handle<Texture> TextureSystem::CreateTextureWithData(r::TextureDescription description, const u8* data) {
    description.Usage |= r::TextureUsageFlags::TEXTURE_USAGE_FLAGS_TRANSFER_DST;
    r::Handle<Texture> handle = r::ResourceManager->CreateTexture(description);

    KASSERT(!handle.IsInvalid());

    u64 size = (u64)(description.Dimensions.x * description.Dimensions.y * description.Dimensions.z * description.Dimensions.w);
    r::BufferView staging_buf = r::ResourceManager->CreateTempBuffer(size);
    MemCpy(staging_buf.Ptr, data, size);

    if (!r::ResourceManager->UploadTexture(handle, staging_buf.GPUBuffer, staging_buf.Offset)) {
        KERROR("Texture upload failed");
        // TODO (amn): Delete texture handle
    } else {
        texture_system_state->dirty_textures.Push(handle);
    }

    return handle;
}

kraft::BufferView TextureSystem::CreateEmptyTexture(u32 width, u32 height, u8 channels) {
    u32 texture_size = width * height * channels;
    u8* pixels = (u8*)Malloc(texture_size, MEMORY_TAG_TEXTURE);
    MemSet(pixels, 255, texture_size);

    const int segments = 8;
    const int box_size = width / segments;
    unsigned char white[4] = {255, 255, 255, 255};
    unsigned char black[4] = {0, 0, 0, 255};
    unsigned char* color = NULL;
    bool swap = 0;
    bool fill = 0;
    for (u32 i = 0; i < width * height; i++) {
        if (i > 0) {
            if (i % (width * box_size) == 0)
                swap = !swap;

            if (i % box_size == 0)
                fill = !fill;
        }

        if (fill) {
            if (swap)
                color = black;
            else
                color = white;
        } else {
            if (swap)
                color = white;
            else
                color = black;
        }

        for (u32 j = 0; j < channels; j++) {
            pixels[i * channels + j] = color[j];
        }
    }

    return {.Memory = pixels, .Size = texture_size};

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

r::Handle<Texture> TextureSystem::GetDefaultDiffuseTexture() {
    return texture_system_state->cache[KRAFT_DEFAULT_DIFFUSE_TEXTURE_NAME].handle;
}

Array<r::Handle<Texture>> TextureSystem::GetDirtyTextures() {
    return texture_system_state->dirty_textures;
}

void TextureSystem::ClearDirtyTextures() {
    texture_system_state->dirty_textures.Clear();
}

//
// Internal methods
//

static void _createDefaultTextures() {
    const i32 width = 256;
    const i32 height = 256;
    const i8 channels = 4;
    kraft::BufferView TextureData = TextureSystem::CreateEmptyTexture(width, height, channels);

    r::Handle<Texture> TextureResource = TextureSystem::CreateTextureWithData(
        {
            .DebugName = "Default-Diffuse-Texture",
            .Dimensions = {(f32)width, (f32)height, 1, (f32)channels},
            .Format = r::Format::RGBA8_UNORM,
            .Usage = r::TextureUsageFlags::TEXTURE_USAGE_FLAGS_TRANSFER_SRC | r::TextureUsageFlags::TEXTURE_USAGE_FLAGS_TRANSFER_DST | r::TextureUsageFlags::TEXTURE_USAGE_FLAGS_SAMPLED,
        },
        TextureData.Memory
    );

    texture_system_state->cache[KRAFT_DEFAULT_DIFFUSE_TEXTURE_NAME] = TextureReference(TextureResource, false);
    Free((void*)TextureData.Memory, TextureData.Size, MEMORY_TAG_TEXTURE);
}

} // namespace kraft
