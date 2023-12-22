#ifndef PASCAL_EXPR_H
#define PASCAL_EXPR_H

#include "Compiler/Data.h"

VarLocation CompileExpr(PascalCompiler *Compiler);
VarLocation CompileExprIntoReg(PascalCompiler *Compiler);
VarLocation CompileVariableExpr(PascalCompiler *Compiler);
/* OpToken here is only for error reporting */
void CompileExprInto(PascalCompiler *Compiler, 
        const Token *OpToken, VarLocation *Location
);
bool LiteralEqual(VarLiteral A, VarLiteral B, IntegralType Type);
void FreeExpr(PascalCompiler *Compiler, VarLocation Expr);


bool ConvertTypeImplicitly(PascalCompiler *Compiler, IntegralType To, VarLocation *From);
IntegralType CoerceTypes(PascalCompiler *Compiler, 
        const Token *Op, IntegralType Left, IntegralType Right
);


#endif /* PASCAL_EXPR_H */

