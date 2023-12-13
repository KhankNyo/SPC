#ifndef PASCAL_EXPR_H
#define PASCAL_EXPR_H

#include "Compiler/Data.h"

VarLocation CompileExpr(PVMCompiler *Compiler);
VarLocation CompileExprIntoReg(PVMCompiler *Compiler);
VarLocation CompileVariableExpr(PVMCompiler *Compiler);
/* OpToken here is only for error reporting */
void CompileExprInto(PVMCompiler *Compiler, 
        const Token *OpToken, VarLocation *Location
);
void FreeExpr(PVMCompiler *Compiler, VarLocation Expr);


bool ConvertTypeImplicitly(PVMCompiler *Compiler, IntegralType To, VarLocation *From);
IntegralType CoerceTypes(PVMCompiler *Compiler, 
        const Token *Op, IntegralType Left, IntegralType Right
);


#endif /* PASCAL_EXPR_H */

