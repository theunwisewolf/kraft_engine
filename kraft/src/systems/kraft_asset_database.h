#pragma once

#include <core/kraft_core.h>

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
struct MeshAsset;
struct TextureAsset;
struct ArenaAllocator;

namespace renderer {
template<typename T>
struct Handle;
}

KRAFT_API struct AssetDatabase
{
    static void          Init();
    static void          Shutdown();
    static MeshAsset*    LoadMesh(ArenaAllocator* arena, String8 path);
    static TextureAsset* LoadTexture(ArenaAllocator* arena, String8 path);

private:
#if USE_ASSIMP
    void ProcessAINode(MeshAsset& BaseMesh, aiNode* Node, const aiScene* Scene, const Mat4f& ParentTransform);
    void ProcessAIMesh(MeshAsset& BaseMesh, MeshT& Out, aiMesh* Mesh, const aiScene* Scene);
    void LoadAIMaterialTextures(const MeshAsset& BaseMesh, MeshT& Out, aiMaterial* Material, int Type, TextureMapType MapType);
#endif
};

}