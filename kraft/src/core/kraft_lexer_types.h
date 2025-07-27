#pragma once

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"

namespace kraft {

namespace TokenType {
enum Enum : uint32
{
    TOKEN_TYPE_UNKNOWN,

    // Symbols
    TOKEN_TYPE_OPEN_PARENTHESIS,    // (
    TOKEN_TYPE_CLOSE_PARENTHESIS,   // )
    TOKEN_TYPE_OPEN_BRACE,          // {
    TOKEN_TYPE_CLOSE_BRACE,         // }
    TOKEN_TYPE_OPEN_BRACKET,        // [
    TOKEN_TYPE_CLOSE_BRACKET,       // ]
    TOKEN_TYPE_OPEN_ANGLE_BRACKET,  // <
    TOKEN_TYPE_CLOSE_ANGLE_BRACKET, // >
    TOKEN_TYPE_EQUALS,              // =
    TOKEN_TYPE_HASH,                // #
    TOKEN_TYPE_COMMA,               // ,
    TOKEN_TYPE_COLON,               // :
    TOKEN_TYPE_SEMICOLON,           // ;
    TOKEN_TYPE_ASTERISK,            // *

    TOKEN_TYPE_STRING,
    TOKEN_TYPE_IDENTIFIER,
    TOKEN_TYPE_NUMBER,
    TOKEN_TYPE_END_OF_STREAM,

    TOKEN_TYPE_COUNT
};

static const char* Strings[] = {
    "TOKEN_TYPE_UNKNOWN",
    "TOKEN_TYPE_OPEN_PARENTHESIS",
    "TOKEN_TYPE_CLOSE_PARENTHESIS",
    "TOKEN_TYPE_OPEN_BRACE",
    "TOKEN_TYPE_CLOSE_BRACE",
    "TOKEN_TYPE_OPEN_BRACKET",
    "TOKEN_TYPE_CLOSE_BRACKET",
    "TOKEN_TYPE_OPEN_ANGLE_BRACKET",
    "TOKEN_TYPE_CLOSE_ANGLE_BRACKET",
    "TOKEN_TYPE_EQUALS",
    "TOKEN_TYPE_HASH",
    "TOKEN_TYPE_COMMA",
    "TOKEN_TYPE_COLON",
    "TOKEN_TYPE_SEMICOLON",
    "TOKEN_TYPE_ASTERISK",
    "TOKEN_TYPE_STRING",
    "TOKEN_TYPE_IDENTIFIER",
    "TOKEN_TYPE_NUMBER",
    "TOKEN_TYPE_END_OF_STREAM",
};

static const char* String(Enum Value)
{
    return (Value < Enum::TOKEN_TYPE_COUNT ? Strings[(int)Value] : "Unknown");
}
} // namespace TokenType

struct LexerToken
{
    TokenType::Enum Type;
    char*           Text;
    uint64          Length;
    float64         FloatValue;

    bool       MatchesKeyword(const String& ExpectedKeyword) const;
    String     ToString() const;
    StringView ToStringView() const;
};

struct LexerNamedToken
{
    StringView Key;
    LexerToken Value;
};

} // namespace kraft

#pragma clang diagnostic pop