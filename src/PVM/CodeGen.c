
#include "PVM/PVM.h"
#include "PVM/CodeGen.h"


typedef UInt Operand;
typedef struct PVMCompiler
{
    const PascalAst *Ast;
    CodeChunk *Chunk;
    bool HasError;
    U32 RegisterList[32]; /* 32 scopes */

    UInt ScopeDepth;
} PVMCompiler;


PVMCompiler PVMCompilerInit(CodeChunk *Code, const PascalAst *Ast);
void PVMCompilerDeinit(PVMCompiler *Compiler);


static void PVMCompileBlock(PVMCompiler *Compiler, const AstBlock *Block);
static void PVMCompileVar(PVMCompiler *Compiler, const AstVarBlock *Declarations);
static void PVMCompileFunction(PVMCompiler *Compiler, const AstFunctionBlock *Function);
static void PVMCompileStmts(PVMCompiler *Compiler, const AstStmtBlock *Block);

static Operand PVMCompileExpr(PVMCompiler *Compiler, const AstExpr *Expr);
static Operand PVMCompileSimpleExpr(PVMCompiler *Compiler, const AstSimpleExpr *SimpleExpr);
static Operand PVMCompileTerm(PVMCompiler *Compiler, const AstTerm *Term);
static Operand PVMCompileFactor(PVMCompiler *Compiler, const AstFactor *Factor);

static void PVMEmitMovI32(PVMCompiler *Compiler, Operand Dest, Operand Src);
static void PVMEmitLoadI32(PVMCompiler *Compiler, Operand Dest, I32 Integer);
static void PVMEmitAddI32(PVMCompiler *Compiler, Operand Dest, Operand Left, Operand Right);
static void PVMEmitSubI32(PVMCompiler *Compiler, Operand Dest, Operand Left, Operand Right);
static void PVMEmitMulI32(PVMCompiler *Compiler, Operand Dest, Operand Left, Operand Right);
static void PVMEmitDivI32(PVMCompiler *Compiler, Operand Dividend, Operand Remainder, Operand Left, Operand Right);
static void PVMEmitSetCCU(
        PVMCompiler *Compiler, TokenType Op, 
        Operand Dest, Operand Left, Operand Right, UInt OperandSize
);
static void PVMEmitExit(PVMCompiler *Compiler);

static Operand PVMAllocateRegister(PVMCompiler *Compiler);
static void PVMFreeRegister(PVMCompiler *Compiler, Operand Register);
static CodeChunk *PVMCurrentChunk(PVMCompiler *Compiler);

static void PVMBeginScope(PVMCompiler *Compiler);
static void PVMEndScope(PVMCompiler *Compiler);





bool PVMCompile(CodeChunk *Chunk, const PascalAst *Ast)
{
    PVMCompiler Compiler = PVMCompilerInit(Chunk, Ast);
    PVMCompileBlock(&Compiler, Ast->Block);

    bool HasError = Compiler.HasError;
    PVMCompilerDeinit(&Compiler);
    return !HasError;
}




PVMCompiler PVMCompilerInit(CodeChunk *Chunk, const PascalAst *Ast)
{
    PVMCompiler Compiler = {
        .Ast = Ast,
        .Chunk = Chunk,
        .HasError = false,
        .RegisterList = { 0 },
        .ScopeDepth = 0,
    };
    return Compiler;
}

void PVMCompilerDeinit(PVMCompiler *Compiler)
{
    PVMEmitExit(Compiler);
}



static void PVMCompileBlock(PVMCompiler *Compiler, const AstBlock *Block)
{
    switch (Block->Type)
    {
    case AST_BLOCK_VAR:
    {
        PVMCompileVar(Compiler, (const AstVarBlock*)Block);
    } break;
    case AST_BLOCK_FUNCTION:
    {
        PVMCompileFunction(Compiler (const AstFunctionBlock*)Block);
    } break;
    case AST_BLOCK_STATEMENTS:
    {
        PVMBeginScope(Compiler);
        PVMCompileStmts(Compiler (const AstStmtBlock*)Block);
        PVMEndScope(Compiler);
    } break;

    case AST_BLOCK_INVALID:
        break;
    }

}




static void PVMCompileVar(PVMCompiler *Compiler, const AstVarBlock *Declarations)
{
    const AstVarList *I = &Declarations->Decl;
    do {
        Operand Register = PVMAllocateRegister(Compiler);
        PVMBindVariableToOperand(Compiler, I->Identifier, Register);
        I = I->Next;
    } while (NULL != I);
}


