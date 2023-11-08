
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
void PVMCompilerDeinit(PVMCompiler *Compiler, Operand ExitCode);


static Operand PVMCompileExpr(PVMCompiler *Compiler, const AstExpr *Expr);
static Operand PVMCompileSimpleExpr(PVMCompiler *Compiler, const AstSimpleExpr *SimpleExpr);
static Operand PVMCompileTerm(PVMCompiler *Compiler, const AstTerm *Term);
static Operand PVMCompileFactor(PVMCompiler *Compiler, const AstFactor *Factor);

static Operand PVMEmitLoadI32(PVMCompiler *Compiler, I32 Integer);
static Operand PVMEmitAddI32(PVMCompiler *Compiler, Operand Left, Operand Right);
static Operand PVMEmitSubI32(PVMCompiler *Compiler, Operand Left, Operand Right);
static Operand PVMEmitMulI32(PVMCompiler *Compiler, Operand Left, Operand Right);
static Operand PVMEmitDivI32(PVMCompiler *Compiler, Operand Left, Operand Right);
static void PVMEmitExit(PVMCompiler *Compiler, Operand ExitCode);

static Operand PVMAllocateRegister(PVMCompiler *Compiler);
static void PVMFreeRegister(PVMCompiler *Compiler, Operand Register);





CodeChunk PVMCompile(const PascalAst *Ast)
{
    PVMCompiler Compiler = PVMCompilerInit(Ast);
    Operand ReturnValue = PVMCompileExpr(&Compiler, &Ast->Expression);
    PVMCompilerDeinit(&Compiler, ReturnValue);

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

void PVMCompilerDeinit(PVMCompiler *Compiler, Operand ExitCode)
{
    PVMEmitExit(Compiler, ExitCode);
}




static Operand PVMCompileExpr(PVMCompiler *Compiler, const AstExpr *Expr)
{
    Operand Left = PVMCompileSimpleExpr(Compiler, &Expr->Left);
    return Left;
}

static Operand PVMCompileSimpleExpr(PVMCompiler *Compiler, const AstSimpleExpr *SimpleExpr)
{
    Operand Left = PVMCompileTerm(Compiler, &SimpleExpr->Left);
    Operand Result = Left;

    const AstOpTerm *RightSimpleExpr = SimpleExpr->Right;
    while (NULL != RightSimpleExpr)
    {
        Operand Right = PVMCompileTerm(Compiler, &RightSimpleExpr->Term);

        switch (RightSimpleExpr->Op)
        {
        case TOKEN_PLUS:
        {
            Result = PVMEmitAddI32(Compiler, Result, Right);
        } break;
        case TOKEN_MINUS:
        {
            Result = PVMEmitSubI32(Compiler, Result, Right);
        } break;

        case TOKEN_NOT:
        default: 
        {
            PASCAL_UNREACHABLE("CompileSimpleExpr: %s is an invalid op\n", 
                    TokenTypeToStr(RightSimpleExpr->Op)
            );
        } break;
        }

        PVMFreeRegister(Compiler, Right);
        RightSimpleExpr = RightSimpleExpr->Next;
    } 

    if (Result != Left)
    {
        PVMFreeRegister(Compiler, Left);
    }
    return Result;
}


static Operand PVMCompileTerm(PVMCompiler *Compiler, const AstTerm *Term)
{
    Operand Left = PVMCompileFactor(Compiler, &Term->Left);
    Operand Result = Left;

    const AstOpFactor *RightFactor = Term->Right;
    while (NULL != RightFactor)
    {
        Operand Right = PVMCompileFactor(Compiler, &RightFactor->Factor);
        switch (RightFactor->Op)
        {
        case TOKEN_STAR:
        {
            Result = PVMEmitMulI32(Compiler, Result, Right);
        } break;

        case TOKEN_SLASH:
        case TOKEN_DIV:
        {
            Result = PVMEmitDivI32(Compiler, Result, Right);
        } break;

        case TOKEN_MOD:
        case TOKEN_AND:
        default:
        {
            PASCAL_UNREACHABLE("CompileTerm: %s is an invalid op\n", 
                    TokenTypeToStr(RightFactor->Op)
            );
        } break;
        }

        PVMFreeRegister(Compiler, Right);
        RightFactor = RightFactor->Next;
    }

    if (Result != Left)
    {
        PVMFreeRegister(Compiler, Left);
    }
    return Result;
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
        PASCAL_UNREACHABLE("CompileTerm: Invalid factor: %d\n", Factor->Type);
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

static Operand PVMEmitMulI32(PVMCompiler *Compiler, Operand Left, Operand Right)
{
    Operand Result = PVMAllocateRegister(Compiler);
    Operand Dummy = PVMAllocateRegister(Compiler);
    CodeChunkWrite(&Compiler->Code, PVM_DI_SPECIAL_INS(MUL, Result, Left, Right, false, Dummy));
    PVMFreeRegister(Compiler, Dummy);
    return Result;
}

static Operand PVMEmitDivI32(PVMCompiler *Compiler, Operand Left, Operand Right)
{
    Operand Result = PVMAllocateRegister(Compiler);
    Operand Dummy = PVMAllocateRegister(Compiler);
    CodeChunkWrite(&Compiler->Code, PVM_DI_SPECIAL_INS(DIV, Result, Left, Right, false, Dummy));
    PVMFreeRegister(Compiler, Dummy);
    return Result;
}


static void PVMEmitExit(PVMCompiler *Compiler, Operand ExitCode)
{
    CodeChunkWrite(&Compiler->Code, PVM_DI_TRANSFER_INS(MOV, PVM_REG_RET, ExitCode));
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

