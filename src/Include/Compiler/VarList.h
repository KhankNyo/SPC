#ifndef PASCAL_COMPILER_VARLIST_H
#define PASCAL_COMPILER_VARLIST_H


#include "Common.h"
#include "Compiler/Data.h"


/* nothing can be NULL */
bool CompileParameterList(PVMCompiler *Compiler, VarSubroutine *CurrentFunction);

/* returns the same addr passed in on error, else returns the next addr,
 * Alignment must be a power of 2 */
U32 CompileVarList(PVMCompiler *Compiler, U32 StartAddr, UInt Alignment, bool Global);

/* returns the record, or NULL on error */
PascalVar *CompileRecordDefinition(PVMCompiler *Compiler, const Token *Name);

/* nothing can be NULL */
void CompileArgumentList(PVMCompiler *Compiler, 
        const Token *FunctionName, const VarSubroutine *Subroutine
);

/* NULL can be passed into OutReturnValue */
void CompileSubroutineCall(PVMCompiler *Compiler,
        VarSubroutine *Callee, const Token *Name, VarLocation *OutReturnValue
);


#endif /* PASCAL_COMPILER_VARLIST_H */

