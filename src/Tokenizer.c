

#include "StringView.h"
#include "Tokenizer.h"





static bool TokenizerIsAtEnd(const PascalTokenizer *Lexer);
static bool IsNumber(U8 ch);
static bool IsAlpha(U8 ch);
static bool IsAlphaNum(U8 Ch);
static bool IsHex(U8 ch);

/* updates the Line and LineOffset field, only call when a newline si encountered */
static void UpdateLine(PascalTokenizer *Lexer);

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

/* creates an error token with the given info */
static Token ErrorToken(PascalTokenizer *Lexer, const char *Msg);

/* consumes a hexadecimal literal and return its token */
static Token ConsumeHex(PascalTokenizer *Lexer);

/* consumes an octal literal and return its token */
static Token ConsumeOct(PascalTokenizer *Lexer);

/* consumes a binary literal and return its token */
static Token ConsumeBinary(PascalTokenizer *Lexer);

/* consumes a numeric literal and return its token */
static Token ConsumeNumber(PascalTokenizer *Lexer);

/* consumes a string, including escape codes */
static Token ConsumeString(PascalTokenizer *Lexer);

/* consumes an identifier or a keyword */
static Token ConsumeWord(PascalTokenizer *Lexer);

/* return the type of the current lexeme */
static TokenType GetLexemeType(PascalTokenizer *Lexer);








PascalTokenizer TokenizerInit(const U8 *Source, U32 Line)
{
    return (PascalTokenizer) {
        .Start = Source,
        .Curr = Source,
        .Line = Line,
        .LinePtr = Source,
    };
}


Token TokenizerGetToken(PascalTokenizer *Lexer)
{
    SkipWhitespace(Lexer);
    if (TokenizerIsAtEnd(Lexer))
        return MakeToken(Lexer, TOKEN_EOF);

    U8 PrevChr = AdvanceChrPtr(Lexer);
    if (IsNumber(PrevChr))
    {
        return ConsumeNumber(Lexer);
    }
    if ('_' == PrevChr || IsAlpha(PrevChr))
    {
        return ConsumeWord(Lexer);
    }

    switch (PrevChr)
    {
    case '#':
    case '\'': 
        return ConsumeString(Lexer);
    case '$': return ConsumeHex(Lexer);
    case '&': return ConsumeOct(Lexer);
    case '^': return MakeToken(Lexer, TOKEN_CARET);
    case '@': return MakeToken(Lexer, TOKEN_AT);
    case '[': return MakeToken(Lexer, TOKEN_LEFT_BRACKET);
    case ']': return MakeToken(Lexer, TOKEN_RIGHT_BRACKET);
    case '(': return MakeToken(Lexer, TOKEN_LEFT_PAREN);
    case ')': return MakeToken(Lexer, TOKEN_RIGHT_PAREN);
    case '.': 
    {
        if (AdvanceIfEqual(Lexer, '.'))
            return MakeToken(Lexer, TOKEN_DOT_DOT);
        return MakeToken(Lexer, TOKEN_DOT);
    } break;
    case ',': return MakeToken(Lexer, TOKEN_COMMA);
    case ';': return MakeToken(Lexer, TOKEN_SEMICOLON);
    case '=': 
    {
        if (AdvanceIfEqual(Lexer, '='))
            return ErrorToken(Lexer, "This is Pascal, use '=' to check for equality.");
        else return MakeToken(Lexer, TOKEN_EQUAL);
    } break;
    case '%': 
    {
        if (AdvanceIfEqual(Lexer, '='))
            return MakeToken(Lexer, TOKEN_PERCENT_EQUAL);
        return ConsumeBinary(Lexer);
    } break;
    case '!': 
    {
        if (AdvanceIfEqual(Lexer, '='))
            return ErrorToken(Lexer, "This is Pascal, use '<>' to check for inequality.");
        return MakeToken(Lexer, TOKEN_BANG);
    } break;

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
        if (AdvanceIfEqual(Lexer, '='))
            return MakeToken(Lexer, TOKEN_LESS_EQUAL);
        else return MakeToken(Lexer, TOKEN_LESS);
    } break;

    case '>':
    {
        if (AdvanceIfEqual(Lexer, '>'))
            return MakeToken(Lexer, TOKEN_GREATER_GREATER);
        if (AdvanceIfEqual(Lexer, '='))
            return MakeToken(Lexer, TOKEN_GREATER_EQUAL);
        else return MakeToken(Lexer, TOKEN_GREATER);
    } break;

    default: break;
    }

    return ErrorToken(Lexer, "Unknown token.");
}



