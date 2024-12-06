#pragma once

#include <containers/kraft_buffer.h>
#include <containers/kraft_hashmap.h>
#include <core/kraft_core.h>
#include <core/kraft_string.h>
#include <math/kraft_math.h>

#if USE_ASSIMP
struct aiScene;
struct aiNode;
struct aiMesh;
struct aiMaterial;
#else
struct ufbx_scene;
struct ufbx_node;
struct ufbx_mesh;
struct ufbx_material;
#endif

namespace kraft {

enum TextureMapType : uint8;

struct Geometry;
struct Material;
struct Texture;
namespace renderer {
template<typename T>
struct Handle;
}

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

KRAFT_API class AssetDatabase
{
private:
    kraft::FlatHashMap<String, uint16> AssetsIndexMap;
    Array<Asset>                       Assets;
    Array<MeshAsset>                   Meshes;
    uint16                             MeshCount;

public:
    static AssetDatabase* Ptr;

    AssetDatabase() : Meshes(1024), MeshCount(0)
    {}

    ~AssetDatabase()
    {}

    MeshAsset* LoadMesh(const String& Path);

private:
#if USE_ASSIMP
    void ProcessAINode(MeshAsset& BaseMesh, aiNode* Node, const aiScene* Scene, const Mat4f& ParentTransform);
    void ProcessAIMesh(MeshAsset& BaseMesh, MeshT& Out, aiMesh* Mesh, const aiScene* Scene);
    void LoadAIMaterialTextures(const MeshAsset& BaseMesh, MeshT& Out, aiMaterial* Material, int Type, TextureMapType MapType);
#endif
};

}