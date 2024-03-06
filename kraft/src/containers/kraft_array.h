#pragma once

#include "core/kraft_core.h"
#include "core/kraft_memory.h"

namespace kraft
{

template<typename ValueType>
struct Array
{
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

    constexpr void EnlargeBufferIfRequired(SizeType NewSize)
    {
        if (NewSize <= Allocated)
        {
            return;
        }

        ValueType* OldBuffer = Data();
        SizeType OldAllocationSize = GetBufferSizeInBytes();
        
        Allocated = ChooseAllocationSize(NewSize);
        ValueType* NewBuffer = (ValueType*)Malloc(GetBufferSizeInBytes(), MEMORY_TAG_ARRAY, true);
        MemCpy(NewBuffer, OldBuffer, Length * sizeof(ValueType));
        InternalBuffer = NewBuffer;

        Free(OldBuffer, OldAllocationSize, MEMORY_TAG_ARRAY);
    }

    constexpr void ReallocIfRequired(SizeType NewSize)
    {
        if (NewSize <= Allocated)
        {
            return;
        }

        Free(Data(), GetBufferSizeInBytes(), MEMORY_TAG_ARRAY);

        Allocated = ChooseAllocationSize(NewSize);
        Alloc(Allocated);
    }

public:
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
            InternalBuffer[i] = ValueType();
        }
    }

    Array(SizeType Size, ValueType Value)
    {
        Length = Size;
        Allocated = Size;
        Alloc(Allocated);
        for (int i = 0; i < Length; i++)
        {
            InternalBuffer[i] = Value;
        }
    }

    Array(const Array& Arr)
    {
        Length = Arr.Length;
        ReallocIfRequired(Length);

        MemCpy(Data(), Arr.Data(), Length * sizeof(ValueType));
    }

    constexpr Array(Array&& Arr) noexcept
    {
        SizeType _Length = Length;
        SizeType _Allocated = Allocated;
        ValueType* _Buffer = InternalBuffer;

        Length = Arr.Length;
        InternalBuffer = Arr.InternalBuffer;
        Allocated = Arr.Allocated;

        Arr.Length = _Length;
        Arr.Allocated = _Allocated;
        Arr.InternalBuffer = _Buffer;
    }

    constexpr Array& operator=(const Array& Arr)
    {
        if (&Arr == this)
        {
            return *this;
        }

        Length = Arr.Length;
        ReallocIfRequired(Length);

        MemCpy(Data(), Arr.Data(), Length * sizeof(ValueType));

        return *this;
    }

    ~Array()
    {
        Free(InternalBuffer, GetBufferSizeInBytes(), MEMORY_TAG_ARRAY);
    }

    constexpr const ValueType* Data() const noexcept
    {
        return InternalBuffer;
    }

    constexpr ValueType* Data() noexcept
    {
        return InternalBuffer;
    }

    constexpr const ValueType* operator*() const noexcept
    {
        return InternalBuffer;
    }

    constexpr ValueType* operator*()
    {
        return InternalBuffer;
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
        InternalBuffer = (ValueType*)Malloc(GetBufferSizeInBytes(), MEMORY_TAG_ARRAY, true);
    }

    void Push(const ValueType& Value)
    {
        EnlargeBufferIfRequired(Length + 1);
        InternalBuffer[Length] = Value;
        Length++;
    }

    KRAFT_INLINE uint64 GetLengthInBytes() const noexcept
    {
        return Length * sizeof(ValueType);
    }
};

}