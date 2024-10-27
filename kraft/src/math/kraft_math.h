#pragma once

#include "core/kraft_core.h"

// 
// Useful defines
//

#define KRAFT_PI 3.14159265358979323846f
#define KRAFT_DEG_TO_RADIANS(degrees) (degrees * (KRAFT_PI / 180.0f))

namespace kraft
{

namespace math
{

template <typename T>
KRAFT_API KRAFT_INLINE T Min(T a, T b)
{
    return a > b ? b : a;
}

template <typename T>
KRAFT_API KRAFT_INLINE T Max(T a, T b)
{
    return a > b ? a : b;
}

KRAFT_INLINE uint64 AlignUp(uint64 UnalignedValue, uint64 Alignment) 
{
    return ((UnalignedValue + (Alignment - 1)) & ~(Alignment - 1));
}

template<typename T>
KRAFT_INLINE T Clamp(T Value, T Min, T Max)
{
    return Value < Min ? Min : Value > Max ? Max : Value;
}

}

//
// Useful functions
//

KRAFT_API float32 Sin(float32 x);
KRAFT_API float32 Cos(float32 x);
KRAFT_API float32 Tan(float32 x);
KRAFT_API float32 Acos(float32 x);
KRAFT_API float32 Sqrt(float32 x);
KRAFT_API float32 Abs(float32 x);

KRAFT_INLINE float32 DegToRadians(float32 degrees) 
{
    return degrees * (KRAFT_PI / 180.0f);
}

KRAFT_INLINE float32 RadiansToDegrees(float32 radians) 
{
    return (radians * 180.0f) / KRAFT_PI;
}

// Converts the given angle in degrees to radians
KRAFT_INLINE float32 Radians(float32 Degrees) 
{
    return DegToRadians(Degrees);
}

// Converts the given angle in radians to degrees
KRAFT_INLINE float32 Degrees(float32 Radians) 
{
    return RadiansToDegrees(Radians);
}

}

/**
 *
 * Vectors
 *
 */

#include <stdio.h>
#include <assert.h>
#include <cmath>
#include <initializer_list>

#define KRGB(r, g, b) Vec3f(r / 255.f, g / 255.f, b / 255.f)
#define KRGBA(r, g, b, a) Vec4f(r / 255.f, g / 255.f, b / 255.f, a / 255.f)

#define COMMON_VECTOR_MEMBERS(n)                                       \
    Vector() {}                                                        \
    Vector(std::initializer_list<T> list)                              \
    {                                                                  \
        int count = (int)list.size() < n ? (int)list.size() : n;       \
        auto it = list.begin();                                        \
        for (int i = 0; i < count; ++i, ++it)                          \
            _data[i] = *it;                                            \
                                                                       \
        for (int i = count; i < n; i++)                                \
            _data[i] = 0;                                              \
    }                                                                  \
                                                                       \
    T &operator[](int i) { return _data[i]; }                          \
    const T &operator[](int i) const { return _data[i]; }              \
    /* Initialize the vector with a common value for all components */ \
    explicit Vector(T value)                                           \
    {                                                                  \
        for (int i = 0; i < n; ++i)                                    \
            _data[i] = value;                                          \
    }                                                                  \
                                                                       \
    /* C array conversions */                                          \
    typedef T(&array_t)[n];                                            \
    operator array_t () { return _data; }                              \
    typedef const T(&const_array_t)[n];                                \
    operator const_array_t () const { return _data; }                  \
    private: operator bool();                                          \
    public:                                                            \

#define UNARY_OPERATOR(op)                    \
    template <typename T, int n>              \
    Vector<T, n> operator op(Vector<T, n> in) \
    {                                         \
        Vector<T, n> out;                     \
        for (int i = 0; i < n; i++)           \
            out[i] = op in[i];                \
        return out;                           \
    }

