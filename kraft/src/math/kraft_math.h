#pragma once

#ifndef KRAFT_MATH_H_
#define KRAFT_MATH_H_
#endif

#include "core/kraft_core.h"

//
// Useful defines
//

#define KRAFT_PI                      3.14159265358979323846f
#define KRAFT_DEG_TO_RADIANS(degrees) (degrees * (KRAFT_PI / 180.0f))

namespace kraft {

template<typename T, int n>
struct Vector;

template<typename T, int n>
void Normalize(Vector<T, n>* in);

namespace math {

static const float Epsilon = 1e-6f;

template<typename T>
KRAFT_API KRAFT_INLINE static void Swap(T& a, T& b)
{
    T c = a;
    a = b;
    b = c;
}

template<typename T>
KRAFT_API KRAFT_INLINE static T Min(T a, T b)
{
    return a > b ? b : a;
}

template<typename T>
KRAFT_API KRAFT_INLINE T Max(T a, T b)
{
    return a > b ? a : b;
}

KRAFT_INLINE static uint64 AlignUp(uint64 UnalignedValue, uint64 Alignment)
{
    return ((UnalignedValue + (Alignment - 1)) & ~(Alignment - 1));
}

template<typename T>
KRAFT_INLINE static T Clamp(T Value, T Min, T Max)
{
    return Value < Min ? Min : Value > Max ? Max : Value;
}
} // namespace math

//
// Useful functions
//

KRAFT_API f32 Sin(f32 x);
KRAFT_API f32 Cos(f32 x);
KRAFT_API f32 Tan(f32 x);
KRAFT_API f32 Acos(f32 x);
KRAFT_API f32 Sqrt(f32 x);
KRAFT_API f32 Abs(f32 x);

KRAFT_INLINE static f32 DegToRadians(f32 degrees)
{
    return degrees * (KRAFT_PI / 180.0f);
}

KRAFT_INLINE static f32 RadiansToDegrees(f32 radians)
{
    return (radians * 180.0f) / KRAFT_PI;
}

// Converts the given angle in degrees to radians
KRAFT_INLINE static f32 Radians(f32 Degrees)
{
    return DegToRadians(Degrees);
}

// Converts the given angle in radians to degrees
KRAFT_INLINE static f32 Degrees(f32 Radians)
{
    return RadiansToDegrees(Radians);
}

} // namespace kraft

/**
 *
 * Vectors
 *
 */

#include <assert.h>
#include <cmath>
#include <initializer_list>
#include <stdio.h>

#define KRGB(r, g, b)     Vec3f(r / 255.f, g / 255.f, b / 255.f)
#define KRGBA(r, g, b, a) Vec4f(r / 255.f, g / 255.f, b / 255.f, a / 255.f)

