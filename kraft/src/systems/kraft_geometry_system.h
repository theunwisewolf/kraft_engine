#pragma once

#include "core/kraft_core.h"
#include "systems/kraft_material_system.h"
#include "resources/kraft_resource_types.h"

#define KRAFT_GEOMETRY_NAME_MAX_LENGTH 256

namespace kraft
{

struct GeometrySystemConfig
{
    uint32 MaxGeometriesCount;
};

struct GeometryData
{
    uint32  VertexCount;
    uint32  IndexCount;
    uint32  VertexSize; // Size of a single vertex
    uint32  IndexSize; // Size of a single index
    void*   Vertices;
    void*   Indices;
    String  Name;
};

struct GeometrySystem
{
    static void Init(GeometrySystemConfig Config);
    static void Shutdown();

    static Geometry* GetDefaultGeometry();
    static Geometry* AcquireGeometry(uint32 ID);
    static Geometry* AcquireGeometryWithData(GeometryData Data, bool AutoRelease = true);
    static void ReleaseGeometry(Geometry* Geometry);
    static void ReleaseGeometry(uint32 ID);
    static void DestroyGeometry(Geometry* Geometry);
};

}