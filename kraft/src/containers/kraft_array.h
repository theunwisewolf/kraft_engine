#pragma once

#include <core/kraft_asserts.h>
#include <core/kraft_core.h>
#include <core/kraft_memory.h>

#include <initializer_list>

namespace kraft {

template<typename ValueType>
struct Array
{
    typedef uint64 SizeType;

    ValueType* InternalBuffer = nullptr;

    // Number of elements currently in the buffer
    SizeType Length = 0;

    // Number of elements that can fit in the buffer
    SizeType Allocated = 0;

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
        SizeType   OldAllocationSize = GetBufferSizeInBytes();

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

    Array(std::initializer_list<ValueType> List)
    {
        Length = List.size();
        Alloc(Length);

        MemCpy(InternalBuffer, List.begin(), sizeof(ValueType) * Length);
    }

    // Creates an array with the provided size
    // All the elements are default initialized
    Array(SizeType Size)
    {
        Length = Size;
        Alloc(Length);
        for (int i = 0; i < Length; i++)
        {
            InternalBuffer[i] = ValueType();
        }
    }

    Array(SizeType Size, ValueType Value)
    {
        Length = Size;
        Alloc(Length);

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
        SizeType   _Length = Length;
        SizeType   _Allocated = Allocated;
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

    constexpr Array& operator=(Array&& Arr)
    {
        SizeType   _Length = Length;
        SizeType   _Allocated = Allocated;
        ValueType* _Buffer = InternalBuffer;

        Length = Arr.Length;
        InternalBuffer = Arr.InternalBuffer;
        Allocated = Arr.Allocated;

        Arr.Length = _Length;
        Arr.Allocated = _Allocated;
        Arr.InternalBuffer = _Buffer;

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

    // constexpr const ValueType* operator*() const noexcept
    // {
    //     return InternalBuffer;
    // }

    // constexpr ValueType* operator*()
    // {
    //     return InternalBuffer;
    // }

    constexpr ValueType& operator[](SizeType Index)
    {
        KASSERT(Index >= 0 && Index < Length);
        return Data()[Index];
    }

    constexpr const ValueType& operator[](SizeType Index) const
    {
        KASSERT(Index >= 0 && Index < Length);
        return Data()[Index];
    }

    KRAFT_INLINE SizeType Size() const
    {
        return Length;
    }

    KRAFT_INLINE void Alloc(SizeType Size)
    {
        if (Size > Allocated)
        {
            Allocated = Size;
            InternalBuffer = (ValueType*)Malloc(GetBufferSizeInBytes(), MEMORY_TAG_ARRAY, true);
        }
    }

    KRAFT_INLINE void Reserve(SizeType Size)
    {
        EnlargeBufferIfRequired(Size, true);
    }

    KRAFT_INLINE void Resize(SizeType Size)
    {
        EnlargeBufferIfRequired(Size, true);
        Length = Size;
    }

    /// @brief Inserts a new element at the back of the array
    /// @param Value The value to insert
    /// @return Returns the index the value was inserted at
    SizeType Push(const ValueType& Value)
    {
        EnlargeBufferIfRequired(Length + 1);
        InternalBuffer[Length] = Value;
        Length++;

        return Length - 1;
    }

    bool Pop(SizeType Index)
    {
        if (Index >= 0 && Index < Length)
        {
            Data()[Index].~ValueType();
            Data()[Index] = Data()[Length - 1];
            Length--;

            return true;
        }

        return false;
    }

    KRAFT_INLINE uint64 GetLengthInBytes() const noexcept
    {
        return Length * sizeof(ValueType);
    }

    /**
     * Sets the length of the array to Zero
     * Does not release the memory
     */
    KRAFT_INLINE void Clear() noexcept
    {
        for (int i = 0; i < Length; i++)
            Data()[i].~ValueType();

        Length = 0;
    }
};

}