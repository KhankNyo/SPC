

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

/* returns ExpectedType if the lexeme from Start to Curr is equal to the Rest string,
 * else return TOKEN_IDENTIFIER */
static TokenType IdentifierOrKeyword(Tokenizer *Lexer, 
        UInt StartIdx, 
        UInt RestLen, const char *Rest, 
        TokenType ExpectedType
);



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
    UInt LexemeLen = Lexer->Curr - Lexer->Start;
    if (LexemeLen < 2)
    {
        return TOKEN_IDENTIFIER;
    }

#define LETTER(Index) CHR_TO_UPPER(Lexer->Start[Index])
    switch (CHR_TO_UPPER(Lexer->Start[0]))
    {

    case 'A':
    {
        switch (LETTER(1))
        {
        case 'N': return IdentifierOrKeyword(Lexer, 2, 1, "D", TOKEN_AND);
        case 'R': return IdentifierOrKeyword(Lexer, 2, 3, "RAY", TOKEN_ARRAY);
        case 'S': return IdentifierOrKeyword(Lexer, 2, 1, "M", TOKEN_ASM);
        default: break;
        }
    } break;
    

    case 'B':
    {
        if ('E' == LETTER(1))
            return IdentifierOrKeyword(Lexer, 2, 3, "GIN", TOKEN_BEGIN);
        else return IdentifierOrKeyword(Lexer, 1, 4, "REAK", TOKEN_BREAK);
    } break;


    case 'C':
    {
        if ('A' == LETTER(1))
            return IdentifierOrKeyword(Lexer, 2, 2, "SE", TOKEN_CASE);

        if ((LexemeLen >= sizeof("CONST") - 1)
        && LETTER(1) == 'O' && LETTER(2) == 'N')
        {
            TokenType Type = IdentifierOrKeyword(Lexer, 3, 2, "ST", TOKEN_CONST);
            if (TOKEN_IDENTIFIER != Type)
                return Type;

            Type = IdentifierOrKeyword(Lexer, 3, 5, "TINUE", TOKEN_CONTINUE);
            if (TOKEN_IDENTIFIER != Type)
                return Type;

            return IdentifierOrKeyword(Lexer, 3, 8, "STRUCTOR", TOKEN_CONSTRUCTOR);
        }
    } break;


    case 'D':
    {
        switch (LETTER(1))
        {
        case 'E': return IdentifierOrKeyword(Lexer, 2, 8, "STRUCTOR", TOKEN_DESTRUCTOR);
        case 'I': return IdentifierOrKeyword(Lexer, 2, 1, "V", TOKEN_DIV);
        case 'O': 
        {
            if (LexemeLen == 2) 
                return TOKEN_DO;
            return IdentifierOrKeyword(Lexer, 2, 4, "WNTO", TOKEN_DOWNTO);
        } break;
        default: break;
        }
    } break;


    case 'E': 
    {
        if ('L' == LETTER(1))
            return IdentifierOrKeyword(Lexer, 2, 2, "SE", TOKEN_ELSE);
        else return IdentifierOrKeyword(Lexer, 1, 2, "ND", TOKEN_END);
    } break;


    case 'F':
    {
        switch (LETTER(1))
        {
        case 'A': return IdentifierOrKeyword(Lexer, 2, 3, "LSE", TOKEN_FALSE);
        case 'I': return IdentifierOrKeyword(Lexer, 2, 2, "LE", TOKEN_FILE);
        case 'O': return IdentifierOrKeyword(Lexer, 2, 1, "R", TOKEN_FOR);
        case 'U': return IdentifierOrKeyword(Lexer, 2, 6, "NCTION", TOKEN_FUNCTION);
        default: break;
        }
    } break;


    case 'G': return IdentifierOrKeyword(Lexer, 1, 3, "OTO", TOKEN_GOTO);


    case 'I':
    {
        switch (LETTER(1))
        {
        case 'F': return LexemeLen == 2 ? TOKEN_IF : TOKEN_IDENTIFIER;
        case 'M': return IdentifierOrKeyword(Lexer, 2, 12, "PLEMENTATION", TOKEN_IMPLEMENTATION);
        case 'N':
        {
            if (LexemeLen == 2)
                return TOKEN_IN;
            else if ('L' == LETTER(2))
                return IdentifierOrKeyword(Lexer, 3, 3, "INE", TOKEN_INLINE);
            else return IdentifierOrKeyword(Lexer, 2, 7, "TERFACE", TOKEN_INTERFACE);
        } break;
        default: break;
        }
    } break;


    case 'L': return IdentifierOrKeyword(Lexer, 1, 4, "ABEL", TOKEN_LABEL);
    case 'M': return IdentifierOrKeyword(Lexer, 1, 3, "ODE", TOKEN_MODE);

    case 'N': 
    {
        if ('I' == LETTER(1))
            return IdentifierOrKeyword(Lexer, 2, 1, "L", TOKEN_NIL);
        else return IdentifierOrKeyword(Lexer, 1, 2, "OT", TOKEN_NOT);
    } break;


    case 'O':
    {
        switch (LETTER(1))
        {
        case 'R': return LexemeLen == 2 ? TOKEN_OR : TOKEN_IDENTIFIER;
        case 'F': return LexemeLen == 2 ? TOKEN_OF : TOKEN_IDENTIFIER;
        case 'N': return LexemeLen == 2 ? TOKEN_ON : TOKEN_IDENTIFIER;
        case 'B': return IdentifierOrKeyword(Lexer, 2, 4, "JECT", TOKEN_OBJECT);
        case 'P': return IdentifierOrKeyword(Lexer, 2, 6, "ERATOR", TOKEN_OPERATOR);
        default: break;
        }
    } break;


    case 'P': 
    {
        if ('A' == LETTER(1))
            return IdentifierOrKeyword(Lexer, 2, 4, "CKED", TOKEN_PACKED);

        if ((LexemeLen >= sizeof("PROGRAM") - 1)
        && ('R' == LETTER(1) && 'O' == LETTER(2)))
        {
            if ('G' == LETTER(3))
                return IdentifierOrKeyword(Lexer, 4, 3, "RAM", TOKEN_PROGRAM);
            else return IdentifierOrKeyword(Lexer, 3, 6, "CEDURE", TOKEN_PROCEDURE);
        }
    } break;


    case 'R':
    {
        if ((LexemeLen == (sizeof("RECORD") - 1))
        && 'E' == LETTER(1))
        {
            if ('C' == LETTER(2))
                return IdentifierOrKeyword(Lexer, 3, 3, "ORD", TOKEN_RECORD);
            else return IdentifierOrKeyword(Lexer, 2, 4, "PEAT", TOKEN_REPEAT);
        }
    } break;


    case 'S':
    {
        switch (LETTER(1))
        {
        case 'E': return IdentifierOrKeyword(Lexer, 2, 1, "T", TOKEN_SET);
        case 'T': return IdentifierOrKeyword(Lexer, 2, 4, "RING", TOKEN_STRING);
        case 'H': 
        {
            if (LexemeLen == sizeof("SHL") - 1)
            {
                if ('L' == LETTER(2))
                    return TOKEN_SHL;
                if ('R' == LETTER(2))
                    return TOKEN_SHR;
            }
        } break;
        default: break;
        }
    } break;


    case 'T': 
    {
        switch (LETTER(1))
        {
        case 'R': return IdentifierOrKeyword(Lexer, 2, 2, "UE", TOKEN_TRUE);
        case 'H': return IdentifierOrKeyword(Lexer, 2, 2, "EN", TOKEN_THEN);
        case 'Y': return IdentifierOrKeyword(Lexer, 2, 2, "PE", TOKEN_TYPE);
        default: break;
        }
    } break;


    case 'U':
    {
        if ('S' == LETTER(1))
            return IdentifierOrKeyword(Lexer, 2, 2, "ES", TOKEN_USES);

        if (LexemeLen >= sizeof("UNIT") - 1
        && 'N' == LETTER(1))
        {
            if ('I' == LETTER(2))
                return IdentifierOrKeyword(Lexer, 3, 1, "T", TOKEN_UNIT);
            else return IdentifierOrKeyword(Lexer, 2, 3, "TIL", TOKEN_UNTIL);
        }
    } break;


    case 'V': return IdentifierOrKeyword(Lexer, 1, 2, "AR", TOKEN_VAR);


    case 'W': 
    {
        if ('H' == LETTER(1))
            return IdentifierOrKeyword(Lexer, 2, 3, "ILE", TOKEN_WHILE);
        else return IdentifierOrKeyword(Lexer, 1, 3, "ITH", TOKEN_WITH);
    } break;


    case 'X': return IdentifierOrKeyword(Lexer, 1, 2, "OR", TOKEN_XOR);

        
    default: break;
    }
#undef LETTER

    return TOKEN_IDENTIFIER;
}


static TokenType IdentifierOrKeyword(Tokenizer *Lexer, UInt StartIdx, UInt RestLen, const char *Rest, TokenType ExpectedType)
{
    UInt ExpectedLen = StartIdx + RestLen;
    UInt LexemeLen = Lexer->Curr - Lexer->Start;

    if (ExpectedLen == LexemeLen
    && AlphaArrayEquNoCase(Lexer->Start + StartIdx, Rest, RestLen))
    {
        return ExpectedType;
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


