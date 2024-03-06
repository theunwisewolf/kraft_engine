#pragma once

#include "core/kraft_core.h"
#include "core/kraft_string.h"

// Much of this code and inspiration has been taken from the excellent article
// by Gabriel Sassone at https://jorenjoestar.github.io/
// The code generator article is used as a ref for this - 
// https://jorenjoestar.github.io/post/writing_a_simple_code_generator/

namespace kraft
{

enum TokenType
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
};

struct Token
{
    TokenType Type;
    char*     Text;
    uint64    Length;
    double    FloatValue;

    KRAFT_INLINE constexpr bool MatchesKeyword(const String& ExpectedKeyword) const 
    {
        if (this->Length != ExpectedKeyword.Length) return false;
        return MemCmp(this->Text, *ExpectedKeyword, this->Length) == 0;
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
    bool ExpectToken(Token& Token, TokenType ExpectedType);
    bool ExpectToken(Token& Token, TokenType ExpectedType, const String& ExpectedKeyword);

    // Check if the token is of the given type
    bool EqualsToken(Token& Token, TokenType ExpectedType);

    // Check the current token for errors
    bool CheckToken(Token& Token, TokenType ExpectedType);

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