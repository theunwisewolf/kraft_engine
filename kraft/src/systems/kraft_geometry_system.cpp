#include "kraft_geometry_system.h"

#include <core/kraft_base_includes.h>
#include <containers/kraft_containers_includes.h>
#include <core/kraft_base_includes.h>
#include <renderer/kraft_renderer_includes.h>
#include <systems/kraft_systems_includes.h>

#include <resources/kraft_resource_types.h>

namespace kraft {

static GeometrySystemState* geometry_system_state = nullptr;

static void _createDefaultGeometries();

void GeometrySystem::Init(GeometrySystemConfig Config)
{
    u32 total_size = sizeof(GeometrySystemState) + sizeof(GeometryReference) * Config.MaxGeometriesCount;

    u8* memory = (u8*)Malloc(total_size, MEMORY_TAG_GEOMETRY_SYSTEM, true);
    geometry_system_state = (GeometrySystemState*)memory;
    geometry_system_state->max_geometries_count = Config.MaxGeometriesCount;
    geometry_system_state->geometries = (GeometryReference*)(memory + sizeof(GeometrySystemState));

    _createDefaultGeometries();
}

void GeometrySystem::Shutdown()
{
    for (u32 i = 0; i < geometry_system_state->max_geometries_count; ++i)
    {
        GeometryReference* reference = &geometry_system_state->geometries[i];
        if (reference->ref_count > 0)
        {
            GeometrySystem::DestroyGeometry(&reference->geometry);
        }
    }

    u32 total_size = sizeof(GeometrySystemState) + sizeof(GeometryReference) * geometry_system_state->max_geometries_count;
    Free(geometry_system_state, total_size, MEMORY_TAG_GEOMETRY_SYSTEM);
}

Geometry* GeometrySystem::GetDefaultGeometry()
{
    return &geometry_system_state->geometries[0].geometry;
}

Geometry* GeometrySystem::GetDefault2DGeometry()
{
    return &geometry_system_state->geometries[1].geometry;
}

Geometry* GeometrySystem::AcquireGeometry(u32 id)
{
    for (u32 i = 0; i < geometry_system_state->max_geometries_count; ++i)
    {
        GeometryReference* reference = &geometry_system_state->geometries[i];
        if (reference->geometry.ID == id)
        {
            return &reference->geometry;
        }
    }

    KERROR("[GeometrySystem::AcquireGeometry]: Geometry with id %d not found", id);

    return nullptr;
}

Geometry* GeometrySystem::AcquireGeometryWithData(GeometryData data, bool auto_release)
{
    int index = -1;
    for (u32 i = 0; i < geometry_system_state->max_geometries_count; ++i)
    {
        GeometryReference* reference = &geometry_system_state->geometries[i];
        if (reference->ref_count == 0)
        {
            index = (int)i;
            break;
        }
    }

    if (index == -1)
    {
        KERROR("[GeometrySystem::AcquireGeometryWithData]: No free slots available; Out of memory!");
        return nullptr;
    }

    GeometryReference* reference = &geometry_system_state->geometries[index];
    reference->ref_count = 1;
    reference->auto_release = auto_release;
    reference->geometry.ID = index;

#ifdef KRAFT_GUI_APP
    if (!g_Renderer->CreateGeometry(&reference->geometry, data.VertexCount, data.Vertices, data.VertexSize, data.IndexCount, data.Indices, data.IndexSize))
    {
        reference->ref_count = 0;
        reference->geometry.ID = geometry_system_state->max_geometries_count;

        KERROR("[GeometrySystem::AcquireGeometryWithData]: Failed to create geometry!");
        return nullptr;
    }
#endif

    return &reference->geometry;
}

bool GeometrySystem::UpdateGeometry(Geometry* geometry, GeometryData data)
{
#ifdef KRAFT_GUI_APP
    if (!g_Renderer->UpdateGeometry(geometry, data.VertexCount, data.Vertices, data.VertexSize, data.IndexCount, data.Indices, data.IndexSize))
    {
        return false;
    }
#endif

    return true;
}

bool GeometrySystem::UpdateGeometry(u32 id, GeometryData data)
{
#ifdef KRAFT_GUI_APP
    if (id < geometry_system_state->max_geometries_count)
    {
        GeometryReference* reference = &geometry_system_state->geometries[id];
        if (!g_Renderer->UpdateGeometry(&reference->geometry, data.VertexCount, data.Vertices, data.VertexSize, data.IndexCount, data.Indices, data.IndexSize))
        {
            return false;
        }
    }
    else
    {
        return false;
    }
#endif

    return true;
}

void GeometrySystem::ReleaseGeometry(Geometry* geometry)
{
    ReleaseGeometry(geometry->ID);
}

void GeometrySystem::ReleaseGeometry(u32 id)
{
    if (id < geometry_system_state->max_geometries_count)
    {
        GeometryReference* ref = &geometry_system_state->geometries[id];
        if (ref->ref_count > 0)
        {
            ref->ref_count--;
        }

        if (ref->ref_count == 0 && ref->auto_release)
        {
            DestroyGeometry(&ref->geometry);
        }
    }
    else
    {
        KERROR("[GeometrySystem::ReleaseGeometry]: Invalid geometry id %d", id);
    }
}

void GeometrySystem::DestroyGeometry(Geometry* geometry)
{
    geometry->ID = geometry_system_state->max_geometries_count;
    geometry->DrawData = {};
}

void _createDefaultGeometries()
{
    GeometryReference* ref = &geometry_system_state->geometries[0];
    r::Vertex3D        vertices[] = {
        { Vec3f(+0.5f, +0.5f, +0.0f), { 1.f, 1.f }, { 0, 0, 0 }, { 1, 0, 0, 1 } },
        { Vec3f(+0.5f, -0.5f, +0.0f), { 1.f, 0.f }, { 0, 0, 0 }, { 1, 0, 0, 1 } },
        { Vec3f(-0.5f, -0.5f, +0.0f), { 0.f, 0.f }, { 0, 0, 0 }, { 1, 0, 0, 1 } },
        { Vec3f(-0.5f, +0.5f, +0.0f), { 0.f, 1.f }, { 0, 0, 0 }, { 1, 0, 0, 1 } },
    };

    u32 indices[] = { 0, 1, 2, 2, 3, 0 };

    ref->geometry.ID = 0;
    ref->auto_release = false;
    ref->ref_count = 1;

#ifdef KRAFT_GUI_APP
    if (!g_Renderer->CreateGeometry(&ref->geometry, 4, vertices, sizeof(r::Vertex3D), 6, indices, sizeof(indices[0])))
    {
        KFATAL("[GeometrySystem]: Failed to create default geometries");
        return;
    }
#endif

    // 2D Geometry
    // TODO(vertex-pulling): Disabled because Vertex2D (36 bytes) in the shared vertex SSBO
    // breaks the uniform Vertex3D (32 byte) stride assumption in shaders.
    // Re-enable once per-format vertex buffers or byte-offset addressing is implemented.
#if 0
    {
        GeometryReference* Ref = &geometry_system_state->geometries[1];
        r::Vertex2D        Vertices[] = {
            {
                Vec3f(+0.5f, +0.5f, +0.0f),
                { 1.f, 1.f },
            },
            {
                Vec3f(+0.5f, -0.5f, +0.0f),
                { 1.f, 0.f },
            },
            {
                Vec3f(-0.5f, -0.5f, +0.0f),
                { 0.f, 0.f },
            },
            {
                Vec3f(-0.5f, +0.5f, +0.0f),
                { 0.f, 1.f },
            },
        };

        u32 Indices[] = { 0, 1, 2, 2, 3, 0 };

        Ref->geometry.ID = 1;
        Ref->auto_release = false;
        Ref->ref_count = 1;

#ifdef KRAFT_GUI_APP
        if (!g_Renderer->CreateGeometry(&Ref->Geometry, 4, Vertices, sizeof(r::Vertex2D), 6, Indices, sizeof(Indices[0])))
        {
            KFATAL("[GeometrySystem]: Failed to create default geometries");
            return;
        }
#endif
    }
#endif
}

} // namespace kraft