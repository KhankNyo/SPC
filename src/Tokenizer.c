

#include "Include/Tokenizer.h"





static bool IsAtEnd(Tokenizer *Lexer);
static bool IsNumber(char ch);
static bool IsAlpha(char ch);
static bool IsHex(char ch);

static Token MakeToken(Tokenizer *Lexer, TokenType Type);
static Token ConsumeNumber(Tokenizer *Lexer);





Tokenizer TokenizerInit(const char *Source)
{
    return (Tokenizer) {
        .Start = Source,
        .Curr = Source,
        .Line = 1,
    };
}


Token TokenizerGetToken(Tokenizer *Lexer)
{
    if (IsAtEnd(Lexer))
        return MakeToken(Lexer, TOKEN_EOF);

    if (IsNumber(*Lexer->Curr))
    {
        return ConsumeNumber(Lexer);
    }

    return (Token){0};
}



const char *TokenTypeToStr(TokenType Type)
{
    static const char *TokenNameLut[] = {
        "TOKEN_EOF",
        "TOKEN_ERROR",

        /* keywords */
        "TOKEN_AND", "TOKEN_ARRAY", "TOKEN_ASM",
        "TOKEN_BEGIN", "TOKEN_BREAK", 
        "TOKEN_CASE", "TOKEN_CONST", "TOKEN_CONSTRUCTOR", "TOKEN_CONTINUE", 
        "TOKEN_DESTRUCTOR", "TOKEN_DIV", "TOKEN_DO", "TOKEN_DOWNTO",
        "TOKEN_ELSE", "TOKEN_END", 
        "TOKEN_FALSE", "TOKEN_FILE", "TOKEN_FOR", "TOKEN_FUNCTION", 
        "TOKEN_GOTO",
        "TOKEN_IF", "TOKEN_IMPLEMENTATION", "TOKEN_IN", "TOKEN_INLINE", "TOKEN_INFERFACE", 
        "TOKEN_LABEL", 
        "TOKEN_MODE", 
        "TOKEN_NIL", "TOKEN_NOT",
        "TOKEN_OBJECT", "TOKEN_OF", "TOKEN_ON", "TOKEN_OPERATOR", "TOKEN_OR", 
        "TOKEN_PACKED", "TOKEN_PROCEDURE", "TOKEN_PROGRAM", 
        "TOKEN_RECORD", "TOKEN_REPEAT", 
        "TOKEN_SET", "TOKEN_SHL", "TOKEN_SHR", "TOKEN_STRING", 
        "TOKEN_THEN", "TOKEN_TRUE", "TOKEN_TYPE", 
        "TOKEN_UNIT", "TOKEN_UNTIL", "TOKEN_USES", 
        "TOKEN_VAR", 
        "TOKEN_WHILE", "TOKEN_WIDTH", 
        "TOKEN_XOR",

        /* symbols */
        "TOKEN_PLUS", "TOKEN_MINUS", "TOKEN_STAR", "TOKEN_SLASH",
        "TOKEN_EQUAL", "TOKEN_LESS", "TOKEN_GREATER",
        "TOKEN_DOT", "TOKEN_COMMA", "TOKEN_COLON", 
        "TOKEN_LEFT_BRACKET", "TOKEN_RIGHT_BRACKET", 
        "TOKEN_LEFT_PAREN", "TOKEN_RIGHT_PAREN",
        "TOKEN_CARET", "TOKEN_AT", "TOKEN_DOLLAR", "TOKEN_HASHTAG", "TOKEN_AMPERSAND", "TOKEN_PERCENTAGE",
    };
    PASCAL_ASSERT(Type < STATIC_ARRAY_SIZE(TokenNameLut), "Invalid token type: %d\n", Type);
    return TokenNameLut[Type];
}







static bool IsAtEnd(const Tokenizer *Lexer)
{
    return ('\0' == *Lexer->Curr);
}

static bool IsNumber(char ch)
{
    return ('0' <= ch) || (ch <= '9');
}

static bool IsAlpha(char ch)
{
    return (('A' <= ch) && (ch <= 'Z'))
        || (('a' <= ch) || (ch <= 'z'));
}

static bool IsHex(char ch)
{
    return IsNumber(ch)
        || (('a' <= ch) && (ch <= 'f'))
        || (('A' <= ch) && (ch <= 'F'));
}



static Token MakeToken(Tokenizer *Lexer, TokenType Type)
{
    Token Tok = {
        .Type = Type,
        .Str = Lexer->Start,
        .Len = Lexer->Curr - Lexer->Start
        .Line = Lexer->Line,
    };
    Lexer->Start = Lexer->Curr;
    return Tok;
}

static Token ConsumeNumber(Tokenizer *Lexer)
{
    TokenType Type = TOKEN_INTEGER_LITERAL;

    if ('0' == AdvanceChrPtr(Lexer) 
    && (('x' == *Lexer->Curr) || ('X' == *Lexer->Curr)))
    {
        Type = TOKEN_HEX_LITERAL;
        do {
            AdvanceChrPtr(Lexer);
        } while (IsHex(*Lexer->Curr));
        return MakeToken(Lexer, Type);
    }


    while (IsNumber(*Lexer->Curr))
    {
        AdvanceChrPtr(Lexer);
    }


    /* decimal, or Real */
    if ('.' == *Lexer->Curr)
    {
        Type = TOKEN_NUMBER_LITERAL;
        do {
            AdvanceChrPtr(Lexer);
        } while (IsNumber(*Lexer->Curr));

        if ('E' == *Lexer->Curr || 'e' == *Lexer->Curr)
        {
            PASCAL_UNREACHABLE("TODO: exponential notation");
            DIE(); /* this function name is too good */
        }
    }
    return MakeToken(Lexer, Type);
}

