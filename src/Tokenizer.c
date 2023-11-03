

#include "Include/Tokenizer.h"





static bool IsAtEnd(const Tokenizer *Lexer);
static bool IsNumber(char ch);
static bool IsAlpha(char ch);
static bool IsHex(char ch);
/* returns true if Str1 is equal to UpperStr2, 
 * case does not matter except for Str2 which has to be upper case */
static bool AlphaArrayEquNoCase(const char *Str1, const char *UpperStr2, UInt Len);

/* returns the token that Curr is pointing at,
 * then increment curr if curr is not pointing at the null terminator */
static char AdvanceChrPtr(Tokenizer *Lexer);

/* return *Curr == Test, 
 * and if the condition was true, perform Curr++ */
static bool AdvanceIfEqual(Tokenizer *Lexer, char Test);

/* if curr is at end, returns 0,
 * else return the char ahead of curr */
static char PeekChr(const Tokenizer *Lexer);

/* skips whitespace, newline, comments */
static void SkipWhitespace(Tokenizer *Lexer);

/* creates a token with the given type */
static Token MakeToken(Tokenizer *Lexer, TokenType Type);

/* consumes a numeric literal and return its token */
static Token ConsumeNumber(Tokenizer *Lexer);

/* consumes a string, including escape codes */
static Token ConsumeString(Tokenizer *Lexer);

/* consumes an identifier or a keyword */
static Token ConsumeWord(Tokenizer *Lexer);

/* return the type of the current lexeme */
static TokenType GetLexemeType(Tokenizer *Lexer);



#define CHR_TO_UPPER(Chr) ((Chr) & ~((U8)1 << 5)) 





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

    char PrevChr = AdvanceChrPtr(Lexer);
    if (IsNumber(PrevChr))
    {
        return ConsumeNumber(Lexer);
    }
    if (IsAlpha(PrevChr))
    {
        return ConsumeWord(Lexer);
    }

    switch (PrevChr)
    {
    case '&': return MakeToken(Lexer, TOKEN_AMPERSAND);
    case '^': return MakeToken(Lexer, TOKEN_CARET);
    case '@': return MakeToken(Lexer, TOKEN_AT);
    case '$': return MakeToken(Lexer, TOKEN_DOLLAR);
    case '%': return MakeToken(Lexer, TOKEN_PERCENTAGE);
    case '[': return MakeToken(Lexer, TOKEN_LEFT_BRACKET);
    case ']': return MakeToken(Lexer, TOKEN_RIGHT_BRACKET);
    case '(': return MakeToken(Lexer, TOKEN_LEFT_PAREN);
    case ')': return MakeToken(Lexer, TOKEN_RIGHT_PAREN);
    case '.': return MakeToken(Lexer, TOKEN_DOT);
    case ',': return MakeToken(Lexer, TOKEN_COMMA);
    case ';': return MakeToken(Lexer, TOKEN_SEMICOLON);
    case '=': return MakeToken(Lexer, TOKEN_EQUAL);
    case '\'': return ConsumeString(Lexer);

    case ':': 
    {
        if (AdvanceIfEqual(Lexer, '='))
            return MakeToken(Lexer, TOKEN_COLON_EQUAL);
        else return MakeToken(Lexer, TOKEN_COLON);
    } break;


    case '+': 
    {
        if (AdvanceIfEqual(Lexer, '='))
            return MakeToken(Lexer, TOKEN_PLUS_EQUAL);
        else return MakeToken(Lexer, TOKEN_PLUS);
    } break;

    case '-': 
    {
        if (AdvanceIfEqual(Lexer, '='))
            return MakeToken(Lexer, TOKEN_MINUS_EQUAL);
        else return MakeToken(Lexer, TOKEN_MINUS);
    } break;

    case '*': 
    {
        if (AdvanceIfEqual(Lexer, '*'))
            return MakeToken(Lexer, TOKEN_STAR_STAR);
        if (AdvanceIfEqual(Lexer, '='))
            return MakeToken(Lexer, TOKEN_STAR_EQUAL);
        else return MakeToken(Lexer, TOKEN_STAR);
    } break;

    case '/':
    {
        if (AdvanceIfEqual(Lexer, '='))
            return MakeToken(Lexer, TOKEN_SLASH_EQUAL);
        else return MakeToken(Lexer, TOKEN_SLASH);
    } break;



    case '<': 
    {
        if (AdvanceIfEqual(Lexer, '>'))
            return MakeToken(Lexer, TOKEN_LESS_GREATER);
        if (AdvanceIfEqual(Lexer, '<'))
            return MakeToken(Lexer, TOKEN_LESS_LESS);
        else return MakeToken(Lexer, TOKEN_LESS);
    } break;

    case '>':
    {
        if (AdvanceIfEqual(Lexer, '>'))
            return MakeToken(Lexer, TOKEN_GREATER_GREATER);
        else return MakeToken(Lexer, TOKEN_GREATER);
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
        "TOKEN_WHILE", "TOKEN_WITH", 
        "TOKEN_XOR",

        /* symbols */
        "TOKEN_PLUS", "TOKEN_MINUS", "TOKEN_STAR", "TOKEN_SLASH",
        "TOKEN_PLUS_EQUAL", "TOKEN_MINUS_EQUAL", "TOKEN_STAR_EQUAL", "TOKEN_SLASH_EQUAL",
        "TOKEN_STAR_STAR",
        "TOKEN_EQUAL", "TOKEN_LESS", "TOKEN_GREATER", "TOKEN_LESS_GREATER",
        "TOKEN_LESS_LESS", "TOKEN_GREATER_GREATER",
        "TOKEN_DOT", "TOKEN_COMMA", "TOKEN_COLON", "TOKEN_SEMICOLON",
        "TOKEN_COLON_EQUAL",
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
    char Ret = *Lexer->Curr;
    if (!IsAtEnd(Lexer))
    {
        Lexer->Curr++;
    }
    return Ret;
}


static bool AdvanceIfEqual(Tokenizer *Lexer, char Test)
{
    bool Ret = *Lexer->Curr == Test;
    if (Ret)
    {
        AdvanceChrPtr(Lexer);
    }
    return Ret;
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
                /* skip ')', because the above loop did not skip it, 
                 * only skipped the '*' in '*)' */
                AdvanceChrPtr(Lexer); 
            }
            else goto out;
        } break;

        case '{': /* { comment } */
        {
            Lexer->Curr++; /* skip '{' */
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
            else goto out;
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

    if ('0' == Lexer->Start[0]
    && ('X' == CHR_TO_UPPER(Lexer->Start[1])))
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

        if ('E' == CHR_TO_UPPER(*Lexer->Curr))
        {
            PASCAL_UNREACHABLE("TODO: exponential notation");
            DIE(); /* this function name is too good */
        }
    }
    return MakeToken(Lexer, Type);
}


