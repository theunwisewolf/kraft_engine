#include "kraft_geometry_system.h"

#include "renderer/kraft_renderer_frontend.h"
#include "systems/kraft_material_system.h"

namespace kraft
{

using namespace renderer;

struct GeometryReference
{
    uint32   ReferenceCount;
    bool     AutoRelease;
    Geometry Geometry;
};

struct GeometrySystemState
{
    uint32              MaxGeometriesCount;
    GeometryReference*  Geometries;
}; 

GeometrySystemState* State = nullptr;

void GeometrySystem::Init(GeometrySystemConfig Config)
{
    uint32 TotalSize = sizeof(GeometrySystemState) + sizeof(GeometryReference) * Config.MaxGeometriesCount;

    State = (GeometrySystemState*)kraft::Malloc(TotalSize, MEMORY_TAG_GEOMETRY_SYSTEM, true);
    State->MaxGeometriesCount = Config.MaxGeometriesCount;
    State->Geometries = (GeometryReference*)(State + sizeof(GeometrySystemState));

    // _createDefaultGeometries();
}

void GeometrySystem::Shutdown()
{
    for (int i = 0; i < State->MaxGeometriesCount; ++i)
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

}

Geometry* GeometrySystem::AcquireGeometry(uint32 ID)
{
    int Index = -1;
    for (int i = 0; i < State->MaxGeometriesCount; ++i)
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
    for (int i = 0; i < State->MaxGeometriesCount; ++i)
    {
        GeometryReference* Reference = &State->Geometries[i];
        if (Reference->ReferenceCount == 0)
        {
            Index = i;
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
    
    if (!Renderer->CreateGeometry(&Reference->Geometry, Data.VertexCount, Data.Vertices, Data.IndexCount, Data.Indices))
    {
        Reference->ReferenceCount = 0;
        Reference->Geometry.ID = State->MaxGeometriesCount;
        Reference->Geometry.InternalID = State->MaxGeometriesCount;

        KERROR("[GeometrySystem::AcquireGeometryWithData]: Failed to create geometry!");
        return nullptr;
    }

    if (!StringEqual(Data.MaterialName, ""))
    {
        Reference->Geometry.Material = MaterialSystem::AcquireMaterial(Data.MaterialName);
        if (Reference->Geometry.Material == nullptr)
        {
            KWARN("[GeometrySystem::AcquireGeometryWithData]: Failed to acquire material");
            Reference->Geometry.Material = MaterialSystem::GetDefaultMaterial();
        }
    }
    
    return &Reference->Geometry;
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
}

}