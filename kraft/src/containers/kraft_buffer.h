#pragma once

#include "core/kraft_core.h"
#include "core/kraft_string.h"
#include "core/kraft_memory.h"
#include "core/kraft_string_view.h"
#include "containers/kraft_array.h"

#include <type_traits>

namespace kraft
{

#define WRITE_TYPE(Type)                                    \
    KRAFT_INLINE void Write(Type Value)                     \
    {                                                       \
        EnlargeBufferIfRequired(Length + sizeof(Type));     \
        MemCpy(Data() + Length, &Value, sizeof(Type));      \
        Length += sizeof(Type);                             \
    }

#define READ_TYPE(Type)                                     \
    KRAFT_INLINE void Read(Type* Value)                     \
    {                                                       \
        MemCpy(Value, Data() + Length, sizeof(Type));       \
        Length += sizeof(Type);                             \
    }

struct Buffer
{
    using SizeType  = uint64;
    using ValueType = char;

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
    Buffer()
    {
        this->InternalBuffer = nullptr;
        this->Length = 0;
    }

    Buffer(SizeType Size)
    {
        this->InternalBuffer = (char*)Malloc(Size, MEMORY_TAG_BUFFER, true);
        this->Length = 0;
        this->Allocated = Size;
    }

    Buffer(ValueType* In, SizeType Size)
    {
        this->InternalBuffer = In;
        this->Length = 0;
        this->Allocated = Size;
    }

    KRAFT_INLINE void Alloc(SizeType Size)
    {
        InternalBuffer = (ValueType*)Malloc(GetBufferSizeInBytes(), MEMORY_TAG_ARRAY, true);
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

    WRITE_TYPE(uint8);
    WRITE_TYPE(uint16);
    WRITE_TYPE(uint32);
    WRITE_TYPE(uint64);
    WRITE_TYPE(int8);
    WRITE_TYPE(int16);
    WRITE_TYPE(int32);
    WRITE_TYPE(int64);
    WRITE_TYPE(float32);
    WRITE_TYPE(float64);
    WRITE_TYPE(bool);

    KRAFT_INLINE void Write(const String& Value)
    {
        EnlargeBufferIfRequired(Length + Value.GetLengthInBytes() + sizeof(uint64));
        
        // Write the length
        MemCpy(Data() + Length, &Value.Length, sizeof(uint64));
        Length += sizeof(uint64);

        MemCpy(Data() + Length, *Value, Value.GetLengthInBytes());
        Length += Value.GetLengthInBytes();
    }

    KRAFT_INLINE void Write(StringView Value)
    {
        EnlargeBufferIfRequired(Length + Value.GetLengthInBytes() + sizeof(uint64));

        // Write the length
        MemCpy(Data() + Length, &Value.Length, sizeof(uint64));
        Length += sizeof(uint64);

        MemCpy(Data() + Length, *Value, Value.GetLengthInBytes());
        Length += Value.GetLengthInBytes();
    }

    KRAFT_INLINE void Write(void* InData, SizeType InDataSize)
    {
        EnlargeBufferIfRequired(Length + InDataSize);
        MemCpy(Data() + Length, InData, InDataSize);
        Length += InDataSize;
    }

    // For trivially copyable types
    template <typename ArrayValueType>
    KRAFT_INLINE typename std::enable_if<std::is_trivially_copyable<ArrayValueType>::value>::type
    Write(Array<ArrayValueType> Value)
    {
        EnlargeBufferIfRequired(Length + Value.GetLengthInBytes() + sizeof(uint64));

        // Write the length
        MemCpy(Data() + Length, &Value.Length, sizeof(uint64));
        Length += sizeof(uint64);

        MemCpy(Data() + Length, &Value[0], Value.GetLengthInBytes());
        Length += Value.GetLengthInBytes();
    }

    template <typename CopyableValueType>
    KRAFT_INLINE typename std::enable_if<std::is_trivially_copyable<CopyableValueType>::value>::type
    Write(CopyableValueType Value)
    {
        EnlargeBufferIfRequired(Length + sizeof(Value));

        MemCpy(Data() + Length, &Value, sizeof(Value));
        Length += sizeof(Value);
    }

    template <typename NonCopyableValueType>
    KRAFT_INLINE typename std::enable_if<!std::is_trivially_copyable<NonCopyableValueType>::value>::type
    Write(NonCopyableValueType Value)
    {
        Value.WriteTo(this);
    }

    template <typename ArrayValueType>
    KRAFT_INLINE typename std::enable_if<!std::is_trivially_copyable<ArrayValueType>::value>::type
    Write(Array<ArrayValueType> Value)
    {
        Write(Value.Length);
        for (int i = 0; i < Value.Length; i++)
        {
            Value[i].WriteTo(this);
        }
    }

    READ_TYPE(uint8);
    READ_TYPE(uint16);
    READ_TYPE(uint32);
    READ_TYPE(uint64);
    READ_TYPE(int8);
    READ_TYPE(int16);
    READ_TYPE(int32);
    READ_TYPE(int64);
    READ_TYPE(float32);
    READ_TYPE(float64);
    READ_TYPE(bool);

    KRAFT_INLINE void Read(String* Value)
    {
        // Read the length
        uint64 ValueLength;
        Read(&ValueLength);

        *Value = String(Data() + Length, ValueLength);
        Length += Value->GetLengthInBytes();
    }

    KRAFT_INLINE void Read(StringView* Value)
    {
        // Read the length
        uint64 ValueLength;
        Read(&ValueLength);

        *Value = StringView(Data() + Length, ValueLength);
        Length += Value->GetLengthInBytes();
    }

    // For trivially copyable types
    template <class ArrayValueType>
    KRAFT_INLINE typename std::enable_if<std::is_trivially_copyable<ArrayValueType>::value>::type
    Read(Array<ArrayValueType>* Value)
    {
        // Read the length
        uint64 ValueLength;
        Read(&ValueLength);

        *Value = Array<ArrayValueType>(ValueLength);
        MemCpy(Value->Data(), Data() + Length, Value->GetLengthInBytes());
        Length += Value->GetLengthInBytes();
    }

    template <class ArrayValueType>
    KRAFT_INLINE typename std::enable_if<!std::is_trivially_copyable<ArrayValueType>::value>::type
    Read(Array<ArrayValueType>* Value)
    {
        // Read the length
        uint64 ValueLength;
        Read(&ValueLength);

        *Value = Array<ArrayValueType>(ValueLength);
        for (int i = 0; i < ValueLength; i++)
        {
            (*Value)[i].ReadFrom(this);
        }
    }

    template <typename CopyableValueType>
    KRAFT_INLINE typename std::enable_if<std::is_trivially_copyable<CopyableValueType>::value>::type
    Read(CopyableValueType* Value)
    {
        MemCpy(Value, Data() + Length, sizeof(CopyableValueType));
        Length += sizeof(CopyableValueType);
    }

    template <typename NonCopyableValueType>
    KRAFT_INLINE typename std::enable_if<!std::is_trivially_copyable<NonCopyableValueType>::value>::type
    Read(NonCopyableValueType* Value)
    {
        Value->ReadFrom(this);
    }
};

#undef WRITE_TYPE
#undef READ_TYPE

}