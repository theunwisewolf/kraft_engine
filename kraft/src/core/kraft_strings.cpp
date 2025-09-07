namespace kraft {

String8 StringFormat(ArenaAllocator* arena, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    String8 result = StringFormatV(arena, format, args);
    va_end(args);

    return result;
}

String8 StringFormatV(ArenaAllocator* arena, const char* format, va_list args)
{
    String8 result = {};

    va_list args_copy;
    va_copy(args_copy, args);

    // All strings returned by stb sprintf are null terminated, so + 1
    u32 final_string_length = kraft_vsnprintf(0, 0, format, args) + 1;
    result.ptr = ArenaPushArray(arena, u8, final_string_length);
    result.count = kraft_vsnprintf((char*)result.ptr, final_string_length, format, args_copy);

    va_end(args_copy);

    return result;
}

bool StringEqual(String8 a, String8 b)
{
    if (a.count != b.count)
        return false;
    for (u32 i = 0; i < a.count; i++)
    {
        if (a.ptr[i] != b.ptr[i])
            return false;
    }

    return true;
}

bool StringEndsWith(String8 str, String8 suffix)
{
    if (str.count < suffix.count)
    {
        return false;
    }

    u64 start = str.count - suffix.count;
    for (u64 i = start, j = 0; j < suffix.count; i++, j++)
    {
        if (str.ptr[i] != suffix.ptr[j])
        {
            return false;
        }
    }

    return true;
}

String8 StringCat(ArenaAllocator* arena, String8 a, String8 b)
{
    String8 result = {};
    result.count = a.count + b.count;
    result.ptr = ArenaPushArray(arena, u8, result.count + 1);
    MemCpy(result.ptr, a.ptr, a.count);
    MemCpy(result.ptr + a.count, b.ptr, b.count);
    result.ptr[result.count] = 0;

    return result;
}

u64 CString8Length(u8* c_str)
{
    u8* ptr = c_str;
    for (; *ptr != '0'; ptr++)
        ;

    return ptr - c_str;
}

String8 String8FromPtrAndLength(u8* ptr, u64 length)
{
    return String8{ .ptr = ptr, .count = length };
}

String8 String8FromCString(char* c_str)
{
    return String8{ .ptr = (u8*)c_str, .count = CString8Length((u8*)c_str) };
}

} // namespace kraft