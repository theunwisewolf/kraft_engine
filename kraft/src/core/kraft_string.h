#pragma once

#include "core/kraft_core.h"
#include "core/kraft_memory.h"
#include "core/kraft_string_utils.h"
#include "core/kraft_string_view.h"
#include "math/kraft_math.h"

/*
    Much of the inspiration for this class is taken from the excellent talk 
    "Optimizing A String Class for Computer Graphics in Cpp - Zander Majercik, Morgan McGuire CppCon 22" at CppCon
    https://www.youtube.com/watch?v=fglXeSWGVDc
 */
#include <ctype.h>
#include <string.h>

constexpr size_t KRAFT_STRING_SSO_ALIGNMENT = 16;

namespace kraft {

template<typename ValueType, uint64 InternalBufferSize = 128>
struct
#ifdef KRAFT_COMPILER_MSVC
    __declspec(align(KRAFT_STRING_SSO_ALIGNMENT))
#else
    alignas(KRAFT_STRING_SSO_ALIGNMENT)
#endif
    KString
{
    typedef uint64 SizeType;

    union BufferUnion
    {
        ValueType  StackBuffer[InternalBufferSize];
        ValueType* HeapBuffer = nullptr;
    } Buffer;

    // Number of characters currently in the string
    SizeType Length = 0;

    // Number of character that can fit in the string
    SizeType Allocated = 0;

protected:
    static_assert(InternalBufferSize % KRAFT_STRING_SSO_ALIGNMENT == 0, "KString InternalBufferSize must be a multiple of 16");

    constexpr KRAFT_INLINE SizeType GetBufferSizeInBytes()
    {
        return Allocated * sizeof(ValueType);
    }

    constexpr KRAFT_INLINE static SizeType ChooseAllocationSize(SizeType Size)
    {
        return (Size <= InternalBufferSize ? InternalBufferSize : math::Max(2 * Size + 1, 2 * InternalBufferSize + 1));
    }

    KRAFT_INLINE void Alloc(SizeType Size)
    {
        if (Size > InternalBufferSize)
        {
            Buffer.HeapBuffer = (ValueType*)Malloc(GetBufferSizeInBytes(), MEMORY_TAG_STRING, true);
        }
    }

    // ReallocIfRequired will simply allocates enough memory to accomodate
    // NewCharCount bytes. It does not preserve the old memory.
    // Does not take nullbyte into account
    constexpr void ReallocIfRequired(SizeType NewCharCount)
    {
        if (NewCharCount <= Allocated)
        {
            return;
        }

        if (InHeap())
        {
            Free(Data(), GetBufferSizeInBytes(), MEMORY_TAG_STRING);
        }

        Allocated = ChooseAllocationSize(NewCharCount);
        Alloc(Allocated);
    }

    constexpr void EnlargeBufferIfRequired(SizeType NewCharCount)
    {
        if (NewCharCount <= Allocated)
        {
            return;
        }

        bool       WasInHeap = InHeap();
        ValueType* OldBuffer = Data();
        SizeType   OldAllocationSize = GetBufferSizeInBytes();
        Allocated = ChooseAllocationSize(NewCharCount);
        if (InHeap())
        {
            ValueType* NewBuffer = (ValueType*)Malloc(GetBufferSizeInBytes(), MEMORY_TAG_STRING, true);
            MemCpy(NewBuffer, OldBuffer, Length * sizeof(SizeType));
            Buffer.HeapBuffer = NewBuffer;
        }

        if (WasInHeap)
        {
            Free(OldBuffer, OldAllocationSize, MEMORY_TAG_STRING);
        }
    }

    constexpr inline bool InHeap() const
    {
        return Allocated > InternalBufferSize;
    }

    constexpr inline bool InStack() const
    {
        return Allocated == InternalBufferSize;
    }

public:
    KString(std::nullptr_t)
    {
        Buffer.StackBuffer[0] = 0;
        Allocated = InternalBufferSize;
        Length = 0;
    }

    KRAFT_INLINE constexpr KString()
    {
        Buffer.StackBuffer[0] = 0;
        Allocated = InternalBufferSize;
        Length = 0;
    }

    constexpr KString(SizeType CharCount, ValueType Char)
    {
        Length = CharCount;
        Allocated = ChooseAllocationSize(Length + 1);
        Alloc(Allocated);
        MemSet(Data(), static_cast<ValueType>(Char), Allocated);
        Data()[Length] = 0;
    }

    constexpr KString(const ValueType* Str)
    {
        if (Str)
        {
            Length = Strlen(Str);
            Allocated = ChooseAllocationSize(Length + 1);
            Alloc(Allocated);
            MemCpy(Data(), Str, (Length + 1) * sizeof(ValueType));
            Data()[Length] = 0;
        }
    }

    // constexpr KString(KStringView<ValueType> View) : KString(View.Buffer, View.Length)
    // {}

    constexpr KString(const ValueType* Str, SizeType Count)
    {
        Length = Count;
        Allocated = ChooseAllocationSize(Length + 1);
        Alloc(Allocated);
        MemCpy(Data(), Str, Length * sizeof(ValueType));
        Data()[Length] = 0;
    }

    constexpr KString(const ValueType* Str, SizeType Position, SizeType Count)
    {
        Length = Count;
        Allocated = ChooseAllocationSize(Length + 1);
        Alloc(Allocated);
        MemCpy(Data(), Str + Position, Length * sizeof(ValueType));
        Data()[Length] = 0;
    }

    // Copy constructor
    constexpr KString(const KString& Str)
    {
        Length = Str.Length;
        ReallocIfRequired(Length + 1);

        MemCpy(Data(), Str.Data(), (Length + 1) * sizeof(ValueType));
        Data()[Length] = 0;
    }

    // Move constructor
    constexpr KString(KString&& Str) noexcept
    {
        SizeType    _Length = Length;
        SizeType    _Allocated = Allocated;
        BufferUnion _Buffer = Buffer;

        // Swap!
        Length = Str.Length;
        Allocated = Str.Allocated;
        Buffer = Str.Buffer;

        Str.Length = _Length;
        Str.Allocated = _Allocated;
        Str.Buffer = _Buffer;
    }

    constexpr KString(const KString& Str, SizeType Position)
    {
        Length = Str.Length - Position;
        Allocated = ChooseAllocationSize(Length + 1);
        Alloc(Allocated);
        MemCpy(Data(), Str.Data() + Position, (Length + 1) * sizeof(ValueType));
        Data()[Length] = 0;
    }

    constexpr KString(const KString& Str, SizeType Position, SizeType Count)
    {
        Length = ((Position + Count) >= Str.Size() ? Str.Size() - Position : Count);
        Allocated = ChooseAllocationSize(Length + 1);
        Alloc(Allocated);
        MemCpy(Data(), Str.Data() + Position, (Length + 1) * sizeof(ValueType));
        Data()[Length] = 0;
    }

    constexpr KString& operator=(const ValueType* Str)
    {
        Length = Strlen(Str);
        ReallocIfRequired(Length + 1);

        MemCpy(Data(), Str, (Length + 1) * sizeof(ValueType));
        Data()[Length] = 0;

        return *this;
    }

    constexpr KString& operator=(const KString& Str)
    {
        if (&Str == this)
        {
            return *this;
        }

        Length = Str.Length;
        ReallocIfRequired(Length + 1);

        MemCpy(Data(), Str.Data(), Length * sizeof(ValueType));
        Data()[Length] = 0;

        return *this;
    }

    constexpr KString& operator+=(const ValueType* Str)
    {
        SizeType StrLength = Strlen(Str);
        EnlargeBufferIfRequired(Length + StrLength + 1);
        MemCpy(Data() + Length, Str, StrLength + 1);
        Length += StrLength;
        Data()[Length] = 0;

        return *this;
    }

    constexpr KString& operator+=(const ValueType Char)
    {
        EnlargeBufferIfRequired(Length + 2);
        Data()[Length] = Char;
        Length++;
        Data()[Length] = 0;

        return *this;
    }

    constexpr KRAFT_INLINE friend KString operator+(const KString& StrA, const KString& StrB)
    {
        KString Out;
        Out.Length = StrA.Length + StrB.Length;
        Out.Allocated = Out.ChooseAllocationSize(Out.Length + 1);
        Out.Alloc(Out.Allocated);

        MemCpy(Out.Data(), StrA.Data(), StrA.Length * sizeof(ValueType));
        MemCpy(Out.Data() + StrA.Length, StrB.Data(), StrB.Length * sizeof(ValueType));
        Out.Data()[Out.Length] = 0;

        return Out;
    }

    constexpr KRAFT_INLINE friend KString operator+(const KString& StrA, KStringView<ValueType> StrB)
    {
        KString Out;
        Out.Length = StrA.Length + StrB.Length;
        Out.Allocated = Out.ChooseAllocationSize(Out.Length + 1);
        Out.Alloc(Out.Allocated);

        MemCpy(Out.Data(), StrA.Data(), StrA.Length * sizeof(ValueType));
        MemCpy(Out.Data() + StrA.Length, StrB.Buffer, StrB.Length * sizeof(ValueType));
        Out.Data()[Out.Length] = 0;

        return Out;
    }

    constexpr KRAFT_INLINE friend KString operator+(const KString& StrA, const ValueType* StrB)
    {
        SizeType StrBLength = Strlen(StrB);
        KString  Out;
        Out.Length = StrA.Length + StrBLength;
        Out.Allocated = Out.ChooseAllocationSize(Out.Length + 1);
        Out.Alloc(Out.Allocated);

        MemCpy(Out.Data(), StrA.Data(), StrA.Length * sizeof(ValueType));
        MemCpy(Out.Data() + StrA.Length, StrB, StrBLength * sizeof(ValueType));
        Out.Data()[Out.Length] = 0;

        return Out;
    }

    constexpr KString& Append(const ValueType* Str)
    {
        return (*this += Str);
    }

    constexpr KString& Append(const ValueType Char)
    {
        return (*this += Char);
    }

    constexpr const ValueType* Data() const noexcept
    {
        return InStack() ? Buffer.StackBuffer : Buffer.HeapBuffer;
    }

    constexpr ValueType* Data() noexcept
    {
        return InStack() ? Buffer.StackBuffer : Buffer.HeapBuffer;
    }

    constexpr const ValueType* operator*() const noexcept
    {
        return InStack() ? Buffer.StackBuffer : Buffer.HeapBuffer;
    }

    constexpr ValueType* operator*()
    {
        return InStack() ? Buffer.StackBuffer : Buffer.HeapBuffer;
    }

    KRAFT_INLINE constexpr SizeType Size() const noexcept
    {
        return Length;
    }

    ~KString()
    {
        if (InHeap())
        {
            Free(Data(), Allocated, MEMORY_TAG_STRING);
        }
    }

    KRAFT_INLINE constexpr ValueType& operator[](SizeType Index)
    {
        return Data()[Index];
    }

    KRAFT_INLINE constexpr const ValueType& operator[](SizeType Index) const
    {
        return Data()[Index];
    }

    // In-place trim
    KString& Trim();

    constexpr int Compare(const ValueType* Str) const
    {
        return (int)Compare(Data(), Length, Str, Strlen(Str));
    }

    constexpr int Compare(const KString& Str) const
    {
        if (Data() == Str.Data() && Length == Str.Length)
            return 0;

        return (int)Compare(Data(), Length, Str.Data(), Str.Length);
    }

    constexpr int Compare(KStringView<ValueType> View) const
    {
        if (Data() == View.Buffer && Length == View.Length)
            return 0;

        return (int)Compare(Data(), Length, View.Buffer, View.Length);
    }

    constexpr KRAFT_INLINE SizeType Compare(const ValueType* a, SizeType aLen, const ValueType* b, SizeType bLen) const
    {
        const SizeType Count = math::Min(aLen, bLen);
        SizeType       Result = MemCmp(a, b, Count);
        return Result ? Result : aLen - bLen;
    }

    constexpr KRAFT_INLINE bool operator==(const KString& Str) const
    {
        return !Compare(Str);
    }

    constexpr KRAFT_INLINE bool operator!=(const KString& Str) const
    {
        return Compare(Str);
    }

    constexpr KRAFT_INLINE bool operator==(KStringView<ValueType> Str) const
    {
        return !Compare(Str);
    }

    constexpr KRAFT_INLINE bool operator!=(KStringView<ValueType> Str) const
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

    friend constexpr KRAFT_INLINE bool operator==(const ValueType* a, const KString& b)
    {
        return b == a;
    }

    KRAFT_INLINE uint64 GetLengthInBytes() const noexcept
    {
        return this->Length * sizeof(ValueType);
    }

    bool EndsWith(const KString& Suffix)
    {
        if (Length < Suffix.Length)
        {
            return false;
        }

        SizeType Start = Length - Suffix.Length;
        for (SizeType i = Start, j = 0; j < Suffix.Length; i++, j++)
        {
            if (Data()[i] != Suffix.Data()[j])
            {
                return false;
            }
        }

        return true;
    }

    KRAFT_INLINE bool HasSuffix(const KString& Suffix)
    {
        return EndsWith(Suffix);
    }

    bool StartsWith(const KString& Prefix)
    {
        if (Length < Prefix.Length)
        {
            return false;
        }

        for (int i = 0; i < Prefix.Length; i++)
        {
            if (Data()[i] != Prefix.Data()[i])
            {
                return false;
            }
        }

        return true;
    }

    KRAFT_INLINE bool HasPrefix(const KString& Prefix)
    {
        return StartsWith(Prefix);
    }

    KRAFT_INLINE constexpr bool Empty() const
    {
        return Length == 0;
    }
};

typedef KString<char, 128>    String;
typedef KString<wchar_t, 128> WString;

#ifdef UNICODE
typedef WString TString;
#else
typedef String TString;
#endif

static WString CharToWChar(const String& Source)
{
    uint64  BufferSize = mbstowcs(nullptr, Source.Data(), 0);
    WString Output(BufferSize, 0);
    mbstowcs(Output.Data(), Source.Data(), BufferSize);

    return Output;
}

static String WCharToChar(const WString& Source)
{
    uint64 BufferSize = wcstombs(nullptr, Source.Data(), 0);
    String Output(BufferSize, 0);
    wcstombs(Output.Data(), Source.Data(), BufferSize);

    return Output;
}

#if defined(KRAFT_PLATFORM_WINDOWS)
static WString MultiByteStringToWideCharString(const String& Source);
#endif

KRAFT_INLINE uint64 StringLength(const char* in)
{
    return strlen(in);
}

KRAFT_INLINE uint64 StringLengthClamped(const char* in, uint64 max)
{
    uint64 length = StringLength(in);
    return length > max ? max : length;
}

KRAFT_INLINE char* StringCopy(char* dst, const char* src)
{
    return strcpy(dst, src);
}

KRAFT_INLINE char* StringNCopy(char* dst, const char* src, uint64 length)
{
    return strncpy(dst, src, length);
}

KRAFT_INLINE const char* StringTrim(const char* in)
{
    if (!in)
        return in;

    uint64 length = StringLength(in);
    if (length == 0)
        return in;

    uint64 start = 0;
    uint64 end = length - 1;

    while (start < length && isspace(in[start]))
        start++;
    while (end >= start && isspace(in[end]))
        end--;

    length = end - start + 1;
    char* out = (char*)kraft::Malloc((length + 1) * sizeof(char), MEMORY_TAG_STRING, true);
    kraft::MemCpy(out, (void*)(in + start), length * sizeof(char));

    return (const char*)out;
}

// Will not work with string-literals as they are read only!
KRAFT_INLINE char* StringTrimLight(char* in)
{
    if (!in)
        return in;

    while (isspace(*in))
        in++;
    char* p = in;

    while (*p)
        p++;
    p--;

    while (isspace(*p))
        p--;
    p[1] = 0;

    return in;
}

KRAFT_INLINE uint64 StringEqual(const char* a, const char* b)
{
    return strcmp(a, b) == 0;
}

KRAFT_INLINE int32 StringFormatV(char* buffer, int n, const char* format, va_list args);

KRAFT_INLINE int32 StringFormat(char* buffer, int n, const char* format, ...)
{
    if (buffer)
    {
#ifdef KRAFT_COMPILER_MSVC
        va_list args;
#else
        __builtin_va_list args;
#endif
        va_start(args, format);
        int32 written = StringFormatV(buffer, n, format, args);
        va_end(args);
        return written;
    }

    return -1;
}

KRAFT_INLINE int32 StringFormatV(char* buffer, int n, const char* format, va_list args)
{
    if (buffer)
    {
        return vsnprintf(buffer, n, format, args);
    }

    return -1;
}

// static wchar_t* CharToWChar(const char* Source)
// {
//     // TODO: Deallocate!
//     wchar_t* Output;
//     uint64 BufferSize = mbstowcs(nullptr, Source, 0) + 1;
//     Output = (wchar_t*)Malloc(BufferSize * sizeof(wchar_t), MEMORY_TAG_STRING);
//     mbstowcs(Output, Source, BufferSize);

//     return Output;
// }

// static char* WCharToChar(const wchar_t* Source)
// {
//     // TODO: Deallocate!
//     char* Output;
//     uint64 BufferSize = wcstombs(nullptr, Source, 0) + 1;
//     Output = (char*)Malloc(BufferSize * sizeof(char), MEMORY_TAG_STRING);
//     wcstombs(Output, Source, BufferSize);

//     return Output;
// }

}