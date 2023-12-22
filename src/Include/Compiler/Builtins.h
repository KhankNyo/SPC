#ifndef PASCAL_BUILTINS_H
#define PASCAL_BUILTINS_H


#include "Vartab.h"
#include "Typedefs.h"
#include "Compiler/Data.h"

void DefineCrtSubroutines(PascalCompiler *Compiler);
void DefineSystemSubroutines(PascalCompiler *Compiler);

struct OptionalReturnValue 
{
    bool HasReturnValue;
    VarLocation ReturnValue;
};
OptionalReturnValue CompileCallToBuiltin(PascalCompiler *Compiler, VarBuiltinRoutine BuiltinCallee);


#endif /* PASCAL_BUILTINS_H */