const U8 *TokenTypeToStr(TokenType Type)
{
    static const char *TokenNameLut[] = {
        "TOKEN_EOF",
        "TOKEN_ERROR",

        /* keywords */
        "TOKEN_AND", "TOKEN_ARRAY", "TOKEN_ASR", "TOKEN_ASM",
        "TOKEN_BEGIN", "TOKEN_BREAK",
        "TOKEN_CASE", "TOKEN_CONST", "TOKEN_CONSTRUCTOR", "TOKEN_CONTINUE", 
        "TOKEN_DESTRUCTOR", "TOKEN_DIV", "TOKEN_DO", "TOKEN_DOWNTO",
        "TOKEN_ELSE", "TOKEN_END", "TOKEN_EXIT",
        "TOKEN_FALSE", "TOKEN_FILE", "TOKEN_FOR", "TOKEN_FUNCTION", "TOKEN_FORWARD",
        "TOKEN_GOTO",
        "TOKEN_IF", "TOKEN_IMPLEMENTATION", "TOKEN_IN", "TOKEN_INLINE", "TOKEN_INFERFACE", 
        "TOKEN_LABEL", 
        "TOKEN_MOD", 
        "TOKEN_NOT",
        "TOKEN_OBJECT", "TOKEN_OF", "TOKEN_ON", "TOKEN_OPERATOR", "TOKEN_OR", 
        "TOKEN_PACKED", "TOKEN_PROCEDURE", "TOKEN_PROGRAM", 
        "TOKEN_RECORD", "TOKEN_REPEAT", "TOKEN_RESULT",
        "TOKEN_SET", "TOKEN_SHL", "TOKEN_SHR",  
        "TOKEN_THEN", "TOKEN_TRUE", "TOKEN_TYPE", "TOKEN_TO",
        "TOKEN_UNIT", "TOKEN_UNTIL", "TOKEN_USES", 
        "TOKEN_VAR", 
        "TOKEN_WHILE", "TOKEN_WITH", 
        "TOKEN_XOR",

        /* symbols */
        "TOKEN_PLUS", "TOKEN_MINUS", "TOKEN_STAR", "TOKEN_SLASH",
        "TOKEN_PLUS_EQUAL", "TOKEN_MINUS_EQUAL", "TOKEN_STAR_EQUAL", "TOKEN_SLASH_EQUAL",
        "TOKEN_PERCENT_EQUAL",
        "TOKEN_STAR_STAR",
        "TOKEN_BANG",
        "TOKEN_EQUAL", "TOKEN_LESS", "TOKEN_GREATER", "TOKEN_LESS_GREATER",
        "TOKEN_LESS_EQUAL", "TOKEN_GREATER_EQUAL",
        "TOKEN_LESS_LESS", "TOKEN_GREATER_GREATER",
        "TOKEN_DOT", "TOKEN_DOT_DOT", "TOKEN_COMMA", "TOKEN_COLON", "TOKEN_SEMICOLON",
        "TOKEN_COLON_EQUAL",
        "TOKEN_LEFT_BRACKET", "TOKEN_RIGHT_BRACKET", 
        "TOKEN_LEFT_PAREN", "TOKEN_RIGHT_PAREN",
        "TOKEN_CARET", "TOKEN_AT", "TOKEN_HASHTAG",

        "TOKEN_NUMBER_LITERAL", "TOKEN_INTEGER_LITERAL", 
        "TOKEN_STRING_LITERAL", 
        "TOKEN_CHAR_LITERAL", 
        "TOKEN_IDENTIFIER"
    };
    PASCAL_STATIC_ASSERT(
        STATIC_ARRAY_SIZE(TokenNameLut) == TOKEN_TYPE_COUNT, "Missing types in string lookup table"
    );
    PASCAL_ASSERT(Type < (int)STATIC_ARRAY_SIZE(TokenNameLut), "Invalid token type: %d\n", Type);
    return (const U8*)TokenNameLut[Type];
}




bool TokenEqualNoCase(const U8 *s1, const U8 *s2, USize Len)
{
    PASCAL_ASSERT(s1 != NULL && s2 != NULL, "Cannot pass NULL to TokenEqualNoCase");
    for (USize i = 0; i < Len; i++)
    {
        /* toggling bit 5 off for both, both must be equal in any case */
        if (CHR_TO_UPPER(s1[i]) != CHR_TO_UPPER(s2[i]))
            return false;
    }
    return true;
}







