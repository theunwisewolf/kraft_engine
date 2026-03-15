#pragma once

#include "core/kraft_core.h"
#include "core/kraft_math.h"

namespace kraft {

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
            f32 Pitch;
            f32 Yaw;
            f32 Roll;
        };

        Vec3f Euler;
    };

    CameraProjectionType ProjectionType;
    Mat4f                ProjectionMatrix; // Computed matrix
    Mat4f                ViewMatrix;       // Computed matrix
    bool                 Dirty = true;
    f32              FOVRadians = kraft::Radians(45.0f);

    // The target the camera is supposed to look at
    Vec3f Target;

    // The direction of the camera to the target
    Vec3f Direction;

    // Direction vectors
    Vec3f Up;
    Vec3f Right;
    Vec3f Front;

    Camera();

    void Reset();
    // void SetPosition(Vec3f position);
    // void SetRotation(Vec3f rotation);
    void SetOrthographicProjection(u32 Width, u32 Height, f32 NearClip, f32 FarClip);
    void SetPerspectiveProjection(f32 FOVRadians, u32 Width, u32 Height, f32 NearClip, f32 FarClip);
    void SetPerspectiveProjection(f32 FOVRadians, f32 AspectRatio, f32 NearClip, f32 FarClip);

    KRAFT_INLINE Mat4f GetViewMatrix()
    {
        if (this->ProjectionType == CameraProjectionType::Orthographic)
        {
            // this->ViewMatrix = Mat4f(Identity);
            // this->ViewMatrix = kraft::LookAt(this->Position, this->Position + this->Front, this->Up);
            this->ViewMatrix = TranslationMatrix(this->Position);
        }
        else
        {
            this->ViewMatrix = LookAt(this->Position, this->Position + this->Front, this->Up);
        }

        return this->ViewMatrix;
        // UpdateViewMatrix();
        // return this->ViewMatrix;
    }

    void UpdateVectors()
    {
        Vec3f front;
        front.x = Cos(Radians(Yaw)) * Cos(Radians(Pitch));
        front.y = Sin(Radians(Pitch));
        front.z = Sin(Radians(Yaw)) * Cos(Radians(Pitch));
        this->Front = Normalize(front);
        this->Right = Normalize(Cross(this->Front, { 0.0f, 1.0f, 0.0f }));
        this->Up = Normalize(Cross(this->Right, this->Front));
    }
};

} // namespace kraft