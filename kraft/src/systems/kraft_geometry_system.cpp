#include "kraft_geometry_system.h"

#include <renderer/kraft_renderer_frontend.h>
#include <systems/kraft_material_system.h>
#include <core/kraft_memory.h>
#include <core/kraft_log.h>
#include <core/kraft_string.h>
#include <containers/kraft_array.h>
#include <containers/kraft_hashmap.h>

#include <renderer/kraft_renderer_types.h>
#include <resources/kraft_resource_types.h>

namespace kraft {

using namespace renderer;

struct GeometryReference
{
    uint32   ReferenceCount;
    bool     AutoRelease;
    Geometry Geometry;
};

struct GeometrySystemState
{
    uint32             MaxGeometriesCount;
    GeometryReference* Geometries;
};

static GeometrySystemState* State = nullptr;

static void _createDefaultGeometries();

void GeometrySystem::Init(GeometrySystemConfig Config)
{
    uint32 TotalSize = sizeof(GeometrySystemState) + sizeof(GeometryReference) * Config.MaxGeometriesCount;

    State = (GeometrySystemState*)kraft::Malloc(TotalSize, MEMORY_TAG_GEOMETRY_SYSTEM, true);
    State->MaxGeometriesCount = Config.MaxGeometriesCount;
    State->Geometries = (GeometryReference*)(State + sizeof(GeometrySystemState));

    _createDefaultGeometries();
}

void GeometrySystem::Shutdown()
{
    for (uint32 i = 0; i < State->MaxGeometriesCount; ++i)
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

Geometry* GeometrySystem::AcquireGeometry(uint32 ID)
{
    for (uint32 i = 0; i < State->MaxGeometriesCount; ++i)
    {
        GeometryReference* Reference = &State->Geometries[i];
        if (Reference->Geometry.ID == ID)
        {
            return &Reference->Geometry;
        }
    }

    KERROR("[GeometrySystem::AcquireGeometry]: Geometry with id %d not found", ID);

    return nullptr;
}

Geometry* GeometrySystem::AcquireGeometryWithData(GeometryData Data, bool AutoRelease)
{
    int Index = -1;
    for (uint32 i = 0; i < State->MaxGeometriesCount; ++i)
    {
        GeometryReference* Reference = &State->Geometries[i];
        if (Reference->ReferenceCount == 0)
        {
            Index = (int)i;
            break;
        }
    }

    if (Index == -1)
    {
        KERROR("[GeometrySystem::AcquireGeometryWithData]: No free slots available; Out of memory!");
        return nullptr;
    }

    GeometryReference* Reference = &State->Geometries[Index];
    Reference->ReferenceCount = 1;
    Reference->AutoRelease = AutoRelease;
    Reference->Geometry.ID = Index;

    if (!Renderer->CreateGeometry(
            &Reference->Geometry, Data.VertexCount, Data.Vertices, Data.VertexSize, Data.IndexCount, Data.Indices, Data.IndexSize
        ))
    {
        Reference->ReferenceCount = 0;
        Reference->Geometry.ID = State->MaxGeometriesCount;
        Reference->Geometry.InternalID = State->MaxGeometriesCount;

        KERROR("[GeometrySystem::AcquireGeometryWithData]: Failed to create geometry!");
        return nullptr;
    }

    StringCopy(Reference->Geometry.Name, Data.Name);

    return &Reference->Geometry;
}

void GeometrySystem::ReleaseGeometry(Geometry* Geometry)
{
    uint32 ID = Geometry->ID;
    ReleaseGeometry(ID);
}

void GeometrySystem::ReleaseGeometry(uint32 ID)
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
    Renderer->DestroyGeometry(Geometry);
    Geometry->ID = State->MaxGeometriesCount;
    Geometry->InternalID = State->MaxGeometriesCount;
    StringCopy(Geometry->Name, "");
}

void _createDefaultGeometries()
{
    GeometryReference* Ref = &State->Geometries[0];
    Vertex3D           Vertices[] = {
        { Vec3f(+0.5f, +0.5f, +0.0f), { 1.f, 1.f }, { 0, 0, 0 } },
        { Vec3f(+0.5f, -0.5f, +0.0f), { 1.f, 0.f }, { 0, 0, 0 } },
        { Vec3f(-0.5f, -0.5f, +0.0f), { 0.f, 0.f }, { 0, 0, 0 } },
        { Vec3f(-0.5f, +0.5f, +0.0f), { 0.f, 1.f }, { 0, 0, 0 } },
    };

    uint32 Indices[] = { 0, 1, 2, 2, 3, 0 };

    Ref->Geometry.ID = 0;
    Ref->AutoRelease = false;
    Ref->ReferenceCount = 1;
    StringCopy(Ref->Geometry.Name, "default-geometry");
    if (!Renderer->CreateGeometry(&Ref->Geometry, 4, Vertices, sizeof(Vertex3D), 6, Indices, sizeof(Indices[0])))
    {
        KFATAL("[GeometrySystem]: Failed to create default geometries");
        return;
    }
}

}