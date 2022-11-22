#pragma once

#include "core/kraft_core.h"
#include "math/kraft_math.h"

namespace kraft
{

struct Camera
{
    Vec3f   Position;

    union
    {
        struct
        {
            float32 Pitch;
            float32 Yaw;
            float32 Roll;
        };

        Vec3f Euler;
    };
    
    Mat4f    ViewMatrix; // Computed matrix
    bool     Dirty = true;

    Camera();

    void Reset();
    void SetPosition(Vec3f position);
    void SetRotation(Vec3f rotation);
    void SetPitch(float32 pitch);
    void SetYaw(float32 yaw);
    void SetRoll(float32 roll);

    Mat4f& GetViewMatrix();
};



}