#define BINARY_OPERATOR(op)                                  \
    template <typename T, int n>                             \
    Vector<T, n> operator op(Vector<T, n> a, Vector<T, n> b) \
    {                                                        \
        Vector<T, n> out;                                    \
        for (int i = 0; i < n; ++i)                          \
            out[i] = a[i] op b[i];                           \
        return out;                                          \
    }                                                        \
                                                             \
    /* Scalar addition; vec2 + a */                          \
    template <typename T, int n>                             \
    Vector<T, n> operator op(Vector<T, n> a, T b)            \
    {                                                        \
        Vector<T, n> out;                                    \
        for (int i = 0; i < n; ++i)                          \
            out[i] = a[i] op b;                              \
        return out;                                          \
    }                                                        \
                                                             \
    /* Scalar addition; a + vec2 */                          \
    template <typename T, int n>                             \
    Vector<T, n> operator op(T a, Vector<T, n> b)            \
    {                                                        \
        Vector<T, n> out;                                    \
        for (int i = 0; i < n; ++i)                          \
            out[i] = a op b[i];                              \
        return out;                                          \
    }

#define BINARY_INPLACE_OPERATOR(op)                            \
    template <typename T, int n>                               \
    Vector<T, n> &operator op(Vector<T, n> &a, Vector<T, n> b) \
    {                                                          \
        for (int i = 0; i < n; i++)                            \
            a[i] op b[i];                                      \
        return a;                                              \
    }                                                          \
                                                               \
    template <typename T, int n>                               \
    Vector<T, n> &operator op(Vector<T, n> &a, T b)            \
    {                                                          \
        for (int i = 0; i < n; i++)                            \
            a[i] op b;                                         \
        return a;                                              \
    }

namespace kraft
{

template <typename T, int n>
struct Vector
{
    T _data[n];

    COMMON_VECTOR_MEMBERS(n);
};

#pragma warning(push)
#pragma warning(disable : 4201) // Nameless struct/union

template <typename T>
struct Vector<T, 2>
{
    union
    {
        T _data[2];
        struct
        {
            T x;
            T y;
        };

        struct
        {
            T u;
            T v;
        };

        struct
        {
            T s;
            T t;
        };
    };

    Vector(T x, T y)
    {
        _data[0] = x;
        _data[1] = y;
    }

    Vector(Vector<T, 3> xyz)
    {
        _data[0] = xyz.x;
        _data[1] = xyz.y;
    }

    Vector(const Vector<T, 3>& xyz)
    {
        _data[0] = xyz.x;
        _data[1] = xyz.y;
    }

    Vector(Vector<T, 4> xyzw)
    {
        _data[0] = xyzw.x;
        _data[1] = xyzw.y;
    }

    Vector(const Vector<T, 4>& xyzw)
    {
        _data[0] = xyzw.x;
        _data[1] = xyzw.y;
    }

    COMMON_VECTOR_MEMBERS(2);
};

template <typename T>
struct Vector<T, 3>
{
    union
    {
        T _data[3];
        struct
        {
            T x;
            T y;
            T z;
        };

        struct
        {
            T u;
            T v;
        };

        struct
        {
            T r;
            T g;
            T b;
        };

        struct
        {
            Vector<T, 2> xy;
        };

        struct
        {
            Vector<T, 2> uv;
        };
    };

    Vector(T x, T y, T z)
    {
        _data[0] = x;
        _data[1] = y;
        _data[2] = z;
    }

    Vector(Vector<T, 2> xy, T z)
    {
        _data[0] = xy.x;
        _data[1] = xy.y;
        _data[2] = z;
    }

    Vector(Vector<T, 4> xyzw)
    {
        _data[0] = xyzw.x;
        _data[1] = xyzw.y;
        _data[2] = xyzw.z;
    }

    Vector(const Vector<T, 4>& xyzw)
    {
        _data[0] = xyzw.x;
        _data[1] = xyzw.y;
        _data[2] = xyzw.z;
    }

    COMMON_VECTOR_MEMBERS(3);
};

template <typename T>
struct Vector<T, 4>
{
    union
    {
        T _data[4];
        struct
        {
            T x;
            T y;
            T z;
            T w;
        };

        struct
        {
            T r;
            T g;
            T b;
            T a;
        };

        struct
        {
            Vector<T, 2> xy;
        };

        struct
        {
            Vector<T, 2> uv;
        };

        struct
        {
            Vector<T, 3> xyz;
        };

