#include "kraft_math.h"

#include <cmath>

namespace kraft {

const Vec2f Vec2fZero = Vec2f(0);
const Vec3f Vec3fZero = Vec3f(0);
const Vec4f Vec4fZero = Vec4f(0);
const Vec2f Vec2fOne = Vec2f(1);
const Vec3f Vec3fOne = Vec3f(1);
const Vec4f Vec4fOne = Vec4f(1);

f32 Sin(f32 x)
{
    return sinf(x);
}

f32 Cos(f32 x)
{
    return cosf(x);
}

f32 Tan(f32 x)
{
    return tanf(x);
}

f32 Acos(f32 x)
{
    return acosf(x);
}

f32 Sqrt(f32 x)
{
    return sqrtf(x);
}

f32 Abs(f32 x)
{
    return fabsf(x);
}

Mat4f OrthographicMatrix(f32 left, f32 right, f32 bottom, f32 top, f32 zNear, f32 zFar)
{
    Mat4f out(Identity);

    f32 leftRight = 1.0f / (right - left);
    f32 bottomTop = 1.0f / (top - bottom);
    f32 nearFar = 1.0f / (zFar - zNear);

    // We have specified minDepth & maxDepth in vkViewport to be [0,1]
    // The commented part will work if we would've used [-1,1]
    out._data[0] = 2.0f * leftRight;
    out._data[5] = 2.0f * bottomTop;
    // out._data[10] = 2.0f * nearFar;
    out._data[10] = 1.0f * nearFar;

    out._data[12] = -(left + right) * leftRight;
    out._data[13] = -(top + bottom) * bottomTop;
    // out._data[14] = -(farClip + nearClip) * nearFar;
    out._data[14] = -zNear / (zFar - zNear);

    return out;
}

Mat4f PerspectiveMatrix(f32 fieldOfViewRadians, f32 aspectRatio, f32 nearClip, f32 farClip)
{
    Mat4f out(0.0f);

    f32 halfTanFov = Tan(fieldOfViewRadians * 0.5f);

    out._data[0] = 1.0f / (aspectRatio * halfTanFov);
    out._data[5] = 1.0f / halfTanFov;
    out._data[10] = -((farClip + nearClip) / (farClip - nearClip));
    out._data[11] = -1.0f;
    out._data[14] = -((2.0f * farClip * nearClip) / (farClip - nearClip));

    return out;
}

Mat4f RotationMatrixX(f32 angleRadians)
{
    Mat4f out = Mat4f(Identity);
    f32   cosA = Cos(angleRadians);
    f32   sinA = Sin(angleRadians);

    // Mat4f out = {
    //     1,     0,     0, 0,
    //     0, +cosA, +sinA, 0,
    //     0, -sinA, +cosA, 0,
    //     0,     0,     0, 1
    // };

    out._data[5] = +cosA;
    out._data[6] = +sinA;
    out._data[9] = -sinA;
    out._data[10] = +cosA;

    return out;
}

Mat4f RotationMatrixY(f32 angleRadians)
{
    Mat4f out = Mat4f(Identity);
    f32   cosA = Cos(angleRadians);
    f32   sinA = Sin(angleRadians);

    out._data[0] = +cosA;
    out._data[2] = -sinA;
    out._data[8] = +sinA;
    out._data[10] = +cosA;

    return out;
}

Mat4f RotationMatrixZ(f32 angleRadians)
{
    Mat4f out = Mat4f(Identity);
    f32   cosA = Cos(angleRadians);
    f32   sinA = Sin(angleRadians);

    out._data[0] = +cosA;
    out._data[1] = +sinA;
    out._data[4] = -sinA;
    out._data[5] = +cosA;

    return out;
}

Mat4f RotationMatrixFromEulerAngles(Vec3f euler)
{
    return RotationMatrixFromEulerAngles(euler.x, euler.y, euler.z);
}

Mat4f RotationMatrixFromEulerAngles(f32 rotationXRadians, f32 rotationYRadians, f32 rotationZRadians)
{
    Mat4f rotationX = RotationMatrixX(rotationXRadians);
    Mat4f rotationY = RotationMatrixY(rotationYRadians);
    Mat4f rotationZ = RotationMatrixZ(rotationZRadians);
    Mat4f out = rotationX * rotationY * rotationZ;

    return out;
}

Mat4f LookAt(Vec3f Eye, Vec3f Center, Vec3f _Up)
{
    const Vec3f Front(Normalize(Center - Eye));
    const Vec3f Right(Normalize(Cross(Front, _Up)));
    const Vec3f Up(Cross(Right, Front));

    Mat4f Result(Identity);
    Result[0][0] = Right.x;
    Result[1][0] = Right.y;
    Result[2][0] = Right.z;
    Result[0][1] = Up.x;
    Result[1][1] = Up.y;
    Result[2][1] = Up.z;
    Result[0][2] = -Front.x;
    Result[1][2] = -Front.y;
    Result[2][2] = -Front.z;
    Result[3][0] = -Dot(Right, Eye);
    Result[3][1] = -Dot(Up, Eye);
    Result[3][2] = Dot(Front, Eye);

    return Result;
}

} // namespace kraft