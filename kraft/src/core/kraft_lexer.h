#pragma once

#include <core/kraft_core.h>

// Much of this code and inspiration has been taken from the excellent article
// by Gabriel Sassone at https://jorenjoestar.github.io/
// The code generator article is used as a ref for this -
// https://jorenjoestar.github.io/post/writing_a_simple_code_generator/

namespace kraft {

struct LexerToken;

namespace TokenType {
enum Enum : uint32;
}

enum LexerError
{
    LEXER_ERROR_NONE,
    LEXER_ERROR_UNEXPECTED_EOF,
    LEXER_ERROR_UNTERMINATED_COMMENT,
    LEXER_ERROR_UNTERMINATED_STRING,
    LEXER_ERROR_UNEXPECTED_CHARACTER,
};

struct Lexer
{
    // Input text
    char* Text;

    // Length of the input text
    uint64 Length;

    // Current cursor position
    uint64 Position;

    // Current cursor line
    uint32 Line;

    // Current cursor column on the line
    uint32 Column;

    // If an error occurs, this is set to something other than LEXER_ERROR_NONE
    LexerError Error = LEXER_ERROR_NONE;

    void Create(char* Text, uint64 Length);
    void Destroy();

    LexerError NextToken(LexerToken* OutToken);
    LexerError ParseNumber(const LexerToken* Token, double* OutNumber);
    LexerError ConsumeWhitespaces();

    KRAFT_INLINE bool ReachedEOF()
    {
        return (this->Position == this->Length);
    }

    KRAFT_INLINE uint64 BytesLeft()
    {
        return (this->Length - this->Position);
    }

    KRAFT_INLINE void Advance()
    {
        this->Position++;
    }

    // Moves the cursor to this position
    KRAFT_INLINE LexerError MoveCursorTo(uint64 Position)
    {
        if (this->Position >= this->Length)
        {
            return LexerError::LEXER_ERROR_UNEXPECTED_EOF;
        }

        this->Position = Position;
        return LexerError::LEXER_ERROR_NONE;
    }

    // Expect the token to be of the given type
    // Sets an error if types do not match
    bool ExpectToken(LexerToken* Token, TokenType::Enum ExpectedType);
    bool ExpectToken(LexerToken* Token, TokenType::Enum ExpectedType, const String& ExpectedKeyword);

    // Check if the token is of the given type
    bool EqualsToken(LexerToken* Token, TokenType::Enum ExpectedType);

    // Check the current token for errors
    bool CheckToken(const LexerToken* Token, TokenType::Enum ExpectedType);

    // Checks if the provided token matches the expected keyword
    bool ExpectKeyword(const LexerToken* Token, String ExpectedKeyword);

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

} // namespace kraft