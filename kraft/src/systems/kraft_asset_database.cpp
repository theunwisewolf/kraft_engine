#include "kraft_asset_database.h"

#if USE_ASSIMP
#include <assimp/cimport.h>
#include <assimp/matrix4x4.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#else
#include <ufbx/ufbx.h>
#endif

#include <containers/kraft_array.h>
#include <containers/kraft_hashmap.h>
#include <core/kraft_asserts.h>
#include <platform/kraft_filesystem.h>
#include <renderer/kraft_renderer_types.h>
#include <systems/kraft_geometry_system.h>
#include <systems/kraft_texture_system.h>

#include <systems/kraft_asset_types.h>
#include <resources/kraft_resource_types.h>

namespace kraft {

struct AssetDatabaseStateT
{
    kraft::FlatHashMap<String, uint16> AssetsIndexMap;
    Array<Asset>                       Assets;
    Array<MeshAsset>                   Meshes;
    uint16                             MeshCount = 0;

    AssetDatabaseStateT() : Meshes(1024) {};
} AssetDatabaseState;

AssetDatabase* AssetDatabase::Ptr = new AssetDatabase();

#if USE_ASSIMP
void AssetDatabase::LoadAIMaterialTextures(const MeshAsset& BaseMesh, MeshT& Out, aiMaterial* Material, int Type, TextureMapType MapType)
{
    for (unsigned int i = 0; i < Material->GetTextureCount((aiTextureType)Type); i++)
    {
        aiString TexturePath;
        Material->GetTexture((aiTextureType)Type, i, &TexturePath);

        Handle<Texture> LoadedTexture = TextureSystem::AcquireTexture(BaseMesh.Directory + "/" + TexturePath.C_Str());
        if (LoadedTexture.IsInvalid())
        {
            KERROR("[AssetDatabase::LoadMesh]: Failed to load texture %s", TexturePath.C_Str());
            continue;
        }

        Out.Textures.Push(LoadedTexture);
    }
}

void AssetDatabase::ProcessAIMesh(MeshAsset& BaseMesh, MeshT& Out, aiMesh* Mesh, const aiScene* Scene)
{
    Array<Vertex3D> Vertices;
    Array<uint32>   Indices;

    for (uint32 i = 0; i < Mesh->mNumVertices; i++)
    {
        Vertex3D Vertex = {};

        Vertex.Position.x = Mesh->mVertices[i].x;
        Vertex.Position.y = Mesh->mVertices[i].y;
        Vertex.Position.z = Mesh->mVertices[i].z;

        if (Mesh->HasNormals())
        {
            Vertex.Normal.x = mesh->mNormals[i].x;
            Vertex.Normal.y = mesh->mNormals[i].y;
            Vertex.Normal.z = mesh->mNormals[i].z;
        }

        // texture coordinates
        if (Mesh->mTextureCoords[0])
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
    for (unsigned int i = 0; i < Mesh->mNumFaces; i++)
    {
        aiFace Face = Mesh->mFaces[i];
        for (unsigned int j = 0; j < Face.mNumIndices; j++)
            Indices.Push(Face.mIndices[j]);
    }

    aiMaterial* Material = Scene->mMaterials[Mesh->mMaterialIndex];

    this->LoadAIMaterialTextures(BaseMesh, Out, Material, aiTextureType_DIFFUSE, TEXTURE_MAP_TYPE_DIFFUSE);
    this->LoadAIMaterialTextures(BaseMesh, Out, Material, aiTextureType_SPECULAR, TEXTURE_MAP_TYPE_SPECULAR);
    this->LoadAIMaterialTextures(BaseMesh, Out, Material, aiTextureType_NORMALS, TEXTURE_MAP_TYPE_NORMAL);
    this->LoadAIMaterialTextures(BaseMesh, Out, Material, aiTextureType_HEIGHT, TEXTURE_MAP_TYPE_HEIGHT);

    kraft::GeometryData Geometry = {};
    Geometry.IndexCount = (uint32)Indices.Size();
    Geometry.IndexSize = sizeof(uint32);
    Geometry.Indices = Indices.Data();
    Geometry.VertexCount = (uint32)Vertices.Size();
    Geometry.VertexSize = sizeof(Vertex3D);
    Geometry.Vertices = Vertices.Data();
    Geometry.Name = kraft::String(Mesh->mName.data, Mesh->mName.length);

    Out.Geometry = kraft::GeometrySystem::AcquireGeometryWithData(Geometry);
}

void AssetDatabase::ProcessAINode(MeshAsset& BaseMesh, aiNode* Node, const aiScene* Scene, const Mat4f& ParentTransform)
{
    Mat4f LocalTransform;
    LocalTransform[0][0] = Node->mTransformation.a1;
    LocalTransform[0][1] = Node->mTransformation.b1;
    LocalTransform[0][2] = Node->mTransformation.c1;
    LocalTransform[0][3] = Node->mTransformation.d1;

    LocalTransform[1][0] = Node->mTransformation.a2;
    LocalTransform[1][1] = Node->mTransformation.b2;
    LocalTransform[1][2] = Node->mTransformation.c2;
    LocalTransform[1][3] = Node->mTransformation.d2;

    LocalTransform[2][0] = Node->mTransformation.a3;
    LocalTransform[2][1] = Node->mTransformation.b3;
    LocalTransform[2][2] = Node->mTransformation.c3;
    LocalTransform[2][3] = Node->mTransformation.d3;

    LocalTransform[3][0] = Node->mTransformation.a4;
    LocalTransform[3][1] = Node->mTransformation.b4;
    LocalTransform[3][2] = Node->mTransformation.c4;
    LocalTransform[3][3] = Node->mTransformation.d4;

    Mat4f Transform = ParentTransform * LocalTransform;

    for (unsigned int i = 0; i < Node->mNumMeshes; i++)
    {
        aiMesh* Mesh = Scene->mMeshes[Node->mMeshes[i]];
        BaseMesh.SubMeshes.Push(MeshT());
        MeshT& SubMesh = BaseMesh.SubMeshes[BaseMesh.SubMeshes.Length - 1];

        ProcessAIMesh(BaseMesh, SubMesh, Mesh, Scene);
        SubMesh.Transform = Transform;
    }

    // then do the same for each of its children
    for (unsigned int i = 0; i < Node->mNumChildren; i++)
    {
        ProcessAINode(BaseMesh, Node->mChildren[i], Scene, Transform);
    }
}
#else

#endif

MeshAsset* AssetDatabase::LoadMesh(const String& Path)
{
#if USE_ASSIMP
    const struct aiScene* Scene =
        // aiImportFile(*Path, aiProcess_CalcTangentSpace | aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_SortByPType);
        aiImportFile(*Path, aiProcess_Triangulate | aiProcess_JoinIdenticalVertices);

    // If the import failed, report it
    if (nullptr == Scene || Scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !Scene->mRootNode)
    {
        KERROR("[AssetDatabase::LoadMesh]: Failed to import asset. Error: %s", aiGetErrorString());
        return nullptr;
    }

    // The scene may contain more meshes, but we want to return the first one
    MeshAsset& Mesh = this->Meshes[this->MeshCount];
    Mesh.Directory = filesystem::Dirname(Path);
    Mesh.Filename = filesystem::Basename(Path);
    Mesh.SubMeshes.Reserve(Scene->mNumMeshes);

    this->ProcessAINode(Mesh, Scene->mRootNode, Scene, kraft::Mat4f(Identity));

    aiReleaseImport(Scene);
#else
    ufbx_load_opts Options = { 0 };
    Options.target_axes = ufbx_axes_right_handed_y_up;
    // Options.target_unit_meters = 1.0f;
    // Options.space_conversion = UFBX_SPACE_CONVERSION_MODIFY_GEOMETRY;
    Options.generate_missing_normals = true;
    ufbx_error  UfbxError;
    ufbx_scene* Scene = ufbx_load_file(*Path, &Options, &UfbxError);

    if (!Scene)
    {
        KERROR("[AssetDatabase::LoadMesh]: Failed to import asset. Error: %s", UfbxError.description.data);
        return nullptr;
    }

    MeshAsset& Mesh = AssetDatabaseState.Meshes[AssetDatabaseState.MeshCount];
    Mesh.Directory = filesystem::Dirname(Path);
    Mesh.Filename = filesystem::Basename(Path);
    Mesh.SubMeshes.Reserve(Scene->meshes.count);

    Array<int32>    NodeToMeshInstanceMapping(Scene->nodes.count, -1);
    Array<Vertex3D> Vertices;
    Array<uint32>   Indices;
    Array<uint32>   TriangleIndices;
    for (int i = 0; i < Scene->meshes.count; i++)
    {
        ufbx_mesh* UfbxMesh = Scene->meshes[i];
        if (UfbxMesh->instances.count == 0)
            continue;

        uint32 MaxTriangles = 0;
        for (int PartIdx = 0; PartIdx < UfbxMesh->material_parts.count; PartIdx++)
        {
            ufbx_mesh_part* Part = &UfbxMesh->material_parts.data[PartIdx];
            if (Part->num_triangles == 0)
                continue;

            MaxTriangles = kraft::math::Max(MaxTriangles, (uint32)Part->num_triangles);
        }

        uint32 MaxIndices = UfbxMesh->max_face_triangles * 3;
        Vertices.Resize(MaxTriangles * 3);
        TriangleIndices.Resize(MaxIndices);
        for (size_t PartIdx = 0; PartIdx < UfbxMesh->material_parts.count; PartIdx++)
        {
            uint32          NumIndices = 0;
            ufbx_mesh_part* UfbxMeshPart = &UfbxMesh->material_parts.data[PartIdx];
            if (UfbxMeshPart->num_triangles == 0)
                continue;

            for (uint32 FaceIdx = 0; FaceIdx < UfbxMeshPart->num_faces; FaceIdx++)
            {
                ufbx_face UfbxFace = UfbxMesh->faces.data[UfbxMeshPart->face_indices.data[FaceIdx]];
                uint32    NumTriangles = ufbx_triangulate_face(TriangleIndices.Data(), MaxIndices, UfbxMesh, UfbxFace);
                for (uint32 TriangleIdx = 0; TriangleIdx < NumTriangles * 3; TriangleIdx++)
                {
                    uint32    Index = TriangleIndices[TriangleIdx];
                    ufbx_vec3 Position = ufbx_get_vertex_vec3(&UfbxMesh->vertex_position, Index);
                    Vertex3D& Vertex = Vertices[NumIndices];
                    Vertex.Position.x = Position.x;
                    Vertex.Position.y = Position.y;
                    Vertex.Position.z = Position.z;

                    ufbx_vec3 Normal = ufbx_get_vertex_vec3(&UfbxMesh->vertex_normal, Index);
                    Vertex.Normal.x = Normal.x;
                    Vertex.Normal.y = Normal.y;
                    Vertex.Normal.z = Normal.z;

                    if (UfbxMesh->vertex_uv.exists)
                    {
                        ufbx_vec2 UV = ufbx_get_vertex_vec2(&UfbxMesh->vertex_uv, Index);
                        Vertex.UV.x = UV.x;
                        Vertex.UV.y = UV.y;
                    }
                    else
                    {
                        Vertex.UV = kraft::Vec2fZero;
                    }

                    Vertices.Push(Vertex);
                    NumIndices++;
                }
            }

            Indices.Clear();
            Indices.Reserve(NumIndices);
            ufbx_vertex_stream Streams[1];
            size_t             NumStreams = 1;

            Streams[0].data = Vertices.Data();
            Streams[0].vertex_count = NumIndices;
            Streams[0].vertex_size = sizeof(Vertex3D);
            ufbx_error Error;
            uint32     NumVertices = ufbx_generate_indices(Streams, NumStreams, Indices.Data(), NumIndices, NULL, &Error);
            if (Error.type != UFBX_ERROR_NONE)
            {
                char ErrorStrBuffer[1024] = { 0 };
                ufbx_format_error(ErrorStrBuffer, sizeof(ErrorStrBuffer), &Error);
                KERROR("Failed to generate index buffer %s", ErrorStrBuffer);

                ufbx_free_scene(Scene);
                return nullptr;
            }

            GeometryData Geometry = {};
            Geometry.IndexCount = NumIndices;
            Geometry.IndexSize = sizeof(uint32);
            Geometry.Indices = Indices.Data();
            Geometry.VertexCount = NumVertices;
            Geometry.VertexSize = sizeof(Vertex3D);
            Geometry.Vertices = Vertices.Data();
            StringNCopy(Geometry.Name, UfbxMesh->name.data, UfbxMesh->name.length);

            for (int j = 0; j < UfbxMesh->instances.count; j++)
            {
                ufbx_node* UfbxNode = UfbxMesh->instances[j];
                NodeToMeshInstanceMapping[UfbxNode->typed_id] = Mesh.SubMeshes.Length;

                Mesh.SubMeshes.Push(MeshT());
                MeshT& SubMesh = Mesh.SubMeshes[Mesh.SubMeshes.Length - 1];
                SubMesh.Geometry = GeometrySystem::AcquireGeometryWithData(Geometry);

                ufbx_material* UfbxMaterial = UfbxMesh->materials[UfbxMeshPart->index];
                ufbx_texture*  UfbxTexture = UfbxMaterial->pbr.base_color.texture;
                if (UfbxTexture)
                {
                    Handle<Texture> LoadedTexture =
                        TextureSystem::AcquireTexture(String(UfbxTexture->filename.data, UfbxTexture->filename.length));
                    if (LoadedTexture.IsInvalid())
                    {
                        KERROR("[AssetDatabase::LoadMesh]: Failed to load texture %s", UfbxTexture->filename.data);
                        continue;
                    }

                    SubMesh.Textures.Push(LoadedTexture);
                }
            }
        }
    }

    // Update the node hierarchy
    Mesh.NodeHierarchy.Resize(Scene->nodes.count);
    for (int NodeIdx = 0; NodeIdx < Scene->nodes.count; NodeIdx++)
    {
        kraft::MeshAsset::Node& ThisNode = Mesh.NodeHierarchy[NodeIdx];
        ufbx_node*              UfbxNode = Scene->nodes[NodeIdx];
        MemCpy(ThisNode.Name, UfbxNode->name.data, sizeof(ThisNode.Name));

        if (UfbxNode->parent)
        {
            kraft::MeshAsset::Node& Parent = Mesh.NodeHierarchy[UfbxNode->parent->typed_id];
            Mat4f                   LocalTransform;
            LocalTransform[0][0] = UfbxNode->node_to_parent.m00;
            LocalTransform[0][1] = UfbxNode->node_to_parent.m10;
            LocalTransform[0][2] = UfbxNode->node_to_parent.m20;
            LocalTransform[0][3] = 0;
            LocalTransform[1][0] = UfbxNode->node_to_parent.m01;
            LocalTransform[1][1] = UfbxNode->node_to_parent.m11;
            LocalTransform[1][2] = UfbxNode->node_to_parent.m21;
            LocalTransform[1][3] = 0;
            LocalTransform[2][0] = UfbxNode->node_to_parent.m02;
            LocalTransform[2][1] = UfbxNode->node_to_parent.m12;
            LocalTransform[2][2] = UfbxNode->node_to_parent.m22;
            LocalTransform[2][3] = 0;
            LocalTransform[3][0] = UfbxNode->node_to_parent.m03;
            LocalTransform[3][1] = UfbxNode->node_to_parent.m13;
            LocalTransform[3][2] = UfbxNode->node_to_parent.m23;
            LocalTransform[3][3] = 1;

            ThisNode.WorldTransform = LocalTransform * Parent.WorldTransform;
            // ThisNode.WorldTransform = Parent.WorldTransform * LocalTransform;
            // ThisNode.WorldTransform = kraft::ScaleMatrix(kraft::Vec3f{20.0f, 20.0f, 20.0f}) * ThisNode.WorldTransform;
            ThisNode.ParentNodeIdx = UfbxNode->parent->typed_id;
        }
        else
        {
            // ThisNode.WorldTransform = kraft::Mat4f(Identity) * kraft::ScaleMatrix(kraft::Vec3f{20.0f, 20.0f, 20.0f});
            ThisNode.WorldTransform = kraft::Mat4f(Identity);
            ThisNode.ParentNodeIdx = -1;
        }

        if (UfbxNode->mesh)
        {
            KASSERT(NodeToMeshInstanceMapping[UfbxNode->typed_id] != -1);
            int32  MeshIdx = NodeToMeshInstanceMapping[UfbxNode->typed_id];
            MeshT& SubMesh = Mesh.SubMeshes[MeshIdx];
            SubMesh.Transform = ThisNode.WorldTransform;
            ThisNode.MeshIdx = MeshIdx;
            SubMesh.NodeIdx = NodeIdx;
        }
        else
        {
            ThisNode.MeshIdx = -1;
        }
    }

    ufbx_free_scene(Scene);
#endif

    return &AssetDatabaseState.Meshes[AssetDatabaseState.MeshCount++];
}

}
