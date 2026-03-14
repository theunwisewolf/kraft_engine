#pragma once

#include "parallel_hashmap/phmap.h"

namespace kraft {

template<typename KeyType, typename ValueType>
using FlatHashMap = phmap::flat_hash_map<KeyType, ValueType>;

template<typename ValueType>
using FlatHashSet = phmap::flat_hash_set<ValueType>;

} // namespace kraft

namespace std {

template<>
struct hash<String8>
{
    std::size_t operator()(String8 const& key) const
    {
        return kraft::FNV1AHashBytes(key.ptr, key.count);
    }
};
} // namespace std
