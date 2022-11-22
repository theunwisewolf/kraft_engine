#include "kraft_camera.h"

namespace kraft
{

Camera::Camera()
{
    Reset();
}

void Camera::Reset()
{
    this->Dirty = false;
    this->Position = Vec3fZero;
    this->Pitch = this->Yaw = this->Roll = 0.f;
    this->ViewMatrix = Mat4f(Identity);
}

void Camera::SetPosition(Vec3f position)
{
    this->Dirty = true;
    this->Position = position;
}

void Camera::SetRotation(Vec3f rotation)
{
    this->Dirty = true;
    this->Pitch = rotation.x;
    this->Yaw = rotation.y;
    this->Roll = rotation.z;
}

void Camera::SetPitch(float32 pitch)
{
    this->Dirty = true;
    this->Pitch = pitch;
}

void Camera::SetYaw(float32 yaw)
{
    this->Dirty = true;
    this->Yaw = yaw;
}

void Camera::SetRoll(float32 roll)
{
    this->Dirty = true;
    this->Roll = roll;
}

Mat4f& Camera::GetViewMatrix()
{
    if (this->Dirty)
    {
        this->Dirty = false;
        
        Mat4f rotation = RotationMatrixFromEulerAngles(this->Pitch, this->Yaw, this->Roll);
        Mat4f translation = TranslationMatrix(this->Position);
        this->ViewMatrix = rotation * translation;
        this->ViewMatrix = Inverse(this->ViewMatrix);
    }

    return this->ViewMatrix;
}

}