#define COMMON_VECTOR_MEMBERS(n)                                                                                                                                                                       \
    Vector(){};                                                                                                                                                                                        \
                                                                                                                                                                                                       \
    Vector(std::initializer_list<T> list)                                                                                                                                                              \
    {                                                                                                                                                                                                  \
        int  count = (int)list.size() < n ? (int)list.size() : n;                                                                                                                                      \
        auto it = list.begin();                                                                                                                                                                        \
        for (int i = 0; i < count; ++i, ++it)                                                                                                                                                          \
            _data[i] = *it;                                                                                                                                                                            \
                                                                                                                                                                                                       \
        for (int i = count; i < n; i++)                                                                                                                                                                \
            _data[i] = 0;                                                                                                                                                                              \
    }                                                                                                                                                                                                  \
                                                                                                                                                                                                       \
    T& operator[](int i)                                                                                                                                                                               \
    {                                                                                                                                                                                                  \
        return _data[i];                                                                                                                                                                               \
    }                                                                                                                                                                                                  \
                                                                                                                                                                                                       \
    const T& operator[](int i) const                                                                                                                                                                   \
    {                                                                                                                                                                                                  \
        return _data[i];                                                                                                                                                                               \
    }                                                                                                                                                                                                  \
                                                                                                                                                                                                       \
    /* Initialize the vector with a common value for all components */                                                                                                                                 \
    explicit Vector(T value)                                                                                                                                                                           \
    {                                                                                                                                                                                                  \
        for (int i = 0; i < n; ++i)                                                                                                                                                                    \
            _data[i] = value;                                                                                                                                                                          \
    }                                                                                                                                                                                                  \
                                                                                                                                                                                                       \
    /* C array conversions */                                                                                                                                                                          \
    typedef T(&array_t)[n];                                                                                                                                                                            \
    operator array_t()                                                                                                                                                                                 \
    {                                                                                                                                                                                                  \
        return _data;                                                                                                                                                                                  \
    }                                                                                                                                                                                                  \
                                                                                                                                                                                                       \
    typedef const T(&const_array_t)[n];                                                                                                                                                                \
    operator const_array_t() const                                                                                                                                                                     \
    {                                                                                                                                                                                                  \
        return _data;                                                                                                                                                                                  \
    }                                                                                                                                                                                                  \
                                                                                                                                                                                                       \
    KRAFT_INLINE T Length() const                                                                                                                                                                      \
    {                                                                                                                                                                                                  \
        T LengthSquared = T(0);                                                                                                                                                                        \
        for (int i = 0; i < n; ++i)                                                                                                                                                                    \
        {                                                                                                                                                                                              \
            LengthSquared += _data[i] * _data[i];                                                                                                                                                      \
        }                                                                                                                                                                                              \
        return Sqrt(LengthSquared);                                                                                                                                                                    \
    }                                                                                                                                                                                                  \
                                                                                                                                                                                                       \
    KRAFT_INLINE T LengthSquared() const                                                                                                                                                               \
    {                                                                                                                                                                                                  \
        T LengthSquared = T(0);                                                                                                                                                                        \
        for (int i = 0; i < n; ++i)                                                                                                                                                                    \
        {                                                                                                                                                                                              \
            LengthSquared += _data[i] * _data[i];                                                                                                                                                      \
        }                                                                                                                                                                                              \
        return LengthSquared;                                                                                                                                                                          \
    }                                                                                                                                                                                                  \
                                                                                                                                                                                                       \
    KRAFT_INLINE void Normalize()                                                                                                                                                                      \
    {                                                                                                                                                                                                  \
        kraft::Normalize(this);                                                                                                                                                                        \
    }                                                                                                                                                                                                  \
                                                                                                                                                                                                       \
private:                                                                                                                                                                                               \
    operator bool();                                                                                                                                                                                   \
                                                                                                                                                                                                       \
public:

#define UNARY_OPERATOR(op)                                                                                                                                                                             \
    template<typename T, int n>                                                                                                                                                                        \
    Vector<T, n> operator op(Vector<T, n> in)                                                                                                                                                          \
    {                                                                                                                                                                                                  \
        Vector<T, n> out;                                                                                                                                                                              \
        for (int i = 0; i < n; i++)                                                                                                                                                                    \
            out[i] = op in[i];                                                                                                                                                                         \
        return out;                                                                                                                                                                                    \
    }

#define BINARY_OPERATOR(op)                                                                                                                                                                            \
    template<typename T, int n>                                                                                                                                                                        \
    Vector<T, n> operator op(Vector<T, n> a, Vector<T, n> b)                                                                                                                                           \
    {                                                                                                                                                                                                  \
        Vector<T, n> out;                                                                                                                                                                              \
        for (int i = 0; i < n; ++i)                                                                                                                                                                    \
            out[i] = a[i] op b[i];                                                                                                                                                                     \
        return out;                                                                                                                                                                                    \
    }                                                                                                                                                                                                  \
                                                                                                                                                                                                       \
    /* Scalar addition; vec2 + a */                                                                                                                                                                    \
    template<typename T, int n>                                                                                                                                                                        \
    Vector<T, n> operator op(Vector<T, n> a, T b)                                                                                                                                                      \
    {                                                                                                                                                                                                  \
        Vector<T, n> out;                                                                                                                                                                              \
        for (int i = 0; i < n; ++i)                                                                                                                                                                    \
            out[i] = a[i] op b;                                                                                                                                                                        \
        return out;                                                                                                                                                                                    \
    }                                                                                                                                                                                                  \
                                                                                                                                                                                                       \
    /* Scalar addition; a + vec2 */                                                                                                                                                                    \
    template<typename T, int n>                                                                                                                                                                        \
    Vector<T, n> operator op(T a, Vector<T, n> b)                                                                                                                                                      \
    {                                                                                                                                                                                                  \
        Vector<T, n> out;                                                                                                                                                                              \
        for (int i = 0; i < n; ++i)                                                                                                                                                                    \
            out[i] = a op b[i];                                                                                                                                                                        \
        return out;                                                                                                                                                                                    \
    }

