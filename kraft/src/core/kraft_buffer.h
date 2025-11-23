#pragma once

namespace kraft {

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

    // Best used with trivially copyable types
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

    KRAFT_INLINE void WriteString(String8 str)
    {
        WriteArray((void*)str.ptr, sizeof(*str.ptr), str.count);
    }

    KRAFT_INLINE void Write(void* InData, SizeType InDataSize)
    {
        EnlargeBufferIfRequired(Length + InDataSize);
        MemCpy(Data() + Length, InData, InDataSize);
        Length += InDataSize;
    }

    READ_TYPE_RETURN(u8);
    READ_TYPE_RETURN(u16);
    READ_TYPE_RETURN(u32);
    READ_TYPE_RETURN(i16);
    READ_TYPE_RETURN(i32);
    READ_TYPE_RETURN(i64);
    READ_TYPE_RETURN(f32);
    READ_TYPE_RETURN(bool);

    KRAFT_INLINE void ReadRaw(void* dst, u64 element_size)
    {
        MemCpy(dst, Data() + Length, element_size);
        Length += (element_size);
    }

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
};

#undef WRITE_TYPE
#undef READ_TYPE_RETURN

struct BufferView
{
    const u8* Memory;
    u64       Size;
};

} // namespace kraft