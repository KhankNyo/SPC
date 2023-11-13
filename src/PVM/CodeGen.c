
#include <string.h> /* memcmp */

#include "Parser.h"
#include "PVM/PVM.h"
#include "PVM/CodeGen.h"


#define PVM_SCOPE_COUNT 128
#define PVM_MAX_LOCAL_COUNT PVM_REG_COUNT

typedef struct Operand 
{
    ParserType IntegralType;
    union {
        UInt Size, RegID;
    };
    bool InRegister;
} Operand;


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



static Operand sReturnRegister = {
    .IntegralType = TYPE_U64,
    .InRegister = true,
    .RegID = PVM_REG_RET,
};




PVMCompiler PVMCompilerInit(CodeChunk *Code, const PascalAst *Ast);
void PVMCompilerDeinit(PVMCompiler *Compiler);




static void PVMDeclareLocal(PVMCompiler *Compiler, Token Name, Operand Dest);
static Operand *PVMGetLocationOf(PVMCompiler *Compiler, const Token *Variable);



static void PVMCompileBlock(PVMCompiler *Compiler, const AstBlock *Block);
static void PVMCompileVar(PVMCompiler *Compiler, const AstVarBlock *Declarations);
static void PVMCompileFunction(PVMCompiler *Compiler, const AstFunctionBlock *Function);
static void PVMCompileStmts(PVMCompiler *Compiler, const AstStmtBlock *Block);
static void PVMCompileAssignStmt(PVMCompiler *Compiler, const AstAssignStmt *Assignment);
static void PVMCompileReturnStmt(PVMCompiler *Compiler, const AstReturnStmt *RetStmt);




static void PVMCompileExprInto(PVMCompiler *Compiler, Operand *Dest, const AstExpr *Expr);
static void PVMCompileSimpleExprInto(PVMCompiler *Compiler, Operand *Dest, const AstSimpleExpr *SimpleExpr);
static void PVMCompileTermInto(PVMCompiler *Compiler, Operand *Dest, const AstTerm *Term);
static void PVMCompileFactorInto(PVMCompiler *Compiler, Operand *Dest, const AstFactor *Factor);

static void PVMEmitMov(PVMCompiler *Compiler, Operand *Dest, Operand *Src);
static void PVMEmitLoadI32(PVMCompiler *Compiler, Operand *Dest, I32 Integer);
static void PVMEmitAdd(PVMCompiler *Compiler, Operand *Dest, Operand *Left, Operand *Right);
static void PVMEmitSub(PVMCompiler *Compiler, Operand *Dest, Operand *Left, Operand *Right);
static void PVMEmitMul(PVMCompiler *Compiler, Operand *Dest, Operand *Left, Operand *Right);
static void PVMEmitDiv(PVMCompiler *Compiler, Operand *Dividend, Operand *Remainder, Operand *Left, Operand *Right);
static void PVMEmitSetCC(
        PVMCompiler *Compiler, TokenType Op, 
        Operand *Dest, Operand *Left, Operand *Right, 
        UInt OperandSize, bool IsSigned
);
static void PVMEmitReturn(PVMCompiler *Compiler);
static void PVMEmitExit(PVMCompiler *Compiler);

static Operand PVMAllocateRegister(PVMCompiler *Compiler, ParserType Type);
static void PVMFreeRegister(PVMCompiler *Compiler, Operand *Register);
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
    PASCAL_ASSERT(0, "TODO: PVMBindVariableToCurrentLocation()");
}

static Operand *PVMGetLocationOf(PVMCompiler *Compiler, const Token *Variable)
{
    for (int i = Compiler->CurrScopeDepth; i >= 0; i--)
    {
        for (int k = 0; k < Compiler->LocalCount[i]; k++)
        {
            /* TODO: not compare strings because parser already has info for variables, 
             * cmp hash and index or smth */
            LocalVar *Local = &Compiler->Locals[i][k];
            if (Variable->Len == Local->Name.Len 
            && TokenEqualNoCase(Variable->Str, Local->Name.Str, Variable->Len))
            {
                return &Local->Location;
            }
        }
    }

    PASCAL_UNREACHABLE("TODO: static analysis to prevent undef variable");
    return NULL;
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

    /* TODO: loop instead */
    if (NULL != Block->Next)
    {
        PVMCompileBlock(Compiler, Block->Next);
    }
}