        struct
        {
            Vector<T, 3> rgb;
        };
    };

    Vector(T x, T y, T z, T w)
    {
        _data[0] = x;
        _data[1] = y;
        _data[2] = z;
        _data[3] = w;
    }

    Vector(Vector<T, 2> xy, T z, T w)
    {
        _data[0] = xy.x;
        _data[1] = xy.y;
        _data[2] = z;
        _data[3] = w;
    }

    Vector(Vector<T, 3> xyz, T w)
    {
        _data[0] = xyz.x;
        _data[1] = xyz.y;
        _data[2] = xyz.z;
        _data[3] = w;
    }

    COMMON_VECTOR_MEMBERS(4);
};

#pragma warning(pop)

UNARY_OPERATOR(-);
UNARY_OPERATOR(!);
UNARY_OPERATOR(~);

BINARY_OPERATOR(-);
BINARY_OPERATOR(+);
BINARY_OPERATOR(*);
BINARY_OPERATOR(/);
BINARY_OPERATOR(&);
BINARY_OPERATOR(|);
BINARY_OPERATOR(^);

BINARY_INPLACE_OPERATOR(-=);
BINARY_INPLACE_OPERATOR(+=);
BINARY_INPLACE_OPERATOR(*=);
BINARY_INPLACE_OPERATOR(/=);
BINARY_INPLACE_OPERATOR(&=);
BINARY_INPLACE_OPERATOR(|=);
BINARY_INPLACE_OPERATOR(^=);

#undef UNARY_OPERATOR
#undef BINARY_OPERATOR
#undef BINARY_INPLACE_OPERATOR
#undef COMMON_VECTOR_MEMBERS

template <typename T, int n>
T Dot(Vector<T, n> a, Vector<T, n> b)
{
    T out = 0;
    for (int i = 0; i < n; ++i)
        out += a[i] * b[i];

    return out;
}

template <typename T, int n>
T LengthSquared(Vector<T, n> a)
{
    return Dot(a, a);
}

template <typename T, int n>
T Length(Vector<T, n> a)
{
    return Sqrt(LengthSquared(a));
}

template <typename T, int n>
Vector<T, n> Cross(Vector<T, n> a, Vector<T, n> b)
{
    return 
    {
        a.y*b.z - a.z*b.y,
        a.z*b.x - a.x*b.z,
        a.x*b.y - a.y*b.x
    };
}

template <typename T, int n>
void Normalize(Vector<T, n>* in) 
{
    T length = Length(*in);
    for (int i = 0; i < n; i++)
        (*in)[i] /= length;
}

template <typename T, int n>
Vector<T, n> Normalize(Vector<T, n> in) 
{
    Vector<T, n> out;
    const T length = Length(in);

    for (int i = 0; i < n; i++)
        out[i] = in[i] / length;

    return out;
}

template <typename T, int n>
T Distance(Vector<T, n> a, Vector<T, n> b) 
{
    Vector<T, n> vec;
    for (int i = 0 ; i < n; i++)
        vec[i] = a[i] - b[i];

    return Length(vec);
}

KRAFT_INLINE const bool Vec3fCompare(Vector<float, 3> a, Vector<float, 3> b, float32 tolerance) 
{
    if (Abs(a.x - b.x) > tolerance) 
    {
        return false;
    }

    if (Abs(a.y - b.y) > tolerance) 
    {
        return false;
    }

    if (Abs(a.z - b.z) > tolerance) 
    {
        return false;
    }

    return true;
}

typedef Vector<float32, 2> Vec2f;
typedef Vector<float32, 3> Vec3f;
typedef Vector<float32, 4> Vec4f;

extern const Vec2f Vec2fZero;
extern const Vec3f Vec3fZero;
extern const Vec4f Vec4fZero;
extern const Vec3f Vec3fOne;
extern const Vec4f Vec4fOne;

}

/**
 *
 * Matrix
 * Row-Major operations
 *
 */

#define UNARY_OPERATOR(op)                                      \
    template <typename T, int rows, int cols>                   \
    Matrix<T, rows, cols> operator op(Matrix<T, rows, cols> in) \
    {                                                           \
        Matrix<T, rows, cols> out;                              \
        for (int i = 0; i < rows * cols; ++i)                   \
            out._data[i] = op in._data[i];                      \
        return out;                                             \
    }

