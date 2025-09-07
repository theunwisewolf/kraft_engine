#include "kraft_lexer.h"

#include <core/kraft_string.h>
#include <core/kraft_string_view.h>

#include <core/kraft_lexer_types.h>

#include <core/kraft_base_includes.h>

#define CHECK_EOF_ERROR()                                                                                                                                                                              \
    if (this->ReachedEOF())                                                                                                                                                                            \
        return LEXER_ERROR_UNEXPECTED_EOF;

namespace kraft {

bool LexerToken::MatchesKeyword(const String& ExpectedKeyword) const
{
    if (this->Length != ExpectedKeyword.Length)
        return false;
    return MemCmp(this->Text, *ExpectedKeyword, this->Length) == 0;
}

kraft::String LexerToken::ToString() const
{
    return kraft::String(Text, Length);
}

String8 LexerToken::ToString8()
{
    return String8FromPtrAndLength((u8*)Text, Length);
}

kraft::StringView LexerToken::ToStringView() const
{
    return kraft::StringView(Text, Length);
}

void Lexer::Create(char* Text, uint64 Length)
{
    this->Text = Text;
    this->Position = 0;
    this->Line = 0;
    this->Column = 0;
    this->Error = LEXER_ERROR_NONE;
    this->Length = Length;
}

LexerError Lexer::NextToken(LexerToken* OutToken)
{
    CHECK_EOF_ERROR();

    // Skip through whitespaces
    this->ConsumeWhitespaces();
    OutToken->Type = TokenType::TOKEN_TYPE_UNKNOWN;

    char Char = this->Text[this->Position];
    OutToken->Text = this->Text + this->Position;
    OutToken->Length = 1;
    OutToken->FloatValue = 0.0;
    this->Position++;

    switch (Char)
    {
        case 0:
        {
            OutToken->Type = TokenType::TOKEN_TYPE_END_OF_STREAM;
        }
        break;

        // Symbols
        case '(':
        {
            OutToken->Type = TokenType::TOKEN_TYPE_OPEN_PARENTHESIS;
        }
        break;
        case ')':
        {
            OutToken->Type = TokenType::TOKEN_TYPE_CLOSE_PARENTHESIS;
        }
        break;
        case '{':
        {
            OutToken->Type = TokenType::TOKEN_TYPE_OPEN_BRACE;
        }
        break;
        case '}':
        {
            OutToken->Type = TokenType::TOKEN_TYPE_CLOSE_BRACE;
        }
        break;
        case '[':
        {
            OutToken->Type = TokenType::TOKEN_TYPE_OPEN_BRACKET;
        }
        break;
        case ']':
        {
            OutToken->Type = TokenType::TOKEN_TYPE_CLOSE_BRACKET;
        }
        break;
        case '<':
        {
            OutToken->Type = TokenType::TOKEN_TYPE_OPEN_ANGLE_BRACKET;
        }
        break;
        case '>':
        {
            OutToken->Type = TokenType::TOKEN_TYPE_CLOSE_ANGLE_BRACKET;
        }
        break;
        case '=':
        {
            OutToken->Type = TokenType::TOKEN_TYPE_EQUALS;
        }
        break;
        case '#':
        {
            OutToken->Type = TokenType::TOKEN_TYPE_HASH;
        }
        break;
        case ',':
        {
            OutToken->Type = TokenType::TOKEN_TYPE_COMMA;
        }
        break;
        case ':':
        {
            OutToken->Type = TokenType::TOKEN_TYPE_COLON;
        }
        break;
        case ';':
        {
            OutToken->Type = TokenType::TOKEN_TYPE_SEMICOLON;
        }
        break;
        case '*':
        {
            OutToken->Type = TokenType::TOKEN_TYPE_ASTERISK;
        }
        break;

        case '"':
        {
            CHECK_EOF_ERROR();

            OutToken->Type = TokenType::TOKEN_TYPE_STRING;
            OutToken->Text = this->Text + this->Position; // Skip the starting quote
            while (this->Text[this->Position] != '"')
            {
                // Handle escape character
                if (this->Text[this->Position] == '\\')
                {
                    this->Position++; // Skip the escape backslash '\'
                    CHECK_EOF_ERROR();
                }

                // Newlines aren't allowed inside string literals
                if (this->IsNewLine(this->Text[this->Position]))
                {
                    return LEXER_ERROR_UNTERMINATED_STRING;
                }

                this->Position++;
                CHECK_EOF_ERROR();
            }

            OutToken->Length = this->Text + this->Position - OutToken->Text;
            this->Position++; // Skip the end quote
        }
        break;

        default:
        {
            if (IsAlpha(Char) || Char == '_')
            {
                OutToken->Type = TokenType::TOKEN_TYPE_IDENTIFIER;
                while (IsAlpha(this->Text[this->Position]) || IsNumber(this->Text[this->Position]) || this->Text[this->Position] == '_')
                {
                    Position++;
                    CHECK_EOF_ERROR();
                }

                OutToken->Length = this->Text + this->Position - OutToken->Text;
            }
            else if (IsNumber(Char) || Char == '-')
            {
                OutToken->Type = TokenType::TOKEN_TYPE_NUMBER;
                this->Position--;

                ParseNumber(OutToken, &OutToken->FloatValue);
                OutToken->Length = this->Text + this->Position - OutToken->Text;
            }
            else
            {
                return LEXER_ERROR_UNEXPECTED_CHARACTER;
            }
        }
        break;
    }

    return LEXER_ERROR_NONE;
}

LexerError Lexer::ParseNumber(const LexerToken* Token, double* OutNumber)
{
    CHECK_EOF_ERROR();

    // Number formats: 10, -10, 10.10, 000.10

    // Consume the sign
    int32 Sign = 1;
    if (this->Text[this->Position] == '-')
    {
        Sign = -1;
        this->Position++;
        CHECK_EOF_ERROR();
    }

    // Consume starting zeroes
    while (this->Text[this->Position] == '0')
    {
        this->Position++;
        CHECK_EOF_ERROR();
    }

    // Consume decimal part
    int32 Decimal = 0;
    if (IsNumber(this->Text[this->Position]))
    {
        Decimal = this->Text[this->Position] - '0';
        this->Position++;
        CHECK_EOF_ERROR();

        while (IsNumber(this->Text[this->Position]))
        {
            Decimal = Decimal * 10 + this->Text[this->Position] - '0';
            this->Position++;
            CHECK_EOF_ERROR();
        }
    }

    // Consume fractional part
    int32 Fraction = 0;
    int32 FractionalDivisor = 1;
    if (this->Text[this->Position] == '.')
    {
        this->Position++;
        CHECK_EOF_ERROR();

        while (IsNumber(this->Text[this->Position]))
        {
            Fraction = Fraction * 10 + this->Text[this->Position] - '0';
            FractionalDivisor *= 10;

            this->Position++;
            CHECK_EOF_ERROR();
        }
    }

    *OutNumber = (double)Sign * ((double)Decimal + ((double)Fraction / (double)FractionalDivisor));

    return LEXER_ERROR_NONE;
}

LexerError Lexer::ConsumeWhitespaces()
{
    while (true)
    {
        CHECK_EOF_ERROR();

        if (IsSpace(this->Text[this->Position]))
        {
            if (IsNewLine(this->Text[this->Position]))
                Line++;

            this->Position++;
        }
        // Comments - Single-line
        else if (this->BytesLeft() >= 2 && this->Text[this->Position] == '/' && this->Text[this->Position + 1] == '/')
        {
            this->Position += 2;

            CHECK_EOF_ERROR();
            while (this->Text[this->Position] && !IsNewLine(this->Text[this->Position]))
            {
                this->Position++;
                CHECK_EOF_ERROR();
            }
        }
        // Comments - Multi-line
        else if (this->BytesLeft() >= 2 && this->Text[this->Position] == '/' && this->Text[this->Position + 1] == '*')
        {
            this->Position += 2;

            CHECK_EOF_ERROR();
            while (this->BytesLeft() >= 2 && this->Text[this->Position] != '*' && this->Text[this->Position + 1] != '/')
            {
                if (IsNewLine(this->Text[this->Position]))
                    Line++;

                this->Position++;
                CHECK_EOF_ERROR();
            }

            if (this->BytesLeft() >= 2 && this->Text[this->Position] == '*' && this->Text[this->Position + 1] == '/')
            {
                this->Position += 2;
            }
            else
            {
                return LEXER_ERROR_UNTERMINATED_COMMENT;
            }
        }
        else
        {
            break;
        }
    }

    return LEXER_ERROR_NONE;
}

// Expect the token to be of the given type
// Sets an error if types do not match
bool Lexer::ExpectToken(LexerToken* Token, TokenType::Enum ExpectedType)
{
    if (this->NextToken(Token) != LEXER_ERROR_NONE)
    {
        return false;
    }

    if (Token->Type != ExpectedType)
    {
        return false;
    }

    return true;
}

bool Lexer::ExpectToken(LexerToken* Token, TokenType::Enum ExpectedType, const String& ExpectedKeyword)
{
    return ExpectToken(Token, ExpectedType) && Token->MatchesKeyword(ExpectedKeyword);
}

// Check if the token is of the given type
bool Lexer::EqualsToken(LexerToken* Token, TokenType::Enum ExpectedType)
{
    if (this->NextToken(Token) != LEXER_ERROR_NONE)
    {
        return false;
    }

    if (Token->Type != ExpectedType)
    {
        return false;
    }

    return true;
}

// Check the current token for errors
bool Lexer::CheckToken(const LexerToken* Token, TokenType::Enum ExpectedType)
{
    // TODO:
    return true;
}

bool Lexer::ExpectKeyword(const LexerToken* Token, String ExpectedKeyword)
{
    if (Token->Length != ExpectedKeyword.Length)
        return false;

    return MemCmp(Token->Text, *ExpectedKeyword, Token->Length) == 0;
}

} // namespace kraft