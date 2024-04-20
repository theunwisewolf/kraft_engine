#pragma once

#include "math/kraft_math.h"
#include "resources/kraft_resource_types.h"
#include "renderer/kraft_camera.h"

enum ProjectionType
{
    Perspective,
    Orthographic
};

struct SimpleObjectState
{
    kraft::Mat4f             ModelMatrix;
    kraft::Vec3f             Scale = kraft::Vec3fZero;
    kraft::Vec3f             Position = kraft::Vec3fZero;
    kraft::Vec3f             Rotation = kraft::Vec3fZero;

    kraft::Material*         Material = nullptr;
    kraft::Geometry*         Geometry = nullptr;
    bool                     Dirty;
};

struct SceneState
{
    ProjectionType           Projection;
    kraft::Mat4f             ProjectionMatrix;
    kraft::Mat4f             ViewMatrix;
    kraft::Camera            SceneCamera;
    SimpleObjectState        ObjectState;
};