#define BINARY_OPERATOR(op)                                                                           \
    template <typename T, int rows, int cols>                                                         \
    Matrix<T, rows, cols> operator op(const Matrix<T, rows, cols> &a, const Matrix<T, rows, cols> &b) \
    {                                                                                                 \
        Matrix<T, rows, cols> out;                                                                    \
        for (int i = 0; i < rows * cols; ++i)                                                         \
            out._data[i] = a._data[i] op b._data[i];                                                  \
        return out;                                                                                   \
    }

#define BINARY_SCALAR_OPERATOR(op)                                   \
    template <typename T, int rows, int cols>                        \
    Matrix<T, rows, cols> operator op(T a, Matrix<T, rows, cols> in) \
    {                                                                \
        Matrix<T, rows, cols> out;                                   \
        for (int i = 0; i < rows * cols; ++i)                        \
            out._data[i] = a op in._data[i];                         \
        return out;                                                  \
    }                                                                \
                                                                     \
    template <typename T, int rows, int cols>                        \
    Matrix<T, rows, cols> operator op(Matrix<T, rows, cols> in, T a) \
    {                                                                \
        Matrix<T, rows, cols> out;                                   \
        for (int i = 0; i < rows * cols; ++i)                        \
            out._data[i] = in._data[i] op a;                         \
        return out;                                                  \
    }

#define BINARY_INPLACE_OPERATOR(op)                                                             \
    template <typename T, int rows, int cols>                                                   \
    Matrix<T, rows, cols> operator op(Matrix<T, rows, cols> &a, const Matrix<T, rows, cols> &b) \
    {                                                                                           \
        Matrix<T, rows, cols> out;                                                              \
        for (int i = 0; i < rows * cols; ++i)                                                   \
            a._data[i] op b._data[i];                                                           \
        return out;                                                                             \
    }

#define BINARY_INPLACE_SCALAR_OPERATOR(op)                           \
    template <typename T, int rows, int cols>                        \
    Matrix<T, rows, cols> operator op(Matrix<T, rows, cols> &a, T b) \
    {                                                                \
        Matrix<T, rows, cols> out;                                   \
        for (int i = 0; i < rows * cols; ++i)                        \
            a._data[i] op b;                                         \
        return out;                                                  \
    }


namespace kraft
{

enum IdentityTag
{
    Identity
};

template <typename T, int rows, int cols>
struct Matrix
{
    T _data[rows * cols];

    Matrix() {}
    Matrix(std::initializer_list<T> input)
    {
        int size = rows * cols;
        int m = size < input.size() ? size : input.size();

        auto it = input.begin();
        for (int i = 0; i < m; ++i, it++)
            _data[i] = *it;

        for (int i = m; i < size; i++)
            _data[i] = T(0);
    }

    explicit Matrix(T value)
    {
        for (int i = 0; i < rows * cols; i++)
            _data[i] = value;
    }

    explicit Matrix(IdentityTag)
    {
        assert(rows == cols && "Matrix is not square");
        for (int i = 0; i < rows * cols; i++)
            _data[i] = (i % (rows + 1) == 0) ? (T)1 : (T)0;
    }

    template <typename U>
    explicit Matrix(const U* other)
    {
        for (int i = 0; i < rows*cols; ++i)
            _data[i] = T(other[i]);
    }

    Vector<T, cols> &operator[](int index)
    {
        return (Vector<T, cols> &)(_data[cols * index]);
    }

    const Vector<T, cols> &operator[](int index) const
    {
        return (const Vector<T, cols> &)(_data[cols * index]);
    }

    typedef T (&array_t)[rows*cols];
    operator array_t () { return _data; }
    typedef const T (&const_array_t)[rows*cols];
    operator const_array_t () const { return _data; }
    