static bool TokenizerIsAtEnd(const PascalTokenizer *Lexer)
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

static bool IsAlphaNum(U8 Ch)
{
    return IsAlpha(Ch) || IsNumber(Ch);
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
    if (!TokenizerIsAtEnd(Lexer))
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
    if (TokenizerIsAtEnd(Lexer)) 
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
            AdvanceChrPtr(Lexer);
            UpdateLine(Lexer);
        } break;

        case '(': /* (* comment *) */
        {
            if ('*' == PeekChr(Lexer))
            {
                /* skip '(*' */
                Lexer->Curr += 2;

                while (!TokenizerIsAtEnd(Lexer) 
                && !('*' == AdvanceChrPtr(Lexer) && ')' != *Lexer->Curr))
                {
                    if ('\n' == *Lexer->Curr)
                        UpdateLine(Lexer);
                }
                /* skip ')', because the above loop did not skip it, 
                 * only skipped the '*' in '*)' */
                AdvanceChrPtr(Lexer); 
            }
            else goto out;
        } break;

        case '{': /* { comment } */
        /* TODO: compiler directive */
        {
            AdvanceChrPtr(Lexer);
            do {
                if ('\n' == *Lexer->Curr)
                    UpdateLine(Lexer);
            } while (!TokenizerIsAtEnd(Lexer) && ('}' != AdvanceChrPtr(Lexer)));
            /* pointing at EOF or next U8 */
        } break;

        case '/': // comment 
        {
            if ('/' == PeekChr(Lexer))
            {
                Lexer->Curr += 2; /* skip '//' */
                while (!TokenizerIsAtEnd(Lexer) && ('\n' != *Lexer->Curr))
                {
                    AdvanceChrPtr(Lexer);
                }
                UpdateLine(Lexer);
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
        .Lexeme = STRVIEW_INIT(Lexer->Start, Lexer->Curr - Lexer->Start),
        .Line = Lexer->Line,
        .LineOffset = Lexer->Start - Lexer->LinePtr + 1,
    };
    Lexer->Start = Lexer->Curr;
    return Tok;
}

static Token ErrorToken(PascalTokenizer *Lexer, const char *ErrMsg)
{
    Token Err = MakeToken(Lexer, TOKEN_ERROR);
    Err.Literal.Err = ErrMsg;
    return Err;
}


static U64 ConsumeHexadecimalNumber(PascalTokenizer *Lexer)
{
    U64 Hex = 0;
    while (IsHex(*Lexer->Curr))
    {
        UInt Hexit = AdvanceChrPtr(Lexer);
        if (IsNumber(Hexit))
            Hexit -= '0';
        else 
            Hexit = CHR_TO_UPPER(Hexit) - 'A' + 10;

        Hex *= 16;
        Hex += Hexit;
    }
    return Hex;
}

static U64 ConsumeBinaryNumber(PascalTokenizer *Lexer)
{
    U64 Bin = 0;
    while ('0' == *Lexer->Curr || '1' == *Lexer->Curr)
    {
        Bin *= 2;
        Bin += AdvanceChrPtr(Lexer) - '0';
    }
    return Bin;
}

static U64 ConsumeOctalNumber(PascalTokenizer *Lexer)
{
    U64 Oct = 0;
    while ('0' <= *Lexer->Curr || *Lexer->Curr <= '7')
    {
        Oct *= 8;
        Oct += AdvanceChrPtr(Lexer) - '0';
    }
    return Oct;
}

static U64 ConsumeInteger(PascalTokenizer *Lexer)
{
    U64 n = 0;
    while (!TokenizerIsAtEnd(Lexer) && IsNumber(*Lexer->Curr))
    {
        n *= 10;
        n += AdvanceChrPtr(Lexer) - '0';
    }
    return n;
}


static Token ConsumeHex(PascalTokenizer *Lexer)
{
    U64 Hex = ConsumeHexadecimalNumber(Lexer);
    if (IsAlphaNum(*Lexer->Curr) || '_' == *Lexer->Curr)
        return ErrorToken(Lexer, "Invalid character after hexadecimal number.");

    Token HexadecimalNumber = MakeToken(Lexer, TOKEN_INTEGER_LITERAL);
    HexadecimalNumber.Literal.Int = Hex;
    return HexadecimalNumber;
}

static Token ConsumeBinary(PascalTokenizer *Lexer)
{
    U64 Bin = ConsumeBinaryNumber(Lexer);
    if (IsAlphaNum(*Lexer->Curr) || '_' == *Lexer->Curr)
        return ErrorToken(Lexer, "Invalid character after binary number.");

    Token BinNumber = MakeToken(Lexer, TOKEN_INTEGER_LITERAL);
    BinNumber.Literal.Int = Bin;
    return BinNumber;
}

