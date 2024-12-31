#pragma once

#include "core/kraft_core.h"

namespace kraft {

struct Geometry;

struct GeometrySystemConfig
{
    uint32 MaxGeometriesCount;
};

struct GeometryData
{
    uint32 VertexCount;
    uint32 IndexCount;
    uint32 VertexSize; // Size of a single vertex
    uint32 IndexSize;  // Size of a single index
    void*  Vertices;
    void*  Indices;
};

struct GeometrySystem
{
    static void Init(GeometrySystemConfig Config);
    static void Shutdown();

    static Geometry* GetDefaultGeometry();
    static Geometry* GetDefault2DGeometry();
    static Geometry* AcquireGeometry(uint32 ID);
    static Geometry* AcquireGeometryWithData(GeometryData Data, bool AutoRelease = true);
    static void      ReleaseGeometry(Geometry* Geometry);
    static void      ReleaseGeometry(uint32 ID);
    static void      DestroyGeometry(Geometry* Geometry);
};

}