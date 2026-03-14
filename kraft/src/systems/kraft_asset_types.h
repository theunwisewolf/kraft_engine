#pragma once

namespace kraft {

struct Asset
{
    enum Type
    {
        Image,
        Mesh,
    } Type;

    String8 Directory;
    String8 Filename;
};

struct MeshT
{
    Geometry*                 Geometry;
    Material*                 MaterialInstance;
    Array<r::Handle<Texture>> Textures;
    Mat4f                     Transform;
    i32                       NodeIdx;
};

struct MeshAsset : public Asset
{
    struct Node
    {
        char  Name[128];
        Mat4f WorldTransform;
        i32   MeshIdx;
        i32   ParentNodeIdx;
    };

    Array<MeshT> SubMeshes;
    Array<Node>  NodeHierarchy;
};

struct TextureAsset : public Asset
{
    Texture texture;
};

} // namespace kraft