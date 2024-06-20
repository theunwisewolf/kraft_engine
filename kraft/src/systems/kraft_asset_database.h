#pragma once

#include <core/kraft_core.h>
#include <core/kraft_string.h>
#include <containers/kraft_buffer.h>
#include <containers/kraft_hashmap.h>

struct aiNode;
struct aiScene;
struct aiMesh;
struct aiMaterial;

namespace kraft
{

enum TextureMapType : uint8;

struct Geometry;
struct Texture;
namespace renderer
{
    template <typename T>
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


struct MeshAsset : public Asset
{
    kraft::Geometry* Geometry;
    Array<renderer::Handle<Texture>> Textures;
};

KRAFT_API class AssetDatabase
{
private:
    kraft::FlatHashMap<String, uint16> AssetsIndexMap;
    Array<Asset> Assets;
    Array<MeshAsset> Meshes;
    uint16 MeshCount;

public:
    static AssetDatabase* Ptr;

    AssetDatabase() :
        MeshCount(0),
        Meshes(1024)
    {
    }

    ~AssetDatabase() {}

    MeshAsset* LoadMesh(const String& Path);

private:
    void ProcessAINode(aiNode *Node, const aiScene *Scene);
    void ProcessAIMesh(aiMesh *Mesh, const aiScene *Scene, MeshAsset& Out);
    void LoadAIMaterialTextures(MeshAsset& Out, aiMaterial *Material, int Type, TextureMapType MapType);
};

}