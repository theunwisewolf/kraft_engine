#pragma once

#include "core/kraft_core.h"
#include "core/kraft_string.h"
#include "core/kraft_string_view.h"

// Much of this code and inspiration has been taken from the excellent article
// by Gabriel Sassone at https://jorenjoestar.github.io/
// The code generator article is used as a ref for this - 
// https://jorenjoestar.github.io/post/writing_a_simple_code_generator/

namespace kraft
{

namespace TokenType
{
    enum Enum 
    {
        TOKEN_TYPE_UNKNOWN,

        // Symbols
        TOKEN_TYPE_OPEN_PARENTHESIS,            // (
        TOKEN_TYPE_CLOSE_PARENTHESIS,           // )
        TOKEN_TYPE_OPEN_BRACE,                  // {
        TOKEN_TYPE_CLOSE_BRACE,                 // }
        TOKEN_TYPE_OPEN_BRACKET,                // [
        TOKEN_TYPE_CLOSE_BRACKET,               // ]
        TOKEN_TYPE_OPEN_ANGLE_BRACKET,          // <
        TOKEN_TYPE_CLOSE_ANGLE_BRACKET,         // >
        TOKEN_TYPE_EQUALS,                      // =
        TOKEN_TYPE_HASH,                        // #
        TOKEN_TYPE_COMMA,                       // ,
        TOKEN_TYPE_COLON,                       // :
        TOKEN_TYPE_SEMICOLON,                   // ;
        TOKEN_TYPE_ASTERISK,                    // *

        TOKEN_TYPE_STRING,
        TOKEN_TYPE_IDENTIFIER,
        TOKEN_TYPE_NUMBER,
        TOKEN_TYPE_END_OF_STREAM,

        TOKEN_TYPE_COUNT
    };

    static const char* Strings[] = 
    {
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
}

struct Token
{
    TokenType::Enum Type;
    char*     Text;
    uint64    Length;
    float64   FloatValue;

    KRAFT_INLINE constexpr bool MatchesKeyword(const String& ExpectedKeyword) const 
    {
        if (this->Length != ExpectedKeyword.Length) return false;
        return MemCmp(this->Text, *ExpectedKeyword, this->Length) == 0;
    }

    KRAFT_INLINE kraft::String String()
    {
        return kraft::String(Text, Length);
    }

    KRAFT_INLINE kraft::StringView StringView()
    {
        return kraft::StringView(Text, Length);
    }
};

struct Lexer
{
    char*   Text;
    uint64  Position;
    uint64  Line;
    uint64  Column;
    uint64  ErrorLine;
    bool    Error;

    void Create(char* Text);
    void Destroy();

    Token NextToken();
    double ParseNumber(Token& Token);
    void ConsumeWhitespaces();

    // Expect the token to be of the given type
    // Sets an error if types do not match
    bool ExpectToken(Token& Token, TokenType::Enum ExpectedType);
    bool ExpectToken(Token& Token, TokenType::Enum ExpectedType, const String& ExpectedKeyword);

    // Check if the token is of the given type
    bool EqualsToken(Token& Token, TokenType::Enum ExpectedType);

    // Check the current token for errors
    bool CheckToken(Token& Token, TokenType::Enum ExpectedType);

    // Checks if the provided token matches the expected keyword
    bool ExpectKeyword(Token Token, String ExpectedKeyword)
    {
        if (Token.Length != ExpectedKeyword.Length) return false;

        return MemCmp(Token.Text, *ExpectedKeyword, Token.Length) == 0;
    }

    KRAFT_INLINE bool IsNewLine(char c)
    {
        return (c == '\n' || c == '\r');
    }

    KRAFT_INLINE bool IsSpace(char c)
    {
        return (c == ' ' || c == '\t' || c == '\v' || c == '\f' || IsNewLine(c));
    }

    KRAFT_INLINE bool IsAlpha(char c)
    {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
    }

    KRAFT_INLINE bool IsNumber(char c)
    {
        return (c >= '0' && c <= '9');
    }
};

}