static void PVMCompileVar(PVMCompiler *Compiler, const AstVarBlock *Declarations)
{
    const AstVarList *I = &Declarations->Decl;

    /* TODO: global variables */
    do {
        Operand Register = PVMAllocateRegister(Compiler, I->Type);
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
    Operand *LValue = PVMGetLocationOf(Compiler, &Assignment->Variable);
    PVMCompileExprInto(Compiler, LValue, &Assignment->Expr);
}


static void PVMCompileReturnStmt(PVMCompiler *Compiler, const AstReturnStmt *RetStmt)
{
    if (NULL != RetStmt->Expr)
    {
        PVMCompileExprInto(Compiler, &sReturnRegister, RetStmt->Expr);
    }
    PVMEmitReturn(Compiler);
}









/*
 *================================================================
 *                          EXPRESSIONS
 *================================================================
 */











static void PVMCompileExprInto(PVMCompiler *Compiler, Operand *Dest, const AstExpr *Expr)
{
    const AstOpSimpleExpr *RightSimpleExpr = Expr->Right;
    if (NULL == RightSimpleExpr)
    {
        PVMCompileSimpleExprInto(Compiler, Dest, &Expr->Left);
        return;
    }

    Operand Left = PVMAllocateRegister(Compiler, Expr->Type);
    PVMCompileSimpleExprInto(Compiler, &Left, &Expr->Left);

    Operand Right = PVMAllocateRegister(Compiler, Expr->Type);
    do {
        PVMCompileSimpleExprInto(Compiler, &Right, &RightSimpleExpr->SimpleExpr);
        PVMEmitSetCC(Compiler, RightSimpleExpr->Op, 
                &Left, &Left, &Right, 
                sizeof(U32), true
        );
        RightSimpleExpr = RightSimpleExpr->Next;
    } while (NULL != RightSimpleExpr);
    PVMFreeRegister(Compiler, &Right);

    PVMEmitMov(Compiler, Dest, &Left);
    PVMFreeRegister(Compiler, &Left);
}

static void PVMCompileSimpleExprInto(PVMCompiler *Compiler, Operand *Dest, const AstSimpleExpr *SimpleExpr)
{
    const AstOpTerm *RightTerm = SimpleExpr->Right;
    if (NULL == RightTerm)
    {
        PVMCompileTermInto(Compiler, Dest, &SimpleExpr->Left);
        return;
    }

    Operand Left = PVMAllocateRegister(Compiler, SimpleExpr->Type);
    PVMCompileTermInto(Compiler, &Left, &SimpleExpr->Left);

    Operand Right = PVMAllocateRegister(Compiler, SimpleExpr->Type);
    do {
        PVMCompileTermInto(Compiler, &Right, &RightTerm->Term);

        switch (RightTerm->Op)
        {
        case TOKEN_PLUS:
        {
            PVMEmitAdd(Compiler, &Left, &Left, &Right);
        } break;
        case TOKEN_MINUS:
        {
            PVMEmitSub(Compiler, &Left, &Left, &Right);
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
    PVMFreeRegister(Compiler, &Right);

    PVMEmitMov(Compiler, Dest, &Left);
    PVMFreeRegister(Compiler, &Left);
}


static void PVMCompileTermInto(PVMCompiler *Compiler, Operand *Dest, const AstTerm *Term)
{
    const AstOpFactor *RightFactor = Term->Right;
    if (NULL == RightFactor)
    {
        PVMCompileFactorInto(Compiler, Dest, &Term->Left);
        return;
    }

    Operand Left = PVMAllocateRegister(Compiler, Term->Type);
    PVMCompileFactorInto(Compiler, &Left, &Term->Left);

    Operand Right = PVMAllocateRegister(Compiler, Term->Type);
    do {
        PVMCompileFactorInto(Compiler, &Right, &RightFactor->Factor);

        switch (RightFactor->Op)
        {
        case TOKEN_STAR:
        {
            PVMEmitMul(Compiler, &Left, &Left, &Right);
        } break;

        case TOKEN_SLASH:
        case TOKEN_DIV:
        {
            Operand Dummy = PVMAllocateRegister(Compiler, RightFactor->Type);
            PVMEmitDiv(Compiler, &Left, &Dummy, &Left, &Right);
            PVMFreeRegister(Compiler, &Dummy);
        } break;

        case TOKEN_MOD:
        {
            Operand Dummy = PVMAllocateRegister(Compiler, RightFactor->Type);
            PVMEmitDiv(Compiler, &Dummy, &Left, &Left, &Right);
            PVMFreeRegister(Compiler, &Dummy);
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
    PVMFreeRegister(Compiler, &Right);

    PVMEmitMov(Compiler, Dest, &Left);
    PVMFreeRegister(Compiler, &Left);
}


static void PVMCompileFactorInto(PVMCompiler *Compiler, Operand *Dest, const AstFactor *Factor)
{
    switch (Factor->FactorType)
    {
    case FACTOR_INTEGER:
    {
        PVMEmitLoadI32(Compiler, Dest, Factor->As.Integer);
    } break;
    case FACTOR_REAL:
    {
    } break;
    case FACTOR_VARIABLE:
    {
        Operand *Variable = PVMGetLocationOf(Compiler, &Factor->As.Variable.Name);
        PVMEmitMov(Compiler, Dest, Variable);
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












static void PVMEmitMov(PVMCompiler *Compiler, Operand *Dest, Operand *Src)
{
    if (Dest->InRegister && Src->InRegister)
    {
        ChunkWriteCode(PVMCurrentChunk(Compiler), PVM_IDAT_TRANSFER_INS(MOV, Dest->RegID, Src->RegID));
    }
    else 
    {
        PASCAL_UNREACHABLE("TODO Mov mem/reg, reg/mem");
    }
}

static void PVMEmitLoadI32(PVMCompiler *Compiler, Operand *Dest, I32 Integer)
{
    if (Dest->InRegister)
    {
        Dest->IntegralType = TYPE_I32;
        ChunkWriteCode(PVMCurrentChunk(Compiler), PVM_IRD_ARITH_INS(LDI, Dest->RegID, Integer));
    }
    else 
    {
        PASCAL_UNREACHABLE("TODO Load mem, imm");
    }
}


static void PVMEmitAdd(PVMCompiler *Compiler, Operand *Dest, Operand *Left, Operand *Right)
{
    if (Dest->InRegister && Left->InRegister && Right->InRegister)
    {
        ChunkWriteCode(PVMCurrentChunk(Compiler), PVM_IDAT_ARITH_INS(ADD, Dest->RegID, Left->RegID, Right->RegID, 0));
    }
    else 
    {
        PASCAL_UNREACHABLE("TODO Add dest, left, right");
    }
}


static void PVMEmitSub(PVMCompiler *Compiler, Operand *Dest, Operand *Left, Operand *Right)
{
    if (Dest->InRegister && Left->InRegister && Right->InRegister)
    {
        ChunkWriteCode(PVMCurrentChunk(Compiler), PVM_IDAT_ARITH_INS(SUB, Dest->RegID, Left->RegID, Right->RegID, 0));
    }
    else 
    {
        PASCAL_UNREACHABLE("TODO Sub dest, left, right");
    }
}

static void PVMEmitMul(PVMCompiler *Compiler, Operand *Dest, Operand *Left, Operand *Right)
{
    if (Dest->InRegister && Left->InRegister && Right->InRegister)
    {
        ChunkWriteCode(PVMCurrentChunk(Compiler), PVM_IDAT_SPECIAL_INS(MUL, Dest->RegID, Left->RegID, Right->RegID, true, 0));
    }
    else 
    {
        PASCAL_UNREACHABLE("TODO Mul dest, left, right");
    }
}

static void PVMEmitDiv(PVMCompiler *Compiler, Operand *Dividend, Operand *Remainder, Operand *Left, Operand *Right)
{
    if (Dividend->InRegister && Remainder->InRegister && Left->InRegister && Right->InRegister)
    {
        ChunkWriteCode(PVMCurrentChunk(Compiler), PVM_IDAT_SPECIAL_INS(DIV, Dividend->RegID, Left->RegID, Right->RegID, true, Remainder->RegID));
    }
    else 
    {
        PASCAL_UNREACHABLE("TODO Div dest, left, right, rem");
    }
}

static void PVMEmitSetCC(PVMCompiler *Compiler, TokenType Op, Operand *Dest, Operand *Left, Operand *Right, UInt OperandSize, bool IsSigned)
{
    PASCAL_ASSERT(Dest->InRegister && Left->InRegister && Right->InRegister, "TODO: SetCC Dest, Left, Right");
#define SET(Size, ...)\
    do {\
        switch (Op) {\
        case TOKEN_EQUAL:           ChunkWriteCode(PVMCurrentChunk(Compiler), PVM_IDAT_CMP_INS(SEQ ## Size, Dest->RegID, Left->RegID, Right->RegID)); break;\
        case TOKEN_LESS_GREATER:    ChunkWriteCode(PVMCurrentChunk(Compiler), PVM_IDAT_CMP_INS(SNE ## Size, Dest->RegID, Left->RegID, Right->RegID)); break;\
        case TOKEN_LESS:            ChunkWriteCode(PVMCurrentChunk(Compiler), PVM_IDAT_CMP_INS(SLT ## Size, Dest->RegID, Left->RegID, Right->RegID)); break;\
        case TOKEN_GREATER:         ChunkWriteCode(PVMCurrentChunk(Compiler), PVM_IDAT_CMP_INS(SGT ## Size, Dest->RegID, Left->RegID, Right->RegID)); break;\
        case TOKEN_GREATER_EQUAL:   ChunkWriteCode(PVMCurrentChunk(Compiler), PVM_IDAT_CMP_INS(SLT ## Size, Dest->RegID, Right->RegID, Left->RegID)); break;\
        case TOKEN_LESS_EQUAL:      ChunkWriteCode(PVMCurrentChunk(Compiler), PVM_IDAT_CMP_INS(SGT ## Size, Dest->RegID, Right->RegID, Left->RegID)); break;\
        default: {\
            PASCAL_UNREACHABLE(__VA_ARGS__);\
        } break;\
        }\
    }while(0)

#define SIGNEDSET(Size, ...)\
    do {\
        switch (Op) {\
        case TOKEN_EQUAL:           ChunkWriteCode(PVMCurrentChunk(Compiler), PVM_IDAT_CMP_INS(SEQ ## Size, Dest->RegID, Left->RegID, Right->RegID)); break;\
        case TOKEN_LESS_GREATER:    ChunkWriteCode(PVMCurrentChunk(Compiler), PVM_IDAT_CMP_INS(SNE ## Size, Dest->RegID, Left->RegID, Right->RegID)); break;\
        case TOKEN_LESS:            ChunkWriteCode(PVMCurrentChunk(Compiler), PVM_IDAT_CMP_INS(SSLT ## Size, Dest->RegID, Left->RegID, Right->RegID)); break;\
        case TOKEN_GREATER:         ChunkWriteCode(PVMCurrentChunk(Compiler), PVM_IDAT_CMP_INS(SSGT ## Size, Dest->RegID, Left->RegID, Right->RegID)); break;\
        case TOKEN_GREATER_EQUAL:   ChunkWriteCode(PVMCurrentChunk(Compiler), PVM_IDAT_CMP_INS(SSLT ## Size, Dest->RegID, Right->RegID, Left->RegID)); break;\
        case TOKEN_LESS_EQUAL:      ChunkWriteCode(PVMCurrentChunk(Compiler), PVM_IDAT_CMP_INS(SSGT ## Size, Dest->RegID, Right->RegID, Left->RegID)); break;\
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
    ChunkWriteCode(PVMCurrentChunk(Compiler), PVM_RET_INS);
}


static void PVMEmitExit(PVMCompiler *Compiler)
{
    ChunkWriteCode(PVMCurrentChunk(Compiler), PVM_SYS_INS(EXIT));
}





static Operand PVMAllocateRegister(PVMCompiler *Compiler, ParserType Type)
{
    PASCAL_ASSERT(Type != TYPE_INVALID, "Compiler received invalid type");
    for (UInt i = 0; i < PVM_REG_COUNT; i++)
    {
        /* found free reg */
        if ((Compiler->RegisterList[Compiler->CurrScopeDepth] >> i & 1u) == 0)
        {
            /* mark reg as allocated */
            Compiler->RegisterList[Compiler->CurrScopeDepth] |= 1u << i;
            return (Operand) {
                .RegID = i,
                .InRegister = true,
                .IntegralType = Type,
            };
        }
    }
    PASCAL_UNREACHABLE("TODO: Register spilling");
    return (Operand) { 0 };
}

static void PVMFreeRegister(PVMCompiler *Compiler, Operand *Register)
{
    Compiler->RegisterList[Compiler->CurrScopeDepth] &= ~(1u << Register->RegID);
}


static CodeChunk *PVMCurrentChunk(PVMCompiler *Compiler)
{
    return Compiler->Chunk;
}




static void PVMBeginScope(PVMCompiler *Compiler)
{
    Compiler->RegisterList[Compiler->CurrScopeDepth + 1] = 
        Compiler->RegisterList[Compiler->CurrScopeDepth];
    Compiler->CurrScopeDepth++;
}

static void PVMEndScope(PVMCompiler *Compiler)
{
    Compiler->CurrScopeDepth--;
}