    // Disallow bool conversions (without this, they'd happen implicitly via the array conversions)
    private: operator bool();
};

typedef Matrix<float32, 4, 4> Mat4f;

// Matrix multiplication
template <typename T, int rows, int cols>
Matrix<T, rows, cols> operator*(const Matrix<T, rows, cols> &a, const Matrix<T, rows, cols> &b)
{
    Matrix<T, rows, cols> out(0);
    for (int i = 0; i < rows; i++)
    {
        for (int j = 0; j < cols; ++j)
        {
            for (int k = 0; k < rows; ++k)
            {
                out[i][j] += a[i][k] * b[k][j];
            }
        }
    }

    return out;
}

template <typename T>
Matrix<T, 4, 4> operator*(const Matrix<T, 4, 4> &a, const Matrix<T, 4, 4> &b)
{
    Matrix<T, 4, 4> out(0);
    const T *aPtr = a._data;
    const T *bPtr = b._data;
    T *outPtr = out._data;

    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; ++j)
        {
            *outPtr = aPtr[0] * bPtr[0 + j]
                    + aPtr[1] * bPtr[4 + j]
                    + aPtr[2] * bPtr[8 + j]
                    + aPtr[3] * bPtr[12 + j];

            outPtr++;
        }

        aPtr += 4;
    }

    return out;
}

template <typename T, int rows, int cols>
Matrix<T, rows, cols> &operator*=(Matrix<T, rows, cols> &a, const Matrix<T, rows, cols> &b)
{
    a = a * b;
    return a;
}

template <typename T, int rows, int cols>
Vector<T, rows> operator*(const Matrix<T, rows, cols> &a, Vector<T, rows> b)
{
    Vector<T, rows> out((T)0);
    for (int i = 0; i < rows; i++)
        for (int j = 0; j < cols; ++j)
            out[i] += a[i][j] * b[j];

    return out;
}

template <typename T, int rows, int cols>
Vector<T, cols> operator*(Vector<T, cols> b, const Matrix<T, rows, cols> &a)
{
    Vector<T, cols> out((T)0);
    for (int i = 0; i < rows; i++)
        for (int j = 0; j < cols; ++j)
            out[i] += a[i][j] * b[j];

    return out;
}

template <typename T, int n>
Vector<T, n> operator*=(Vector<T, n> &a, const Matrix<T, n, n> &b)
{
    a = a * b;
    return a;
}

template <typename T, int rows, int cols>
Matrix<T, rows, cols> Transpose(const Matrix<T, rows, cols> &in)
{
    Matrix<T, rows, cols> out;
    for (int i = 0; i < rows; i++)
        for (int j = 0; j < cols; j++)
            out[j][i] = in[i][j];

    return out;
}

template <typename T, int n>
Matrix<T, n + 1, n + 1> TranslationMatrix(Vector<T, n> position)
{
    Matrix<T, n + 1, n + 1> out(Identity);
    for (int i = 0; i < n; i++)
    {
        out[n][i] = position[i];
    }
    
    return out;
}

template <typename T, int n>
Matrix<T, n + 1, n + 1> ScaleMatrix(Vector<T, n> scale)
{
    Matrix<T, n + 1, n + 1> out(Identity);
    out._data[0] = scale[0];
    out._data[5] = scale[1];
    out._data[10] = scale[2];
    
    return out;
}

template <typename T>
Matrix<T, 2, 2> Inverse(const Matrix<T, 2, 2> &in)
{
    T determinant = (in[0][0] * in[1][1] - in[0][1] * in[1][0]);
	return Matrix<T, 2, 2>
    { 
        +in[1][1], -in[0][1], 
        -in[1][0], +in[0][0] 
    } / determinant;
}

