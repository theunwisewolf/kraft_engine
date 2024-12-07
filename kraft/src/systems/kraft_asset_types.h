#pragma once

namespace kraft {

struct Asset
{
    enum Type
    {
        Image,
        Mesh,
    } Type;

    String Directory;
    String Filename;
};

struct MeshT
{
    kraft::Geometry*                 Geometry;
    kraft::Material*                 MaterialInstance;
    Array<renderer::Handle<Texture>> Textures;
    kraft::Mat4f                     Transform;
    int32                            NodeIdx;
};

struct MeshAsset : public Asset
{
    struct Node
    {
        char  Name[128];
        Mat4f WorldTransform;
        int32 MeshIdx;
        int32 ParentNodeIdx;
    };

    Array<MeshT> SubMeshes;
    Array<Node>  NodeHierarchy;
};

}