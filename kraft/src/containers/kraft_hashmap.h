#pragma once

#include "core/kraft_memory.h"

// TODO: 
// #define EXLBR_ALLOC(sizeInBytes, alignment)
// #define EXLBR_FREE

#include "ExcaliburHash/ExcaliburHash.h"

namespace kraft
{

template <typename KeyType, typename ValueType>
using HashMap = Excalibur::HashMap<KeyType, ValueType>;

template <typename KeyType>
using HashSet = Excalibur::HashSet<KeyType>;

}