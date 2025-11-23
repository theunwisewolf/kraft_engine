#pragma once

#include "containers/kraft_array.h"
#include "core/kraft_core.h"
#include "core/kraft_memory.h"
#include "core/kraft_string.h"
#include "core/kraft_string_view.h"
#include "core/kraft_allocators.h"

#include <type_traits>

namespace kraft {

#define READ_TYPE(Type)                                                                                                                                                                                \
    KRAFT_INLINE void Read(Type* Value)                                                                                                                                                                \
    {                                                                                                                                                                                                  \
        MemCpy(Value, Data() + Length, sizeof(Type));                                                                                                                                                  \
        Length += sizeof(Type);                                                                                                                                                                        \
    }

#define READ_TYPE_RETURN(Type)                                                                                                                                                                         \
    KRAFT_INLINE Type Read##Type()                                                                                                                                                                     \
    {                                                                                                                                                                                                  \
        Type value;                                                                                                                                                                                    \
        MemCpy(&value, Data() + Length, sizeof(Type));                                                                                                                                                 \
        Length += sizeof(Type);                                                                                                                                                                        \
        return value;                                                                                                                                                                                  \
    }

#define WRITE_TYPE(Type)                                                                                                                                                                               \
    KRAFT_INLINE void Write##Type(Type value)                                                                                                                                                          \
    {                                                                                                                                                                                                  \
        EnlargeBufferIfRequired(Length + sizeof(Type));                                                                                                                                                \
        MemCpy(Data() + Length, &value, sizeof(Type));                                                                                                                                                 \
        Length += sizeof(Type);                                                                                                                                                                        \
    }

#define KRAFT_BUFFER_SERIALIZE_START                                                                                                                                                                   \
    void WriteTo(Buffer* Out)                                                                                                                                                                          \
    {
#define KRAFT_BUFFER_SERIALIZE_ADD(Field) Out->Write(Field)
#define KRAFT_BUFFER_SERIALIZE_END        }

#define KRAFT_BUFFER_DESERIALIZE_START                                                                                                                                                                 \
    void ReadFrom(const Buffer* In)                                                                                                                                                                    \
    {
#define KRAFT_BUFFER_DESERIALIZE_ADD(Field) In->Read(&Field)
#define KRAFT_BUFFER_DESERIALIZE_END        }

#define KRAFT_BUFFER_SERIALIZE_1(_1)                                                                                                                                                                   \
    KRAFT_BUFFER_SERIALIZE_START                                                                                                                                                                       \
    KRAFT_BUFFER_SERIALIZE_ADD(_1);                                                                                                                                                                    \
    KRAFT_BUFFER_SERIALIZE_END

#define KRAFT_BUFFER_SERIALIZE_2(_1, _2)                                                                                                                                                               \
    KRAFT_BUFFER_SERIALIZE_START                                                                                                                                                                       \
    KRAFT_BUFFER_SERIALIZE_ADD(_1);                                                                                                                                                                    \
    KRAFT_BUFFER_SERIALIZE_ADD(_2);                                                                                                                                                                    \
    KRAFT_BUFFER_SERIALIZE_END

#define KRAFT_BUFFER_SERIALIZE_3(_1, _2, _3)                                                                                                                                                           \
    KRAFT_BUFFER_SERIALIZE_START                                                                                                                                                                       \
    KRAFT_BUFFER_SERIALIZE_ADD(_1);                                                                                                                                                                    \
    KRAFT_BUFFER_SERIALIZE_ADD(_2);                                                                                                                                                                    \
    KRAFT_BUFFER_SERIALIZE_ADD(_3);                                                                                                                                                                    \
    KRAFT_BUFFER_SERIALIZE_END

struct Buffer
{
    using SizeType = uint64;
    using ValueType = char;

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

    constexpr void EnlargeBufferIfRequired(SizeType NewSize)
    {
        if (NewSize <= Allocated)
        {
            return;
        }

        ValueType* OldBuffer = Data();
        SizeType   OldAllocationSize = GetBufferSizeInBytes();

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

    WRITE_TYPE(u8);
    WRITE_TYPE(u16);
    WRITE_TYPE(u32);
    WRITE_TYPE(u64);
    WRITE_TYPE(i8);
    WRITE_TYPE(i16);
    WRITE_TYPE(i32);
    WRITE_TYPE(i64);
    WRITE_TYPE(f32);
    WRITE_TYPE(f64);
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

    KRAFT_INLINE void WriteArray(void* data, u32 element_size, u32 element_count)
    {
        u64 data_size = ((u64)element_size * (u64)element_count);
        EnlargeBufferIfRequired(Length + data_size + sizeof(u32));

        // Write element count
        MemCpy(Data() + Length, (void*)&element_count, sizeof(u32));
        Length += sizeof(u32);

        // Write the data
        MemCpy(Data() + Length, data, data_size);
        Length += data_size;
    }

    KRAFT_INLINE void WriteRaw(void* data, u64 element_size)
    {
        EnlargeBufferIfRequired(Length + element_size);

        // Write the data
        MemCpy(Data() + Length, data, element_size);
        Length += element_size;
    }

    void WriteString(String8 str)
    {
        WriteArray((void*)str.ptr, sizeof(*str.ptr), str.count);
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
    template<typename ArrayValueType>
    KRAFT_INLINE typename std::enable_if<std::is_trivially_copyable<ArrayValueType>::value>::type Write(Array<ArrayValueType> Value)
    {
        EnlargeBufferIfRequired(Length + Value.GetLengthInBytes() + sizeof(uint64));

        // Write the length
        MemCpy(Data() + Length, &Value.Length, sizeof(uint64));
        Length += sizeof(uint64);

        MemCpy(Data() + Length, &Value[0], Value.GetLengthInBytes());
        Length += Value.GetLengthInBytes();
    }

    template<typename CopyableValueType>
    KRAFT_INLINE typename std::enable_if<std::is_trivially_copyable<CopyableValueType>::value>::type Write(CopyableValueType Value)
    {
        EnlargeBufferIfRequired(Length + sizeof(Value));

        MemCpy(Data() + Length, &Value, sizeof(Value));
        Length += sizeof(Value);
    }

    template<typename NonCopyableValueType>
    KRAFT_INLINE typename std::enable_if<!std::is_trivially_copyable<NonCopyableValueType>::value>::type Write(NonCopyableValueType Value)
    {
        Value.WriteTo(this);
    }

    template<typename ArrayValueType>
    KRAFT_INLINE typename std::enable_if<!std::is_trivially_copyable<ArrayValueType>::value>::type Write(Array<ArrayValueType> Value)
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

    READ_TYPE_RETURN(u8);
    READ_TYPE_RETURN(u16);
    READ_TYPE_RETURN(u32);
    READ_TYPE_RETURN(i16);
    READ_TYPE_RETURN(i32);
    READ_TYPE_RETURN(i64);
    READ_TYPE_RETURN(f32);
    READ_TYPE_RETURN(bool);

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

    // KRAFT_INLINE u32 ReadArray(ArenaAllocator* arena, void** value, u32 element_size)
    // {
    //     // Read the length
    //     u32 element_count;
    //     Read(&element_count);

    //     *value = arena->Push(element_size * element_count, AlignOf(T), false)
    //     MemCpy(*value, Data() + Length, element_size * element_count);
    //     Length += (element_count * element_size);

    //     return element_count;
    // }

    KRAFT_INLINE void ReadRaw(void* dst, u64 element_size)
    {
        MemCpy(dst, Data() + Length, element_size);
        Length += (element_size);
    }

    // template <typename T>
    // u32 ReadArray(ArenaAllocator* arena, T data_type)

    String8 ReadString(ArenaAllocator* arena)
    {
        String8 result = {};
        u32     element_count = Readu32();
        result.ptr = ArenaPushArray(arena, u8, element_count);
        result.count = element_count;
        MemCpy(result.ptr, Data() + Length, element_count);
        Length += element_count;

        return result;
    }

    // For trivially copyable types
    template<class ArrayValueType>
    KRAFT_INLINE typename std::enable_if<std::is_trivially_copyable<ArrayValueType>::value>::type Read(Array<ArrayValueType>* Value)
    {
        // Read the length
        uint64 ValueLength;
        Read(&ValueLength);

        *Value = Array<ArrayValueType>(ValueLength);
        MemCpy(Value->Data(), Data() + Length, Value->GetLengthInBytes());
        Length += Value->GetLengthInBytes();
    }

    template<class ArrayValueType>
    KRAFT_INLINE typename std::enable_if<!std::is_trivially_copyable<ArrayValueType>::value>::type Read(Array<ArrayValueType>* Value)
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

    template<typename CopyableValueType>
    KRAFT_INLINE typename std::enable_if<std::is_trivially_copyable<CopyableValueType>::value>::type Read(CopyableValueType* Value)
    {
        MemCpy(Value, Data() + Length, sizeof(CopyableValueType));
        Length += sizeof(CopyableValueType);
    }

    template<typename NonCopyableValueType>
    KRAFT_INLINE typename std::enable_if<!std::is_trivially_copyable<NonCopyableValueType>::value>::type Read(NonCopyableValueType* Value)
    {
        Value->ReadFrom(this);
    }
};

#undef WRITE_TYPE
#undef READ_TYPE

struct BufferView
{
    const uint8* Memory;
    uint64       Size;
};

} // namespace kraft