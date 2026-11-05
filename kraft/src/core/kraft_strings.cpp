#include <string.h>

namespace kraft {

String8 StringFormat(ArenaAllocator* arena, const char* format, ...) {
    va_list args;
    va_start(args, format);
    String8 result = StringFormatV(arena, format, args);
    va_end(args);

    return result;
}

String8 StringFormatV(ArenaAllocator* arena, const char* format, va_list args) {
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

bool StringEqual(String8 a, String8 b) {
    if (a.count != b.count)
        return false;

    for (u64 i = 0; i < a.count; i++) {
        if (a.ptr[i] != b.ptr[i])
            return false;
    }

    return true;
}

bool StringEndsWith(String8 str, String8 suffix) {
    if (str.count < suffix.count) {
        return false;
    }

    u64 start = str.count - suffix.count;
    for (u64 i = start, j = 0; j < suffix.count; i++, j++) {
        if (str.ptr[i] != suffix.ptr[j]) {
            return false;
        }
    }

    return true;
}

String8 StringCat(ArenaAllocator* arena, String8 a, String8 b) {
    String8 result = {};
    result.count = a.count + b.count;
    result.ptr = ArenaPushArray(arena, u8, result.count + 1);
    MemCpy(result.ptr, a.ptr, a.count);
    MemCpy(result.ptr + a.count, b.ptr, b.count);
    result.ptr[result.count] = 0;

    return result;
}

String8 StringCopy(ArenaAllocator* arena, String8 str) {
    String8 result = {};
    result.count = str.count;
    result.ptr = ArenaPushArray(arena, u8, str.count + 1);
    MemCpy(result.ptr, str.ptr, str.count);
    result.ptr[str.count] = 0;

    return result;
}

u64 CStringLength(u8* c_str) {
    u8* ptr = c_str;
    for (; *ptr != 0; ptr++)
        ;

    return ptr - c_str;
}

String8 String8FromPtrAndLength(u8* ptr, u64 length) {
    return String8{.ptr = ptr, .count = length};
}

String8 String8FromCString(char* c_str) {
    return String8{.ptr = (u8*)c_str, .count = CStringLength((u8*)c_str)};
}

bool StringStartsWith(String8 str, String8 prefix) {
    return str.count >= prefix.count && (prefix.count == 0 || memcmp(str.ptr, prefix.ptr, prefix.count) == 0);
}

bool StringEqualsCString(String8 str, const char* c_str) {
    u64 length = strlen(c_str);
    return str.count == length && (length == 0 || memcmp(str.ptr, c_str, length) == 0);
}

bool StringEqualIgnoringCase(String8 a, String8 b) {
    if (a.count != b.count) {
        return false;
    }

    for (u64 i = 0; i < a.count; i++) {
        if (CharToLower(a.ptr[i]) != CharToLower(b.ptr[i])) {
            return false;
        }
    }

    return true;
}

bool StringStartsWithIgnoringCase(String8 str, String8 prefix) {
    return str.count >= prefix.count && StringEqualIgnoringCase(String8{.ptr = str.ptr, .count = prefix.count}, prefix);
}

bool StringEndsWithIgnoringCase(String8 str, String8 suffix) {
    return str.count >= suffix.count && StringEqualIgnoringCase(String8{.ptr = str.ptr + str.count - suffix.count, .count = suffix.count}, suffix);
}

u64 StringIndexOf(String8 str, String8 part, u64 from) {
    if (part.count == 0) {
        return from < str.count ? from : str.count;
    }

    u64 at = from;
    while (at + part.count <= str.count) {
        const u8* first = (const u8*)memchr(str.ptr + at, part.ptr[0], (size_t)(str.count - part.count + 1 - at));
        if (!first) {
            break;
        }

        at = (u64)(first - str.ptr);
        if (memcmp(str.ptr + at + 1, part.ptr + 1, part.count - 1) == 0) {
            return at;
        }

        at++;
    }

    return str.count;
}

bool StringContains(String8 str, String8 part) {
    return part.count == 0 || StringIndexOf(str, part) < str.count;
}

u64 StringIndexOfIgnoringCase(String8 str, String8 part, u64 from) {
    for (u64 at = from; at + part.count <= str.count; at++) {
        if (StringEqualIgnoringCase(String8{.ptr = str.ptr + at, .count = part.count}, part)) {
            return at;
        }
    }

    return str.count;
}

bool StringContainsIgnoringCase(String8 str, String8 part) {
    return part.count == 0 || StringIndexOfIgnoringCase(str, part) < str.count;
}

String8 StringTrimLeft(String8 str) {
    while (str.count > 0 && CharIsSpace(str.ptr[0])) {
        str.ptr++;
        str.count--;
    }

    return str;
}

String8 StringTrimRight(String8 str) {
    while (str.count > 0 && CharIsSpace(str.ptr[str.count - 1])) {
        str.count--;
    }

    return str;
}

String8 StringTrim(String8 str) {
    return StringTrimRight(StringTrimLeft(str));
}

bool StringIsBlank(String8 str) {
    return StringTrimLeft(str).count == 0;
}

String8 StringLower(ArenaAllocator* arena, String8 str) {
    String8 result = StringCopy(arena, str);
    for (u64 i = 0; i < result.count; i++) {
        result.ptr[i] = CharToLower(result.ptr[i]);
    }

    return result;
}

String8 StringUpper(ArenaAllocator* arena, String8 str) {
    String8 result = StringCopy(arena, str);
    for (u64 i = 0; i < result.count; i++) {
        result.ptr[i] = CharToUpper(result.ptr[i]);
    }

    return result;
}

bool StringParseU64(String8 str, u64* out_value) {
    if (str.count == 0) {
        return false;
    }

    u64 value = 0;
    for (u64 i = 0; i < str.count; i++) {
        if (!CharIsDigit(str.ptr[i])) {
            return false;
        }
        u64 digit = (u64)(str.ptr[i] - '0');
        if (value > (0xFFFFFFFFFFFFFFFFull - digit) / 10) {
            return false;
        }
        value = value * 10 + digit;
    }

    *out_value = value;
    return true;
}

bool StringParseU32(String8 str, u32* out_value) {
    u64 value = 0;
    if (!StringParseU64(str, &value) || value > 0xFFFFFFFFull) {
        return false;
    }

    *out_value = (u32)value;
    return true;
}

String8 HexEncode(ArenaAllocator* arena, String8 bytes) {
    static const char digits[] = "0123456789abcdef";
    u8* out = ArenaPushArrayNoZero(arena, u8, bytes.count * 2 + 1);
    for (u64 i = 0; i < bytes.count; i++) {
        out[i * 2] = (u8)digits[bytes.ptr[i] >> 4];
        out[i * 2 + 1] = (u8)digits[bytes.ptr[i] & 15];
    }

    out[bytes.count * 2] = 0;
    return String8{.ptr = out, .count = bytes.count * 2};
}

bool HexDecode(ArenaAllocator* arena, String8 text, String8* out_bytes) {
    if (text.count % 2 != 0) {
        return false;
    }

    u8* out = ArenaPushArrayNoZero(arena, u8, text.count / 2 + 1);
    for (u64 i = 0; i < text.count / 2; i++) {
        i32 high = CharHexValue(text.ptr[i * 2]);
        i32 low = CharHexValue(text.ptr[i * 2 + 1]);
        if (high < 0 || low < 0) {
            return false;
        }
        out[i] = (u8)(high * 16 + low);
    }

    out[text.count / 2] = 0;
    *out_bytes = String8{.ptr = out, .count = text.count / 2};

    return true;
}

String8 PercentEncode(ArenaAllocator* arena, String8 str) {
    static const char digits[] = "0123456789ABCDEF";
    u8* out = ArenaPushArrayNoZero(arena, u8, str.count * 3 + 1);
    u64 length = 0;
    for (u64 i = 0; i < str.count; i++) {
        u8 character = str.ptr[i];
        if (CharIsAlphaNumeric(character) || character == '-' || character == '.' || character == '_' || character == '~') {
            out[length++] = character;
        } else {
            out[length++] = '%';
            out[length++] = (u8)digits[character >> 4];
            out[length++] = (u8)digits[character & 15];
        }
    }

    out[length] = 0;
    return String8{.ptr = out, .count = length};
}

String8 PercentDecode(ArenaAllocator* arena, String8 str, bool plus_is_space) {
    u8* out = ArenaPushArrayNoZero(arena, u8, str.count + 1);
    u64 length = 0;
    for (u64 i = 0; i < str.count; i++) {
        u8 character = str.ptr[i];
        if (character == '+' && plus_is_space) {
            out[length++] = ' ';
        } else if (character == '%' && i + 2 < str.count && CharIsHexDigit(str.ptr[i + 1]) && CharIsHexDigit(str.ptr[i + 2])) {
            out[length++] = (u8)(CharHexValue(str.ptr[i + 1]) * 16 + CharHexValue(str.ptr[i + 2]));
            i += 2;
        } else {
            out[length++] = character;
        }
    }

    out[length] = 0;
    return String8{.ptr = out, .count = length};
}

String8 Base64Encode(ArenaAllocator* arena, String8 bytes) {
    static const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    u64 length = ((bytes.count + 2) / 3) * 4;
    u8* out = ArenaPushArrayNoZero(arena, u8, length + 1);
    u64 written = 0;
    for (u64 i = 0; i < bytes.count; i += 3) {
        u32 byte0 = bytes.ptr[i];
        u32 byte1 = i + 1 < bytes.count ? bytes.ptr[i + 1] : 0;
        u32 byte2 = i + 2 < bytes.count ? bytes.ptr[i + 2] : 0;
        u32 triple = (byte0 << 16) | (byte1 << 8) | byte2;
        out[written++] = (u8)table[(triple >> 18) & 0x3F];
        out[written++] = (u8)table[(triple >> 12) & 0x3F];
        out[written++] = i + 1 < bytes.count ? (u8)table[(triple >> 6) & 0x3F] : '=';
        out[written++] = i + 2 < bytes.count ? (u8)table[triple & 0x3F] : '=';
    }

    out[length] = 0;
    return String8{.ptr = out, .count = length};
}

String8 Base64UrlEncode(ArenaAllocator* arena, String8 bytes) {
    String8 encoded = Base64Encode(arena, bytes);
    while (encoded.count > 0 && encoded.ptr[encoded.count - 1] == '=') {
        encoded.count--;
    }

    for (u64 i = 0; i < encoded.count; i++) {
        if (encoded.ptr[i] == '+') {
            encoded.ptr[i] = '-';
        } else if (encoded.ptr[i] == '/') {
            encoded.ptr[i] = '_';
        }
    }

    encoded.ptr[encoded.count] = 0;
    return encoded;
}

static i32 Base64DigitValue(u8 character) {
    if (CharIsUpper(character)) {
        return character - 'A';
    }

    if (CharIsLower(character)) {
        return character - 'a' + 26;
    }

    if (CharIsDigit(character)) {
        return character - '0' + 52;
    }

    if (character == '+' || character == '-') {
        return 62;
    }

    if (character == '/' || character == '_') {
        return 63;
    }

    return -1;
}

bool Base64Decode(ArenaAllocator* arena, String8 text, String8* out_bytes) {
    u8* out = ArenaPushArray(arena, u8, text.count / 4 * 3 + 3);
    u64 length = 0;
    u32 accumulator = 0;
    u32 bit_count = 0;
    for (u64 i = 0; i < text.count; i++) {
        u8 character = text.ptr[i];
        if (character == '=' || CharIsSpace(character)) {
            continue;
        }
        i32 digit = Base64DigitValue(character);
        if (digit < 0) {
            return false;
        }
        accumulator = (accumulator << 6) | (u32)digit;
        bit_count += 6;
        if (bit_count >= 8) {
            bit_count -= 8;
            out[length++] = (u8)((accumulator >> bit_count) & 0xFF);
        }
    }
    out[length] = 0;
    *out_bytes = String8{.ptr = out, .count = length};

    return true;
}

u32 Utf8Decode(String8 text, u64 offset, u32* out_codepoint) {
    u8 lead = text.ptr[offset];
    u64 remaining = text.count - offset;
    u32 sequence_length = 0;
    u32 codepoint = 0;
    if (lead < 0x80) {
        *out_codepoint = lead;
        return 1;
    } else if ((lead & 0xE0) == 0xC0) {
        sequence_length = 2;
        codepoint = lead & 0x1F;
    } else if ((lead & 0xF0) == 0xE0) {
        sequence_length = 3;
        codepoint = lead & 0x0F;
    } else if ((lead & 0xF8) == 0xF0) {
        sequence_length = 4;
        codepoint = lead & 0x07;
    } else {
        *out_codepoint = 0xFFFD;
        return 1;
    }

    if (remaining < sequence_length) {
        *out_codepoint = 0xFFFD;
        return 1;
    }

    for (u32 i = 1; i < sequence_length; i++) {
        u8 byte = text.ptr[offset + i];
        if (!Utf8IsContinuation(byte)) {
            *out_codepoint = 0xFFFD;
            return 1;
        }

        codepoint = (codepoint << 6) | (byte & 0x3F);
    }

    *out_codepoint = codepoint;
    return sequence_length;
}

u32 Utf8Encode(u32 codepoint, u8* out_bytes) {
    if (codepoint < 0x80) {
        out_bytes[0] = (u8)codepoint;
        return 1;
    }

    if (codepoint < 0x800) {
        out_bytes[0] = (u8)(0xC0 | (codepoint >> 6));
        out_bytes[1] = (u8)(0x80 | (codepoint & 0x3F));
        return 2;
    }

    if (codepoint < 0x10000) {
        out_bytes[0] = (u8)(0xE0 | (codepoint >> 12));
        out_bytes[1] = (u8)(0x80 | ((codepoint >> 6) & 0x3F));
        out_bytes[2] = (u8)(0x80 | (codepoint & 0x3F));
        return 3;
    }

    out_bytes[0] = (u8)(0xF0 | (codepoint >> 18));
    out_bytes[1] = (u8)(0x80 | ((codepoint >> 12) & 0x3F));
    out_bytes[2] = (u8)(0x80 | ((codepoint >> 6) & 0x3F));
    out_bytes[3] = (u8)(0x80 | (codepoint & 0x3F));

    return 4;
}

u64 Utf8Previous(const char* text, u64 offset) {
    if (offset == 0) {
        return 0;
    }

    u64 position = offset - 1;
    while (position > 0 && Utf8IsContinuation((u8)text[position])) {
        position--;
    }

    return position;
}

u64 Utf8Next(const char* text, u64 length, u64 offset) {
    if (offset >= length) {
        return length;
    }

    u64 position = offset + 1;
    while (position < length && Utf8IsContinuation((u8)text[position])) {
        position++;
    }

    return position;
}

} // namespace kraft