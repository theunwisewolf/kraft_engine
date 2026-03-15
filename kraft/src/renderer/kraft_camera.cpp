#include "kraft_camera.h"

namespace kraft {

Camera::Camera() : Pitch(0.0f), Yaw(-90.0f), Roll(0.0f), ViewMatrix(Mat4f(Identity)), Up(Vec3f(0.0f, 1.0f, 0.0f))
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

void Camera::SetOrthographicProjection(u32 width, u32 height, f32 near_clip, f32 far_clip)
{
    this->ProjectionType = CameraProjectionType::Orthographic;
    this->ProjectionMatrix = kraft::OrthographicMatrix(-f32(width) * 0.5f, f32(width) * 0.5f, -f32(height) * 0.5f, f32(height) * 0.5f, near_clip, far_clip);
}

void Camera::SetPerspectiveProjection(f32 fov_radians, u32 width, u32 height, f32 near_clip, f32 far_clip)
{
    this->SetPerspectiveProjection(fov_radians, f32(width) / f32(height), near_clip, far_clip);
}

void Camera::SetPerspectiveProjection(f32 fov_radians, f32 aspect_ratio, f32 near_clip, f32 far_clip)
{
    this->ProjectionType = CameraProjectionType::Perspective;
    this->ProjectionMatrix = kraft::PerspectiveMatrix(fov_radians, aspect_ratio, near_clip, far_clip);
}

} // namespace kraft