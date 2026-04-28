#pragma once

namespace kraft {

struct Asset {
    enum Type {
        Image,
        Mesh,
    } Type;

    String8 Directory;
    String8 Filename;
};

struct MeshT {
    Geometry* Geometry;
    Material* MaterialInstance;
    Array<r::Handle<Texture>> Textures;
    Mat4f Transform;
    i32 NodeIdx;
};

struct MeshAsset : public Asset {
    struct Node {
        char Name[128];
        Mat4f WorldTransform;
        i32 MeshIdx;
        i32 ParentNodeIdx;
    };

    Array<MeshT> SubMeshes;
    Array<Node> NodeHierarchy;
};

struct TextureAsset : public Asset {
    Texture texture;
};

struct SpriteRect {
    String8 name;
    Vec2f uv_min;
    Vec2f uv_max;
    u16 pixel_x;
    u16 pixel_y;
    u16 pixel_w;
    u16 pixel_h;
};

struct SpriteAtlasAsset {
    String8 path;
    r::Handle<Texture> texture;
    u16 atlas_width;
    u16 atlas_height;
    u32 sprite_count;
    FlatHashMap<u64, SpriteRect> sprites; // key = FNV1a hash of sprite name

    const SpriteRect* Get(String8 name) const;
};

} // namespace kraft