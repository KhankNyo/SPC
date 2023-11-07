
#include "PVM/CodeGen.h"


typedef struct PVMCompiler
{
    const PascalAst *Ast;
    CodeChunk Code;
    bool HasError;
} PVMCompiler;

PVMCompiler PVMCompilerInit(const PascalAst *Ast);
void PVMCompilerDeinit(PVMCompiler *Compiler);



Operand PVMCompileExpr(PVMCompiler *Compiler, const AstExpr *Expr);



CodeChunk PVMCompile(const PascalAst *Ast)
{
    PVMCompiler Compiler = PVMCompilerInit(Ast);
    PVMCompileExpr(&Compiler, &Ast->Expression);
    PVMCompilerDeinit(&Compiler);

    if (Compiler.HasError)
    {
        return (CodeChunk) { 0 };
    }
    return Compiler.Code;
}




PVMCompiler PVMCompilerInit(const PascalAst *Ast)
{
    PVMCompiler Compiler = {
        .Ast = Ast,
        .Code = CodeChunkInit(1024),
        .HasError = false,
    };
    return Compiler;
}

void PVMCompilerDeinit(PVMCompiler *Compiler)
{
    (void)Compiler;
}




Operand PVMCompileExpr(PVMCompiler *Compiler, const AstExpr *Expr)
{
    return PVMCompileSimpleExpr(Compiler, Expr->SimpleExpression);
}

Operand PVMCompileSimpleExpr(PVMCompiler *Compiler, const AstSimpleExpr *SimpleExpr)
{
    Operand Left = PVMCompileTerm(Compiler, SimpleExpr->Left);
    switch (SimpleExpr->InfixOp.Type)
    {
    case TOKEN_PLUS:
    case TOKEN_MINUS:
    case TOKEN_NOT:
    default: 
    {
        PASCAL_UNREACHABLE("PVMCompileSimpleExpr: %s is invalid\n", TokenTypeToStr(SimpleExpr->InfixOp.Type));
    } break;
    }
    Operand Right = PVMCompileTerm(Compiler, SimpleExpr->Right);
}