static Token ConsumeString(Tokenizer *Lexer)
{
    /* TODO: escape codes */
    while ('\'' != AdvanceChrPtr(Lexer))
    {}
    Token StringToken = MakeToken(Lexer, TOKEN_STRING_LITERAL);

    /* consume opening and closing quotes */
    StringToken.Str++; 
    StringToken.Len -= 2;
    return StringToken;
}



static Token ConsumeWord(Tokenizer *Lexer)
{
    while (IsNumber(*Lexer->Curr) || IsAlpha(*Lexer->Curr) || '_' == *Lexer->Curr)
    {
        AdvanceChrPtr(Lexer);
    }

    return MakeToken(Lexer, GetLexemeType(Lexer));
}


static TokenType GetLexemeType(Tokenizer *Lexer)
{
    typedef struct Keyword {
        const char *Str;
        UInt Len;
        UInt Type;
    } Keyword;

    Keyword KeywordLut[][5] = 
    {
        ['A'] = {
            {.Str = "ND",   .Len = 2, .Type = TOKEN_AND}, 
            {.Str = "RRAY", .Len = 4, .Type = TOKEN_ARRAY}, 
            {.Str = "SM",   .Len = 2, .Type = TOKEN_ASM}
        },
        ['B'] = {
            {.Str = "EGIN", .Len = 4, .Type = TOKEN_BEGIN},
            {.Str = "REAK", .Len = 4, .Type = TOKEN_BREAK}
        },
        ['C'] = {
            {.Str = "ASE", .Len = 3, .Type = TOKEN_CASE},
            {.Str = "ONST", .Len = 4, .Type = TOKEN_CONST},
            {.Str = "ONTINUE", .Len = 7, .Type = TOKEN_CONTINUE},
            {.Str = "ONSTRUCTOR", .Len = 10, .Type = TOKEN_CONSTRUCTOR},
        },
        ['D'] = {
            {.Str = "O", .Len = 1, .Type = TOKEN_DO},
            {.Str = "IV", .Len = 2, .Type = TOKEN_DIV},
            {.Str = "OWNTO", .Len = 5, .Type = TOKEN_DOWNTO},
            {.Str = "ESTRUCTOR", .Len = 9, .Type = TOKEN_DESTRUCTOR},
        },
        ['E'] = {
            {.Str = "ND", .Len = 2, .Type = TOKEN_END},
            {.Str = "LSE", .Len = 3, .Type = TOKEN_ELSE},
        },
        ['F'] = {
            {.Str = "OR", .Len = 2, .Type = TOKEN_FOR},
            {.Str = "ILE", .Len = 3, .Type = TOKEN_FILE},
            {.Str = "ALSE", .Len = 4, .Type = TOKEN_FALSE},
            {.Str = "UNCTION", .Len = 7, .Type = TOKEN_FUNCTION},
        },
        ['G'] = {
            {.Str = "OTO", .Len = 3, .Type = TOKEN_GOTO},
        },
        ['H'] = { {0} },
        ['I'] = {
            {.Str = "F", .Len = 1, .Type = TOKEN_IF},
            {.Str = "N", .Len = 1, .Type = TOKEN_IN},
            {.Str = "NLINE", .Len = 5, .Type = TOKEN_INLINE},
            {.Str = "NTERFACE", .Len = 8, .Type = TOKEN_INTERFACE},
            {.Str = "MPLEMENTATION", .Len = 13, .Type = TOKEN_IMPLEMENTATION},
        },
        ['J'] = { {0} },
        ['K'] = { {0} },
        ['L'] = {
            {.Str = "ABEL", .Len = 4, .Type = TOKEN_LABEL},
        },
        ['M'] = {
            {.Str = "ODE", .Len = 3, .Type = TOKEN_MODE},
        },
        ['N'] = {
            {.Str = "IL", .Len = 2, .Type = TOKEN_NIL},
            {.Str = "OT", .Len = 2, .Type = TOKEN_NOT},
        },
        ['O'] = {
            {.Str = "F", .Len = 1, .Type = TOKEN_OF},
            {.Str = "N", .Len = 1, .Type = TOKEN_ON},
            {.Str = "R", .Len = 1, .Type = TOKEN_OR},
            {.Str = "BJECT", .Len = 5, .Type = TOKEN_OBJECT},
            {.Str = "NLINE", .Len = 5, .Type = TOKEN_INLINE},
        },
        ['P'] = {
            {.Str = "ACKED", .Len = 5, .Type = TOKEN_PACKED},
            {.Str = "ROGRAM", .Len = 6, .Type = TOKEN_PROGRAM},
            {.Str = "ROCEDURE", .Len = 8, .Type = TOKEN_PROCEDURE},
        },
        ['Q'] = { {0} },
        ['R'] = {
            {.Str = "EPEAT", .Len = 5, .Type = TOKEN_RECORD},
            {.Str = "ECORD", .Len = 5, .Type = TOKEN_REPEAT},
        },
        ['S'] = {
            {.Str = "ET", .Len = 2, .Type = TOKEN_SET},
            {.Str = "HR", .Len = 2, .Type = TOKEN_SHR},
            {.Str = "HL", .Len = 2, .Type = TOKEN_SHL},
            {.Str = "RING", .Len = 4, .Type = TOKEN_STRING},
        },
        ['T'] = {
            {.Str = "HEN", .Len = 3, .Type = TOKEN_THEN},
            {.Str = "RUE", .Len = 3, .Type = TOKEN_TRUE},
            {.Str = "YPE", .Len = 3, .Type = TOKEN_TYPE},
        },
        ['U'] = {
            {.Str = "SES", .Len = 3, .Type = TOKEN_USES},
            {.Str = "NIT", .Len = 3, .Type = TOKEN_UNIT},
            {.Str = "NTIL", .Len = 4, .Type = TOKEN_UNTIL},
        },
        ['V'] = {
            {.Str = "AR", .Len = 2, .Type = TOKEN_VAR},
        },
        ['W'] = {
            {.Str = "ITH", .Len = 3, .Type = TOKEN_WITH},
            {.Str = "HILE", .Len = 4, .Type = TOKEN_WHILE},
        },
        ['X'] = {
            {.Str = "OR", .Len = 2, .Type = TOKEN_XOR},
        },
        ['Y'] = { {0} },
        ['Z'] = { {0} },
    };

    if ('_' == *Lexer->Start)
    {
        return TOKEN_IDENTIFIER;
    }

    U8 Key = CHR_TO_UPPER(*Lexer->Start);
    UInt LexemeLen = Lexer->Curr - Lexer->Start;
    for (UInt i = 0; i < STATIC_ARRAY_SIZE(KeywordLut[Key]); i++)
    {
        Keyword *KeywordSlot = &KeywordLut[Key][i];

        if (LexemeLen - 1 == KeywordSlot->Len
        && AlphaArrayEquNoCase(Lexer->Start + 1, KeywordSlot->Str, KeywordSlot->Len))
        {
            return KeywordSlot->Type;
        }
    }
    return TOKEN_IDENTIFIER;
}




static bool AlphaArrayEquNoCase(const char *Str1, const char *UpperStr2, UInt Len)
{
    while (Len && (CHR_TO_UPPER(*Str1) == *UpperStr2))
    {
        Len--;
        Str1++;
        UpperStr2++;
    }
    return (0 == Len);
}


