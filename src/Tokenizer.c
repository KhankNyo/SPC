

#include "Include/Tokenizer.h"





static bool IsAtEnd(const PascalTokenizer *Lexer);
static bool IsNumber(U8 ch);
static bool IsAlpha(U8 ch);
static bool IsHex(U8 ch);
/* returns true if Str1 is equal to UpperStr2, 
 * case does not matter except for Str2 which has to be upper case */
static bool AlphaArrayEquNoCase(const U8 *Str1, const U8 *UpperStr2, UInt Len);

/* returns the token that Curr is pointing at,
 * then increment curr if curr is not pointing at the null terminator */
static U8 AdvanceChrPtr(PascalTokenizer *Lexer);

/* return *Curr == Test, 
 * and if the condition was true, perform Curr++ */
static bool AdvanceIfEqual(PascalTokenizer *Lexer, U8 Test);

/* if curr is at end, returns 0,
 * else return the U8 ahead of curr */
static U8 PeekChr(const PascalTokenizer *Lexer);

/* skips whitespace, newline, comments */
static void SkipWhitespace(PascalTokenizer *Lexer);

/* creates a token with the given type */
static Token MakeToken(PascalTokenizer *Lexer, TokenType Type);

/* consumes a numeric literal and return its token */
static Token ConsumeNumber(PascalTokenizer *Lexer);

/* consumes a string, including escape codes */
static Token ConsumeString(PascalTokenizer *Lexer);

/* consumes an identifier or a keyword */
static Token ConsumeWord(PascalTokenizer *Lexer);

/* return the type of the current lexeme */
static TokenType GetLexemeType(PascalTokenizer *Lexer);



#define CHR_TO_UPPER(Chr) ((Chr) & ~((U8)1 << 5)) 





PascalTokenizer TokenizerInit(const U8 *Source)
{
    return (Tokenizer) {
        .Start = Source,
        .Curr = Source,
        .Line = 1,
    };
}


Token TokenizerGetToken(PascalTokenizer *Lexer)
{
    SkipWhitespace(Lexer);
    if (IsAtEnd(Lexer))
        return MakeToken(Lexer, TOKEN_EOF);

    U8 PrevChr = AdvanceChrPtr(Lexer);
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



const U8 *TokenTypeToStr(TokenType Type)
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
    return (const U8*)TokenNameLut[Type];
}







static bool IsAtEnd(const PascalTokenizer *Lexer)
{
    return ('\0' == *Lexer->Curr);
}

static bool IsNumber(U8 ch)
{
    return ('0' <= ch) && (ch <= '9');
}

static bool IsAlpha(U8 ch)
{
    return (('A' <= ch) && (ch <= 'Z'))
        || (('a' <= ch) && (ch <= 'z'));
}

static bool IsHex(U8 ch)
{
    return IsNumber(ch)
        || (('a' <= ch) && (ch <= 'f'))
        || (('A' <= ch) && (ch <= 'F'));
}



static U8 AdvanceChrPtr(PascalTokenizer *Lexer)
{
    U8 Ret = *Lexer->Curr;
    if (!IsAtEnd(Lexer))
    {
        Lexer->Curr++;
    }
    return Ret;
}


static bool AdvanceIfEqual(PascalTokenizer *Lexer, U8 Test)
{
    bool Ret = *Lexer->Curr == Test;
    if (Ret)
    {
        AdvanceChrPtr(Lexer);
    }
    return Ret;
}



static U8 PeekChr(const PascalTokenizer *Lexer)
{
    if (IsAtEnd(Lexer)) 
        return '\0';
    return Lexer->Curr[1];
}


static void SkipWhitespace(PascalTokenizer *Lexer)
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
            /* pointing at EOF or next U8 */
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







static Token MakeToken(PascalTokenizer *Lexer, TokenType Type)
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

static Token ConsumeNumber(PascalTokenizer *Lexer)
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


