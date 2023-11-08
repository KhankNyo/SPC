
#include "PVM/PVM.h"
#include "PVM/CodeGen.h"


typedef int Operand;
typedef struct PVMCompiler
{
    const PascalAst *Ast;
    CodeChunk Code;
    bool HasError;
    U32 RegisterList;
} PVMCompiler;

PVMCompiler PVMCompilerInit(const PascalAst *Ast);
void PVMCompilerDeinit(PVMCompiler *Compiler);


static Operand PVMCompileExpr(PVMCompiler *Compiler, const AstExpr *Expr);
static Operand PVMCompileSimpleExpr(PVMCompiler *Compiler, const AstSimpleExpr *SimpleExpr);
static Operand PVMCompileTerm(PVMCompiler *Compiler, const AstTerm *Term);
static Operand PVMCompileFactor(PVMCompiler *Compiler, const AstFactor *Factor);

static Operand PVMEmitLoadI32(PVMCompiler *Compiler, I32 Integer);
static Operand PVMEmitAddI32(PVMCompiler *Compiler, Operand Left, Operand Right);
static Operand PVMEmitSubI32(PVMCompiler *Compiler, Operand Left, Operand Right);
static void PVMEmitReturn(PVMCompiler *Compiler, Operand Value);
static void PVMEmitExit(PVMCompiler *Compiler);

static Operand PVMAllocateRegister(PVMCompiler *Compiler);
static void PVMFreeRegister(PVMCompiler *Compiler, Operand Register);



CodeChunk PVMCompile(const PascalAst *Ast)
{
    PVMCompiler Compiler = PVMCompilerInit(Ast);
    Operand ReturnValue = PVMCompileExpr(&Compiler, &Ast->Expression);
    PVMEmitReturn(&Compiler, ReturnValue);
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
        .RegisterList = 0,
    };
    return Compiler;
}

void PVMCompilerDeinit(PVMCompiler *Compiler)
{
    PVMEmitExit(Compiler);
}




static Operand PVMCompileExpr(PVMCompiler *Compiler, const AstExpr *Expr)
{
    return PVMCompileSimpleExpr(Compiler, &Expr->SimpleExpression);
}

static Operand PVMCompileSimpleExpr(PVMCompiler *Compiler, const AstSimpleExpr *SimpleExpr)
{
    Operand Left = PVMCompileTerm(Compiler, &SimpleExpr->Left);
    if (NULL == SimpleExpr->Right)
    {
        return Left;
    }

    Operand Right = PVMCompileSimpleExpr(Compiler, SimpleExpr->Right);
    Operand Result;

    switch (SimpleExpr->InfixOp.Type)
    {
    case TOKEN_PLUS:
    {
        Result = PVMEmitAddI32(Compiler, Left, Right);
    } break;
    case TOKEN_MINUS:
    {
        Result = PVMEmitSubI32(Compiler, Left, Right);
    } break;

    case TOKEN_NOT:
    default: 
    {
        PASCAL_UNREACHABLE("SimpleExpr: '%s' is invalid\n", TokenTypeToStr(SimpleExpr->InfixOp.Type));
    } break;
    }

    PVMFreeRegister(Compiler, Left);
    PVMFreeRegister(Compiler, Right);
    return Result;
}


static Operand PVMCompileTerm(PVMCompiler *Compiler, const AstTerm *Term)
{
    return PVMCompileFactor(Compiler, &Term->Factor);
}

static Operand PVMCompileFactor(PVMCompiler *Compiler, const AstFactor *Factor)
{
    switch (Factor->Type)
    {
    case FACTOR_INTEGER:
    {
        return PVMEmitLoadI32(Compiler, Factor->As.Integer);
    } break;

    case FACTOR_GROUP_EXPR:
    {
        return PVMCompileExpr(Compiler, Factor->As.Expression);
    } break;


    default: 
    {
        PASCAL_UNREACHABLE("TODO: other factors\n");
    } break;
    }
    return 0;
}






static Operand PVMEmitLoadI32(PVMCompiler *Compiler, I32 Integer)
{
    Operand Reg = PVMAllocateRegister(Compiler);
    CodeChunkWrite(&Compiler->Code, PVM_IRD_ARITH_INS(LDI, Reg, Integer));
    return Reg;
}


static Operand PVMEmitAddI32(PVMCompiler *Compiler, Operand Left, Operand Right)
{
    Operand Result = PVMAllocateRegister(Compiler);
    CodeChunkWrite(&Compiler->Code, PVM_DI_ARITH_INS(ADD, Result, Left, Right, 0));
    return Result;
}


static Operand PVMEmitSubI32(PVMCompiler *Compiler, Operand Left, Operand Right)
{
    Operand Result = PVMAllocateRegister(Compiler);
    CodeChunkWrite(&Compiler->Code, PVM_DI_ARITH_INS(SUB, Result, Left, Right, 0));
    return Result;
}


static void PVMEmitReturn(PVMCompiler *Compiler, Operand Value)
{
    
}


static void PVMEmitExit(PVMCompiler *Compiler)
{
    CodeChunkWrite(&Compiler->Code, PVM_SYS_INS(EXIT));
}





static Operand PVMAllocateRegister(PVMCompiler *Compiler)
{
    for (UInt i = 0; i < PVM_REG_COUNT; i++)
    {
        /* found free reg */
        if ((Compiler->RegisterList >> i & 1u) == 0)
        {
            /* mark reg as allocated */
            Compiler->RegisterList |= 1u << i;
            return i;
        }
    }
    PASCAL_UNREACHABLE("All registers are full");
    return 0;
}

static void PVMFreeRegister(PVMCompiler *Compiler, Operand Register)
{
    Compiler->RegisterList &= ~(1u << Register);
}

