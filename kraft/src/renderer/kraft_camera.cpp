#include "kraft_camera.h"

namespace kraft
{

Camera::Camera() :
    Up(Vec3f(0.0f, 1.0f, 0.0f)),
    Yaw(-90.0f),
    Pitch(0.0f),
    Roll(0.0f),
    ViewMatrix(Mat4f(Identity))
{
    // Reset();
}

void Camera::Reset()
{
    // this->Dirty = false;
    // this->Position = Vec3fZero;
    // this->Pitch = this->Yaw = this->Roll = 0.f;
}

// void Camera::SetPosition(Vec3f position)
// {
//     this->Dirty = true;
//     this->Position = position;
// }

// void Camera::SetRotation(Vec3f rotation)
// {
//     this->Dirty = true;
//     this->Pitch = rotation.x;
//     this->Yaw = rotation.y;
//     this->Roll = rotation.z;
// }

void Camera::SetOrthographicProjection(uint32 Width, uint32 Height, float32 NearClip, float32 FarClip)
{
    this->ProjectionType = CameraProjectionType::Orthographic;
    this->ProjectionMatrix = kraft::OrthographicMatrix(-float32(Width) * 0.5f, float32(Width) * 0.5f, -float32(Height) * 0.5f, float32(Height) * 0.5f, NearClip, FarClip);
}

void Camera::SetPerspectiveProjection(float32 FOVRadians, uint32 Width, uint32 Height, float32 NearClip, float32 FarClip)
{
    this->SetPerspectiveProjection(FOVRadians, float32(Width) / float32(Height), NearClip, FarClip);
}

void Camera::SetPerspectiveProjection(float32 FOVRadians, float32 AspectRatio, float32 NearClip, float32 FarClip)
{
    this->ProjectionType = CameraProjectionType::Perspective;
    this->ProjectionMatrix = kraft::PerspectiveMatrix(FOVRadians, AspectRatio, NearClip, FarClip);
}

}