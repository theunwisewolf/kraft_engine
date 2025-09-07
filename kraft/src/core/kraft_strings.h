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
String8 StringCat(ArenaAllocator* arena, String8 a, String8 b);

u64 CString8Length(u8* c_str);

String8 String8FromPtrAndLength(u8* ptr, u64 length);
String8 String8FromCString(char* c_str);

#define String8Raw(str) kraft::String8FromPtrAndLength((u8*)(str), sizeof(str) - 1)

} // namespace kraft