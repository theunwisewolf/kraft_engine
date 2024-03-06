#pragma once

#include "core/kraft_core.h"

namespace kraft
{

template <typename ValueType>
struct KStringView
{
    typedef uint64 SizeType;

    const ValueType *Buffer  = 0;
    SizeType         Length  = 0;

    KStringView()
    {
        this->Buffer = nullptr;
        this->Length = 0;
    }

    KStringView(const ValueType *Buffer, SizeType Length)
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

    KStringView& operator=(const KStringView& other)
    {
        this->Buffer = other.Buffer;
        this->Length = other.Length;
        
        return *this;
    }

    bool operator==(const KStringView& other) const
    {
        // Fast pass
        if (other.Length != Length)
        {
            return false;
        }

        for (int i = 0; i < Length; i++)
        {
            if (other.Buffer[i] != Buffer[i])
            {
                return false;
            }
        }

        return true;
    }

    bool operator!=(const KStringView& other) const
    {
        return !operator==(other);
    }

    constexpr const ValueType* operator*() const noexcept
    {
        return Buffer;
    }

    KRAFT_INLINE uint64 GetLengthInBytes() const noexcept
    {
        return this->Length * sizeof(ValueType);
    }
};

typedef KStringView<char> StringView;

}