#ifndef PASCAL_COMPILER_VARLIST_H
#define PASCAL_COMPILER_VARLIST_H


#include "Common.h"
#include "Compiler/Data.h"


/* nothing can be NULL */
/*
 * compiles a subroutine's parameter list, 
 * returns the parameter list */
SubroutineParameterList CompileParameterList(PVMCompiler *Compiler, PascalVartab *Scope);

/* returns the same addr passed in on error, else returns the next addr,
 * Alignment must be a power of 2 */
U32 CompileVarList(PVMCompiler *Compiler, UInt BaseRegister, U32 StartAddr, U32 Alignment);

/* returns the record, or NULL on error */
PascalVar *CompileRecordDefinition(PVMCompiler *Compiler, const Token *Name);

/* nothing can be NULL, expects next token to be '(' */
void CompileArgumentList(PVMCompiler *Compiler, 
        const Token *FunctionName, const SubroutineData *Subroutine
);

/* nothing can be NULL, expects current token to be '(' */
void CompilePartialArgumentList(PVMCompiler *Compiler, 
        const Token *FunctionName, const SubroutineParameterList *ParameterList, 
        U32 StackArgSize, bool RecordReturnType
);

/* NULL can be passed into OutReturnValue */
void CompileSubroutineCall(PVMCompiler *Compiler,
        VarLocation *Callee, const Token *Name, VarLocation *OutReturnValue
);


#endif /* PASCAL_COMPILER_VARLIST_H */

