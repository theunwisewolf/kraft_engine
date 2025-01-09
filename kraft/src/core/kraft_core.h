#pragma once

// Platform detection
#if defined(WIN32) || defined(_WIN32)
#define KRAFT_PLATFORM_WINDOWS
#elif __linux__
#define KRAFT_PLATFORM_LINUX
#elif __ANDROID__
#define KRAFT_PLATFORM_ANDROID
#elif __APPLE__ // This might look wrong, but this is the correct way!
#include <TargetConditionals.h>
#if TARGET_IPHONE_SIMULATOR
#define KRAFT_PLATFORM_IOS
#define KRAFT_PLATFORM_IOS_SIMULATOR
#elif TARGET_OS_IPHONE
#define KRAFT_PLATFORM_IOS
#elif TARGET_OS_MAC
#define KRAFT_PLATFORM_MACOS
#else
#error "Unknown apple platform"
#endif
#endif

// Debug
#if defined(_DEBUG) || defined(DEBUG)
#define KRAFT_DEBUG
#define KRAFT_RENDERER_DEBUG
#endif

// Comment out for disabling debugging features in the renderer
#define KRAFT_RENDERER_DEBUG

// Compiler
#if defined(_MSC_VER)
#define KRAFT_COMPILER_MSVC
#elif defined(__clang__)
#define KRAFT_COMPILER_CLANG
#elif defined(__gcc__)
#define KRAFT_COMPILER_GCC
#endif

// Inlining
#if defined(_MSC_VER)
#define KRAFT_INLINE   __forceinline
#define KRAFT_NOINLINE __declspec(noinline)
#elif defined(__clang__) || defined(__gcc__)
#define KRAFT_INLINE   __attribute__((always_inline)) inline
#define KRAFT_NOINLINE __attribute__((noinline))
#else
#define KRAFT_INLINE inline
#define KRAFT_NOINLINE
#endif

// Library export
#if defined(KRAFT_STATIC)
#define KRAFT_API
#elif defined(KRAFT_SHARED)
#ifdef _MSC_VER
#define KRAFT_API __declspec(dllexport)
#else
#define KRAFT_API __attribute__((visibility("default")))
#endif
#else
#ifdef _MSC_VER
#define KRAFT_API __declspec(dllimport)
#else
#define KRAFT_API
#endif
#endif

// Taken from rapidjson's src
// Endianness - Required by some third-party code
#define KRAFT_BYTEORDER_LITTLE_ENDIAN 0 // Little endian machine.
#define KRAFT_BYTEORDER_BIG_ENDIAN    1 // Big endian machine.

#ifndef KRAFT_BYTEORDER_ENDIANNESS
// Detect with GCC 4.6's macro.
#if defined(__BYTE_ORDER__)
#if (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#define KRAFT_BYTEORDER_ENDIANNESS KRAFT_BYTEORDER_LITTLE_ENDIAN
#elif (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#define KRAFT_BYTEORDER_ENDIANNESS KRAFT_BYTEORDER_BIG_ENDIAN
#else
#error "Unknown machine byteorder endianness detected. User needs to define KRAFT_BYTEORDER_ENDIANNESS."
#endif
// Detect with GLIBC's endian.h.
#elif defined(__GLIBC__)
#include <endian.h>
#if (__BYTE_ORDER == __LITTLE_ENDIAN)
#define KRAFT_BYTEORDER_ENDIANNESS KRAFT_BYTEORDER_LITTLE_ENDIAN
#elif (__BYTE_ORDER == __BIG_ENDIAN)
#define KRAFT_BYTEORDER_ENDIANNESS KRAFT_BYTEORDER_BIG_ENDIAN
#else
#error "Unknown machine byteorder endianness detected. User needs to define KRAFT_BYTEORDER_ENDIANNESS."
#endif
// Detect with _LITTLE_ENDIAN and _BIG_ENDIAN macro.
#elif defined(_LITTLE_ENDIAN) && !defined(_BIG_ENDIAN)
#define KRAFT_BYTEORDER_ENDIANNESS KRAFT_BYTEORDER_LITTLE_ENDIAN
#elif defined(_BIG_ENDIAN) && !defined(_LITTLE_ENDIAN)
#define KRAFT_BYTEORDER_ENDIANNESS KRAFT_BYTEORDER_BIG_ENDIAN
// Detect with architecture macros.
#elif defined(__sparc) || defined(__sparc__) || defined(_POWER) || defined(__powerpc__) || defined(__ppc__) || defined(__hpux) ||          \
    defined(__hppa) || defined(_MIPSEB) || defined(_POWER) || defined(__s390__)
