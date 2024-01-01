#include "kraft_string.h"

namespace kraft
{

template<>
KString<char>::SizeType KString<char>::_kstring_strlen(const char* Str) const
{
    return strlen(Str);
}

template<>
KString<wchar_t>::SizeType KString<wchar_t>::_kstring_strlen(const wchar_t* Str) const
{
    return wcslen(Str);
}

}