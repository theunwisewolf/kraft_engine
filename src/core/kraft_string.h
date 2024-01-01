#pragma once

#include "core/kraft_core.h"
#include "math/kraft_math.h"
#include "core/kraft_memory.h"

/*
    Much of the inspiration for this class is taken from the excellent talk 
    "Optimizing A String Class for Computer Graphics in Cpp - Zander Majercik, Morgan McGuire CppCon 22" at CppCon
    https://www.youtube.com/watch?v=fglXeSWGVDc
 */
#include <string.h>
#include <ctype.h>

#ifdef UNICODE
    #define ANSI_TO_TCHAR(x) CharToWChar(x).Data()
    #define TCHAR_TO_ANSI(x) WCharToChar(x).Data()
#else
    #define ANSI_TO_TCHAR(x) x
    #define TCHAR_TO_ANSI(x) x
#endif

constexpr size_t KRAFT_STRING_SSO_ALIGNMENT = 16;

namespace kraft
{

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
        ValueType    StackBuffer[InternalBufferSize];   
        ValueType*   HeapBuffer = nullptr;
    } Buffer;

    // Number of characters currently in the string
    SizeType     Length = 0;

    // Number of character that can fit in the string
    SizeType     Allocated = 0;

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

    // Does not take nullbyte into account
    constexpr void ReallocIfRequired(SizeType NewCharCount)
    {
        if (NewCharCount <= Allocated)
        {
            return;
        }

        Free(Data(), GetBufferSizeInBytes(), MEMORY_TAG_STRING);

        Allocated = ChooseAllocationSize(NewCharCount);
        Alloc(Allocated);
    }

    constexpr void EnlargeBufferIfRequired(SizeType NewCharCount)
    {
        if (NewCharCount <= Allocated)
        {
            return;
        }

        bool WasInHeap = InHeap();
        ValueType* OldBuffer = Data();
        SizeType OldAllocationSize = GetBufferSizeInBytes();
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
        Allocated = 0;
        Length = 0;
    }

    KRAFT_INLINE constexpr KString()
    {
        Buffer.StackBuffer[0] = 0;
        Allocated = 0;
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
        Length = _kstring_strlen(Str);
        Allocated = ChooseAllocationSize(Length + 1);
        Alloc(Allocated);
        MemCpy(Data(), Str, (Length + 1) * sizeof(ValueType));
        Data()[Length] = 0;
    }

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
        SizeType _Length = Length;
        SizeType _Allocated = Allocated;
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
        Length = _kstring_strlen(Str);
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
        SizeType StrLength = _kstring_strlen(Str);
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

    KString& Trim();

    constexpr int Compare(const ValueType* Str) const
    {
        return _kstring_compare(Data(), Length, Str, _kstring_strlen(Str));
    }

    constexpr int Compare(const KString& Str) const
    {
        if (Data() == Str.Data() && Length == Str.Length) return 0;

        return _kstring_compare(Data(), Length, Str.Data(), Str.Length);
    }

    constexpr KRAFT_INLINE bool operator==(const KString& Str) const
    {
        return !Compare(Str);
    }

    constexpr KRAFT_INLINE bool operator==(const ValueType* Str) const
    {
        return !Compare(Str);
    }

    friend constexpr KRAFT_INLINE bool operator==(const ValueType* a, const KString& b) 
    {
        return b == a;
    }

private:
    SizeType _kstring_strlen(const ValueType* Str) const;
    
    constexpr KRAFT_INLINE SizeType _kstring_compare(const ValueType* a, SizeType aLen, const ValueType* b, SizeType bLen) const
    {
        const SizeType Count = math::Min(aLen, bLen);
        SizeType Result = MemCmp(a, b, Count);
        return Result ? Result : aLen - bLen;
    }
};

typedef KString<char> String;
typedef KString<wchar_t> WString;

#ifdef UNICODE
typedef WString TString;
#else
typedef String TString;
#endif