template <typename T>
Matrix<T, 4, 4> Inverse(const Matrix<T, 4, 4> &in)
{
    const float32* m = in._data;

    float32 t0 = m[10] * m[15];
    float32 t1 = m[14] * m[11];
    float32 t2 = m[6] * m[15];
    float32 t3 = m[14] * m[7];
    float32 t4 = m[6] * m[11];
    float32 t5 = m[10] * m[7];
    float32 t6 = m[2] * m[15];
    float32 t7 = m[14] * m[3];
    float32 t8 = m[2] * m[11];
    float32 t9 = m[10] * m[3];
    float32 t10 = m[2] * m[7];
    float32 t11 = m[6] * m[3];
    float32 t12 = m[8] * m[13];
    float32 t13 = m[12] * m[9];
    float32 t14 = m[4] * m[13];
    float32 t15 = m[12] * m[5];
    float32 t16 = m[4] * m[9];
    float32 t17 = m[8] * m[5];
    float32 t18 = m[0] * m[13];
    float32 t19 = m[12] * m[1];
    float32 t20 = m[0] * m[9];
    float32 t21 = m[8] * m[1];
    float32 t22 = m[0] * m[5];
    float32 t23 = m[4] * m[1];

    Mat4f out;
    float32* o = out._data;

    o[0] = (t0 * m[5] + t3 * m[9] + t4 * m[13]) - (t1 * m[5] + t2 * m[9] + t5 * m[13]);
    o[1] = (t1 * m[1] + t6 * m[9] + t9 * m[13]) - (t0 * m[1] + t7 * m[9] + t8 * m[13]);
    o[2] = (t2 * m[1] + t7 * m[5] + t10 * m[13]) - (t3 * m[1] + t6 * m[5] + t11 * m[13]);
    o[3] = (t5 * m[1] + t8 * m[5] + t11 * m[9]) - (t4 * m[1] + t9 * m[5] + t10 * m[9]);

    float32 d = 1.0f / (m[0] * o[0] + m[4] * o[1] + m[8] * o[2] + m[12] * o[3]);

    o[0] = d * o[0];
    o[1] = d * o[1];
    o[2] = d * o[2];
    o[3] = d * o[3];
    o[4] = d * ((t1 * m[4] + t2 * m[8] + t5 * m[12]) - (t0 * m[4] + t3 * m[8] + t4 * m[12]));
    o[5] = d * ((t0 * m[0] + t7 * m[8] + t8 * m[12]) - (t1 * m[0] + t6 * m[8] + t9 * m[12]));
    o[6] = d * ((t3 * m[0] + t6 * m[4] + t11 * m[12]) - (t2 * m[0] + t7 * m[4] + t10 * m[12]));
    o[7] = d * ((t4 * m[0] + t9 * m[4] + t10 * m[8]) - (t5 * m[0] + t8 * m[4] + t11 * m[8]));
    o[8] = d * ((t12 * m[7] + t15 * m[11] + t16 * m[15]) - (t13 * m[7] + t14 * m[11] + t17 * m[15]));
    o[9] = d * ((t13 * m[3] + t18 * m[11] + t21 * m[15]) - (t12 * m[3] + t19 * m[11] + t20 * m[15]));
    o[10] = d * ((t14 * m[3] + t19 * m[7] + t22 * m[15]) - (t15 * m[3] + t18 * m[7] + t23 * m[15]));
    o[11] = d * ((t17 * m[3] + t20 * m[7] + t23 * m[11]) - (t16 * m[3] + t21 * m[7] + t22 * m[11]));
    o[12] = d * ((t14 * m[10] + t17 * m[14] + t13 * m[6]) - (t16 * m[14] + t12 * m[6] + t15 * m[10]));
    o[13] = d * ((t20 * m[14] + t12 * m[2] + t19 * m[10]) - (t18 * m[10] + t21 * m[14] + t13 * m[2]));
    o[14] = d * ((t18 * m[6] + t23 * m[14] + t15 * m[2]) - (t22 * m[14] + t14 * m[2] + t19 * m[6]));
    o[15] = d * ((t22 * m[10] + t16 * m[2] + t21 * m[6]) - (t20 * m[6] + t23 * m[10] + t17 * m[2]));

    return out;
}

template <typename T, int rows, int cols>
void PrintMatrix(Matrix<T, rows, cols> in)
{
    printf("{");
    for (int i = 0; i < rows * cols; i++)
    {
        if (i % cols == 0)
            printf("\n\t");

        printf("%f, ", in._data[i]);
    }
    
    printf("\n}\n");
}

template <typename T, int N>
void PrintVector(Vector<T, N> in)
{
    printf("{ ");
    for (int i = 0; i < N; i++)
    {
        printf("%f, ", in._data[i]);
    }
    
    printf(" }\n");
}

