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

/* returns the same addr passed in on error, else returns the next addr,
 * Alignment must be a power of 2 */
U32 CompileVarList(PascalCompiler *Compiler, UInt BaseRegister, U32 StartAddr, U32 Alignment);

/* returns the record, or NULL on error */
PascalVar *CompileRecordDefinition(PascalCompiler *Compiler, const Token *Name);


/* expects PVMStartArg to have been called,
 * '(' or nothing to be the next token */
void CompileArgumentList(PascalCompiler *Compiler, 
        const Token *Callee, const SubroutineData *Subroutine,
        I32 *Base, UInt HiddenParamCount
);

void CompilerEmitCall(PascalCompiler *Compiler, const VarLocation *Location, SaveRegInfo SaveRegs);


#endif /* PASCAL_COMPILER_VARLIST_H */

