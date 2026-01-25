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
#include <core/kraft_log.h>
#include <core/kraft_memory.h>
#include <platform/kraft_filesystem.h>

// TODO: REMOVE
#include <core/kraft_base_includes.h>

#include <renderer/kraft_renderer_types.h>
#include <systems/kraft_geometry_system.h>
#include <systems/kraft_texture_system.h>

#include <resources/kraft_resource_types.h>
#include <systems/kraft_asset_types.h>

#define CGLTF_IMPLEMENTATION
#include <cgltf/cgltf.h>

namespace kraft {

struct AssetDatabaseStateT
{
    Asset*     Assets;
    u64        assets_count;
    MeshAsset* Meshes;
    uint16     MeshCount = 0;
};

static MemoryBlock          AssetDatabaseMemory;
static AssetDatabaseStateT* AssetDatabaseStatePtr;

void AssetDatabase::Init()
{
    const uint64 MaxAssets = 256;
    const uint64 MaxMeshes = 1024;
    uint64       MemoryRequirement = sizeof(AssetDatabaseStateT) + (MaxAssets * sizeof(Asset)) + (MaxMeshes * sizeof(MeshAsset));
    AssetDatabaseMemory = MallocBlock(MemoryRequirement, MEMORY_TAG_ASSET_DB, true);
    AssetDatabaseStatePtr = (AssetDatabaseStateT*)AssetDatabaseMemory.Data;
    new (AssetDatabaseStatePtr) AssetDatabaseStateT;

    AssetDatabaseStatePtr->Assets = (Asset*)(AssetDatabaseMemory.Data + sizeof(AssetDatabaseStateT));
    AssetDatabaseStatePtr->assets_count = 0;
    AssetDatabaseStatePtr->Meshes = (MeshAsset*)(AssetDatabaseMemory.Data + sizeof(AssetDatabaseStateT) + (MaxAssets * sizeof(Asset)));
}

void AssetDatabase::Shutdown()
{
    kraft::FreeBlock(AssetDatabaseMemory);
}

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

static MeshAsset* LoadGLTFMesh(ArenaAllocator* arena, String8 path, MeshAsset* out_mesh);
MeshAsset*        AssetDatabase::LoadMesh(ArenaAllocator* arena, String8 path)
{
    // auto It = AssetDatabaseStatePtr->AssetsIndexMap.find(path);
    // if (It != AssetDatabaseStatePtr->AssetsIndexMap.end())
    // {
    //     return &AssetDatabaseStatePtr->Meshes[It->second];
    // }

    // for (i32 i = 0; i < AssetDatabaseStatePtr->assets_count; i++)
    // {
    //     if (StringEqual(AssetDatabaseStatePtr->Assets[i].)
    // }

    MeshAsset* mesh = &AssetDatabaseStatePtr->Meshes[AssetDatabaseStatePtr->MeshCount];
    mesh->Directory = fs::Dirname(arena, path);
    mesh->Filename = fs::Basename(arena, path);

    // GLTF
    if (StringEndsWith(path, S(".gltf")) || StringEndsWith(path, S(".glb")))
    {
        KASSERT(LoadGLTFMesh(arena, path, mesh));
        return &AssetDatabaseStatePtr->Meshes[AssetDatabaseStatePtr->MeshCount++];
    }

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
    Mesh.Directory = fs::Dirname(Path);
    Mesh.Filename = fs::Basename(Path);
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
    ufbx_scene* scene = ufbx_load_file_len((char*)path.ptr, path.count, &Options, &UfbxError);

    if (!scene)
    {
        KERROR("[AssetDatabase::LoadMesh]: Failed to import asset. Error: %s", UfbxError.description.data);
        return nullptr;
    }

    mesh->SubMeshes.Reserve(scene->meshes.count);

    Array<i32>         node_to_mesh_instance_mapping(scene->nodes.count, -1);
    Array<r::Vertex3D> vertices;
    Array<u32>         indices;
    Array<u32>         triangle_indices;
    for (int i = 0; i < scene->meshes.count; i++)
    {
        ufbx_mesh* ufbx_mesh = scene->meshes[i];
        if (ufbx_mesh->instances.count == 0)
            continue;

        u32 max_triangles = 0;
        for (int PartIdx = 0; PartIdx < ufbx_mesh->material_parts.count; PartIdx++)
        {
            ufbx_mesh_part* Part = &ufbx_mesh->material_parts.data[PartIdx];
            if (Part->num_triangles == 0)
                continue;

            max_triangles = kraft::math::Max(max_triangles, (u32)Part->num_triangles);
        }

        u32 max_indices = ufbx_mesh->max_face_triangles * 3;
        vertices.Resize(max_triangles * 3);
        triangle_indices.Resize(max_indices);
        for (u64 part_idx = 0; part_idx < ufbx_mesh->material_parts.count; part_idx++)
        {
            u32             num_indices = 0;
            ufbx_mesh_part* ufbx_mesh_part = &ufbx_mesh->material_parts.data[part_idx];
            if (ufbx_mesh_part->num_triangles == 0)
                continue;

            for (uint32 face_idx = 0; face_idx < ufbx_mesh_part->num_faces; face_idx++)
            {
                ufbx_face ufbx_face = ufbx_mesh->faces.data[ufbx_mesh_part->face_indices.data[face_idx]];
                u32       num_triangles = ufbx_triangulate_face(triangle_indices.Data(), max_indices, ufbx_mesh, ufbx_face);
                for (u32 triangle_idx = 0; triangle_idx < num_triangles * 3; triangle_idx++)
                {
                    u32          index = triangle_indices[triangle_idx];
                    ufbx_vec3    position = ufbx_get_vertex_vec3(&ufbx_mesh->vertex_position, index);
                    r::Vertex3D& vertex = vertices[num_indices];
                    vertex.Position.x = position.x;
                    vertex.Position.y = position.y;
                    vertex.Position.z = position.z;

                    ufbx_vec3 Normal = ufbx_get_vertex_vec3(&ufbx_mesh->vertex_normal, index);
                    vertex.Normal.x = Normal.x;
                    vertex.Normal.y = Normal.y;
                    vertex.Normal.z = Normal.z;

                    if (ufbx_mesh->vertex_uv.exists)
                    {
                        ufbx_vec2 UV = ufbx_get_vertex_vec2(&ufbx_mesh->vertex_uv, index);
                        vertex.UV.x = UV.x;
                        vertex.UV.y = UV.y;
                    }
                    else
                    {
                        vertex.UV = kraft::Vec2fZero;
                    }

                    // Vertices.Push(Vertex);
                    num_indices++;
                }
            }

            indices.Clear();
            indices.Reserve(num_indices);
            ufbx_vertex_stream streams[1];
            u64                num_streams = 1;

            streams[0].data = vertices.Data();
            streams[0].vertex_count = num_indices;
            streams[0].vertex_size = sizeof(r::Vertex3D);
            ufbx_error error;
            u32        num_vertices = ufbx_generate_indices(streams, num_streams, indices.Data(), num_indices, NULL, &error);
            if (error.type != UFBX_ERROR_NONE)
            {
                char error_str_buf[1024] = { 0 };
                ufbx_format_error(error_str_buf, sizeof(error_str_buf), &error);
                KERROR("Failed to generate index buffer %s", error_str_buf);

                ufbx_free_scene(scene);
                return nullptr;
            }

            GeometryData geometry = {};
            geometry.IndexCount = num_indices;
            geometry.IndexSize = sizeof(u32);
            geometry.Indices = indices.Data();
            geometry.VertexCount = num_vertices;
            geometry.VertexSize = sizeof(r::Vertex3D);
            geometry.Vertices = vertices.Data();

            for (int j = 0; j < ufbx_mesh->instances.count; j++)
            {
                ufbx_node* ufbx_node = ufbx_mesh->instances[j];
                node_to_mesh_instance_mapping[ufbx_node->typed_id] = mesh->SubMeshes.Length;

                mesh->SubMeshes.Push(MeshT());
                MeshT& SubMesh = mesh->SubMeshes[mesh->SubMeshes.Length - 1];
                SubMesh.Geometry = GeometrySystem::AcquireGeometryWithData(geometry);

                ufbx_material* ufbx_material = ufbx_mesh->materials[ufbx_mesh_part->index];
                ufbx_texture*  ufbx_texture = ufbx_material->pbr.base_color.texture;
                if (ufbx_texture)
                {
                    r::Handle<Texture> texture = TextureSystem::AcquireTexture(String(ufbx_texture->filename.data, ufbx_texture->filename.length));
                    if (texture.IsInvalid())
                    {
                        KERROR("[AssetDatabase::LoadMesh]: Failed to load texture %s", ufbx_texture->filename.data);
                        continue;
                    }

                    SubMesh.Textures.Push(texture);
                }
            }
        }
    }

    // Update the node hierarchy
    mesh->NodeHierarchy.Resize(scene->nodes.count);
    for (int node_idx = 0; node_idx < scene->nodes.count; node_idx++)
    {
        MeshAsset::Node& this_node = mesh->NodeHierarchy[node_idx];
        ufbx_node*       ufbx_node = scene->nodes[node_idx];
        MemCpy(this_node.Name, ufbx_node->name.data, sizeof(this_node.Name));

        if (ufbx_node->parent)
        {
            MeshAsset::Node& parent = mesh->NodeHierarchy[ufbx_node->parent->typed_id];
            Mat4f            local_transform;
            local_transform[0][0] = ufbx_node->node_to_parent.m00;
            local_transform[0][1] = ufbx_node->node_to_parent.m10;
            local_transform[0][2] = ufbx_node->node_to_parent.m20;
            local_transform[0][3] = 0;
            local_transform[1][0] = ufbx_node->node_to_parent.m01;
            local_transform[1][1] = ufbx_node->node_to_parent.m11;
            local_transform[1][2] = ufbx_node->node_to_parent.m21;
            local_transform[1][3] = 0;
            local_transform[2][0] = ufbx_node->node_to_parent.m02;
            local_transform[2][1] = ufbx_node->node_to_parent.m12;
            local_transform[2][2] = ufbx_node->node_to_parent.m22;
            local_transform[2][3] = 0;
            local_transform[3][0] = ufbx_node->node_to_parent.m03;
            local_transform[3][1] = ufbx_node->node_to_parent.m13;
            local_transform[3][2] = ufbx_node->node_to_parent.m23;
            local_transform[3][3] = 1;

            this_node.WorldTransform = local_transform * parent.WorldTransform;
            // ThisNode.WorldTransform = Parent.WorldTransform * LocalTransform;
            // ThisNode.WorldTransform = kraft::ScaleMatrix(kraft::Vec3f{20.0f, 20.0f, 20.0f}) * ThisNode.WorldTransform;
            this_node.ParentNodeIdx = ufbx_node->parent->typed_id;
        }
        else
        {
            // ThisNode.WorldTransform = kraft::Mat4f(Identity) * kraft::ScaleMatrix(kraft::Vec3f{20.0f, 20.0f, 20.0f});
            this_node.WorldTransform = kraft::Mat4f(Identity);
            this_node.ParentNodeIdx = -1;
        }

        if (ufbx_node->mesh)
        {
            KASSERT(node_to_mesh_instance_mapping[ufbx_node->typed_id] != -1);
            i32    mesh_idx = node_to_mesh_instance_mapping[ufbx_node->typed_id];
            MeshT& submesh = mesh->SubMeshes[mesh_idx];
            submesh.Transform = this_node.WorldTransform;
            this_node.MeshIdx = mesh_idx;
            submesh.NodeIdx = node_idx;
        }
        else
        {
            this_node.MeshIdx = -1;
        }
    }

    ufbx_free_scene(scene);
#endif

    return &AssetDatabaseStatePtr->Meshes[AssetDatabaseStatePtr->MeshCount++];
}

static MeshAsset* LoadGLTFMesh(ArenaAllocator* arena, String8 path, MeshAsset* out_mesh)
{
    cgltf_options options = {};
    cgltf_data*   data = nullptr;

    TempArena scratch = ScratchBegin(&arena, 1);
    buffer    file_buf = fs::ReadAllBytes(scratch.arena, path);

    cgltf_result result = cgltf_parse(&options, file_buf.ptr, file_buf.count, &data);
    // cgltf_result result = cgltf_parse_file(&options, path.str, &data);

    if (result != cgltf_result_success)
    {
        ScratchEnd(scratch);
        return nullptr;
    }

    char path_cstr[512];
    MemCpy(path_cstr, path.ptr, path.count);
    path_cstr[path.count] = '\0';

    result = cgltf_load_buffers(&options, data, path_cstr);
    if (result != cgltf_result_success)
    {
        cgltf_free(data);
        ScratchEnd(scratch);
        return nullptr;
    }

    for (cgltf_size mesh_idx = 0; mesh_idx < data->meshes_count; mesh_idx++)
    {
        cgltf_mesh* mesh = &data->meshes[mesh_idx];
        for (cgltf_size primitive_idx = 0; primitive_idx < mesh->primitives_count; primitive_idx++)
        {
            cgltf_primitive* primitive = &mesh->primitives[primitive_idx];
            if (primitive->type != cgltf_primitive_type_triangles)
            {
                continue;
            }

            cgltf_accessor* position_accessor = NULL;
            cgltf_accessor* uv_accessor = NULL;
            cgltf_accessor* normal_accessor = NULL;

            for (cgltf_size attribute_idx = 0; attribute_idx < primitive->attributes_count; attribute_idx++)
            {
                cgltf_attribute* attribute = &primitive->attributes[attribute_idx];
                switch (attribute->type)
                {
                    case cgltf_attribute_type_position: position_accessor = attribute->data; break;
                    case cgltf_attribute_type_texcoord: uv_accessor = attribute->data; break;
                    case cgltf_attribute_type_normal:   normal_accessor = attribute->data; break;
                }
            }

            KASSERT(position_accessor);

            u64          arena_pos = ArenaPosition(arena);
            u32          vertex_count = position_accessor->count;
            r::Vertex3D* vertices = ArenaPushArray(arena, r::Vertex3D, vertex_count);
            for (cgltf_size i = 0; i < vertex_count; i++)
            {
                r::Vertex3D* vertex = &vertices[i];
                cgltf_accessor_read_float(position_accessor, i, vertex->Position._data, 3);

                if (uv_accessor)
                {
                    cgltf_accessor_read_float(uv_accessor, i, vertex->UV._data, 2);
                }

                if (normal_accessor)
                {
                    cgltf_accessor_read_float(normal_accessor, i, vertex->Normal._data, 3);
                }
            }

            u32* indices = nullptr;
            u32  index_count = 0;
            if (primitive->indices)
            {
                index_count = primitive->indices->count;
                indices = ArenaPushArray(arena, u32, index_count);
                for (cgltf_size i = 0; i < index_count; i++)
                {
                    indices[i] = (u32)cgltf_accessor_read_index(primitive->indices, i);
                }
            }

            GeometryData geometry{
                .VertexCount = vertex_count,
                .IndexCount = index_count,
                .VertexSize = sizeof(r::Vertex3D),
                .IndexSize = sizeof(u32),
                .Vertices = vertices,
                .Indices = (void*)indices,
            };

            MeshT submesh = {};
            submesh.Geometry = GeometrySystem::AcquireGeometryWithData(geometry);
            submesh.Transform = Mat4f(Identity);

            ArenaPopToPosition(arena, arena_pos);

            out_mesh->SubMeshes.Push(submesh);
        }
    }

    cgltf_free(data);
    ScratchEnd(scratch);
    return out_mesh;
}

} // namespace kraft
