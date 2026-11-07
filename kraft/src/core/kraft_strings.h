#pragma once

#define String8VArg(str) (i32)((str).count), ((str).ptr)

namespace kraft {

struct String8Array {
    String8* ptr;
    u64 count;
};

String8 StringFormat(ArenaAllocator* arena, const char* format, ...);
String8 StringFormatV(ArenaAllocator* arena, const char* format, va_list args);
bool StringEqual(String8 a, String8 b);
bool StringEndsWith(String8 str, String8 suffix);

// Concatenates strings; The resulting string is null-terminated
String8 StringCat(ArenaAllocator* arena, String8 a, String8 b);

// Duplicates a string; The resulting string is null-terminated
String8 StringCopy(ArenaAllocator* arena, String8 str);

u64 CStringLength(u8* c_str);

String8 String8FromPtrAndLength(u8* ptr, u64 length);
String8 String8FromCString(char* c_str);
KRAFT_INLINE String8 String8FromCString(const char* c_str) {
    return String8FromCString((char*)c_str);
}

// String8 from string literals
#define String8Raw(str) kraft::String8FromPtrAndLength((u8*)(str), sizeof(str) - 1)
#define S(str) kraft::String8FromPtrAndLength((u8*)(str), sizeof(str) - 1)

// ASCII character classes; nothing past 127 is in any of them
KRAFT_INLINE bool CharIsDigit(u32 character) {
    return character >= '0' && character <= '9';
}

KRAFT_INLINE bool CharIsUpper(u32 character) {
    return character >= 'A' && character <= 'Z';
}

KRAFT_INLINE bool CharIsLower(u32 character) {
    return character >= 'a' && character <= 'z';
}

KRAFT_INLINE bool CharIsAlpha(u32 character) {
    return CharIsUpper(character) || CharIsLower(character);
}

KRAFT_INLINE bool CharIsAlphaNumeric(u32 character) {
    return CharIsAlpha(character) || CharIsDigit(character);
}

// A letter, a digit or an underscore
KRAFT_INLINE bool CharIsWord(u32 character) {
    return CharIsAlphaNumeric(character) || character == '_';
}

KRAFT_INLINE bool CharIsHexDigit(u32 character) {
    return CharIsDigit(character) || (character >= 'a' && character <= 'f') || (character >= 'A' && character <= 'F');
}

// Space, tab, CR or LF
KRAFT_INLINE bool CharIsSpace(u32 character) {
    return character == ' ' || character == '\t' || character == '\r' || character == '\n';
}

KRAFT_INLINE bool CharIsSpaceOrTab(u32 character) {
    return character == ' ' || character == '\t';
}

KRAFT_INLINE u8 CharToLower(u8 character) {
    return CharIsUpper(character) ? (u8)(character + 32) : character;
}

KRAFT_INLINE u8 CharToUpper(u8 character) {
    return CharIsLower(character) ? (u8)(character - 32) : character;
}

// The hex digit's value, or -1 when it is not one
KRAFT_INLINE i32 CharHexValue(u32 character) {
    if (CharIsDigit(character)) {
        return character - '0';
    }

    if (character >= 'a' && character <= 'f') {
        return character - 'a' + 10;
    }

    if (character >= 'A' && character <= 'F') {
        return character - 'A' + 10;
    }

    return -1;
}

bool StringStartsWith(String8 str, String8 prefix);
bool StringEqualsCString(String8 str, const char* c_str);

// ASCII letters match in either case
bool StringEqualIgnoringCase(String8 a, String8 b);
bool StringStartsWithIgnoringCase(String8 str, String8 prefix);
bool StringEndsWithIgnoringCase(String8 str, String8 suffix);

// Where part first appears at or after from, or str.count when it does not
u64 StringIndexOf(String8 str, String8 part, u64 from = 0);
u64 StringIndexOfIgnoringCase(String8 str, String8 part, u64 from = 0);
bool StringContains(String8 str, String8 part);
bool StringContainsIgnoringCase(String8 str, String8 part);

// Splits str at the first separator into what comes before and after it, and returns whether there was one
// With none, all of str comes before it; nothing is copied
bool StringCut(String8 str, String8 separator, String8* out_before, String8* out_after);

// The string without the spaces, tabs, CRs and LFs at its ends; nothing is copied
String8 StringTrim(String8 str);
String8 StringTrimLeft(String8 str);
String8 StringTrimRight(String8 str);

// Whether it is empty or only spaces, tabs, CRs and LFs
bool StringIsBlank(String8 str);

// A copy with its ASCII letters lowered or raised; null-terminated
String8 StringLower(ArenaAllocator* arena, String8 str);
String8 StringUpper(ArenaAllocator* arena, String8 str);

// False when it is empty, has anything but digits in it or is past what a u64 holds
bool StringParseU64(String8 str, u64* out_value);
bool StringParseU32(String8 str, u32* out_value);

// Two lowercase hex digits a byte; null-terminated
String8 HexEncode(ArenaAllocator* arena, String8 bytes);
// False unless text is an even number of hex digits
bool HexDecode(ArenaAllocator* arena, String8 text, String8* out_bytes);

// %XX for every byte but letters, digits and -._~; null-terminated
String8 PercentEncode(ArenaAllocator* arena, String8 str);
// A % without two hex digits after it is kept as it is
String8 PercentDecode(ArenaAllocator* arena, String8 str, bool plus_is_space);

// Null-terminated
String8 Base64Encode(ArenaAllocator* arena, String8 bytes);
// Base64 with - and _ for + and /, and no padding
String8 Base64UrlEncode(ArenaAllocator* arena, String8 bytes);
// Takes either alphabet, with or without padding; false on any other character
bool Base64Decode(ArenaAllocator* arena, String8 text, String8* out_bytes);

KRAFT_INLINE bool Utf8IsContinuation(u8 byte) {
    return (byte & 0xC0) == 0x80;
}

// Returns the bytes read, at least 1; a bad sequence reads as U+FFFD
u32 Utf8Decode(String8 text, u64 offset, u32* out_codepoint);
// Returns the bytes written, 1 to 4
u32 Utf8Encode(u32 codepoint, u8* out_bytes);
// Returns where the codepoint before offset starts
u64 Utf8Previous(const char* text, u64 offset);
// Returns where the codepoint after offset starts
u64 Utf8Next(const char* text, u64 length, u64 offset);

} // namespace kraft

#ifdef __cplusplus

inline bool operator==(const String8& a, const String8& b) {
    if (a.count != b.count)
        return false;

    for (u64 i = 0; i < a.count; ++i) {
        if (a.ptr[i] != b.ptr[i])
            return false;
    }

    return true;
}

#endif