static Token ConsumeOct(PascalTokenizer *Lexer)
{
    U64 Oct = ConsumeOctalNumber(Lexer);
    if (IsAlphaNum(*Lexer->Curr) || '_' == *Lexer->Curr)
        return ErrorToken(Lexer, "Invalid character after octal number.");

    Token OctNumber = MakeToken(Lexer, TOKEN_INTEGER_LITERAL);
    OctNumber.Literal.Int = Oct;
    return OctNumber;
}

static Token ConsumeNumber(PascalTokenizer *Lexer)
{
    TokenType Type = TOKEN_INTEGER_LITERAL;
    if ('0' == Lexer->Start[0] && ('X' == CHR_TO_UPPER(Lexer->Start[1])))
    {
        AdvanceChrPtr(Lexer); /* skip 'x' in 0x */
        return ConsumeHex(Lexer);
    }


    Lexer->Curr = Lexer->Start;
    U64 Integer = ConsumeInteger(Lexer);

    /* decimal, or Real */
    if ('.' == *Lexer->Curr && PeekChr(Lexer) != '.')
    {
        AdvanceChrPtr(Lexer);

        F64 Decimal = 0;
        F64 Pow10 = 1.0;
        Type = TOKEN_NUMBER_LITERAL;
        /* consume decimal */
        while (IsNumber(*Lexer->Curr))
        {
            Pow10 *= 10;
            Decimal *= 10;
            Decimal += AdvanceChrPtr(Lexer) - '0';
        }
        Decimal /= Pow10;
        Decimal += Integer;

        /* exponent form */
        if ('E' == CHR_TO_UPPER(*Lexer->Curr))
        {
            /* skip 'E' */
            AdvanceChrPtr(Lexer);

            AdvanceIfEqual(Lexer, '+');
            bool Sign = AdvanceIfEqual(Lexer, '-');

            /* consume exponent */
            UInt Exponent = ConsumeInteger(Lexer);
            static const U64 PowersOf10[] = {
                1, 10, 100, 1000,
                10000,
                100000,
                1000000,
                10000000,
                100000000,
                1000000000,
                10000000000,
                100000000000,
                1000000000000,
                10000000000000,
                100000000000000,
                1000000000000000,
                /* 
                 * commented out because F64 precision is capped at 15 digit,
                 * so there is no need for 10^16, 10^17 and 10^18,
                 * while having 16 elements will make the modulo compile to an 
                 * AND instruction
                    10000000000000000,
                    100000000000000000,
                    1000000000000000000,
                 */
            };
            U64 Pow10 = PowersOf10[Exponent % STATIC_ARRAY_SIZE(PowersOf10)];
            Decimal = Sign 
                ? Decimal / Pow10
                : Decimal * Pow10;
        }

        if (IsAlpha(*Lexer->Curr) || '_' == *Lexer->Curr)
            goto Error;

        Token Number = MakeToken(Lexer, Type);
        Number.Literal.Real = Decimal;
        return Number;
    }
    else 
    {
        if (IsAlpha(*Lexer->Curr) || '_' == *Lexer->Curr)
            goto Error;

        Token Number = MakeToken(Lexer, Type);
        Number.Literal.Int = Integer;
        return Number;
    }

Error:
    return ErrorToken(Lexer, "Invalid character after number.");
}