#define BINARY_INPLACE_OPERATOR(op)                                                                                                                                                                    \
    template<typename T, int n>                                                                                                                                                                        \
    Vector<T, n>& operator op(Vector<T, n>& a, Vector<T, n> b)                                                                                                                                         \
    {                                                                                                                                                                                                  \
        for (int i = 0; i < n; i++)                                                                                                                                                                    \
            a[i] op b[i];                                                                                                                                                                              \
        return a;                                                                                                                                                                                      \
    }                                                                                                                                                                                                  \
                                                                                                                                                                                                       \
    template<typename T, int n>                                                                                                                                                                        \
    Vector<T, n>& operator op(Vector<T, n>& a, T b)                                                                                                                                                    \
    {                                                                                                                                                                                                  \
        for (int i = 0; i < n; i++)                                                                                                                                                                    \
            a[i] op b;                                                                                                                                                                                 \
        return a;                                                                                                                                                                                      \
    }

namespace kraft {

template<typename T, int n>
struct Vector
{
    T _data[n];

    COMMON_VECTOR_MEMBERS(n);
};

#pragma warning(push)
#pragma warning(disable : 4201) // Nameless struct/union

template<typename T>
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

template<typename T>
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

template<typename T>
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

template<typename T, int n>
KRAFT_INLINE static T Dot(Vector<T, n> a, Vector<T, n> b)
{
    T Result = T(0);
    for (int i = 0; i < n; ++i)
        Result += a[i] * b[i];

    return Result;
}

template<typename T, int n>
KRAFT_INLINE static T LengthSquared(Vector<T, n> a)
{
    T Result = T(0);
    for (int i = 0; i < n; ++i)
        Result += a[i] * a[i];

    return Result;
}

template<typename T, int n>
KRAFT_INLINE static T Length(Vector<T, n> a)
{
    return Sqrt(LengthSquared(a));
}

template<typename T, int n>
KRAFT_INLINE static Vector<T, n> Cross(Vector<T, n> a, Vector<T, n> b)
{
    return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
}

template<typename T, int n>
KRAFT_INLINE static void Normalize(Vector<T, n>* in)
{
    T length = Length(*in);
    for (int i = 0; i < n; i++)
        (*in)[i] /= length;
}

template<typename T, int n>
KRAFT_INLINE static Vector<T, n> Normalize(Vector<T, n> in)
{
    Vector<T, n> out;
    const T      length = Length(in);

    for (int i = 0; i < n; i++)
        out[i] = in[i] / length;

    return out;
}

template<typename T, int n>
static T Distance(Vector<T, n> a, Vector<T, n> b)
{
    Vector<T, n> vec;
    for (int i = 0; i < n; i++)
        vec[i] = a[i] - b[i];

    return Length(vec);
}

KRAFT_INLINE static const bool Vec3fCompare(Vector<float, 3> a, Vector<float, 3> b, f32 tolerance)
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

typedef Vector<f32, 2> Vec2f;
typedef Vector<f32, 3> Vec3f;
typedef Vector<f32, 4> Vec4f;

extern const Vec2f Vec2fZero;
extern const Vec3f Vec3fZero;
extern const Vec4f Vec4fZero;
extern const Vec3f Vec3fOne;
extern const Vec4f Vec4fOne;

} // namespace kraft

/**
 *
 * Matrix
 * Row-Major operations
 *
 */

#define UNARY_OPERATOR(op)                                                                                                                                                                             \
    template<typename T, int rows, int cols>                                                                                                                                                           \
    Matrix<T, rows, cols> operator op(Matrix<T, rows, cols> in)                                                                                                                                        \
    {                                                                                                                                                                                                  \
        Matrix<T, rows, cols> out;                                                                                                                                                                     \
        for (int i = 0; i < rows * cols; ++i)                                                                                                                                                          \
            out._data[i] = op in._data[i];                                                                                                                                                             \
        return out;                                                                                                                                                                                    \
    }

