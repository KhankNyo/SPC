

#include <stdarg.h>
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
    const U8 *LineStart = Tok->Str - Tok->LineOffset + 1;
    char Highlighter = '^';
    U32 Len = LineLen(LineStart);

    fprintf(LogFile, "\n    \"%.*s\"", Len, LineStart);
    fprintf(LogFile, "\n    %*s", Tok->LineOffset, "");
    for (U32 i = 0; i < Tok->Len; i++)
    {
        fputc(Highlighter, LogFile);
    }
}


void VaListError(PVMCompiler *Compiler, const Token *Tok, const char *Fmt, va_list Args)
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

void ErrorAt(PVMCompiler *Compiler, const Token *Tok, const char *Fmt, ...)
{
    va_list Args;
    va_start(Args, Fmt);
    VaListError(Compiler, Tok, Fmt, Args);
    va_end(Args);
}

bool ConsumeOrError(PVMCompiler *Compiler, TokenType Expected, const char *ErrFmt, ...)
{
    if (!ConsumeIfNextIs(Compiler, Expected))
    {
        va_list Args;
        va_start(Args, ErrFmt);
        VaListError(Compiler, &Compiler->Next, ErrFmt, Args);
        va_end(Args);
        return false;
    }
    return true;
}