static Token ConsumeString(PascalTokenizer *Lexer)
{
    Lexer->Curr = Lexer->Start;
    UInt Trim;
    PascalStr Literal = PStrInit(0);
    do {
        Trim = 2; /* trim both the opening and closing quotes */

        /* skip beginning "'" */
        AdvanceChrPtr(Lexer);

        /* consume string literal */
        const U8 *Slice = Lexer->Curr;
        UInt Len = 0;
        while (!IsAtEnd(Lexer) && '\'' != AdvanceChrPtr(Lexer))
        {
            Len++;
        }
        PStrAppendStr(&Literal, Slice, Len);


        /* consumes escape codes */
        while (!IsAtEnd(Lexer) && '#' == *Lexer->Curr)
        {
            AdvanceChrPtr(Lexer); /* skip '#' */

            /* consume number */
            U8 EscCode = 0;
            while (!IsAtEnd(Lexer) && IsNumber(*Lexer->Curr))
            {
                EscCode *= 10;
                EscCode += AdvanceChrPtr(Lexer) - '0';
            }

            PStrAppendChr(&Literal, EscCode);
            Trim = 1; /* don't want to trim the last character of the number */
        }
    } while (!IsAtEnd(Lexer) && ('\'' == *Lexer->Curr));


    if (IsAtEnd(Lexer))
        return MakeToken(Lexer, TOKEN_ERROR);

    Token StringToken = MakeToken(Lexer, TOKEN_STRING_LITERAL);

    /* consume opening and closing quotes */
    StringToken.Str++;
    StringToken.Len -= Trim;
    StringToken.Literal.Str = Literal;
    return StringToken;
}



static Token ConsumeWord(PascalTokenizer *Lexer)
{
    while (IsNumber(*Lexer->Curr) || IsAlpha(*Lexer->Curr) || '_' == *Lexer->Curr)
    {
        AdvanceChrPtr(Lexer);
    }

    return MakeToken(Lexer, GetLexemeType(Lexer));
}


