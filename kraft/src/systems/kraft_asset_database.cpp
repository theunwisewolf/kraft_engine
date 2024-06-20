#include "kraft_asset_database.h"

#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <containers/kraft_array.h>
#include <containers/kraft_hashmap.h>
#include <systems/kraft_geometry_system.h>
#include <renderer/kraft_renderer_types.h>
#include <platform/kraft_filesystem.h>
#include <systems/kraft_texture_system.h>

namespace kraft
{

AssetDatabase* AssetDatabase::Ptr = new AssetDatabase();

void AssetDatabase::LoadAIMaterialTextures(MeshAsset& Out, aiMaterial *Material, int Type, TextureMapType MapType)
{
    for (unsigned int i = 0; i < Material->GetTextureCount((aiTextureType)Type); i++)
    {
        aiString TexturePath;
        Material->GetTexture((aiTextureType)Type, i, &TexturePath);

        Handle<Texture> LoadedTexture = TextureSystem::AcquireTexture(Out.Directory + "/" + TexturePath.C_Str());
        if (LoadedTexture.IsInvalid())
        {
            KERROR("[AssetDatabase::LoadMesh]: Failed to load texture %s", TexturePath.C_Str());
            continue;
        }

        Out.Textures.Push(LoadedTexture);
    }
}

void AssetDatabase::ProcessAIMesh(aiMesh *Mesh, const aiScene *Scene, MeshAsset& Out)
{
    // data to fill
    Array<Vertex3D> Vertices;
    Array<uint32> Indices;

    // walk through each of the mesh's vertices
    for (uint32 i = 0; i < Mesh->mNumVertices; i++)
    {
        Vertex3D Vertex;

        Vertex.Position.x = Mesh->mVertices[i].x;
        Vertex.Position.y = Mesh->mVertices[i].y;
        Vertex.Position.z = Mesh->mVertices[i].z;

        // normals
        // if (mesh->HasNormals())
        // {
        //     vector.x = mesh->mNormals[i].x;
        //     vector.y = mesh->mNormals[i].y;
        //     vector.z = mesh->mNormals[i].z;
        //     vertex.Normal = vector;
        // }

        // texture coordinates
        if (Mesh->mTextureCoords[0]) // does the mesh contain texture coordinates?
        {
            // a vertex can contain up to 8 different texture coordinates. We thus make the assumption that we won't 
            // use models where a vertex can have multiple texture coordinates so we always take the first set (0).
            Vertex.UV.x = Mesh->mTextureCoords[0][i].x; 
            Vertex.UV.y = Mesh->mTextureCoords[0][i].y;

            // tangent
            // vector.x = mesh->mTangents[i].x;
            // vector.y = mesh->mTangents[i].y;
            // vector.z = mesh->mTangents[i].z;
            // vertex.Tangent = vector;
            // // bitangent
            // vector.x = mesh->mBitangents[i].x;
            // vector.y = mesh->mBitangents[i].y;
            // vector.z = mesh->mBitangents[i].z;
            // vertex.Bitangent = vector;
        }
        else
        {
            Vertex.UV = kraft::Vec2fZero;
        }

        Vertices.Push(Vertex);
    }

    // now wak through each of the mesh's faces (a face is a mesh its triangle) and retrieve the corresponding vertex indices.
    for (int i = 0; i < Mesh->mNumFaces; i++)
    {
        aiFace Face = Mesh->mFaces[i];
        
        // retrieve all indices of the face and store them in the indices vector
        for (unsigned int j = 0; j < Face.mNumIndices; j++)
            Indices.Push(Face.mIndices[j]);
    }

    // process materials
    aiMaterial* Material = Scene->mMaterials[Mesh->mMaterialIndex];    

    this->LoadAIMaterialTextures(Out, Material, aiTextureType_DIFFUSE, TEXTURE_MAP_TYPE_DIFFUSE);
    this->LoadAIMaterialTextures(Out, Material, aiTextureType_SPECULAR, TEXTURE_MAP_TYPE_SPECULAR);
    this->LoadAIMaterialTextures(Out, Material, aiTextureType_NORMALS, TEXTURE_MAP_TYPE_NORMAL);
    this->LoadAIMaterialTextures(Out, Material, aiTextureType_HEIGHT, TEXTURE_MAP_TYPE_HEIGHT);

    kraft::GeometryData Geometry = {};
    Geometry.IndexCount = Indices.Size();
    Geometry.IndexSize = sizeof(uint32);
    Geometry.Indices = Indices.Data();
    Geometry.VertexCount = Vertices.Size();
    Geometry.VertexSize = sizeof(Vertex3D);
    Geometry.Vertices = Vertices.Data();
    Geometry.Name = "VikingHouse";

    Out.Geometry = kraft::GeometrySystem::AcquireGeometryWithData(Geometry);
}

void AssetDatabase::ProcessAINode(aiNode *Node, const aiScene *Scene)
{
    // process all the node's meshes (if any)
    for (unsigned int i = 0; i < Node->mNumMeshes; i++)
    {
        aiMesh *Mesh = Scene->mMeshes[Node->mMeshes[i]];
        MeshAsset& Asset = this->Meshes[this->MeshCount];

        ProcessAIMesh(Mesh, Scene, Asset);
        this->MeshCount++;
    }

    // then do the same for each of its children
    for (unsigned int i = 0; i < Node->mNumChildren; i++)
    {
        ProcessAINode(Node->mChildren[i], Scene);
    }
} 

MeshAsset* AssetDatabase::LoadMesh(const String& Path)
{
    const struct aiScene* Scene = aiImportFile(*Path,
        aiProcess_CalcTangentSpace       |
        aiProcess_Triangulate            |
        aiProcess_JoinIdenticalVertices  |
        aiProcess_SortByPType
    );

    // If the import failed, report it
    if (nullptr == Scene || Scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !Scene->mRootNode)
    {
        KERROR("[AssetDatabase::LoadMesh]: Failed to import asset. Error: %s", aiGetErrorString());
        return nullptr;
    }

    // The scene may contain more meshes, but we want to return the first one
    MeshAsset* Mesh = &this->Meshes[this->MeshCount];
    Mesh->Directory = filesystem::Dirname(Path);
    Mesh->Filename = filesystem::Basename(Path);

    this->ProcessAINode(Scene->mRootNode, Scene);

    aiReleaseImport(Scene);

    return Mesh;
}

}
