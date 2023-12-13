#ifndef PASCAL_BUILTINS_H
#define PASCAL_BUILTINS_H


#include "Vartab.h"
#include "Typedefs.h"
#include "Compiler/Data.h"

void DefineCrtSubroutines(PVMCompiler *Compiler);
void DefineSystemSubroutines(PVMCompiler *Compiler);

struct OptionalReturnValue 
{
    bool HasReturnValue;
    VarLocation ReturnValue;
};
OptionalReturnValue CompileCallToBuiltin(PVMCompiler *Compiler, VarBuiltinRoutine BuiltinCallee);


#endif /* PASCAL_BUILTINS_H */

