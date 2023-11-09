
#include <string.h> /* memcmp */
#include "PVM/PVM.h"
#include "PVM/CodeGen.h"


#define PVM_SCOPE_COUNT 128
#define PVM_MAX_LOCAL_COUNT PVM_REG_COUNT

typedef UInt Operand;


typedef struct LocalVar 
{
    Token Name;
    Operand Location;
} LocalVar;

typedef struct PVMCompiler
{
    const PascalAst *Ast;
    CodeChunk *Chunk;
    bool HasError;
    UInt CurrScopeDepth;

    U32 RegisterList[PVM_SCOPE_COUNT];
    LocalVar Locals[PVM_SCOPE_COUNT][PVM_MAX_LOCAL_COUNT];
    int LocalCount[PVM_SCOPE_COUNT];
} PVMCompiler;


PVMCompiler PVMCompilerInit(CodeChunk *Code, const PascalAst *Ast);
void PVMCompilerDeinit(PVMCompiler *Compiler);




static void PVMDeclareLocal(PVMCompiler *Compiler, Token Name, Operand Dest);
static Operand PVMGetLocationOf(const PVMCompiler *Compiler, Token Variable);



static void PVMCompileBlock(PVMCompiler *Compiler, const AstBlock *Block);
static void PVMCompileVar(PVMCompiler *Compiler, const AstVarBlock *Declarations);
static void PVMCompileFunction(PVMCompiler *Compiler, const AstFunctionBlock *Function);
static void PVMCompileStmts(PVMCompiler *Compiler, const AstStmtBlock *Block);
static void PVMCompileAssignStmt(PVMCompiler *Compiler, const AstAssignStmt *Assignment);
static void PVMCompileReturnStmt(PVMCompiler *Compiler, const AstReturnStmt *RetStmt);




static void PVMCompileExprInto(PVMCompiler *Compiler, Operand Dest, const AstExpr *Expr);
static void PVMCompileSimpleExprInto(PVMCompiler *Compiler, Operand Dest, const AstSimpleExpr *SimpleExpr);
static void PVMCompileTermInto(PVMCompiler *Compiler, Operand Dest, const AstTerm *Term);
static void PVMCompileFactorInto(PVMCompiler *Compiler, Operand Dest, const AstFactor *Factor);

static void PVMEmitMovI32(PVMCompiler *Compiler, Operand Dest, Operand Src);
static void PVMEmitLoadI32(PVMCompiler *Compiler, Operand Dest, I32 Integer);
static void PVMEmitAddI32(PVMCompiler *Compiler, Operand Dest, Operand Left, Operand Right);
static void PVMEmitSubI32(PVMCompiler *Compiler, Operand Dest, Operand Left, Operand Right);
static void PVMEmitMulI32(PVMCompiler *Compiler, Operand Dest, Operand Left, Operand Right);
static void PVMEmitDivI32(PVMCompiler *Compiler, Operand Dividend, Operand Remainder, Operand Left, Operand Right);
static void PVMEmitSetCC(
        PVMCompiler *Compiler, TokenType Op, 
        Operand Dest, Operand Left, Operand Right, 
        UInt OperandSize, bool IsSigned
);
static void PVMEmitReturn(PVMCompiler *Compiler);
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
        .CurrScopeDepth = 0,
        .LocalCount = { 0 },
    };
    return Compiler;
}

void PVMCompilerDeinit(PVMCompiler *Compiler)
{
    PVMEmitExit(Compiler);
}
















static void PVMDeclareLocal(PVMCompiler *Compiler, Token Name, Operand Dest)
{
    int CurrScope = Compiler->CurrScopeDepth;
    int LocalCount = Compiler->LocalCount[CurrScope];
    PASCAL_ASSERT(LocalCount < PVM_MAX_LOCAL_COUNT, "TODO: more locals\n");

    LocalVar *NewLocal = &Compiler->Locals[CurrScope][LocalCount];
    NewLocal->Name = Name;
    NewLocal->Location = Dest;
    Compiler->LocalCount[CurrScope]++;
}


