

#include <stdarg.h>
#include "Compiler/Compiler.h"
#include "Compiler/Error.h"



static U32 LineLen(const U8 *s)
{
    U32 i = 0;
    for (; s[i] && s[i] != '\n' && s[i] != '\r'; i++)
    {}
    return i;
}



static void PrintAndHighlightSource(FILE *LogFile, const Token *Tok)
{
    const U8 *LineStart = Tok->Lexeme.Str - Tok->LineOffset + 1;
    char Highlighter = '^';
    U32 Len = LineLen(LineStart);

    /* offending LOC */
    fprintf(LogFile, "\n    \"%.*s\"", Len, LineStart);

    /* highligher */
    fprintf(LogFile, "\n     ");
    for (int i = 0; i < (int)Tok->LineOffset - 1; i++)
    {
        char WhiteSpace = '\t' == LineStart[i] ? '\t' : ' ';
        fputc(WhiteSpace, LogFile);
    }
    for (U32 i = 0; i < Tok->Lexeme.Len; i++)
    {
        fputc(Highlighter, LogFile);
    }
}


void VaListError(PascalCompiler *Compiler, const Token *Tok, const char *Fmt, va_list Args)
{
    Compiler->Error = true;
    if (!Compiler->Panic)
    {
        /* TODO: file name */
        /* sample:
         * [line 5, offset 10]
         *     "function bad(): integer;"
         *               ^^^
         * Error: ...
         */

        Compiler->Panic = true;
        fprintf(Compiler->LogFile, "\n[line %d, offset %d]", 
                Tok->Line, Tok->LineOffset
        );
        PrintAndHighlightSource(Compiler->LogFile, Tok);

        fprintf(Compiler->LogFile, "\n    Error: ");
        vfprintf(Compiler->LogFile, Fmt, Args);
        fputc('\n', Compiler->LogFile);
    }
}

void ErrorAt(PascalCompiler *Compiler, const Token *Tok, const char *Fmt, ...)
{
    va_list Args;
    va_start(Args, Fmt);
    VaListError(Compiler, Tok, Fmt, Args);
    va_end(Args);
}

bool ConsumeOrError(PascalCompiler *Compiler, TokenType Expected, const char *ErrFmt, ...)
{
    if (!ConsumeIfNextTokenIs(Compiler, Expected))
    {
        va_list Args;
        va_start(Args, ErrFmt);
        VaListError(Compiler, &Compiler->Next, ErrFmt, Args);
        va_end(Args);
        return false;
    }
    return true;
}

void ErrorTypeMismatch(PascalCompiler *Compiler, 
        const Token *At, const char *Place, const char *ExpectedType, IntegralType Got)
{
    ErrorAt(Compiler, At, "Invalid type for %s, expected %s but got %s.",
            Place, ExpectedType, IntegralTypeToStr(Got)
    );

}