static WString CharToWChar(const String& Source)
{
    uint64 BufferSize = mbstowcs(nullptr, Source.Data(), 0);
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

struct StringView
{
    TCHAR       *Buffer;
    uint64      Length;

    bool operator==(const StringView& other)
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
};

KRAFT_INLINE uint64 StringLength(const TCHAR* in)
{
#ifdef UNICODE
    return wcslen(in);
#else
    return strlen(in);
#endif
}

KRAFT_INLINE uint64 StringLengthClamped(const TCHAR* in, uint64 max)
{
    uint64 length = StringLength(in);
    return length > max ? max : length;
}

KRAFT_INLINE TCHAR* StringCopy(TCHAR* dst, TCHAR* src)
{
#ifdef UNICODE
    return wcscpy(dst, src);
#else
    return strcpy(dst, src);
#endif
}

KRAFT_INLINE TCHAR* StringNCopy(TCHAR* dst, const TCHAR* src, uint64 length)
{
#ifdef UNICODE
    return wcsncpy(dst, src, length);
#else
    return strncpy(dst, src, length);
#endif
}

KRAFT_INLINE const TCHAR* StringTrim(const TCHAR* in)
{
    if (!in) return in;
    
    uint64 length = StringLength(in);
    int start = 0;
    int end = length - 1;

#if UNICODE
    while (start < length && iswspace(in[start])) start++;
    while (end >= start && iswspace(in[end])) end--;
#else
    while (start < length && isspace(in[start])) start++;
    while (end >= start && isspace(in[end])) end--;
#endif

    length = end - start + 1;
    TCHAR* out = (TCHAR*)kraft::Malloc((length + 1) * sizeof(TCHAR), MEMORY_TAG_STRING, true);
    kraft::MemCpy(out, (void*)(in+start), length * sizeof(TCHAR));

    return (const TCHAR*)out;
}

// Will not work with string-literals as they are read only!
KRAFT_INLINE TCHAR* StringTrimLight(TCHAR* in)
{
    if (!in) return in;

#if UNICODE
    while (iswspace(*in)) in++;
#else
    while (isspace(*in)) in++;
#endif
    TCHAR *p = in;

    while (*p) p++;
    p--;

#if UNICODE
    while (iswspace(*p)) p--;
#else
    while (isspace(*p)) p--;
#endif
    p[1] = 0;

    return in;
}

KRAFT_INLINE uint64 StringEqual(const TCHAR *a, const TCHAR *b)
{
#if UNICODE
    return wcscmp(a, b) == 0;
#else
    return strcmp(a, b) == 0;
#endif
}

KRAFT_INLINE int32 StringFormatV(TCHAR* buffer, int n, const TCHAR* format, va_list args);

KRAFT_INLINE int32 StringFormat(TCHAR* buffer, int n, const TCHAR* format, ...)
{
    if (buffer) 
    {
        __builtin_va_list args;
        va_start(args, format);
        int32 written = StringFormatV(buffer, n, format, args);
        va_end(args);
        return written;
    }

    return -1;
}

KRAFT_INLINE int32 StringFormatV(TCHAR* buffer, int n, const TCHAR* format, va_list args)
{
    if (buffer) 
    {
        __builtin_va_list argPtr;
#ifdef UNICODE
    // #ifdef KRAFT_PLATFORM_WINDOWS
    //     uint64 FormatStringLength = StringLength(format);
    //     TCHAR _format[32000];
    //     MemCpy(_format, format, FormatStringLength * sizeof(TCHAR));

    //     for (int i = 0; i < FormatStringLength; i++)
    //     {
    //         if (_format[i] == '%' && _format[i+1] == 's')
    //         {
    //             _format[i+1] = 'S';
    //         }
    //         else if (format[i] == '%' && _format[i+1] == 'S')
    //         {
    //             _format[i+1] = 's';
    //         }
    //     }

    //     _format[FormatStringLength] = 0;
    //     int32 written = vswprintf(buffer, n, _format, args);
    // #else
        int32 written = vswprintf(buffer, n, format, args);
    // #endif
#else
        int32 written = vsnprintf(buffer, n, format, args);
#endif

        return written;
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