#define KRAFT_BYTEORDER_ENDIANNESS KRAFT_BYTEORDER_BIG_ENDIAN
#elif defined(__i386__) || defined(__alpha__) || defined(__ia64) || defined(__ia64__) || defined(_M_IX86) || defined(_M_IA64) ||           \
    defined(_M_ALPHA) || defined(__amd64) || defined(__amd64__) || defined(_M_AMD64) || defined(__x86_64) || defined(__x86_64__) ||        \
    defined(_M_X64) || defined(__bfin__)
#define KRAFT_BYTEORDER_ENDIANNESS KRAFT_BYTEORDER_LITTLE_ENDIAN
#elif defined(_MSC_VER) && (defined(_M_ARM) || defined(_M_ARM64))
#define KRAFT_BYTEORDER_ENDIANNESS KRAFT_BYTEORDER_LITTLE_ENDIAN
#else
#error "Unknown machine byteorder endianness detected. User needs to define KRAFT_BYTEORDER_ENDIANNESS."
#endif
#endif

#if KRAFT_BYTEORDER_ENDIANNESS == KRAFT_BYTEORDER_LITTLE_ENDIAN
#define KRAFT_PLATFORM_LITTLE_ENDIAN
#else
#define KRAFT_PLATFORM_BIG_ENDIAN
#endif

// Some types
#include <cstddef>
#include <cstdint>

typedef double   float64;
typedef double   Float64;
typedef float    float32;
typedef float    Float32;
typedef int8_t   int8;
typedef int8_t   Int8;
typedef int16_t  int16;
typedef int16_t  Int16;
typedef int      int32;
typedef int      Int32;
typedef int64_t  int64;
typedef int64_t  Int64;
typedef uint8_t  byte;
typedef uint8_t  Byte;
typedef uint8_t  uint8;
typedef uint8_t  UInt8;
typedef uint16_t uint16;
typedef uint16_t UInt16;
typedef uint32_t uint32;
typedef uint32_t UInt32;
typedef uint64_t uint64;
typedef uint64_t UInt64;

// TODO (amn): Unicode
// #ifdef _UNICODE
//     typedef wchar_t TCHAR;
//     #define TEXT(str) L ## #str
//     #ifdef KRAFT_PLATFORM_WINDOWS
//         #define StrError _wcserror
//     #else
//         #define StrError strerror
//     #endif
// #else
#define StrError strerror
// #endif

#define KRAFT_INVALID_ID       4294967295U
#define KRAFT_INVALID_ID_UINT8 255U

#define KRAFT_C_ARRAY_SIZE(arr) sizeof(arr) / sizeof(arr[0])

// Architecture
#if _WIN32 || _WIN64
#if _WIN64
#define KRAFT_64BIT
#else
#define KRAFT_32BIT
#endif
#endif

// Check GCC
#if __GNUC__
#if __x86_64__ || __ppc64__
#define KRAFT_64BIT
#else
#define KRAFT_32BIT
#endif
#endif

// Some forward declares for most-used types
namespace kraft {
template<typename T, uint64 InternalBufferSize>
struct KString;

template<typename T>
struct KStringView;

typedef KString<char, 128> String;
typedef KStringView<char>  StringView;

template<typename T>
struct Array;

template<typename T, int rows, int cols>
struct Matrix;

template<typename T, int n>
struct Vector;

typedef Vector<float32, 4> Vec4f;
}
