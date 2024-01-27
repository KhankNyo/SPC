#ifndef PASCAL_COMPILER_VARLIST_H
#define PASCAL_COMPILER_VARLIST_H


#include "Common.h"
#include "Compiler/Data.h"




/* nothing can be NULL */
/*
 * expects '(' to have been consumed 
 * compiles a subroutine's parameter list, 
 * returns the parameter list */
SubroutineParameterList CompileParameterList(PascalCompiler *Compiler, PascalVartab *Scope);

/* like the above function but expects '(' and not ')' to be the next token */
SubroutineParameterList CompileParameterListWithParentheses(PascalCompiler *Compiler, PascalVartab *Scope);

/* Parses type name, 
 *      if Identifier is NULL: the returned type is in the Out parameter, 
 *      else                 : in addition to returning the type via Out, 
 *                              this fn also define the given identifier with 
 *                              Out as its type
 * returns true on success, false on failure */
bool ParseAndDefineTypename(PascalCompiler *Compiler, const Token *Identifier, VarType *Out);


/* returns the same addr passed in on error, else returns the next addr,
 * Alignment must be a power of 2 */
U32 CompileVarList(PascalCompiler *Compiler, UInt BaseRegister, U32 StartAddr, U32 Alignment);

/* expects PVMStartArg to have been called,
 * '(' or nothing to be the next token */
void CompileArgumentList(PascalCompiler *Compiler, 
        const Token *Callee, const SubroutineData *Subroutine,
        I32 *Base, UInt HiddenParamCount
);

void CompilerEmitCall(PascalCompiler *Compiler, const VarLocation *Location, SaveRegInfo SaveRegs);


#endif /* PASCAL_COMPILER_VARLIST_H */