#define BINARY_OPERATOR(op)                                                                                                                                                                            \
    template<typename T, int rows, int cols>                                                                                                                                                           \
    Matrix<T, rows, cols> operator op(const Matrix<T, rows, cols>& a, const Matrix<T, rows, cols>& b)                                                                                                  \
    {                                                                                                                                                                                                  \
        Matrix<T, rows, cols> out;                                                                                                                                                                     \
        for (int i = 0; i < rows * cols; ++i)                                                                                                                                                          \
            out._data[i] = a._data[i] op b._data[i];                                                                                                                                                   \
        return out;                                                                                                                                                                                    \
    }

#define BINARY_SCALAR_OPERATOR(op)                                                                                                                                                                     \
    template<typename T, int rows, int cols>                                                                                                                                                           \
    Matrix<T, rows, cols> operator op(T a, Matrix<T, rows, cols> in)                                                                                                                                   \
    {                                                                                                                                                                                                  \
        Matrix<T, rows, cols> out;                                                                                                                                                                     \
        for (int i = 0; i < rows * cols; ++i)                                                                                                                                                          \
            out._data[i] = a op in._data[i];                                                                                                                                                           \
        return out;                                                                                                                                                                                    \
    }                                                                                                                                                                                                  \
                                                                                                                                                                                                       \
    template<typename T, int rows, int cols>                                                                                                                                                           \
    Matrix<T, rows, cols> operator op(Matrix<T, rows, cols> in, T a)                                                                                                                                   \
    {                                                                                                                                                                                                  \
        Matrix<T, rows, cols> out;                                                                                                                                                                     \
        for (int i = 0; i < rows * cols; ++i)                                                                                                                                                          \
            out._data[i] = in._data[i] op a;                                                                                                                                                           \
        return out;                                                                                                                                                                                    \
    }

#define BINARY_INPLACE_OPERATOR(op)                                                                                                                                                                    \
    template<typename T, int rows, int cols>                                                                                                                                                           \
    Matrix<T, rows, cols> operator op(Matrix<T, rows, cols>& a, const Matrix<T, rows, cols>& b)                                                                                                        \
    {                                                                                                                                                                                                  \
        Matrix<T, rows, cols> out;                                                                                                                                                                     \
        for (int i = 0; i < rows * cols; ++i)                                                                                                                                                          \
            a._data[i] op b._data[i];                                                                                                                                                                  \
        return out;                                                                                                                                                                                    \
    }

#define BINARY_INPLACE_SCALAR_OPERATOR(op)                                                                                                                                                             \
    template<typename T, int rows, int cols>                                                                                                                                                           \
    Matrix<T, rows, cols> operator op(Matrix<T, rows, cols>& a, T b)                                                                                                                                   \
    {                                                                                                                                                                                                  \
        Matrix<T, rows, cols> out;                                                                                                                                                                     \
        for (int i = 0; i < rows * cols; ++i)                                                                                                                                                          \
            a._data[i] op b;                                                                                                                                                                           \
        return out;                                                                                                                                                                                    \
    }

namespace kraft {

enum IdentityTag
{
    Identity
};

template<typename T, int rows, int cols>
struct Matrix
{
    union
    {
        T _data[rows * cols];
        T _data4x4[rows][cols];
        struct
        {
            Vector<T, cols> Right, Up, Dir, Position;
        } V;
    };

    Matrix() {};

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

    template<typename U>
    explicit Matrix(const U* other)
    {
        for (int i = 0; i < rows * cols; ++i)
            _data[i] = T(other[i]);
    }

    Vector<T, cols>& operator[](int index)
    {
        return (Vector<T, cols>&)(_data[cols * index]);
    }

    const Vector<T, cols>& operator[](int index) const
    {
        return (const Vector<T, cols>&)(_data[cols * index]);
    }

    typedef T (&array_t)[rows * cols];
    operator array_t()
    {
        return _data;
    }

    typedef const T (&const_array_t)[rows * cols];
    operator const_array_t() const
    {
        return _data;
    }

    void OrthoNormalize()
    {
        V.Right.Normalize();
        V.Up.Normalize();
        V.Dir.Normalize();
    }

