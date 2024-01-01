#pragma once

#include "core/kraft_core.h"
#include "core/kraft_memory.h"

#include <string.h>
#include <ctype.h>
#include <locale.h>

#ifdef UNICODE
    #define ANSI_TO_TCHAR(x) CharToWChar(x).Data()
    #define TCHAR_TO_ANSI(x) WCharToChar(x).Data()
#else
    #define ANSI_TO_TCHAR(x) x
    #define TCHAR_TO_ANSI(x) x
#endif

namespace kraft
{

// using ValueType = char;
template<typename ValueType>
struct KString
{
    typedef uint64 SizeType;

    ValueType   *Buffer = nullptr;
    SizeType     Length = 0;
    SizeType     Allocated = 0;

    KString(std::nullptr_t)
    {
        Buffer = nullptr;
        Allocated = 0;
        Length = 0;
    }

    KRAFT_INLINE constexpr KString()
    {
        Buffer = nullptr;
        Allocated = 0;
        Length = 0;
    }

    constexpr KString(SizeType CharCount, ValueType Char)
    {
        Length = CharCount;
        Allocated = (Length + 1) * sizeof(ValueType);
        Alloc(Allocated);
        MemSet(Data(), static_cast<ValueType>(Char), Allocated);
        Data()[Length] = 0;
    }

    constexpr KString(const ValueType* Str)
    {
        Length = _kstring_strlen(Str);
        Allocated = (Length + 1) * sizeof(ValueType);
        Alloc(Allocated);
        MemCpy(Data(), Str, (Length + 1) * sizeof(ValueType));
        Data()[Length] = 0;
    }

    constexpr KString(const KString& Str, SizeType Position, SizeType Count)
    {
        Length = ((Position + Count) >= Str.Size() ? Str.Size() - Position : Count);
        Allocated = (Length + 1) * sizeof(ValueType);
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
        if (&Str == this || Str.Length == 0)
        {
            return *this;
        }

        Length = Str.Length;
        ReallocIfRequired(Length + 1);

        MemCpy(Data(), Str.Data(), (Length + 1) * sizeof(ValueType));
        Data()[Length] = 0;
        
        return *this;
    }

    constexpr void ReallocIfRequired(SizeType CharCount)
    {
        SizeType BufferSizeRequired = CharCount * sizeof(ValueType);
        if (BufferSizeRequired <= Allocated)
        {
            return;
        }

        Free(Data());

        Allocated = BufferSizeRequired;
        Alloc(BufferSizeRequired);
    }

    KRAFT_INLINE void Alloc(SizeType Size)
    {
        Buffer = (ValueType*)Malloc(Allocated * sizeof(ValueType), MEMORY_TAG_STRING, true);
    }

    constexpr const ValueType* Data() const noexcept
    {
        return Buffer;
    }

    constexpr ValueType* Data() noexcept
    {
        return Buffer;
    }

    KRAFT_INLINE constexpr SizeType Size() const noexcept
    {
        return Length;
    }

    ~KString()
    {
        if (sizeof(ValueType) == 1)
        {
            KSUCCESS(TEXT("Deallocating string %S"), Data());
        }
        else
        {
            KSUCCESS(TEXT("Deallocating wstring %s"), Data());
        }

        Free(Data());
    }

    KRAFT_INLINE constexpr ValueType& operator[](SizeType Index)
    {
        return Data()[Index];
    }

    KRAFT_INLINE constexpr const ValueType& operator[](SizeType Index) const
    {
        return Data()[Index];
    }

private:
    SizeType _kstring_strlen(const ValueType* Str);
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
        int32 written = vswprintf(buffer, n, format, args);
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