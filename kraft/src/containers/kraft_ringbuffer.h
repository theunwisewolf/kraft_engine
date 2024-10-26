#pragma once

#include <core/kraft_core.h>
#include <core/kraft_memory.h>

namespace kraft
{

using ValueType = int;

struct RingBuffer
{
private:
    typedef uint64 SizeType;

    ValueType   *InternalBuffer = nullptr;

    // Number of elements currently in the buffer
    SizeType    Length = 0;

    // Number of elements that can fit in the buffer
    SizeType    Allocated = 0;

protected:
    constexpr KRAFT_INLINE static SizeType ChooseAllocationSize(SizeType Size)
    {
        return 2 * Size;
    }

    constexpr KRAFT_INLINE SizeType GetBufferSizeInBytes()
    {
        return Allocated * sizeof(ValueType);
    }

    constexpr void EnlargeBufferIfRequired(SizeType NewSize, bool ExactFit = false)
    {
        if (NewSize <= Allocated)
        {
            return;
        }

        ValueType* OldBuffer = Data();
        SizeType OldAllocationSize = GetBufferSizeInBytes();
        
        Allocated = ExactFit ? NewSize : ChooseAllocationSize(NewSize);
        ValueType* NewBuffer = (ValueType*)Malloc(GetBufferSizeInBytes(), MEMORY_TAG_ARRAY, true);
        
        // If we had any data, copy over
        if (OldBuffer)
        {
            MemCpy(NewBuffer, OldBuffer, Length * sizeof(ValueType));
            Free(OldBuffer, OldAllocationSize, MEMORY_TAG_ARRAY);
        }

        InternalBuffer = NewBuffer;
    }

    constexpr void ReallocIfRequired(SizeType NewSize)
    {
        if (NewSize <= Allocated)
        {
            return;
        }

        Free(Data(), GetBufferSizeInBytes(), MEMORY_TAG_ARRAY);
        Alloc(ChooseAllocationSize(NewSize));
    }

public:
    constexpr const ValueType* Data() const noexcept
    {
        return InternalBuffer;
    }

    constexpr ValueType* Data() noexcept
    {
        return InternalBuffer;
    }

    KRAFT_INLINE void Alloc(SizeType Size)
    {
        if (Size > Allocated)
        {
            Allocated = Size;
            InternalBuffer = (ValueType*)Malloc(GetBufferSizeInBytes(), MEMORY_TAG_ARRAY, true);
        }
    }
};

}