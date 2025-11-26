#include "kraft_geometry_system.h"

#include <containers/kraft_array.h>
#include <containers/kraft_hashmap.h>
#include <core/kraft_string.h>
#include <renderer/kraft_renderer_frontend.h>
#include <systems/kraft_material_system.h>

// TODO: REMOVE
#include <core/kraft_base_includes.h>

#include <renderer/kraft_renderer_types.h>
#include <resources/kraft_resource_types.h>

namespace kraft {

struct GeometryReference
{
    u32      ReferenceCount;
    bool     AutoRelease;
    Geometry Geometry;
};

struct GeometrySystemState
{
    u32                MaxGeometriesCount;
    GeometryReference* Geometries;
};

static GeometrySystemState* State = nullptr;

static void _createDefaultGeometries();

void GeometrySystem::Init(GeometrySystemConfig Config)
{
    u32 TotalSize = sizeof(GeometrySystemState) + sizeof(GeometryReference) * Config.MaxGeometriesCount;

    u8* Memory = (u8*)Malloc(TotalSize, MEMORY_TAG_GEOMETRY_SYSTEM, true);
    State = (GeometrySystemState*)Memory;
    State->MaxGeometriesCount = Config.MaxGeometriesCount;
    State->Geometries = (GeometryReference*)(Memory + sizeof(GeometrySystemState));

    _createDefaultGeometries();
}

void GeometrySystem::Shutdown()
{
    for (u32 i = 0; i < State->MaxGeometriesCount; ++i)
    {
        GeometryReference* Reference = &State->Geometries[i];
        if (Reference->ReferenceCount > 0)
        {
            GeometrySystem::DestroyGeometry(&Reference->Geometry);
        }
    }

    uint32 TotalSize = sizeof(GeometrySystemState) + sizeof(GeometryReference) * State->MaxGeometriesCount;
    kraft::Free(State, TotalSize, MEMORY_TAG_GEOMETRY_SYSTEM);
}

Geometry* GeometrySystem::GetDefaultGeometry()
{
    return &State->Geometries[0].Geometry;
}

Geometry* GeometrySystem::GetDefault2DGeometry()
{
    return &State->Geometries[1].Geometry;
}

Geometry* GeometrySystem::AcquireGeometry(u32 id)
{
    for (u32 i = 0; i < State->MaxGeometriesCount; ++i)
    {
        GeometryReference* Reference = &State->Geometries[i];
        if (Reference->Geometry.ID == id)
        {
            return &Reference->Geometry;
        }
    }

    KERROR("[GeometrySystem::AcquireGeometry]: Geometry with id %d not found", id);

    return nullptr;
}

Geometry* GeometrySystem::AcquireGeometryWithData(GeometryData Data, bool AutoRelease)
{
    int index = -1;
    for (u32 i = 0; i < State->MaxGeometriesCount; ++i)
    {
        GeometryReference* Reference = &State->Geometries[i];
        if (Reference->ReferenceCount == 0)
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

    GeometryReference* Reference = &State->Geometries[index];
    Reference->ReferenceCount = 1;
    Reference->AutoRelease = AutoRelease;
    Reference->Geometry.ID = index;

#ifdef KRAFT_GUI_APP
    if (!g_Renderer->CreateGeometry(&Reference->Geometry, Data.VertexCount, Data.Vertices, Data.VertexSize, Data.IndexCount, Data.Indices, Data.IndexSize))
    {
        Reference->ReferenceCount = 0;
        Reference->Geometry.ID = State->MaxGeometriesCount;
        Reference->Geometry.InternalID = State->MaxGeometriesCount;

        KERROR("[GeometrySystem::AcquireGeometryWithData]: Failed to create geometry!");
        return nullptr;
    }
#endif

    return &Reference->Geometry;
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
    if (id >= 0 && id < State->MaxGeometriesCount)
    {
        GeometryReference* reference = &State->Geometries[id];
        if (!g_Renderer->UpdateGeometry(&reference->Geometry, data.VertexCount, data.Vertices, data.VertexSize, data.IndexCount, data.Indices, data.IndexSize))
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

void GeometrySystem::ReleaseGeometry(Geometry* Geometry)
{
    u32 ID = Geometry->ID;
    ReleaseGeometry(ID);
}

void GeometrySystem::ReleaseGeometry(u32 ID)
{
    if (ID >= 0 && ID < State->MaxGeometriesCount)
    {
        GeometryReference* Reference = &State->Geometries[ID];
        if (Reference->ReferenceCount > 0)
        {
            Reference->ReferenceCount--;
        }

        if (Reference->ReferenceCount == 0 && Reference->AutoRelease)
        {
            DestroyGeometry(&Reference->Geometry);
        }
    }
    else
    {
        KERROR("[GeometrySystem::ReleaseGeometry]: Invalid geometry id %d", ID);
    }
}

void GeometrySystem::DestroyGeometry(Geometry* Geometry)
{
#ifdef KRAFT_GUI_APP
    g_Renderer->DestroyGeometry(Geometry);
#endif
    Geometry->ID = State->MaxGeometriesCount;
    Geometry->InternalID = State->MaxGeometriesCount;
}

void _createDefaultGeometries()
{
    GeometryReference* Ref = &State->Geometries[0];
    r::Vertex3D        Vertices[] = {
        { Vec3f(+0.5f, +0.5f, +0.0f), { 1.f, 1.f }, { 0, 0, 0 } },
        { Vec3f(+0.5f, -0.5f, +0.0f), { 1.f, 0.f }, { 0, 0, 0 } },
        { Vec3f(-0.5f, -0.5f, +0.0f), { 0.f, 0.f }, { 0, 0, 0 } },
        { Vec3f(-0.5f, +0.5f, +0.0f), { 0.f, 1.f }, { 0, 0, 0 } },
    };

    u32 Indices[] = { 0, 1, 2, 2, 3, 0 };

    Ref->Geometry.ID = 0;
    Ref->AutoRelease = false;
    Ref->ReferenceCount = 1;

#ifdef KRAFT_GUI_APP
    if (!g_Renderer->CreateGeometry(&Ref->Geometry, 4, Vertices, sizeof(r::Vertex3D), 6, Indices, sizeof(Indices[0])))
    {
        KFATAL("[GeometrySystem]: Failed to create default geometries");
        return;
    }
#endif

    // 2D Geometry
    {
        GeometryReference* Ref = &State->Geometries[1];
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

        uint32 Indices[] = { 0, 1, 2, 2, 3, 0 };

        Ref->Geometry.ID = 1;
        Ref->AutoRelease = false;
        Ref->ReferenceCount = 1;

#ifdef KRAFT_GUI_APP
        if (!g_Renderer->CreateGeometry(&Ref->Geometry, 4, Vertices, sizeof(r::Vertex2D), 6, Indices, sizeof(Indices[0])))
        {
            KFATAL("[GeometrySystem]: Failed to create default geometries");
            return;
        }
#endif
    }
}

} // namespace kraft