#include "kraft_math.h" 

#include <math.h>

namespace kraft
{

float32 Sin(float32 x) 
{
    return sinf(x);
}

float32 Cos(float32 x) {
    return cosf(x);
}

float32 Tan(float32 x) {
    return tanf(x);
}

float32 Acos(float32 x) {
    return acosf(x);
}

float32 Sqrt(float32 x) {
    return sqrtf(x);
}

float32 Abs(float32 x) {
    return fabsf(x);
}

Vec4f RotationMatrix(Vec3f euler)
{
    Vec4f out(Identity);
    // for (int i = 0; i < n; i++)
    // {
    //     out[n][i] = position[i];
    // }
    
    return out;
}

Mat4f OrthographicMatrix(float32 left, float32 right, float32 top, float32 bottom, float32 nearClip, float32 farClip)
{
    Mat4f out(Identity);

    float32 leftRight = 1.0f / (right - left);
    float32 bottomTop = 1.0f / (bottom - top);
    float32 nearFar = 1.0f / (farClip - nearClip);

    out._data[0] = 2.0f * leftRight;
    out._data[5] = 2.0f * bottomTop;
    out._data[10] = 2.0f * nearFar;

    out._data[12] = -(left + right) * leftRight;
    out._data[13] = -(top + bottom) * bottomTop;
    out._data[14] = -(farClip + nearClip) * nearFar;

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


}