    void Decompose(Vector<T, cols - 1>& Translation, Vector<T, cols - 1>& Rotation, Vector<T, cols - 1>& Scale)
    {
        static_assert(cols == 4);
        Matrix<T, rows, cols> CopyM = *this;
        Scale._data[0] = CopyM.V.Right.Length();
        Scale._data[1] = CopyM.V.Up.Length();
        Scale._data[2] = CopyM.V.Dir.Length();

        CopyM.OrthoNormalize();

        Rotation._data[0] = atan2f(CopyM._data4x4[1][2], CopyM._data4x4[2][2]);
        Rotation._data[1] = atan2f(-CopyM._data4x4[0][2], Sqrt(CopyM._data4x4[1][2] * CopyM._data4x4[1][2] + CopyM._data4x4[2][2] * CopyM._data4x4[2][2]));
        Rotation._data[2] = atan2f(CopyM._data4x4[0][1], CopyM._data4x4[0][0]);

        Translation._data[0] = CopyM.V.Position.x;
        Translation._data[1] = CopyM.V.Position.y;
        Translation._data[2] = CopyM.V.Position.z;
    }

    static void Decompose(const Matrix<T, rows, cols>& M, Vector<T, cols - 1>& Translation, Vector<T, cols - 1>& Rotation, Vector<T, cols - 1>& Scale)
    {
        static_assert(cols == 4);
        Matrix<T, rows, cols> CopyM = M;
        Scale._data[0] = CopyM.V.Right.Length();
        Scale._data[1] = CopyM.V.Up.Length();
        Scale._data[2] = CopyM.V.Dir.Length();

        CopyM.OrthoNormalize();

        Rotation._data[0] = atan2f(CopyM._data4x4[1][2], CopyM._data4x4[2][2]);
        Rotation._data[1] = atan2f(-CopyM._data4x4[0][2], Sqrt(CopyM._data4x4[1][2] * CopyM._data4x4[1][2] + CopyM._data4x4[2][2] * CopyM._data4x4[2][2]));
        Rotation._data[2] = atan2f(CopyM._data4x4[0][1], CopyM._data4x4[0][0]);

        Translation._data[0] = CopyM.V.Position.x;
        Translation._data[1] = CopyM.V.Position.y;
        Translation._data[2] = CopyM.V.Position.z;
    }

