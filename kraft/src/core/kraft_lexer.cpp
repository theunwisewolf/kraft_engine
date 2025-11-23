#include <core/kraft_string.h>
#include <core/kraft_string_view.h>

#define CHECK_EOF_ERROR()                                                                                                                                                                              \
    if (this->ReachedEOF())                                                                                                                                                                            \
        return LEXER_ERROR_UNEXPECTED_EOF;

namespace kraft {

bool LexerToken::MatchesKeyword(String8 expected_keyword) const
{
    return StringEqual(this->text, expected_keyword);
}

String8 LexerToken::ToString8()
{
    return text;
}

void Lexer::Create(String8 text)
{
    this->text = text;
    this->position = 0;
    this->line = 0;
    this->column = 0;
    this->error = LEXER_ERROR_NONE;
}

LexerError Lexer::NextToken(LexerToken* out_token)
{
    CHECK_EOF_ERROR();

    // Skip through whitespaces
    this->ConsumeWhitespaces();
    out_token->type = TokenType::TOKEN_TYPE_UNKNOWN;

    u8 character = this->text.ptr[this->position];
    out_token->text.ptr = this->text.ptr + this->position;
    out_token->text.count = 1;
    out_token->float_value = 0.0;
    this->position++;

    switch (character)
    {
        case 0:
        {
            out_token->type = TokenType::TOKEN_TYPE_END_OF_STREAM;
        }
        break;

        // Symbols
        case '(':
        {
            out_token->type = TokenType::TOKEN_TYPE_OPEN_PARENTHESIS;
        }
        break;
        case ')':
        {
            out_token->type = TokenType::TOKEN_TYPE_CLOSE_PARENTHESIS;
        }
        break;
        case '{':
        {
            out_token->type = TokenType::TOKEN_TYPE_OPEN_BRACE;
        }
        break;
        case '}':
        {
            out_token->type = TokenType::TOKEN_TYPE_CLOSE_BRACE;
        }
        break;
        case '[':
        {
            out_token->type = TokenType::TOKEN_TYPE_OPEN_BRACKET;
        }
        break;
        case ']':
        {
            out_token->type = TokenType::TOKEN_TYPE_CLOSE_BRACKET;
        }
        break;
        case '<':
        {
            out_token->type = TokenType::TOKEN_TYPE_OPEN_ANGLE_BRACKET;
        }
        break;
        case '>':
        {
            out_token->type = TokenType::TOKEN_TYPE_CLOSE_ANGLE_BRACKET;
        }
        break;
        case '=':
        {
            out_token->type = TokenType::TOKEN_TYPE_EQUALS;
        }
        break;
        case '#':
        {
            out_token->type = TokenType::TOKEN_TYPE_HASH;
        }
        break;
        case ',':
        {
            out_token->type = TokenType::TOKEN_TYPE_COMMA;
        }
        break;
        case ':':
        {
            out_token->type = TokenType::TOKEN_TYPE_COLON;
        }
        break;
        case ';':
        {
            out_token->type = TokenType::TOKEN_TYPE_SEMICOLON;
        }
        break;
        case '*':
        {
            out_token->type = TokenType::TOKEN_TYPE_ASTERISK;
        }
        break;

        case '"':
        {
            CHECK_EOF_ERROR();

            out_token->type = TokenType::TOKEN_TYPE_STRING;
            out_token->text.ptr = this->text.ptr + this->position; // Skip the starting quote
            while (this->text.ptr[this->position] != '"')
            {
                // Handle escape character
                if (this->text.ptr[this->position] == '\\')
                {
                    this->position++; // Skip the escape backslash '\'
                    CHECK_EOF_ERROR();
                }

                // Newlines aren't allowed inside string literals
                if (this->IsNewLine(this->text.ptr[this->position]))
                {
                    return LEXER_ERROR_UNTERMINATED_STRING;
                }

                this->position++;
                CHECK_EOF_ERROR();
            }

            out_token->text.count = this->text.ptr + this->position - out_token->text.ptr;
            this->position++; // Skip the end quote
        }
        break;

        default:
        {
            if (IsAlpha(character) || character == '_')
            {
                out_token->type = TokenType::TOKEN_TYPE_IDENTIFIER;
                while (IsAlpha(this->text.ptr[this->position]) || IsNumber(this->text.ptr[this->position]) || this->text.ptr[this->position] == '_')
                {
                    this->position++;
                    CHECK_EOF_ERROR();
                }

                out_token->text.count = this->text.ptr + this->position - out_token->text.ptr;
            }
            else if (IsNumber(character) || character == '-')
            {
                out_token->type = TokenType::TOKEN_TYPE_NUMBER;
                this->position--;

                ParseNumber(out_token, &out_token->float_value);
                out_token->text.count = this->text.ptr + this->position - out_token->text.ptr;
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
    if (this->text.ptr[this->position] == '-')
    {
        Sign = -1;
        this->position++;
        CHECK_EOF_ERROR();
    }

    // Consume starting zeroes
    while (this->text.ptr[this->position] == '0')
    {
        this->position++;
        CHECK_EOF_ERROR();
    }

    // Consume decimal part
    int32 Decimal = 0;
    if (IsNumber(this->text.ptr[this->position]))
    {
        Decimal = this->text.ptr[this->position] - '0';
        this->position++;
        CHECK_EOF_ERROR();

        while (IsNumber(this->text.ptr[this->position]))
        {
            Decimal = Decimal * 10 + this->text.ptr[this->position] - '0';
            this->position++;
            CHECK_EOF_ERROR();
        }
    }

    // Consume fractional part
    int32 Fraction = 0;
    int32 FractionalDivisor = 1;
    if (this->text.ptr[this->position] == '.')
    {
        this->position++;
        CHECK_EOF_ERROR();

        while (IsNumber(this->text.ptr[this->position]))
        {
            Fraction = Fraction * 10 + this->text.ptr[this->position] - '0';
            FractionalDivisor *= 10;

            this->position++;
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

        if (IsSpace(this->text.ptr[this->position]))
        {
            if (IsNewLine(this->text.ptr[this->position]))
                line++;

            this->position++;
        }
        // Comments - Single-line
        else if (this->BytesLeft() >= 2 && this->text.ptr[this->position] == '/' && this->text.ptr[this->position + 1] == '/')
        {
            this->position += 2;

            CHECK_EOF_ERROR();
            while (this->text.ptr[this->position] && !IsNewLine(this->text.ptr[this->position]))
            {
                this->position++;
                CHECK_EOF_ERROR();
            }
        }
        // Comments - Multi-line
        else if (this->BytesLeft() >= 2 && this->text.ptr[this->position] == '/' && this->text.ptr[this->position + 1] == '*')
        {
            this->position += 2;

            CHECK_EOF_ERROR();
            while (this->BytesLeft() >= 2 && this->text.ptr[this->position] != '*' && this->text.ptr[this->position + 1] != '/')
            {
                if (IsNewLine(this->text.ptr[this->position]))
                    line++;

                this->position++;
                CHECK_EOF_ERROR();
            }

            if (this->BytesLeft() >= 2 && this->text.ptr[this->position] == '*' && this->text.ptr[this->position + 1] == '/')
            {
                this->position += 2;
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
bool Lexer::ExpectToken(LexerToken* token, TokenType::Enum expected_type)
{
    if (this->NextToken(token) != LEXER_ERROR_NONE)
    {
        return false;
    }

    if (token->type != expected_type)
    {
        return false;
    }

    return true;
}

bool Lexer::ExpectToken(LexerToken* token, TokenType::Enum expected_type, String8 expected_keyword)
{
    return ExpectToken(token, expected_type) && token->MatchesKeyword(expected_keyword);
}

// Check if the token is of the given type
bool Lexer::EqualsToken(LexerToken* token, TokenType::Enum expected_type)
{
    if (this->NextToken(token) != LEXER_ERROR_NONE)
    {
        return false;
    }

    if (token->type != expected_type)
    {
        return false;
    }

    return true;
}

// Check the current token for errors
bool Lexer::CheckToken(const LexerToken* token, TokenType::Enum expected_type)
{
    // TODO:
    return true;
}

bool Lexer::ExpectKeyword(const LexerToken* token, String8 expected_keyword)
{
    return StringEqual(token->text, expected_keyword);
}

} // namespace kraft