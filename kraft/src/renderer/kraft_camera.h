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
    float32              FOVRadians = 45.0f;

    Camera();

    void Reset();
    void SetPosition(Vec3f position);
    void SetRotation(Vec3f rotation);
    void SetPitch(float32 pitch);
    void SetYaw(float32 yaw);
    void SetRoll(float32 roll);
    void SetOrthographicProjection(uint32 Width, uint32 Height, float32 NearClip, float32 FarClip);
    void SetPerspectiveProjection(float32 FOVRadians, uint32 Width, uint32 Height, float32 NearClip, float32 FarClip);
    void SetPerspectiveProjection(float32 FOVRadians, float32 AspectRatio, float32 NearClip, float32 FarClip);

    KRAFT_INLINE Mat4f& GetViewMatrix()
    {
        UpdateViewMatrix();
        return this->ViewMatrix;
    }

    void UpdateViewMatrix();
};

}