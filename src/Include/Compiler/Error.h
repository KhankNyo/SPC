#ifndef PASCAL_ERROR_H
#define PASCAL_ERROR_H


#include "Compiler/Data.h"

void VaListError(PVMCompiler *Compiler, const Token *Tok, const char *Fmt, va_list Args);
void ErrorAt(PVMCompiler *Compiler, const Token *Tok, const char *Fmt, ...);
bool ConsumeOrError(PVMCompiler *Compiler, TokenType Expected, const char *ErrFmt, ...);
#define Error(pCompiler, ...) ErrorAt(pCompiler, &(pCompiler)->Next, __VA_ARGS__)


#endif /* PASCAL_ERROR_H */
