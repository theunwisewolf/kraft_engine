#include "kraft_lexer.h"

namespace kraft
{

void Lexer::Create(char* Text)
{
    this->Text = Text;
    this->Position = 0;
    this->Line = 0;
    this->Column = 0;
    this->ErrorLine = 0;
    this->Error = false;
}

Token Lexer::NextToken()
{
    // Skip through whitespaces
    this->ConsumeWhitespaces();

    Token Token;
    Token.Type = TOKEN_TYPE_UNKNOWN;

    char Char = this->Text[this->Position];
    Token.Text = this->Text + this->Position;
    Token.Length = 1;
    Token.FloatValue = 0.0;
    this->Position++;

    switch (Char)
    {
        case 0:
        {
            Token.Type = TOKEN_TYPE_END_OF_STREAM;
        } break;

        // Symbols
        case '(':
        {
            Token.Type = TOKEN_TYPE_OPEN_PARENTHESIS;
        } break;
        case ')':
        {
            Token.Type = TOKEN_TYPE_CLOSE_PARENTHESIS;
        } break;
        case '{':
        {
            Token.Type = TOKEN_TYPE_OPEN_BRACE;
        } break;
        case '}':
        {
            Token.Type = TOKEN_TYPE_CLOSE_BRACE;
        } break;
        case '[':
        {
            Token.Type = TOKEN_TYPE_OPEN_BRACKET;
        } break;
        case ']':
        {
            Token.Type = TOKEN_TYPE_CLOSE_BRACKET;
        } break;
        case '<':
        {
            Token.Type = TOKEN_TYPE_OPEN_ANGLE_BRACKET;
        } break;
        case '>':
        {
            Token.Type = TOKEN_TYPE_CLOSE_ANGLE_BRACKET;
        } break;
        case '=':
        {
            Token.Type = TOKEN_TYPE_EQUALS;
        } break;
        case '#':
        {
            Token.Type = TOKEN_TYPE_HASH;
        } break;
        case ',':
        {
            Token.Type = TOKEN_TYPE_COMMA;
        } break;
        case ':':
        {
            Token.Type = TOKEN_TYPE_COLON;
        } break;
        case ';':
        {
            Token.Type = TOKEN_TYPE_SEMICOLON;
        } break;
        case '*':
        {
            Token.Type = TOKEN_TYPE_ASTERISK;
        } break;

        case '"':
        {
            Token.Type = TOKEN_TYPE_STRING;
            Token.Text = this->Text + this->Position; // To skip the starting quote
            while (this->Text[this->Position] && Text[this->Position] != '"')
            {
                // Handle escape character
                if (Text[this->Position] == '\\' && Text[this->Position+1])
                    this->Position++;

                this->Position++;
            }

            Token.Length = this->Text + this->Position - Token.Text;
            this->Position++; // Skip the end quote
        } break;

        default:
        {
            if (IsAlpha(Char))
            {
                Token.Type = TOKEN_TYPE_IDENTIFIER;
                while (IsAlpha(Text[this->Position]) || IsNumber(Text[this->Position]) || Text[this->Position] == '_')
                {
                    Position++;
                }

                Token.Length = this->Text + this->Position - Token.Text;
            }
            else if (IsNumber(Char) || Char == '-')
            {
                Token.Type = TOKEN_TYPE_NUMBER;
                this->Position--;

                Token.FloatValue = ParseNumber(Token);
                Token.Length = this->Text + this->Position - Token.Text;
            }
        } break;
    }

    return Token;
}

double Lexer::ParseNumber(Token& Token)
{
    // Number formats: 10, -10, 10.10, 000.10

    // Consume the sign
    int32 Sign = 1;
    if (this->Text[this->Position] == '-')
    {
        Sign = -1;
        this->Position++;
    }

    // Consume starting zeroes
    while (this->Text[this->Position] == '0')
        this->Position++;

    // Consume decimal part
    int32 Decimal = 0;
    if (IsNumber(this->Text[this->Position]))
    {
        Decimal = this->Text[this->Position] - '0';
        this->Position++;

        while (IsNumber(this->Text[this->Position]))
        {
            Decimal = Decimal * 10 + this->Text[this->Position] - '0';
            this->Position++;
        }
    }

    // Consume fractional part
    int32 Fraction = 0;
    int32 FractionalDivisor = 1;
    if (this->Text[this->Position] == '.')
    {
        this->Position++;

        while (IsNumber(this->Text[this->Position]))
        {
            Fraction = Fraction * 10 + this->Text[this->Position] - '0';
            FractionalDivisor *= 10;

            this->Position++;
        }
    }

    double Number = (double)Sign * ((double)Decimal + ((double)Fraction / (double)FractionalDivisor));
    return Number;
}

void Lexer::ConsumeWhitespaces()
{
    while (true)
    {
        if (IsSpace(this->Text[this->Position]))
        {
            if (IsNewLine(this->Text[this->Position]))
                Line++;
            
            this->Position++;
        }
        // Comments - Single-line
        else if (this->Text[this->Position] == '/' && this->Text[this->Position+1] == '/')
        {
            this->Position += 2;
            while (this->Text[this->Position] && !IsNewLine(this->Text[this->Position]))
                this->Position++;
        }
        // Comments - Multi-line
        else if (this->Text[this->Position] == '/' && this->Text[this->Position+1] == '*')
        {
            this->Position += 2;
            while (this->Text[this->Position] != '*' && this->Text[this->Position+1] != '/')
            {
                if (IsNewLine(this->Text[this->Position]))
                    Line++;

                this->Position++;
            }

            if (this->Text[this->Position] == '*' && this->Text[this->Position+1] == '/') 
            {
                this->Position += 2;
            }
        }
        else
        {
            break;
        }
    }
}

// Expect the token to be of the given type
// Sets an error if types do not match
bool Lexer::ExpectToken(Token& Token, TokenType ExpectedType)
{
    Token = this->NextToken();
    if (Token.Type != ExpectedType)
    {
        this->Error = true;
        this->ErrorLine = this->Line;
        return false;
    }

    return true;
}

bool Lexer::ExpectToken(Token& Token, TokenType ExpectedType, const String& ExpectedKeyword)
{
    return ExpectToken(Token, ExpectedType) && Token.MatchesKeyword(ExpectedKeyword);
}

// Check if the token is of the given type
bool Lexer::EqualsToken(Token& Token, TokenType ExpectedType)
{
    Token = this->NextToken();
    if (Token.Type != ExpectedType)
    {
        return false;
    }

    return true;
}

// Check the current token for errors
bool Lexer::CheckToken(Token& Token, TokenType ExpectedType)
{
    
}

}