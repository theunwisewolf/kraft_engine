#pragma once

#include "math/kraft_math.h"

namespace kraft {

struct TransformComponent
{
    Vec3f Position = kraft::Vec3fZero;
    Vec3f Rotation = kraft::Vec3fZero;
    Vec3f Scale = kraft::Vec3fOne;

    TransformComponent() = default;
    TransformComponent(Vec3f Position, Vec3f Rotation, Vec3f Scale) : Position(Position), Rotation(Rotation), Scale(Scale)
    {}
};

}