    // Disallow bool conversions (without this, they'd happen implicitly via the array conversions)
private:
    operator bool();
};

typedef Matrix<f32, 4, 4> Mat4f;

// Matrix multiplication
template<typename T, int rows, int cols>
Matrix<T, rows, cols> operator*(const Matrix<T, rows, cols>& a, const Matrix<T, rows, cols>& b)
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

template<typename T>
Matrix<T, 4, 4> operator*(const Matrix<T, 4, 4>& a, const Matrix<T, 4, 4>& b)
{
    Matrix<T, 4, 4> out(0);
    // const T         aPtr = a._data4x4;
    // const T         bPtr = b._data4x4;

    out._data4x4[0][0] = a._data4x4[0][0] * b._data4x4[0][0] + a._data4x4[0][1] * b._data4x4[1][0] + a._data4x4[0][2] * b._data4x4[2][0] + a._data4x4[0][3] * b._data4x4[3][0];
    out._data4x4[0][1] = a._data4x4[0][0] * b._data4x4[0][1] + a._data4x4[0][1] * b._data4x4[1][1] + a._data4x4[0][2] * b._data4x4[2][1] + a._data4x4[0][3] * b._data4x4[3][1];
    out._data4x4[0][2] = a._data4x4[0][0] * b._data4x4[0][2] + a._data4x4[0][1] * b._data4x4[1][2] + a._data4x4[0][2] * b._data4x4[2][2] + a._data4x4[0][3] * b._data4x4[3][2];
    out._data4x4[0][3] = a._data4x4[0][0] * b._data4x4[0][3] + a._data4x4[0][1] * b._data4x4[1][3] + a._data4x4[0][2] * b._data4x4[2][3] + a._data4x4[0][3] * b._data4x4[3][3];
    out._data4x4[1][0] = a._data4x4[1][0] * b._data4x4[0][0] + a._data4x4[1][1] * b._data4x4[1][0] + a._data4x4[1][2] * b._data4x4[2][0] + a._data4x4[1][3] * b._data4x4[3][0];
    out._data4x4[1][1] = a._data4x4[1][0] * b._data4x4[0][1] + a._data4x4[1][1] * b._data4x4[1][1] + a._data4x4[1][2] * b._data4x4[2][1] + a._data4x4[1][3] * b._data4x4[3][1];
    out._data4x4[1][2] = a._data4x4[1][0] * b._data4x4[0][2] + a._data4x4[1][1] * b._data4x4[1][2] + a._data4x4[1][2] * b._data4x4[2][2] + a._data4x4[1][3] * b._data4x4[3][2];
    out._data4x4[1][3] = a._data4x4[1][0] * b._data4x4[0][3] + a._data4x4[1][1] * b._data4x4[1][3] + a._data4x4[1][2] * b._data4x4[2][3] + a._data4x4[1][3] * b._data4x4[3][3];
    out._data4x4[2][0] = a._data4x4[2][0] * b._data4x4[0][0] + a._data4x4[2][1] * b._data4x4[1][0] + a._data4x4[2][2] * b._data4x4[2][0] + a._data4x4[2][3] * b._data4x4[3][0];
    out._data4x4[2][1] = a._data4x4[2][0] * b._data4x4[0][1] + a._data4x4[2][1] * b._data4x4[1][1] + a._data4x4[2][2] * b._data4x4[2][1] + a._data4x4[2][3] * b._data4x4[3][1];
    out._data4x4[2][2] = a._data4x4[2][0] * b._data4x4[0][2] + a._data4x4[2][1] * b._data4x4[1][2] + a._data4x4[2][2] * b._data4x4[2][2] + a._data4x4[2][3] * b._data4x4[3][2];
    out._data4x4[2][3] = a._data4x4[2][0] * b._data4x4[0][3] + a._data4x4[2][1] * b._data4x4[1][3] + a._data4x4[2][2] * b._data4x4[2][3] + a._data4x4[2][3] * b._data4x4[3][3];
    out._data4x4[3][0] = a._data4x4[3][0] * b._data4x4[0][0] + a._data4x4[3][1] * b._data4x4[1][0] + a._data4x4[3][2] * b._data4x4[2][0] + a._data4x4[3][3] * b._data4x4[3][0];
    out._data4x4[3][1] = a._data4x4[3][0] * b._data4x4[0][1] + a._data4x4[3][1] * b._data4x4[1][1] + a._data4x4[3][2] * b._data4x4[2][1] + a._data4x4[3][3] * b._data4x4[3][1];
    out._data4x4[3][2] = a._data4x4[3][0] * b._data4x4[0][2] + a._data4x4[3][1] * b._data4x4[1][2] + a._data4x4[3][2] * b._data4x4[2][2] + a._data4x4[3][3] * b._data4x4[3][2];
    out._data4x4[3][3] = a._data4x4[3][0] * b._data4x4[0][3] + a._data4x4[3][1] * b._data4x4[1][3] + a._data4x4[3][2] * b._data4x4[2][3] + a._data4x4[3][3] * b._data4x4[3][3];

    // for (int i = 0; i < 4; i++)
    // {
    //     for (int j = 0; j < 4; ++j)
    //     {
    //         *outPtr = aPtr[0] * bPtr[0 + j] + aPtr[1] * bPtr[4 + j] + aPtr[2] * bPtr[8 + j] + aPtr[3] * bPtr[12 + j];

    //         outPtr++;
    //     }

    //     aPtr += 4;
    // }

    return out;
}

template<typename T, int rows, int cols>
Matrix<T, rows, cols>& operator*=(Matrix<T, rows, cols>& a, const Matrix<T, rows, cols>& b)
{
    a = a * b;
    return a;
}

template<typename T, int rows, int cols>
Vector<T, rows> operator*(const Matrix<T, rows, cols>& a, Vector<T, rows> b)
{
    Vector<T, rows> out((T)0);
    for (int i = 0; i < rows; i++)
        for (int j = 0; j < cols; ++j)
            out[i] += a[i][j] * b[j];

    return out;
}

template<typename T, int rows, int cols>
Vector<T, cols> operator*(Vector<T, cols> b, const Matrix<T, rows, cols>& a)
{
    Vector<T, cols> out((T)0);
    for (int i = 0; i < rows; i++)
        for (int j = 0; j < cols; ++j)
            out[i] += a[i][j] * b[j];

    return out;
}

template<typename T, int n>
Vector<T, n> operator*=(Vector<T, n>& a, const Matrix<T, n, n>& b)
{
    a = a * b;
    return a;
}

template<typename T, int rows, int cols>
Matrix<T, rows, cols> Transpose(const Matrix<T, rows, cols>& in)
{
    Matrix<T, rows, cols> out;
    for (int i = 0; i < rows; i++)
        for (int j = 0; j < cols; j++)
            out[j][i] = in[i][j];

    return out;
}

template<typename T, int n>
Matrix<T, n + 1, n + 1> TranslationMatrix(Vector<T, n> position)
{
    Matrix<T, n + 1, n + 1> out(Identity);
    for (int i = 0; i < n; i++)
    {
        out[n][i] = position[i];
    }

    return out;
}

template<typename T, int n>
Matrix<T, n + 1, n + 1> ScaleMatrix(Vector<T, n> scale)
{
    Matrix<T, n + 1, n + 1> out(Identity);
    out._data[0] = scale[0];
    out._data[5] = scale[1];
    out._data[10] = scale[2];

    return out;
}

template<typename T>
Matrix<T, 2, 2> Inverse(const Matrix<T, 2, 2>& in)
{
    T determinant = (in[0][0] * in[1][1] - in[0][1] * in[1][0]);
    return Matrix<T, 2, 2>{ +in[1][1], -in[0][1], -in[1][0], +in[0][0] } / determinant;
}

template<typename T>
Matrix<T, 4, 4> Inverse(const Matrix<T, 4, 4>& Input)
{
    // Calculate inverse using Gaussian elimination
    Matrix<T, 4, 4> a = Input;
    Matrix<T, 4, 4> Output(Identity);

    // Loop through columns
    for (int j = 0; j < 4; ++j)
    {
        // Select pivot element: maximum magnitude in this column at or below main diagonal
        int pivot = j;
        for (int i = j + 1; i < 4; ++i)
            if (Abs(a._data4x4[i][j]) > Abs(a._data4x4[pivot][j]))
                pivot = i;

        // If we couldn't find a "sufficiently nonzero" element, the matrix is singular.
        if (Abs(a._data4x4[pivot][j]) < math::Epsilon)
            assert(false);

        // Interchange rows to put pivot element on the diagonal,
        // if it is not already there
        if (pivot != j)
        {
            for (int k = 0; k < 4; k++)
            {
                math::Swap(a._data4x4[j][k], a._data4x4[pivot][k]);
                math::Swap(Output._data4x4[j][k], Output._data4x4[pivot][k]);
            }
            // Swap(a[j], a[pivot]);
            // Swap(Output[j], Output[pivot]);
        }

        // Divide the whole row by the pivot element
        if (a._data4x4[j][j] != T(1)) // Skip if already equal to 1
        {
            T scale = a[j][j];
            for (int k = 0; k < 4; k++)
            {
                a._data4x4[j][k] /= scale;
                Output._data4x4[j][k] /= scale;
            }
            // Now the pivot element has become 1
        }

        // Subtract this row from others to make the rest of column j zero
        for (int i = 0; i < 4; ++i)
        {
            if ((i != j) && (abs(a[i][j]) > math::Epsilon)) // Skip rows with zero already in this column
            {
                T scale = -a[i][j];
                for (int k = 0; k < 4; k++)
                {
                    a._data4x4[i][k] += a._data4x4[j][k] * scale;
                    Output._data4x4[i][k] += Output._data4x4[j][k] * scale;
                }
            }
        }
    }

    return Output;

// No idea where this came from:
#if 0
    const f32* m = in._data;

    f32 t0 = m[10] * m[15];
    f32 t1 = m[14] * m[11];
    f32 t2 = m[6] * m[15];
    f32 t3 = m[14] * m[7];
    f32 t4 = m[6] * m[11];
    f32 t5 = m[10] * m[7];
    f32 t6 = m[2] * m[15];
    f32 t7 = m[14] * m[3];
    f32 t8 = m[2] * m[11];
    f32 t9 = m[10] * m[3];
    f32 t10 = m[2] * m[7];
    f32 t11 = m[6] * m[3];
    f32 t12 = m[8] * m[13];
    f32 t13 = m[12] * m[9];
    f32 t14 = m[4] * m[13];
    f32 t15 = m[12] * m[5];
    f32 t16 = m[4] * m[9];
    f32 t17 = m[8] * m[5];
    f32 t18 = m[0] * m[13];
    f32 t19 = m[12] * m[1];
    f32 t20 = m[0] * m[9];
    f32 t21 = m[8] * m[1];
    f32 t22 = m[0] * m[5];
    f32 t23 = m[4] * m[1];

    Matrix<T, 4, 4>    out;
    f32* o = out._data;

    o[0] = (t0 * m[5] + t3 * m[9] + t4 * m[13]) - (t1 * m[5] + t2 * m[9] + t5 * m[13]);
    o[1] = (t1 * m[1] + t6 * m[9] + t9 * m[13]) - (t0 * m[1] + t7 * m[9] + t8 * m[13]);
    o[2] = (t2 * m[1] + t7 * m[5] + t10 * m[13]) - (t3 * m[1] + t6 * m[5] + t11 * m[13]);
    o[3] = (t5 * m[1] + t8 * m[5] + t11 * m[9]) - (t4 * m[1] + t9 * m[5] + t10 * m[9]);

    f32 d = 1.0f / (m[0] * o[0] + m[4] * o[1] + m[8] * o[2] + m[12] * o[3]);

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
#endif
}

template<typename T, int rows, int cols>
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

template<typename T, int N>
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
KRAFT_API Mat4f OrthographicMatrix(f32 left, f32 right, f32 top, f32 bottom, f32 nearClip, f32 farClip);
KRAFT_API Mat4f PerspectiveMatrix(f32 fieldOfViewRadians, f32 aspectRatio, f32 nearClip, f32 farClip);

// Directional
KRAFT_INLINE static Vec3f ForwardVector(const Mat4f& in)
{
    Vec3f out;
    out.x = -in._data[2];
    out.y = -in._data[6];
    out.z = -in._data[10];

    Normalize(&out);

    return out;
}

template<typename T, int n>
static Vector<T, n> ForwardVector(const Matrix<T, n + 1, n + 1>& in)
{
    Vector<T, n> out;
    for (int i = 0; i < n; i++)
        out[i] = -in._data[(n + 1) * i + 2];

    Normalize(&out);

    return out;
}

KRAFT_INLINE static Vec3f BackwardVector(const Mat4f& in)
{
    Vec3f out;
    out.x = in._data[2];
    out.y = in._data[6];
    out.z = in._data[10];

    Normalize(&out);

    return out;
}

template<typename T, int n>
Vector<T, n> BackwardVector(const Matrix<T, n + 1, n + 1>& in)
{
    Vector<T, n> out;
    for (int i = 0; i < n; i++)
        out[i] = in._data[(n + 1) * i + 2];

    Normalize(&out);

    return out;
}

KRAFT_INLINE static Vec3f UpVector(const Mat4f& in)
{
    Vec3f out;
    out.x = in._data[1];
    out.y = in._data[5];
    out.z = in._data[9];

    Normalize(&out);

    return out;
}

KRAFT_INLINE static Vec3f DownVector(const Mat4f& in)
{
    Vec3f out;
    out.x = -in._data[1];
    out.y = -in._data[5];
    out.z = -in._data[9];

    Normalize(&out);

    return out;
}

KRAFT_INLINE static Vec3f LeftVector(const Mat4f& in)
{
    Vec3f out;
    out.x = -in._data[0];
    out.y = -in._data[4];
    out.z = -in._data[8];

    Normalize(&out);

    return out;
}

KRAFT_INLINE static Vec3f RightVector(const Mat4f& in)
{
    Vec3f out;
    out.x = in._data[0];
    out.y = in._data[4];
    out.z = in._data[8];

    Normalize(&out);

    return out;
}

// Rotation
KRAFT_API Mat4f RotationMatrixX(f32 angleRadians);
KRAFT_API Mat4f RotationMatrixY(f32 angleRadians);
KRAFT_API Mat4f RotationMatrixZ(f32 angleRadians);
KRAFT_API Mat4f RotationMatrixFromEulerAngles(Vec3f euler);
KRAFT_API Mat4f RotationMatrixFromEulerAngles(f32 rotationXRadians, f32 rotationYRadians, f32 rotationZRadians);

KRAFT_API Mat4f LookAt(Vec3f Eye, Vec3f Center, Vec3f Up);

} // namespace kraft