static void PVMBindVariableToCurrentLocation(PVMCompiler *Compiler, Token Name)
{
    (void)Compiler, (void)Name;
}

static Operand PVMGetLocationOf(const PVMCompiler *Compiler, Token Variable)
{
    for (int i = Compiler->CurrScopeDepth; i >= 0; i--)
    {
        for (int k = 0; k < Compiler->LocalCount[i]; k++)
        {
            const LocalVar *Local = &Compiler->Locals[i][k];
            if (Variable.Len == Local->Name.Len 
            && memcmp(Variable.Str, Local->Name.Str, Variable.Len) == 0)
            {
                return Local->Location;
            }
        }
    }
    PASCAL_ASSERT(0, "TODO: static analysis to prevent undef variable");
    return 0;
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
        PVMCompileFunction(Compiler, (const AstFunctionBlock*)Block);
    } break;
    case AST_BLOCK_STATEMENTS:
    {
        PVMBeginScope(Compiler);
        PVMCompileStmts(Compiler, (const AstStmtBlock*)Block);
        PVMEndScope(Compiler);
    } break;

    case AST_BLOCK_INVALID:
        break;
    }

}




static void PVMCompileVar(PVMCompiler *Compiler, const AstVarBlock *Declarations)
{
    const AstVarList *I = &Declarations->Decl;

    /* TODO: global variables */
    do {
        Operand Register = PVMAllocateRegister(Compiler);
        PVMDeclareLocal(Compiler, I->Identifier, Register);
        I = I->Next;
    } while (NULL != I);
}


static void PVMCompileFunction(PVMCompiler *Compiler, const AstFunctionBlock *Function)
{
    //PVMBindVariableToCurrentLocation(Compiler, Function->Identifier);
    PVMCompileBlock(Compiler, Function->Block);
}

static void PVMCompileStmts(PVMCompiler *Compiler, const AstStmtBlock *Block)
{
    const AstStmtList *I = Block->Statements;
    while (NULL != I)
    {
        switch (I->Statement->Type)
        {
        case AST_STMT_ASSIGNMENT:   PVMCompileAssignStmt(Compiler, (const AstAssignStmt*)I->Statement); break;
        case AST_STMT_RETURN:       PVMCompileReturnStmt(Compiler, (const AstReturnStmt*)I->Statement); break;

        case AST_STMT_INVALID:
        {
            PASCAL_UNREACHABLE("PVMCompileStmts: AST_STMT_INVALID encountered\n");
        } break;
        }

        I = I->Next;
    }
}


static void PVMCompileAssignStmt(PVMCompiler *Compiler, const AstAssignStmt *Assignment)
{
    Operand LValue = PVMGetLocationOf(Compiler, Assignment->Variable);
    PVMCompileExprInto(Compiler, LValue, &Assignment->Expr);
}


static void PVMCompileReturnStmt(PVMCompiler *Compiler, const AstReturnStmt *RetStmt)
{
    if (NULL != RetStmt->Expr)
    {
        PVMCompileExprInto(Compiler, PVM_REG_RET, RetStmt->Expr);
    }
    PVMEmitReturn(Compiler);
}









/*
 *================================================================
 *                          EXPRESSIONS
 *================================================================
 */











static void PVMCompileExprInto(PVMCompiler *Compiler, Operand Dest, const AstExpr *Expr)
{
    PVMCompileSimpleExprInto(Compiler, Dest, &Expr->Left);
    const AstOpSimpleExpr *RightSimpleExpr = Expr->Right;
    if (NULL != RightSimpleExpr)
    {
        Operand Right = PVMAllocateRegister(Compiler);

        do {
            PVMCompileSimpleExprInto(Compiler, Right, &RightSimpleExpr->SimpleExpr);
            PVMEmitSetCC(Compiler, RightSimpleExpr->Op, Dest, Dest, Right, sizeof(U32), true);
            RightSimpleExpr = RightSimpleExpr->Next;
        } while (NULL != RightSimpleExpr);

        PVMFreeRegister(Compiler, Right);
    }
}

