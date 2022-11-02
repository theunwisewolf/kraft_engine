#include "kraft_math.h" 
namespace kraft
{

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


}