static Token ConsumeString(PascalTokenizer *Lexer)
{
    Lexer->Curr = Lexer->Start;
    PascalStr Literal = PStrInit(0);
    while (!TokenizerIsAtEnd(Lexer))
    {
        U8 BeginChar = *Lexer->Curr;
        if ('#' == BeginChar)
        {
            while (AdvanceIfEqual(Lexer, '#'))
            {
                /* consume number */
                U8 EscCode = 0;
                if (AdvanceIfEqual(Lexer, '$'))
                {
                    EscCode = ConsumeHexadecimalNumber(Lexer);
                }
                else if (AdvanceIfEqual(Lexer, '%'))
                {
                    EscCode = ConsumeBinaryNumber(Lexer);
                }
                else if (AdvanceIfEqual(Lexer, '&'))
                {
                    EscCode = ConsumeOctalNumber(Lexer);
                }
                else 
                {
                    EscCode = ConsumeInteger(Lexer);
                }

                PStrAppendChr(&Literal, EscCode);
            }
        }
        else if ('\'' == BeginChar)
        {
            AdvanceChrPtr(Lexer); /* skip initial "'" */
            const U8 *StrSlice = Lexer->Curr;
            UInt SliceLength = 0;
            while (!TokenizerIsAtEnd(Lexer) && '\'' != AdvanceChrPtr(Lexer))
            {
                SliceLength++;
            }
            PStrAppendStr(&Literal, StrSlice, SliceLength);
        }
        else break;
    }

    if (TokenizerIsAtEnd(Lexer))
        return ErrorToken(Lexer, "Unterminated string literal.");

    if (1 == PStrGetLen(&Literal))
    {
        Token CharToken = MakeToken(Lexer, TOKEN_CHAR_LITERAL);

        CharToken.Literal.Chr = *PStrGetConstPtr(&Literal);
        PStrDeinit(&Literal);
        return CharToken;
    }
    else 
    {
        Token StringToken = MakeToken(Lexer, TOKEN_STRING_LITERAL);

        /* consume opening and closing quotes */
        StringToken.Literal.Str = Literal;
        /* ownership transfered */
        return StringToken;
    }
}



static Token ConsumeWord(PascalTokenizer *Lexer)
{
    while (IsAlphaNum(*Lexer->Curr) || '_' == *Lexer->Curr)
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

    static const Keyword KeywordLut[][6] = 
    {
        ['A'] = {
            {.Str = (const U8 *)"ND",   .Len = 2, .Type = TOKEN_AND}, 
            {.Str = (const U8 *)"RRAY", .Len = 4, .Type = TOKEN_ARRAY}, 
            {.Str = (const U8 *)"SM",   .Len = 2, .Type = TOKEN_ASM},
            {.Str = (const U8 *)"SR",   .Len = 2, .Type = TOKEN_ASR},
        },
        ['B'] = {
            {.Str = (const U8 *)"EGIN", .Len = 4, .Type = TOKEN_BEGIN},
            {.Str = (const U8 *)"REAK", .Len = 4, .Type = TOKEN_BREAK},
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
            {.Str = (const U8 *)"XIT", .Len = 3, .Type = TOKEN_EXIT},
        },
        ['F'] = {
            {.Str = (const U8 *)"OR", .Len = 2, .Type = TOKEN_FOR},
            {.Str = (const U8 *)"ILE", .Len = 3, .Type = TOKEN_FILE},
            {.Str = (const U8 *)"ALSE", .Len = 4, .Type = TOKEN_FALSE},
            {.Str = (const U8 *)"UNCTION", .Len = 7, .Type = TOKEN_FUNCTION},
            {.Str = (const U8 *)"ORWARD", .Len = 6, .Type = TOKEN_FORWARD},
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
            {.Str = (const U8 *)"OD", .Len = 2, .Type = TOKEN_MOD},
        },
        ['N'] = {
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
            {.Str = (const U8 *)"EPEAT", .Len = 5, .Type = TOKEN_REPEAT},
            {.Str = (const U8 *)"ECORD", .Len = 5, .Type = TOKEN_RECORD},
            {.Str = (const U8 *)"ESULT", .Len = 5, .Type = TOKEN_RESULT},
        },
        ['S'] = {
            {.Str = (const U8 *)"ET", .Len = 2, .Type = TOKEN_SET},
            {.Str = (const U8 *)"HR", .Len = 2, .Type = TOKEN_SHR},
            {.Str = (const U8 *)"HL", .Len = 2, .Type = TOKEN_SHL},
        },
        ['T'] = {
            {.Str = (const U8 *)"HEN", .Len = 3, .Type = TOKEN_THEN},
            {.Str = (const U8 *)"RUE", .Len = 3, .Type = TOKEN_TRUE},
            {.Str = (const U8 *)"YPE", .Len = 3, .Type = TOKEN_TYPE},
            {.Str = (const U8 *)"O", .Len = 1, .Type = TOKEN_TO},
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
        const Keyword *KeywordSlot = &KeywordLut[Key][i];
        if (0 == KeywordSlot->Len)
            break;

        if (LexemeLen - 1 == KeywordSlot->Len
        && AlphaArrayEquNoCase(Lexer->Start + 1, KeywordSlot->Str, KeywordSlot->Len))
        {
            return KeywordSlot->Type;
        }
    }
    return TOKEN_IDENTIFIER;
}






static void UpdateLine(PascalTokenizer *Lexer)
{
    Lexer->LinePtr = Lexer->Curr;
    Lexer->Line++;
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