static void PVMCompileSimpleExprInto(PVMCompiler *Compiler, Operand Dest, const AstSimpleExpr *SimpleExpr)
{
    PVMCompileTermInto(Compiler, Dest, &SimpleExpr->Left);

    const AstOpTerm *RightTerm = SimpleExpr->Right;
    if (NULL != RightTerm)
    {
        Operand Right = PVMAllocateRegister(Compiler);
        do {
            PVMCompileTermInto(Compiler, Right, &RightTerm->Term);

            switch (RightTerm->Op)
            {
            case TOKEN_PLUS:
            {
                PVMEmitAddI32(Compiler, Dest, Dest, Right);
            } break;
            case TOKEN_MINUS:
            {
                PVMEmitSubI32(Compiler, Dest, Dest, Right);
            } break;

            case TOKEN_NOT:
            default: 
            {
                PASCAL_UNREACHABLE("CompileSimpleExpr: %s is an invalid op\n", 
                        TokenTypeToStr(RightTerm->Op)
                );
            } break;
            }

            RightTerm = RightTerm->Next;
        } while (NULL != RightTerm);
        PVMFreeRegister(Compiler, Right);
    }
}


static void PVMCompileTermInto(PVMCompiler *Compiler, Operand Dest, const AstTerm *Term)
{
    PVMCompileFactorInto(Compiler, Dest, &Term->Left);

    const AstOpFactor *RightFactor = Term->Right;
    if (NULL != RightFactor)
    {
        Operand Right = PVMAllocateRegister(Compiler);
        do {
            PVMCompileFactorInto(Compiler, Right, &RightFactor->Factor);

            switch (RightFactor->Op)
            {
            case TOKEN_STAR:
            {
                PVMEmitMulI32(Compiler, Dest, Dest, Right);
            } break;

            case TOKEN_SLASH:
            case TOKEN_DIV:
            {
                Operand Dummy = PVMAllocateRegister(Compiler);
                PVMEmitDivI32(Compiler, Dest, Dummy, Dest, Right);
                PVMFreeRegister(Compiler, Dummy);
            } break;

            case TOKEN_MOD:
            {
                Operand Dummy = PVMAllocateRegister(Compiler);
                PVMEmitDivI32(Compiler, Dummy, Dest, Dest, Right);
                PVMFreeRegister(Compiler, Dummy);
            } break;

            case TOKEN_AND:
            default:
            {
                PASCAL_UNREACHABLE("CompileTerm: %s is an invalid op\n", 
                        TokenTypeToStr(RightFactor->Op)
                );
            } break;
            }

            RightFactor = RightFactor->Next;
        } while (NULL != RightFactor);
        PVMFreeRegister(Compiler, Right);
    }
}


