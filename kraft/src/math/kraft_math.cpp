#include "kraft_math.h" 

#include <cmath>

namespace kraft
{

const Vec2f Vec2fZero = Vec2f(0);
const Vec3f Vec3fZero = Vec3f(0);
const Vec4f Vec4fZero = Vec4f(0);
const Vec3f Vec3fOne = Vec3f(1);
const Vec4f Vec4fOne = Vec4f(1);

float32 Sin(float32 x) 
{
    return sinf(x);
}

float32 Cos(float32 x) 
{
    return cosf(x);
}

float32 Tan(float32 x) 
{
    return tanf(x);
}

float32 Acos(float32 x) 
{
    return acosf(x);
}

float32 Sqrt(float32 x) 
{
    return sqrtf(x);
}

float32 Abs(float32 x) 
{
    return fabsf(x);
}

Mat4f OrthographicMatrix(float32 left, float32 right, float32 bottom, float32 top, float32 zNear, float32 zFar)
{
    Mat4f out(Identity);

    float32 leftRight = 1.0f / (right - left);
    float32 bottomTop = 1.0f / (top - bottom);
    float32 nearFar = 1.0f / (zFar - zNear);

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

Mat4f PerspectiveMatrix(float32 fieldOfViewRadians, float32 aspectRatio, float32 nearClip, float32 farClip)
{
    Mat4f out(0.0f);

    float32 halfTanFov = Tan(fieldOfViewRadians * 0.5f);

    out._data[0] = 1.0f / (aspectRatio * halfTanFov);
    out._data[5] = 1.0f / halfTanFov;
    out._data[10] = -((farClip + nearClip) / (farClip - nearClip));
    out._data[11] = -1.0f;
    out._data[14] = -((2.0f * farClip * nearClip) / (farClip - nearClip));    

    return out;
}

Mat4f RotationMatrixX(float32 angleRadians)
{
    Mat4f out = Mat4f(Identity);
    float32 cosA = Cos(angleRadians);
    float32 sinA = Sin(angleRadians);

    // Mat4f out = {
    //     1,     0,     0, 0,
    //     0, +cosA, +sinA, 0,
    //     0, -sinA, +cosA, 0,
    //     0,     0,     0, 1
    // };

    out._data[5]  = +cosA;
    out._data[6]  = +sinA;
    out._data[9]  = -sinA;
    out._data[10] = +cosA;

    return out;
}

Mat4f RotationMatrixY(float32 angleRadians)
{
    Mat4f out = Mat4f(Identity);
    float32 cosA = Cos(angleRadians);
    float32 sinA = Sin(angleRadians);

    out._data[0]  = +cosA;
    out._data[2]  = -sinA;
    out._data[8]  = +sinA;
    out._data[10] = +cosA;
    
    return out;
}

Mat4f RotationMatrixZ(float32 angleRadians)
{
    Mat4f out = Mat4f(Identity);
    float32 cosA = Cos(angleRadians);
    float32 sinA = Sin(angleRadians);

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

Mat4f RotationMatrixFromEulerAngles(float32 rotationXRadians, float32 rotationYRadians, float32 rotationZRadians) 
{
    Mat4f rotationX = RotationMatrixX(rotationXRadians);
    Mat4f rotationY = RotationMatrixY(rotationYRadians);
    Mat4f rotationZ = RotationMatrixZ(rotationZRadians);
    Mat4f out = rotationX * rotationY * rotationZ;

    return out;
}


}