UNARY_OPERATOR(-);
UNARY_OPERATOR(!);
UNARY_OPERATOR(~);

BINARY_OPERATOR(-);
BINARY_OPERATOR(+);
BINARY_OPERATOR(&);
BINARY_OPERATOR(|);
BINARY_OPERATOR(^);

BINARY_SCALAR_OPERATOR(*);
BINARY_SCALAR_OPERATOR(/);

BINARY_INPLACE_OPERATOR(-=);
BINARY_INPLACE_OPERATOR(+=);
BINARY_INPLACE_OPERATOR(&=);
BINARY_INPLACE_OPERATOR(|=);
BINARY_INPLACE_OPERATOR(^=);

BINARY_INPLACE_SCALAR_OPERATOR(*=);
BINARY_INPLACE_SCALAR_OPERATOR(/=);

#undef UNARY_OPERATOR
#undef BINARY_OPERATOR
#undef BINARY_SCALAR_OPERATOR
#undef BINARY_INPLACE_OPERATOR
#undef BINARY_INPLACE_SCALAR_OPERATOR

// Projection
KRAFT_API Mat4f OrthographicMatrix(float32 left, float32 right, float32 top, float32 bottom, float32 nearClip, float32 farClip);
KRAFT_API Mat4f PerspectiveMatrix(float32 fieldOfViewRadians, float32 aspectRatio, float32 nearClip, float32 farClip);

// Directional
KRAFT_INLINE Vec3f ForwardVector(const Mat4f& in)
{
    Vec3f out;
    out.x = -in._data[2];
    out.y = -in._data[6];
    out.z = -in._data[10];
    
    Normalize(&out);
    
    return out;
}

template <typename T, int n>
Vector<T, n> ForwardVector(const Matrix<T, n+1, n+1>& in)
{
    Vector<T, n> out;
    for (int i = 0; i < n; i++)
        out[i] = -in._data[(n+1) * i + 2];
    
    Normalize(&out);

    return out;
}

KRAFT_INLINE Vec3f BackwardVector(const Mat4f& in)
{
    Vec3f out;
    out.x = in._data[2];
    out.y = in._data[6];
    out.z = in._data[10];
    
    Normalize(&out);
    
    return out;
}

template <typename T, int n>
Vector<T, n> BackwardVector(const Matrix<T, n+1, n+1>& in) 
{
    Vector<T, n> out;
    for (int i = 0; i < n; i++)
        out[i] = in._data[(n+1) * i + 2];
    
    Normalize(&out);

    return out;
}

KRAFT_INLINE Vec3f UpVector(const Mat4f& in) 
{
    Vec3f out;
    out.x = in._data[1];
    out.y = in._data[5];
    out.z = in._data[9];
    
    Normalize(&out);
    
    return out;
}

KRAFT_INLINE Vec3f DownVector(const Mat4f& in) 
{
    Vec3f out;
    out.x = -in._data[1];
    out.y = -in._data[5];
    out.z = -in._data[9];
    
    Normalize(&out);
    
    return out;
}

KRAFT_INLINE Vec3f LeftVector(const Mat4f& in) 
{
    Vec3f out;
    out.x = -in._data[0];
    out.y = -in._data[4];
    out.z = -in._data[8];
    
    Normalize(&out);
    
    return out;
}

KRAFT_INLINE Vec3f RightVector(const Mat4f& in)
{
    Vec3f out;
    out.x = in._data[0];
    out.y = in._data[4];
    out.z = in._data[8];
    
    Normalize(&out);
    
    return out;
}

// Rotation
KRAFT_API Mat4f RotationMatrixX(float32 angleRadians);
KRAFT_API Mat4f RotationMatrixY(float32 angleRadians);
KRAFT_API Mat4f RotationMatrixZ(float32 angleRadians);
KRAFT_API Mat4f RotationMatrixFromEulerAngles(Vec3f euler);
KRAFT_API Mat4f RotationMatrixFromEulerAngles(float32 rotationXRadians, float32 rotationYRadians, float32 rotationZRadians);

KRAFT_API Mat4f LookAt(Vec3f Eye, Vec3f Center, Vec3f Up);

}