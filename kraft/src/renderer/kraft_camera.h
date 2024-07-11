#pragma once

#include "core/kraft_core.h"
#include "math/kraft_math.h"

namespace kraft
{

enum CameraProjectionType
{
    Perspective,
    Orthographic
};

struct Camera
{
    Vec3f Position;

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
    
    CameraProjectionType ProjectionType;
    Mat4f                ProjectionMatrix; // Computed matrix
    Mat4f                ViewMatrix; // Computed matrix
    bool                 Dirty = true;
    float32              FOVRadians = kraft::Radians(45.0f);

    // The target the camera is supposed to look at
    Vec3f                Target;

    // The direction of the camera to the target
    Vec3f                Direction;

    // Direction vectors
    Vec3f                Up;
    Vec3f                Right;
    Vec3f                Front;

    Camera();

    void Reset();
    // void SetPosition(Vec3f position);
    // void SetRotation(Vec3f rotation);
    void SetOrthographicProjection(uint32 Width, uint32 Height, float32 NearClip, float32 FarClip);
    void SetPerspectiveProjection(float32 FOVRadians, uint32 Width, uint32 Height, float32 NearClip, float32 FarClip);
    void SetPerspectiveProjection(float32 FOVRadians, float32 AspectRatio, float32 NearClip, float32 FarClip);

    KRAFT_INLINE Mat4f GetViewMatrix()
    {
        this->ViewMatrix = kraft::LookAt(this->Position, this->Position + this->Front, this->Up);
        return this->ViewMatrix;
        // UpdateViewMatrix();
        // return this->ViewMatrix;
    }

    KRAFT_INLINE void UpdateVectors()
    {
        Vec3f _Front;
        _Front.x = Cos(Radians(Yaw)) * Cos(Radians(Pitch));
        _Front.y = Sin(Radians(Pitch));
        _Front.z = Sin(Radians(Yaw)) * Cos(Radians(Pitch));
        this->Front = Normalize(_Front);
        this->Right = Normalize(Cross(this->Front, {0.0f, 1.0f, 0.0f}));
        this->Up = Normalize(Cross(this->Right, this->Front));
    }
};

}