#include "kraft_string.h"

#include <cstdarg>
#include <wctype.h>

#include <core/kraft_asserts.h>

namespace kraft {

template<>
KString<char>& KString<char>::Trim()
{
    SizeType Start = 0;
    SizeType End = Length - 1;

    while (Start < Length && isspace(Data()[Start]))
        Start++;
    while (End >= Start && isspace(Data()[End]))
        End--;

    Length = End - Start + 1;
    MemCpy(Data(), Data() + Start, Length * sizeof(char));
    Data()[Length] = 0;

    return *this;
}

template<>
KString<wchar_t>& KString<wchar_t>::Trim()
{
    SizeType Start = 0;
    SizeType End = Length - 1;

    while (Start < Length && iswspace(Data()[Start]))
        Start++;
    while (End >= Start && iswspace(Data()[End]))
        End--;

    Length = End - Start + 1;
    MemCpy(Data(), Data() + Start, Length * sizeof(wchar_t));
    Data()[Length] = 0;

    return *this;
}

// #if defined(KRAFT_PLATFORM_WINDOWS)
// #define WIN32_LEAN_AND_MEAN
// #include "Windows.h"
// WString MultiByteStringToWideCharString(const String& Source)
// {
//     int CharacterCount = MultiByteToWideChar(CP_UTF8, 0, *Source, -1, NULL, 0);
//     if (!CharacterCount)
//     {
//         return {};
//     }

//     WString WideString(CharacterCount, 0);
//     if (!MultiByteToWideChar(CP_UTF8, 0, *Source, -1, *WideString, CharacterCount))
//     {
//         return {};
//     }

//     return WideString;
// }
// #endif

} // namespace kraft