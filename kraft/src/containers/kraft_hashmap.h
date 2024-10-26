#pragma once

#include "core/kraft_memory.h"

// TODO:
// #define EXLBR_ALLOC(sizeInBytes, alignment)
// #define EXLBR_FREE

#include "ExcaliburHash/ExcaliburHash.h"
#include "parallel_hashmap/phmap.h"

namespace kraft {

template<typename KeyType, typename ValueType>
using HashMap = Excalibur::HashMap<KeyType, ValueType>;

template<typename KeyType>
using HashSet = Excalibur::HashSet<KeyType>;

template<typename KeyType, typename ValueType>
using FlatHashMap = phmap::flat_hash_map<KeyType, ValueType>;

}

namespace std {
template<>
struct hash<kraft::String>
{
    std::size_t operator()(kraft::String const& Key) const
    {
        return FNV1AHashBytes((const unsigned char*)Key.Data(), Key.GetLengthInBytes());
    }
};
}

namespace Excalibur {

template<>
struct KeyInfo<kraft::String>
{
    static inline bool          isValid(const kraft::String& Key) noexcept { return !Key.Empty() && Key.Data()[0] != char(1); }
    static inline kraft::String getTombstone() noexcept { return kraft::String(1, char(1)); }

    static inline kraft::String getEmpty() noexcept { return kraft::String(); }
    static inline size_t        hash(const kraft::String& Key) noexcept
    {
        return FNV1AHashBytes((const unsigned char*)Key.Data(), Key.GetLengthInBytes());
    }

    static inline bool isEqual(const kraft::String& lhs, const kraft::String& rhs) noexcept { return lhs == rhs; }
};

}