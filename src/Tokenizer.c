

#include "Include/Tokenizer.h"





static bool IsAtEnd(const Tokenizer *Lexer);
static bool IsNumber(char ch);
static bool IsAlpha(char ch);
static bool IsHex(char ch);

/* returns the token that Curr is pointing at,
 * then increment curr if curr is not pointing at the null terminator */
static char AdvanceChrPtr(Tokenizer *Lexer);

/* if curr is at end, returns 0,
 * else return the char ahead of curr */
static char PeekChr(const Tokenizer *Lexer);

/* skips whitespace, newline, comments */
static void SkipWhitespace(Tokenizer *Lexer);

/* creates a token with the given type */
static Token MakeToken(Tokenizer *Lexer, TokenType Type);

/* consumes a numeric literal and return its token */
static Token ConsumeNumber(Tokenizer *Lexer);

/* consumes an identifier or a keyword */
static Token ConsumeWord(Tokenizer *Lexer);





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
    SkipWhitespace(Lexer);
    if (IsAtEnd(Lexer))
        return MakeToken(Lexer, TOKEN_EOF);

    char CurrChr = *Lexer->Curr;
    if (IsNumber(CurrChr))
    {
        return ConsumeNumber(Lexer);
    }
    if (IsAlpha(CurrChr))
    {
        return ConsumeWord(Lexer);
    }

    switch (CurrChr)
    {
    case '+': return MakeToken(Lexer, TOKEN_PLUS);
    case '-': return MakeToken(Lexer, TOKEN_PLUS);
    case '*': 
    {
        if ('*' == PeekChr(Lexer))
            return MakeToken(Lexer, TOKEN_STAR_STAR);
        if ('=' == PeekChr(Lexer))
            return MakeToken(Lexer, TOKEN_STAR_EQUAL);
        else return MakeToken(Lexer, TOKEN_STAR);
    } break;
    case '/':
    {
        if ('=' == PeekChr(Lexer))
            return MakeToken(Lexer, TOKEN_SLASH_EQUAL);
        else return MakeToken(Lexer, TOKEN_SLASH);
    } break;

    default: break;
    }

    return MakeToken(Lexer, TOKEN_ERROR);
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
        "TOKEN_PLUS_EQUAL", "TOKEN_MINUS_EQUAL", "TOKEN_STAR_EQUAL", "TOKEN_SLASH_EQUAL",
        "TOKEN_STAR_STAR",
        "TOKEN_EQUAL", "TOKEN_LESS", "TOKEN_GREATER",
        "TOKEN_DOT", "TOKEN_COMMA", "TOKEN_COLON", 
        "TOKEN_LEFT_BRACKET", "TOKEN_RIGHT_BRACKET", 
        "TOKEN_LEFT_PAREN", "TOKEN_RIGHT_PAREN",
        "TOKEN_CARET", "TOKEN_AT", "TOKEN_DOLLAR", "TOKEN_HASHTAG", "TOKEN_AMPERSAND", "TOKEN_PERCENTAGE",

        "TOKEN_NUMBER_LITERAL", "TOKEN_HEX_LITERAL", "TOKEN_INTEGER_LITERAL", 
        "TOKEN_STRING_LITERAL", 
        "TOKEN_IDENTIFIER"
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
    return ('0' <= ch) && (ch <= '9');
}

static bool IsAlpha(char ch)
{
    return (('A' <= ch) && (ch <= 'Z'))
        || (('a' <= ch) && (ch <= 'z'));
}

static bool IsHex(char ch)
{
    return IsNumber(ch)
        || (('a' <= ch) && (ch <= 'f'))
        || (('A' <= ch) && (ch <= 'F'));
}



static char AdvanceChrPtr(Tokenizer *Lexer)
{
    char ret = *Lexer->Curr;
    if (!IsAtEnd(Lexer))
    {
        Lexer->Curr++;
    }
    return ret;
}


static char PeekChr(const Tokenizer *Lexer)
{
    if (IsAtEnd(Lexer)) 
        return '\0';
    return Lexer->Curr[1];
}


static void SkipWhitespace(Tokenizer *Lexer)
{
    while (1)
    {
        switch (*Lexer->Curr)
        {
        case ' ':
        case '\r':
        case '\t':
        {
            AdvanceChrPtr(Lexer);
        } break;

        case '\n':
        {
            Lexer->Line++;
            AdvanceChrPtr(Lexer);
        } break;

        case '(': /* (* comment *) */
        {
            if ('*' == PeekChr(Lexer))
            {
                Lexer->Curr += 2; /* skip '(*' */
                while (!IsAtEnd(Lexer) 
                && !('*' == AdvanceChrPtr(Lexer) && ')' != *Lexer->Curr))
                {
                    if ('\n' == *Lexer->Curr)
                        Lexer->Line++;
                }

                if (!IsAtEnd(Lexer))
                {
                    Lexer->Curr++; /* skip ')' */
                }
            }
        } break;

        case '{': /* { comment } */
        {
            Lexer->Curr++; /* skip '}' */
            do {
                if ('\n' == *Lexer->Curr)
                    Lexer->Line++;
            } while (!IsAtEnd(Lexer) && ('}' != AdvanceChrPtr(Lexer)));
            /* pointing at EOF or next char */
        } break;

        case '/': // comment 
        {
            if ('/' == PeekChr(Lexer))
            {
                Lexer->Curr += 2; /* skip '//' */
                while (!IsAtEnd(Lexer) && ('\n' != *Lexer->Curr))
                {
                    AdvanceChrPtr(Lexer);
                }
                Lexer->Line++;
            }
        } break;

        default: goto out;
        }
    }

out:
    Lexer->Start = Lexer->Curr;
}







static Token MakeToken(Tokenizer *Lexer, TokenType Type)
{
    Token Tok = {
        .Type = Type,
        .Str = Lexer->Start,
        .Len = Lexer->Curr - Lexer->Start,
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


static Token ConsumeWord(Tokenizer *Lexer)
{
    return MakeToken(Lexer, TOKEN_EOF);
}