static void PVMCompileFunction(PVMCompiler *Compiler, const AstFunctionBlock *Function)
{
    PVMBindVariableToMemory(Compiler, Function->Identifier);
    PVMCompileBlock(Compiler, Function->Block);
}

static void PVMCompileStmts(PVMCompiler *Compiler, const AstStmtBlock *Block)
{
    const AstStmtList *I = Block->Statements;
    while (NULL != I)
    {
        const AstAssignStmt *Assignment = (const AstAssignStmt *)I->Statement;
        const Token *Identifier = &Assignment->Variable;
        switch (I->Statement->Type)
        {
        case AST_STMT_ASSIGNMENT:
        {
            Operand LValue = PVMGetLocationOf(Compiler, Identifier);
            Operand RValue = PVMCompileExpr(Compiler, &Assignment->Expr);
            PVMEmitMovI32(Compiler, LValue, RValue);
            PVMFreeRegister(Compiler, RValue);
        } break;

        case AST_STMT_INVALID:
        {
            PASCAL_UNREACHABLE("PVMCompileStmts: AST_STMT_INVALID encountered\n");
        } break;
        }

        I = I->Next;
    }
}




static void PVMBindVariableToOperand(PVMCompiler *Compiler, Token Variable, Operand Dest)
{
}


static void PVMBindVariableToMemory(PVMCompiler *Compiler, Token Name)
{
}





static Operand PVMCompileExpr(PVMCompiler *Compiler, const AstExpr *Expr)
{
    Operand Result = PVMCompileSimpleExpr(Compiler, &Expr->Left);
    const AstOpSimpleExpr *RightSimpleExpr = Expr->Right;
    while (NULL != RightSimpleExpr)
    {
        Operand Right = PVMCompileSimpleExpr(Compiler, &RightSimpleExpr->SimpleExpr);

        PVMEmitSetCCU(Compiler, RightSimpleExpr->Op, Result, Result, Right, sizeof(U32));

        PVMFreeRegister(Compiler, Right);
        RightSimpleExpr = RightSimpleExpr->Next;
    }

    return Result;
}

static Operand PVMCompileSimpleExpr(PVMCompiler *Compiler, const AstSimpleExpr *SimpleExpr)
{
    Operand Result = PVMCompileTerm(Compiler, &SimpleExpr->Left);

    const AstOpTerm *RightTerm = SimpleExpr->Right;
    while (NULL != RightTerm)
    {
        Operand Right = PVMCompileTerm(Compiler, &RightTerm->Term);

        switch (RightTerm->Op)
        {
        case TOKEN_PLUS:
        {
            PVMEmitAddI32(Compiler, Result, Result, Right);
        } break;
        case TOKEN_MINUS:
        {
            PVMEmitSubI32(Compiler, Result, Result, Right);
        } break;

        case TOKEN_NOT:
        default: 
        {
            PASCAL_UNREACHABLE("CompileSimpleExpr: %s is an invalid op\n", 
                    TokenTypeToStr(RightTerm->Op)
            );
        } break;
        }

        PVMFreeRegister(Compiler, Right);
        RightTerm = RightTerm->Next;
    } 

    return Result;
}