static void PVMCompileFactorInto(PVMCompiler *Compiler, Operand Dest, const AstFactor *Factor)
{
    switch (Factor->Type)
    {
    case FACTOR_INTEGER:
    {
        PVMEmitLoadI32(Compiler, Dest, Factor->As.Integer);
    } break;


    case FACTOR_GROUP_EXPR:
    {
        PVMCompileExprInto(Compiler, Dest, Factor->As.Expression);
    } break;


    default: 
    {
        PASCAL_UNREACHABLE("CompileTerm: Invalid factor: %d\n", Factor->Type);
    } break;
    }
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

static void PVMEmitSetCC(PVMCompiler *Compiler, TokenType Op, Operand Dest, Operand Left, Operand Right, UInt OperandSize, bool IsSigned)
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

#define SIGNEDSET(Size, ...)\
    do {\
        switch (Op) {\
        case TOKEN_EQUAL:           CodeChunkWrite(PVMCurrentChunk(Compiler), PVM_DI_CMP_INS(SEQ ## Size, Dest, Left, Right)); break;\
        case TOKEN_LESS_GREATER:    CodeChunkWrite(PVMCurrentChunk(Compiler), PVM_DI_CMP_INS(SNE ## Size, Dest, Left, Right)); break;\
        case TOKEN_LESS:            CodeChunkWrite(PVMCurrentChunk(Compiler), PVM_DI_CMP_INS(SSLT ## Size, Dest, Left, Right)); break;\
        case TOKEN_GREATER:         CodeChunkWrite(PVMCurrentChunk(Compiler), PVM_DI_CMP_INS(SSGT ## Size, Dest, Left, Right)); break;\
        case TOKEN_GREATER_EQUAL:   CodeChunkWrite(PVMCurrentChunk(Compiler), PVM_DI_CMP_INS(SSLT ## Size, Dest, Right, Left)); break;\
        case TOKEN_LESS_EQUAL:      CodeChunkWrite(PVMCurrentChunk(Compiler), PVM_DI_CMP_INS(SSGT ## Size, Dest, Right, Left)); break;\
        default: {\
            PASCAL_UNREACHABLE(__VA_ARGS__);\
        } break;\
        }\
    }while(0)



    if (IsSigned)
    {
        switch (OperandSize)
        {
        case sizeof(PVMPtr): SIGNEDSET(P, "(Signed) PVMEmitSetCC: PVMPtr: %s is not valid", TokenTypeToStr(Op)); break;
        case sizeof(PVMWord): SIGNEDSET(W, "(Signed) PVMEmitSetCC: PVMWord: %s is not valid", TokenTypeToStr(Op)); break;
        case sizeof(PVMHalf): SIGNEDSET(H, "(Signed) PVMEmitSetCC: PVMHalf: %s is not valid", TokenTypeToStr(Op)); break;
        case sizeof(PVMByte): SIGNEDSET(B, "(Signed) PVMEmitSetCC: PVMByte: %s is not valid", TokenTypeToStr(Op)); break;
        }
    }
    else
    {
        switch (OperandSize)
        {
        case sizeof(PVMPtr): SET(P, "PVMEmitSetCC: PVMPtr: %s is not valid", TokenTypeToStr(Op)); break;
        case sizeof(PVMWord): SET(W, "PVMEmitSetCC: PVMWord: %s is not valid", TokenTypeToStr(Op)); break;
        case sizeof(PVMHalf): SET(H, "PVMEmitSetCC: PVMHalf: %s is not valid", TokenTypeToStr(Op)); break;
        case sizeof(PVMByte): SET(B, "PVMEmitSetCC: PVMByte: %s is not valid", TokenTypeToStr(Op)); break;
        }
    }

#undef SET
#undef SIGNEDSET
}


static void PVMEmitReturn(PVMCompiler *Compiler)
{
    CodeChunkWrite(PVMCurrentChunk(Compiler), PVM_RET_INS);
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
        if ((Compiler->RegisterList[Compiler->CurrScopeDepth] >> i & 1u) == 0)
        {
            /* mark reg as allocated */
            Compiler->RegisterList[Compiler->CurrScopeDepth] |= 1u << i;
            return i;
        }
    }
    PASCAL_UNREACHABLE("TODO: Register spilling");
    return 0;
}

static void PVMFreeRegister(PVMCompiler *Compiler, Operand Register)
{
    Compiler->RegisterList[Compiler->CurrScopeDepth] &= ~(1u << Register);
}


static CodeChunk *PVMCurrentChunk(PVMCompiler *Compiler)
{
    return Compiler->Chunk;
}




static void PVMBeginScope(PVMCompiler *Compiler)
{
    Compiler->RegisterList[Compiler->CurrScopeDepth + 1] = Compiler->RegisterList[Compiler->CurrScopeDepth];
    Compiler->CurrScopeDepth++;
}

static void PVMEndScope(PVMCompiler *Compiler)
{
    Compiler->CurrScopeDepth--;
}

