#include "kraft_string.h"

namespace kraft
{

// char
template<>
KString<char>::SizeType KString<char>::_kstring_strlen(const char* Str) const
{
    return strlen(Str);
}

template<>
KString<char>& KString<char>::Trim()
{
    SizeType Start = 0;
    SizeType End = Length - 1;

    while (Start < Length && isspace(Data()[Start])) Start++;
    while (End >= Start && isspace(Data()[End])) End--;

    Length = End - Start + 1;
    MemCpy(Data(), Data() + Start, Length * sizeof(char));
    Data()[Length] = 0;

    return *this;
}

// wchar_t
template<>
KString<wchar_t>::SizeType KString<wchar_t>::_kstring_strlen(const wchar_t* Str) const
{
    return wcslen(Str);
}

template<>
KString<wchar_t>& KString<wchar_t>::Trim()
{
    SizeType Start = 0;
    SizeType End = Length - 1;

    while (Start < Length && iswspace(Data()[Start])) Start++;
    while (End >= Start && iswspace(Data()[End])) End--;

    Length = End - Start + 1;
    MemCpy(Data(), Data() + Start, Length * sizeof(wchar_t));
    Data()[Length] = 0;

    return *this;
}


}