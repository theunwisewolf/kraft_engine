#pragma once

#include "core/kraft_core.h"

// 
// Useful defines
//

#define KRAFT_PI 3.14159265358979323846f

//
// Useful functions
//

KRAFT_API float32 Sin(float32 x);
KRAFT_API float32 Cos(float32 x);
KRAFT_API float32 Tan(float32 x);
KRAFT_API float32 Acos(float32 x);
KRAFT_API float32 Sqrt(float32 x);
KRAFT_API float32 Abs(float32 x);

KRAFT_INLINE float32 DegreeToRadians(float32 degrees) 
{
    return degrees * (KRAFT_PI / 180.0f);
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
    }

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
    return dot(a, a);
}

template <typename T, int n>
T Length(Vector<T, n> a)
{
    return sqrt(lengthSquared(a));
}

template <typename T, int n>
Vector<T, n> Dot(Vector<T, n> a, Vector<T, n> b)
{
    return {
        a.y*b.z - a.z*b.y,
        a.z*b.x - a.x*b.z,
        a.x*b.y - a.y*b.x
    };
}

typedef Vector<float, 2> Vec2f;
typedef Vector<float, 3> Vec3f;
typedef Vector<float, 4> Vec4f;

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

    Vector<T, cols> &operator[](int index)
    {
        return (Vector<T, cols> &)(_data[cols * index]);
    }

    const Vector<T, cols> &operator[](int index) const
    {
        return (const Vector<T, cols> &)(_data[cols * index]);
    }
};

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

typedef Matrix<float32, 4, 4> Mat4f;

KRAFT_API Mat4f OrthographicMatrix(float32 left, float32 right, float32 top, float32 bottom, float32 nearClip, float32 farClip);
KRAFT_API Mat4f PerspectiveMatrix(float32 fieldOfViewRadians, float32 aspectRatio, float32 nearClip, float32 farClip);

}