static TokenType GetLexemeType(PascalTokenizer *Lexer)
{
    typedef struct Keyword {
        const U8 *Str;
        UInt Len;
        UInt Type;
    } Keyword;

    Keyword KeywordLut[][5] = 
    {
        ['A'] = {
            {.Str = (const U8 *)"ND",   .Len = 2, .Type = TOKEN_AND}, 
            {.Str = (const U8 *)"RRAY", .Len = 4, .Type = TOKEN_ARRAY}, 
            {.Str = (const U8 *)"SM",   .Len = 2, .Type = TOKEN_ASM}
        },
        ['B'] = {
            {.Str = (const U8 *)"EGIN", .Len = 4, .Type = TOKEN_BEGIN},
            {.Str = (const U8 *)"REAK", .Len = 4, .Type = TOKEN_BREAK}
        },
        ['C'] = {
            {.Str = (const U8 *)"ASE", .Len = 3, .Type = TOKEN_CASE},
            {.Str = (const U8 *)"ONST", .Len = 4, .Type = TOKEN_CONST},
            {.Str = (const U8 *)"ONTINUE", .Len = 7, .Type = TOKEN_CONTINUE},
            {.Str = (const U8 *)"ONSTRUCTOR", .Len = 10, .Type = TOKEN_CONSTRUCTOR},
        },
        ['D'] = {
            {.Str = (const U8 *)"O", .Len = 1, .Type = TOKEN_DO},
            {.Str = (const U8 *)"IV", .Len = 2, .Type = TOKEN_DIV},
            {.Str = (const U8 *)"OWNTO", .Len = 5, .Type = TOKEN_DOWNTO},
            {.Str = (const U8 *)"ESTRUCTOR", .Len = 9, .Type = TOKEN_DESTRUCTOR},
        },
        ['E'] = {
            {.Str = (const U8 *)"ND", .Len = 2, .Type = TOKEN_END},
            {.Str = (const U8 *)"LSE", .Len = 3, .Type = TOKEN_ELSE},
        },
        ['F'] = {
            {.Str = (const U8 *)"OR", .Len = 2, .Type = TOKEN_FOR},
            {.Str = (const U8 *)"ILE", .Len = 3, .Type = TOKEN_FILE},
            {.Str = (const U8 *)"ALSE", .Len = 4, .Type = TOKEN_FALSE},
            {.Str = (const U8 *)"UNCTION", .Len = 7, .Type = TOKEN_FUNCTION},
        },
        ['G'] = {
            {.Str = (const U8 *)"OTO", .Len = 3, .Type = TOKEN_GOTO},
        },
        ['H'] = { {0} },
        ['I'] = {
            {.Str = (const U8 *)"F", .Len = 1, .Type = TOKEN_IF},
            {.Str = (const U8 *)"N", .Len = 1, .Type = TOKEN_IN},
            {.Str = (const U8 *)"NLINE", .Len = 5, .Type = TOKEN_INLINE},
            {.Str = (const U8 *)"NTERFACE", .Len = 8, .Type = TOKEN_INTERFACE},
            {.Str = (const U8 *)"MPLEMENTATION", .Len = 13, .Type = TOKEN_IMPLEMENTATION},
        },
        ['J'] = { {0} },
        ['K'] = { {0} },
        ['L'] = {
            {.Str = (const U8 *)"ABEL", .Len = 4, .Type = TOKEN_LABEL},
        },
        ['M'] = {
            {.Str = (const U8 *)"ODE", .Len = 3, .Type = TOKEN_MODE},
        },
        ['N'] = {
            {.Str = (const U8 *)"IL", .Len = 2, .Type = TOKEN_NIL},
            {.Str = (const U8 *)"OT", .Len = 2, .Type = TOKEN_NOT},
        },
        ['O'] = {
            {.Str = (const U8 *)"F", .Len = 1, .Type = TOKEN_OF},
            {.Str = (const U8 *)"N", .Len = 1, .Type = TOKEN_ON},
            {.Str = (const U8 *)"R", .Len = 1, .Type = TOKEN_OR},
            {.Str = (const U8 *)"BJECT", .Len = 5, .Type = TOKEN_OBJECT},
            {.Str = (const U8 *)"PERATOR", .Len = 7, .Type = TOKEN_OPERATOR},
        },
        ['P'] = {
            {.Str = (const U8 *)"ACKED", .Len = 5, .Type = TOKEN_PACKED},
            {.Str = (const U8 *)"ROGRAM", .Len = 6, .Type = TOKEN_PROGRAM},
            {.Str = (const U8 *)"ROCEDURE", .Len = 8, .Type = TOKEN_PROCEDURE},
        },
        ['Q'] = { {0} },
        ['R'] = {
            {.Str = (const U8 *)"EPEAT", .Len = 5, .Type = TOKEN_RECORD},
            {.Str = (const U8 *)"ECORD", .Len = 5, .Type = TOKEN_REPEAT},
        },
        ['S'] = {
            {.Str = (const U8 *)"ET", .Len = 2, .Type = TOKEN_SET},
            {.Str = (const U8 *)"HR", .Len = 2, .Type = TOKEN_SHR},
            {.Str = (const U8 *)"HL", .Len = 2, .Type = TOKEN_SHL},
            {.Str = (const U8 *)"TRING", .Len = 5, .Type = TOKEN_STRING},
        },
        ['T'] = {
            {.Str = (const U8 *)"HEN", .Len = 3, .Type = TOKEN_THEN},
            {.Str = (const U8 *)"RUE", .Len = 3, .Type = TOKEN_TRUE},
            {.Str = (const U8 *)"YPE", .Len = 3, .Type = TOKEN_TYPE},
        },
        ['U'] = {
            {.Str = (const U8 *)"SES", .Len = 3, .Type = TOKEN_USES},
            {.Str = (const U8 *)"NIT", .Len = 3, .Type = TOKEN_UNIT},
            {.Str = (const U8 *)"NTIL", .Len = 4, .Type = TOKEN_UNTIL},
        },
        ['V'] = {
            {.Str = (const U8 *)"AR", .Len = 2, .Type = TOKEN_VAR},
        },
        ['W'] = {
            {.Str = (const U8 *)"ITH", .Len = 3, .Type = TOKEN_WITH},
            {.Str = (const U8 *)"HILE", .Len = 4, .Type = TOKEN_WHILE},
        },
        ['X'] = {
            {.Str = (const U8 *)"OR", .Len = 2, .Type = TOKEN_XOR},
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




static bool AlphaArrayEquNoCase(const U8 *Str1, const U8 *UpperStr2, UInt Len)
{
    while (Len && (CHR_TO_UPPER(*Str1) == *UpperStr2))
    {
        Len--;
        Str1++;
        UpperStr2++;
    }
    return (0 == Len);
}


