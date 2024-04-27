#pragma once

#include "math/kraft_math.h"
#include "resources/kraft_resource_types.h"
#include "renderer/kraft_camera.h"

struct SimpleObjectState : public kraft::Renderable
{
    kraft::Vec3f             Position = kraft::Vec3fZero;
    kraft::Vec3f             Rotation = kraft::Vec3fZero;
    kraft::Vec3f             Scale = kraft::Vec3fZero;

    void SetTransform(kraft::Vec3f Position, kraft::Vec3f Rotation, kraft::Vec3f Scale)
    {
        this->Position = Position;
        this->Rotation = Rotation;
        this->Scale = Scale;

        this->ModelMatrix = kraft::ScaleMatrix(Scale) * kraft::RotationMatrixFromEulerAngles(Rotation) * kraft::TranslationMatrix(Position);
    }
};

struct SceneState
{
    kraft::Camera            SceneCamera;
    SimpleObjectState        ObjectStates[1024];
    uint32                   SelectedObjectIndex;
    uint32                   ObjectsCount;

    KRAFT_INLINE SimpleObjectState& GetSelectedEntity() { return ObjectStates[SelectedObjectIndex]; }

    SceneState()
    {
        SelectedObjectIndex = 0;
        ObjectsCount = 0;
    }

    void AddEntity(SimpleObjectState Entity)
    {
        ObjectStates[ObjectsCount++] = Entity;
    }
};