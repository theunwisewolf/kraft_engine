#pragma once

#include <core/kraft_core.h>

// Much of this code and inspiration has been taken from the excellent article
// by Gabriel Sassone at https://jorenjoestar.github.io/
// The code generator article is used as a ref for this -
// https://jorenjoestar.github.io/post/writing_a_simple_code_generator/

namespace kraft {

struct Token;

namespace TokenType {
enum Enum : uint32;
}

struct Lexer
{
    char*  Text;
    uint64 Position;
    uint64 Line;
    uint64 Column;
    uint64 ErrorLine;
    bool   Error;

    void Create(char* Text);
    void Destroy();

    Token  NextToken();
    double ParseNumber(Token& Token);
    void   ConsumeWhitespaces();

    // Expect the token to be of the given type
    // Sets an error if types do not match
    bool ExpectToken(Token* Token, TokenType::Enum ExpectedType);
    bool ExpectToken(Token* Token, TokenType::Enum ExpectedType, const String& ExpectedKeyword);

    // Check if the token is of the given type
    bool EqualsToken(Token* Token, TokenType::Enum ExpectedType);

    // Check the current token for errors
    bool CheckToken(const Token& Token, TokenType::Enum ExpectedType);

    // Checks if the provided token matches the expected keyword
    bool ExpectKeyword(const Token& Token, String ExpectedKeyword);

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