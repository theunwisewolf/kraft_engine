#pragma once

#include "core/kraft_string.h"
#include "core/kraft_hash.h"

#include "wyhash.h"

namespace Excalibur
{

// generic type (without implementation)
template <typename T> struct KeyInfo
{
    //    static inline T getTombstone() noexcept;
    //    static inline T getEmpty() noexcept;
    //    static inline size_t hash(const T& key) noexcept;
    //    static inline bool isEqual(const T& lhs, const T& rhs) noexcept;
    //    static inline bool isValid(const T& key) noexcept;
};

template <> struct KeyInfo<int32_t>
{
    static inline bool isValid(const int32_t& key) noexcept { return key < INT32_C(0x7ffffffe); }
    static inline int32_t getTombstone() noexcept { return INT32_C(0x7fffffff); }
    static inline int32_t getEmpty() noexcept { return INT32_C(0x7ffffffe); }
    static inline size_t hash(const int32_t& key) noexcept { return Excalibur::wyhash::hash(key); }
    static inline bool isEqual(const int32_t& lhs, const int32_t& rhs) noexcept { return lhs == rhs; }
};

template <> struct KeyInfo<uint32_t>
{
    static inline bool isValid(const uint32_t& key) noexcept { return key < UINT32_C(0xfffffffe); }
    static inline uint32_t getTombstone() noexcept {return UINT32_C(0xfffffffe); }
    static inline uint32_t getEmpty() noexcept {return UINT32_C(0xffffffff); }
    static inline size_t hash(const uint32_t& key) noexcept { return Excalibur::wyhash::hash(key); }
    static inline bool isEqual(const uint32_t& lhs, const uint32_t& rhs) noexcept { return lhs == rhs; }
};

template <> struct KeyInfo<int64_t>
{
    static inline bool isValid(const int64_t& key) noexcept { return key < INT64_C(0x7ffffffffffffffe); }
    static inline int64_t getTombstone() noexcept { return INT64_C(0x7fffffffffffffff); }
    static inline int64_t getEmpty() noexcept { return INT64_C(0x7ffffffffffffffe); }
    static inline size_t hash(const int64_t& key) noexcept { return Excalibur::wyhash::hash(key); }
    static inline bool isEqual(const int64_t& lhs, const int64_t& rhs) noexcept { return lhs == rhs; }
};

template <> struct KeyInfo<uint64_t>
{
    static inline bool isValid(const uint64_t& key) noexcept { return key < UINT64_C(0xfffffffffffffffe); }
    static inline uint64_t getTombstone() noexcept { return UINT64_C(0xfffffffffffffffe); }
    static inline uint64_t getEmpty() noexcept { return UINT64_C(0xffffffffffffffff); }
    static inline size_t hash(const uint64_t& key) noexcept { return Excalibur::wyhash::hash(key); }
    static inline bool isEqual(const uint64_t& lhs, const uint64_t& rhs) noexcept { return lhs == rhs; }
};

template <> struct KeyInfo<kraft::String>
{
    static inline bool isValid(const kraft::String& Key) noexcept { return !Key.Empty() && Key.Data()[0] != char(1); }
    static inline kraft::String getTombstone() noexcept
    {
        return kraft::String(1, char(1));
    }
    
    static inline kraft::String getEmpty() noexcept { return kraft::String(); }
    static inline size_t hash(const kraft::String& Key) noexcept 
    {
        return FNV1AHashBytes((const unsigned char*)Key.Data(), Key.GetLengthInBytes());
    }

    static inline bool isEqual(const kraft::String& lhs, const kraft::String& rhs) noexcept { return lhs == rhs; }
};


} // namespace Excalibur
