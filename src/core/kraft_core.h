#pragma once

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

#if defined(_DEBUG) || defined(DEBUG)
    #define KRAFT_DEBUG
#endif

// Some types
#include <cstdint>

typedef double      float64;
typedef float       float32;
typedef int8_t      int8;
typedef int16_t     int16;
typedef int         int32;
typedef int64_t     int64;
typedef uint8_t     byte;
typedef uint8_t     uint8;
typedef uint16_t    uint16;
typedef uint32_t    uint32;
typedef uint64_t    uint64;