static Operand PVMCompileTerm(PVMCompiler *Compiler, const AstTerm *Term)
{
    Operand Result = PVMCompileFactor(Compiler, &Term->Left);

    const AstOpFactor *RightFactor = Term->Right;
    while (NULL != RightFactor)
    {
        Operand Right = PVMCompileFactor(Compiler, &RightFactor->Factor);
        switch (RightFactor->Op)
        {
        case TOKEN_STAR:
        {
            PVMEmitMulI32(Compiler, Result, Result, Right);
        } break;

        case TOKEN_SLASH:
        case TOKEN_DIV:
        {
            Operand Dummy = PVMAllocateRegister(Compiler);
            PVMEmitDivI32(Compiler, Result, Dummy, Result, Right);
            PVMFreeRegister(Compiler, Dummy);
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

    return Result;
}


static Operand PVMCompileFactor(PVMCompiler *Compiler, const AstFactor *Factor)
{
    switch (Factor->Type)
    {
    case FACTOR_INTEGER:
    {
        Operand Ret = PVMAllocateRegister(Compiler);
        PVMEmitLoadI32(Compiler, Ret, Factor->As.Integer);
        return Ret;
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





static void PVMEmitMovI32(PVMCompiler *Compiler, Operand Dest, Operand Src)
{
    CodeChunkWrite(PVMCurrentChunk(Compiler), PVM_DI_TRANSFER_INS(MOV, Dest, Src));
}

static void PVMEmitLoadI32(PVMCompiler *Compiler, Operand Dest, I32 Integer)
{
    CodeChunkWrite(PVMCurrentChunk(Compiler), PVM_IRD_ARITH_INS(LDI, Dest, Integer));
}


static void PVMEmitAddI32(PVMCompiler *Compiler, Operand Dest, Operand Left, Operand Right)
{
    CodeChunkWrite(PVMCurrentChunk(Compiler), PVM_DI_ARITH_INS(ADD, Dest, Left, Right, 0));
}


static void PVMEmitSubI32(PVMCompiler *Compiler, Operand Dest, Operand Left, Operand Right)
{
    CodeChunkWrite(PVMCurrentChunk(Compiler), PVM_DI_ARITH_INS(SUB, Dest, Left, Right, 0));
}

static void PVMEmitMulI32(PVMCompiler *Compiler, Operand Dest, Operand Left, Operand Right)
{
    CodeChunkWrite(PVMCurrentChunk(Compiler), PVM_DI_SPECIAL_INS(MUL, Dest, Left, Right, true, 0));
}

static void PVMEmitDivI32(PVMCompiler *Compiler, Operand Dividend, Operand Remainder, Operand Left, Operand Right)
{
    CodeChunkWrite(PVMCurrentChunk(Compiler), PVM_DI_SPECIAL_INS(DIV, Dividend, Left, Right, true, Remainder));
}

static void PVMEmitSetCCU(PVMCompiler *Compiler, TokenType Op, Operand Dest, Operand Left, Operand Right, UInt OperandSize)
{
#define SET(Size, ...)\
    do {\
        switch (Op) {\
        case TOKEN_EQUAL:           CodeChunkWrite(PVMCurrentChunk(Compiler), PVM_DI_CMP_INS(SEQ ## Size, Dest, Left, Right)); break;\
        case TOKEN_LESS_GREATER:    CodeChunkWrite(PVMCurrentChunk(Compiler), PVM_DI_CMP_INS(SNE ## Size, Dest, Left, Right)); break;\
        case TOKEN_LESS:            CodeChunkWrite(PVMCurrentChunk(Compiler), PVM_DI_CMP_INS(SLT ## Size, Dest, Left, Right)); break;\
        case TOKEN_GREATER:         CodeChunkWrite(PVMCurrentChunk(Compiler), PVM_DI_CMP_INS(SGT ## Size, Dest, Left, Right)); break;\
        case TOKEN_GREATER_EQUAL:   CodeChunkWrite(PVMCurrentChunk(Compiler), PVM_DI_CMP_INS(SLT ## Size, Dest, Right, Left)); break;\
        case TOKEN_LESS_EQUAL:      CodeChunkWrite(PVMCurrentChunk(Compiler), PVM_DI_CMP_INS(SGT ## Size, Dest, Right, Left)); break;\
        default: {\
            PASCAL_UNREACHABLE(__VA_ARGS__);\
        } break;\
        }\
    }while(0)


    switch (OperandSize)
    {
    case sizeof(PVMPtr): SET(P, "PVMEmitSetCCU: PVMPtr: %s is not valid", TokenTypeToStr(Op)); break;
    case sizeof(PVMWord): SET(W, "PVMEmitSetCCU: PVMWord: %s is not valid", TokenTypeToStr(Op)); break;
    case sizeof(PVMHalf): SET(H, "PVMEmitSetCCU: PVMHalf: %s is not valid", TokenTypeToStr(Op)); break;
    case sizeof(PVMByte): SET(B, "PVMEmitSetCCU: PVMByte: %s is not valid", TokenTypeToStr(Op)); break;
    }

#undef SET
}


static void PVMEmitExit(PVMCompiler *Compiler)
{
    CodeChunkWrite(PVMCurrentChunk(Compiler), PVM_SYS_INS(EXIT));
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


static CodeChunk *PVMCurrentChunk(PVMCompiler *Compiler)
{
    return Compiler->Chunk;
}




static void PVMBeginScope(PVMCompiler *Compiler)
{
    Compiler->RegisterList[Compiler->ScopeDepth + 1] = Compiler->RegisterList[Compiler->ScopeDepth];
    Compiler->ScopeDepth++;
}

static void PVMEndScope(PVMCompiler *Compiler)
{
    Compiler->ScopeDepth--;
}

