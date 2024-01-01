#pragma once

#include "core/kraft_core.h"
#include "core/kraft_memory.h"

namespace kraft
{

template<typename ValueType>
struct Array
{
    typedef uint64 SizeType;

    ValueType   *Buffer = nullptr;
    SizeType    Length = 0;
    SizeType    Allocated = 0;

    Array(std::nullptr_t)
    {
        Length = 0;
        Allocated = 0;
    }

    Array()
    {
        Length = 0;
        Allocated = 0;
    }

    Array(SizeType Size)
    {
        Length = Size;
        Allocated = Size;
        Alloc(Allocated);
        for (int i = 0; i < Length; i++)
        {
            Buffer[i] = ValueType();
        }
    }

    Array(SizeType Size, ValueType Value)
    {
        Length = Size;
        Allocated = Size;
        Alloc(Allocated);
        for (int i = 0; i < Length; i++)
        {
            Buffer[i] = Value;
        }
    }

    constexpr const ValueType* Data() const noexcept
    {
        return Buffer;
    }

    constexpr ValueType* Data() noexcept
    {
        return Buffer;
    }

    constexpr ValueType& operator[](SizeType Index)
    {
        return Data()[Index];
    }

    constexpr const ValueType& operator[](SizeType Index) const
    {
        return Data()[Index];
    }

    inline SizeType Size() const
    {
        return Length;
    }

    KRAFT_INLINE void Alloc(SizeType Size)
    {
        Buffer = (ValueType*)Malloc(Allocated * sizeof(ValueType), MEMORY_TAG_ARRAY, true);
    }
};

}