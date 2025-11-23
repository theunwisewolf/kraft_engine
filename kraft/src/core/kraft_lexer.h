#pragma once

// Much of this code and inspiration has been taken from the excellent article
// by Gabriel Sassone at https://jorenjoestar.github.io/
// The code generator article is used as a ref for this -
// https://jorenjoestar.github.io/post/writing_a_simple_code_generator/

#pragma once

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"

namespace kraft {

namespace TokenType {
enum Enum : u32
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

static String8 strings[] = {
    String8Raw("TOKEN_TYPE_UNKNOWN"),
    String8Raw("TOKEN_TYPE_OPEN_PARENTHESIS"),
    String8Raw("TOKEN_TYPE_CLOSE_PARENTHESIS"),
    String8Raw("TOKEN_TYPE_OPEN_BRACE"),
    String8Raw("TOKEN_TYPE_CLOSE_BRACE"),
    String8Raw("TOKEN_TYPE_OPEN_BRACKET"),
    String8Raw("TOKEN_TYPE_CLOSE_BRACKET"),
    String8Raw("TOKEN_TYPE_OPEN_ANGLE_BRACKET"),
    String8Raw("TOKEN_TYPE_CLOSE_ANGLE_BRACKET"),
    String8Raw("TOKEN_TYPE_EQUALS"),
    String8Raw("TOKEN_TYPE_HASH"),
    String8Raw("TOKEN_TYPE_COMMA"),
    String8Raw("TOKEN_TYPE_COLON"),
    String8Raw("TOKEN_TYPE_SEMICOLON"),
    String8Raw("TOKEN_TYPE_ASTERISK"),
    String8Raw("TOKEN_TYPE_STRING"),
    String8Raw("TOKEN_TYPE_IDENTIFIER"),
    String8Raw("TOKEN_TYPE_NUMBER"),
    String8Raw("TOKEN_TYPE_END_OF_STREAM"),
};

static String8 String(Enum value)
{
    return (value < Enum::TOKEN_TYPE_COUNT ? strings[(int)value] : String8Raw("Unknown"));
}
} // namespace TokenType

struct LexerToken
{
    TokenType::Enum type;
    String8         text;
    float64         float_value;

    bool    MatchesKeyword(String8 expected_keyword) const;
    String8 ToString8();
};

struct LexerNamedToken
{
    String8    key;
    LexerToken value;
};

#pragma clang diagnostic pop

struct LexerToken;

namespace TokenType {
enum Enum : u32;
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
    String8 text;

    // Current cursor position
    u64 position;

    // Current cursor line
    u32 line;

    // Current cursor column on the line
    u32 column;

    // If an error occurs, this is set to something other than LEXER_ERROR_NONE
    LexerError error = LEXER_ERROR_NONE;

    void Create(String8 text);
    void Destroy();

    LexerError NextToken(LexerToken* out_token);
    LexerError ParseNumber(const LexerToken* token, f64* out_number);
    LexerError ConsumeWhitespaces();

    KRAFT_INLINE bool ReachedEOF()
    {
        return (position == text.count);
    }

    KRAFT_INLINE u64 BytesLeft()
    {
        return (text.count - position);
    }

    KRAFT_INLINE void Advance()
    {
        this->position++;
    }

    // Moves the cursor to this position
    KRAFT_INLINE LexerError MoveCursorTo(u64 position)
    {
        if (this->position >= text.count)
        {
            return LexerError::LEXER_ERROR_UNEXPECTED_EOF;
        }

        this->position = position;
        return LexerError::LEXER_ERROR_NONE;
    }

    // Expect the token to be of the given type
    // Sets an error if types do not match
    bool ExpectToken(LexerToken* token, TokenType::Enum expected_type);
    bool ExpectToken(LexerToken* token, TokenType::Enum expected_type, String8 expected_keyword);

    // Check if the token is of the given type
    bool EqualsToken(LexerToken* token, TokenType::Enum expected_type);

    // Check the current token for errors
    bool CheckToken(const LexerToken* token, TokenType::Enum expected_type);

    // Checks if the provided token matches the expected keyword
    bool ExpectKeyword(const LexerToken* token, String8 expected_keyword);

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