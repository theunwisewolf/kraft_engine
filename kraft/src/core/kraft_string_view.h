#pragma once

#include "core/kraft_core.h"
#include "core/kraft_memory.h"
#include "core/kraft_string_utils.h"
#include "core/kraft_math.h"

namespace kraft {

template<typename ValueType, uint64 InternalBufferSize>
struct KString;

template<typename ValueType>
struct KStringView
{
    typedef uint64 SizeType;

    const ValueType* Buffer = 0;
    SizeType         Length = 0;

    KStringView()
    {
        this->Buffer = nullptr;
        this->Length = 0;
    }

    KStringView(const KString<ValueType, 128>& Str)
    {
        this->Buffer = Str.Data();
        this->Length = Str.Length;
    }

    // C-style strings
    KStringView(const ValueType* CStr)
    {
        if (CStr)
        {
            this->Buffer = CStr;
            this->Length = Strlen(CStr);
        }
    }

    KStringView(const ValueType* Buffer, SizeType Length)
    {
        this->Buffer = Buffer;
        this->Length = Length;
    }

    KStringView(const KStringView& other)
    {
        *this = other;
    }

    KStringView(KStringView&& other)
    {
        this->Buffer = other.Buffer;
        this->Length = other.Length;

        other.Buffer = 0;
        other.Length = 0;
    }

    constexpr int Compare(const ValueType* Str) const
    {
        return (int)Compare(Buffer, Length, Str, Strlen(Str));
    }

    constexpr int Compare(KStringView Str) const
    {
        if (Buffer == Str.Buffer && Length == Str.Length)
            return 0;

        return (int)Compare(Buffer, Length, Str.Buffer, Str.Length);
    }

    constexpr KRAFT_INLINE SizeType Compare(const ValueType* a, SizeType aLen, const ValueType* b, SizeType bLen) const
    {
        const SizeType Count = math::Min(aLen, bLen);
        SizeType       Result = MemCmp(a, b, Count);
        return Result ? Result : aLen - bLen;
    }

    constexpr KRAFT_INLINE bool operator==(KStringView Str) const
    {
        return !Compare(Str);
    }

    constexpr KRAFT_INLINE bool operator!=(KStringView Str) const
    {
        return Compare(Str);
    }

    constexpr KRAFT_INLINE bool operator==(const ValueType* Str) const
    {
        return !Compare(Str);
    }

    constexpr KRAFT_INLINE bool operator!=(const ValueType* Str) const
    {
        return Compare(Str);
    }

    friend constexpr KRAFT_INLINE bool operator==(const ValueType* a, const KStringView& b)
    {
        return b == a;
    }

    KStringView& operator=(const KStringView& other)
    {
        this->Buffer = other.Buffer;
        this->Length = other.Length;

        return *this;
    }

    constexpr const ValueType* operator*() const noexcept
    {
        return Buffer;
    }

    KRAFT_INLINE SizeType GetLengthInBytes() const noexcept
    {
        return this->Length * sizeof(ValueType);
    }
};

typedef KStringView<char> StringView;

}