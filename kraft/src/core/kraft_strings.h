#pragma once

#define String8VArg(str) (i32)((str).count), ((str).ptr)

namespace kraft {

struct String8Array
{
    String8* ptr;
    u64      count;
};

String8 StringFormat(ArenaAllocator* arena, const char* format, ...);
String8 StringFormatV(ArenaAllocator* arena, const char* format, va_list args);
bool    StringEqual(String8 a, String8 b);
bool    StringEndsWith(String8 str, String8 suffix);

// Concatenates strings; The resulting string is null-terminated
String8 StringCat(ArenaAllocator* arena, String8 a, String8 b);

// Duplicates a string; The resulting string is null-terminated
String8 StringCopy(ArenaAllocator* arena, String8 str);

u64 CString8Length(u8* c_str);

String8 String8FromPtrAndLength(u8* ptr, u64 length);
String8 String8FromCString(char* c_str);

// String8 from string literals
#define String8Raw(str) kraft::String8FromPtrAndLength((u8*)(str), sizeof(str) - 1)
#define S(str)          kraft::String8FromPtrAndLength((u8*)(str), sizeof(str) - 1)

} // namespace kraft

#ifdef __cplusplus

static bool operator==(const String8& a, const String8& b)
{
    if (a.count != b.count)
        return false;

    for (u64 i = 0; i < a.count; ++i)
    {
        if (a.ptr[i] != b.ptr[i])
            return false;
    }

    return true;
}

#endif