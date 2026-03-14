#pragma once

#include "core/kraft_core.h"

namespace kraft {

struct GeometrySystemConfig
{
    u32 MaxGeometriesCount;
};

struct GeometryData
{
    u32   VertexCount;
    u32   IndexCount;
    u32   VertexSize; // Size of a single vertex
    u32   IndexSize;  // Size of a single index
    void* Vertices;
    void* Indices;
};

struct GeometryReference
{
    u32      ref_count;
    bool     auto_release;
    Geometry geometry;
};

struct GeometrySystemState
{
    u32                max_geometries_count;
    GeometryReference* geometries;
};

struct GeometrySystem
{
    static void Init(GeometrySystemConfig config);
    static void Shutdown();

    static Geometry* GetDefaultGeometry();
    static Geometry* GetDefault2DGeometry();
    static Geometry* AcquireGeometry(u32 ID);
    static Geometry* AcquireGeometryWithData(GeometryData data, bool auto_release = true);
    static bool      UpdateGeometry(Geometry* geometry, GeometryData data);
    static bool      UpdateGeometry(u32 id, GeometryData data);
    static void      ReleaseGeometry(Geometry* geometry);
    static void      ReleaseGeometry(u32 id);
    static void      DestroyGeometry(Geometry* geometry);
};

} // namespace kraft