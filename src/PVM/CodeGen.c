
#include <string.h> /* memcmp */

#include "Parser.h"
#include "PVM/PVM.h"
#include "PVM/CodeGen.h"


#define PVM_SCOPE_COUNT 128
#define PVM_MAX_LOCAL_COUNT PVM_REG_COUNT
#define BRANCH_ALWAYS 26
#define BRANCH_CONDITIONAL 21

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
    U32 ID;
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




static void PVMDeclareLocal(PVMCompiler *Compiler, U32 ID, Operand Dest);
static Operand *PVMGetLocationOf(PVMCompiler *Compiler, U32 LocationID);



static void PVMCompileBlock(PVMCompiler *Compiler, const AstBlock *Block);
static void PVMCompileVarBlock(PVMCompiler *Compiler, const AstVarBlock *Declarations);
static void PVMCompileFunctionBlock(PVMCompiler *Compiler, const AstFunctionBlock *Function);
static void PVMCompileStmtBlock(PVMCompiler *Compiler, const AstStmtBlock *Block);

static void PVMCompileStmt(PVMCompiler *Compiler, const AstStmt *Statement);
static void PVMCompileIfStmt(PVMCompiler *Compiler, const AstIfStmt *IfStmt);
static void PVMCompileBeginEndStmt(PVMCompiler *Compiler, const AstBeginEndStmt *BeginEnd);
static void PVMCompileForStmt(PVMCompiler *Compiler, const AstForStmt *ForStmt);
static void PVMCompileWhileStmt(PVMCompiler *Compiler, const AstWhileStmt *WhileStmt);
static Operand *PVMCompileAssignStmt(PVMCompiler *Compiler, const AstAssignStmt *Assignment);
static void PVMCompileReturnStmt(PVMCompiler *Compiler, const AstReturnStmt *RetStmt);



static void PVMCompileExprInto(PVMCompiler *Compiler, Operand *Dest, const AstExpr *Expr);
static void PVMCompileSimpleExprInto(PVMCompiler *Compiler, Operand *Dest, const AstSimpleExpr *SimpleExpr);
static void PVMCompileTermInto(PVMCompiler *Compiler, Operand *Dest, const AstTerm *Term);
static void PVMCompileFactorInto(PVMCompiler *Compiler, Operand *Dest, const AstFactor *Factor);



static U64 PVMEmitBranchIfFalse(PVMCompiler *Compiler, Operand *Condition);
static void PVMPatchBranch(PVMCompiler *Compiler, U64 StreamOffset, UInt ImmSize); 
static U64 PVMEmitBranch(PVMCompiler *Compiler, U64 Location);
static void PVMEmitMov(PVMCompiler *Compiler, Operand *Dest, Operand *Src);
static void PVMEmitLoad(PVMCompiler *Compiler, Operand *Dest, U64 Integer, ParserType IntegerType);
static void PVMEmitAddImm(PVMCompiler *Compiler, Operand *Dest, I16 Imm);
static void PVMEmitAdd(PVMCompiler *Compiler, Operand *Dest, Operand *Left, Operand *Right);
static void PVMEmitSub(PVMCompiler *Compiler, Operand *Dest, Operand *Left, Operand *Right);
static void PVMEmitMul(PVMCompiler *Compiler, Operand *Dest, Operand *Left, Operand *Right);
static void PVMEmitDiv(PVMCompiler *Compiler, Operand *Dividend, Operand *Remainder, Operand *Left, Operand *Right);
static void PVMEmitSetCC(
        PVMCompiler *Compiler, TokenType Op, 
        Operand *Dest, Operand *Left, Operand *Right
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
















static void PVMDeclareLocal(PVMCompiler *Compiler, U32 ID, Operand Dest)
{
    int CurrScope = Compiler->CurrScopeDepth;
    int LocalCount = Compiler->LocalCount[CurrScope];
    PASCAL_ASSERT(LocalCount < PVM_MAX_LOCAL_COUNT, "TODO: more locals\n");

    LocalVar *NewLocal = &Compiler->Locals[CurrScope][LocalCount];
    NewLocal->ID = ID;
    NewLocal->Location = Dest;
    Compiler->LocalCount[CurrScope]++;
}


static void PVMBindVariableToCurrentLocation(PVMCompiler *Compiler, Token Name)
{
    (void)Compiler, (void)Name;
    PASCAL_ASSERT(0, "TODO: PVMBindVariableToCurrentLocation()");
}

static Operand *PVMGetLocationOf(PVMCompiler *Compiler, U32 LocationID)
{
    for (int i = Compiler->CurrScopeDepth; i >= 0; i--)
    {
        for (int k = 0; k < Compiler->LocalCount[i]; k++)
        {
            /* TODO: not compare strings because parser already has info for variables, 
             * cmp hash and index or smth */
            LocalVar *Local = &Compiler->Locals[i][k];
            if (LocationID == Local->ID)
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
        PVMCompileVarBlock(Compiler, (const AstVarBlock*)Block);
    } break;
    case AST_BLOCK_FUNCTION:
    {
        PVMCompileFunctionBlock(Compiler, (const AstFunctionBlock*)Block);
    } break;
    case AST_BLOCK_STATEMENTS:
    {
        PVMBeginScope(Compiler);
        PVMCompileStmtBlock(Compiler, (const AstStmtBlock*)Block);
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




static void PVMCompileVarBlock(PVMCompiler *Compiler, const AstVarBlock *Declarations)
{
    const AstVarList *I = &Declarations->Decl;

    /* TODO: global variables */
    do {
        Operand Register = PVMAllocateRegister(Compiler, I->Type);
        PVMDeclareLocal(Compiler, I->ID, Register);
        I = I->Next;
    } while (NULL != I);
}


static void PVMCompileFunctionBlock(PVMCompiler *Compiler, const AstFunctionBlock *Function)
{
    //PVMBindVariableToCurrentLocation(Compiler, Function->Identifier);
    const AstVarList *I = Function->Params;
    while (NULL != I)
    {
        Operand Register = PVMAllocateRegister(Compiler, I->Type);
        PVMDeclareLocal(Compiler, I->ID, Register);
        I = I->Next;
    }
    PVMCompileBlock(Compiler, Function->Block);
}

static void PVMCompileStmtBlock(PVMCompiler *Compiler, const AstStmtBlock *Block)
{
    const AstStmtList *I = Block->Statements;
    while (NULL != I)
    {
        PVMCompileStmt(Compiler, I->Statement);
        I = I->Next;
    }
}











static void PVMCompileStmt(PVMCompiler *Compiler, const AstStmt *Statement)
{
    switch (Statement->Type)
    {
    case AST_STMT_BEGINEND:     PVMCompileBeginEndStmt(Compiler, (const AstBeginEndStmt*)Statement); break;
    case AST_STMT_IF:           PVMCompileIfStmt(Compiler, (const AstIfStmt*)Statement); break;
    case AST_STMT_FOR:          PVMCompileForStmt(Compiler, (const AstForStmt*)Statement); break;
    case AST_STMT_WHILE:        PVMCompileWhileStmt(Compiler, (const AstWhileStmt*)Statement); break;
    case AST_STMT_ASSIGNMENT:   PVMCompileAssignStmt(Compiler, (const AstAssignStmt*)Statement); break;
    case AST_STMT_RETURN:       PVMCompileReturnStmt(Compiler, (const AstReturnStmt*)Statement); break;


    case AST_STMT_INVALID:
    {
        PASCAL_UNREACHABLE("PVMCompileStmts: AST_STMT_INVALID encountered\n");
    } break;
    }
}



static void PVMCompileBeginEndStmt(PVMCompiler *Compiler, const AstBeginEndStmt *BeginEnd)
{
    const AstStmtList *I = BeginEnd->Statements;
    while (NULL != I)
    {
        PVMCompileStmt(Compiler, I->Statement);
        I = I->Next;
    }
}

static void PVMCompileIfStmt(PVMCompiler *Compiler, const AstIfStmt *IfStmt)
{
    Operand Tmp = PVMAllocateRegister(Compiler, IfStmt->Condition.Type);
    PVMCompileExprInto(Compiler, &Tmp, &IfStmt->Condition);

    /*
     * Start:
     *   SETCC Tmp
     *   BEZ Tmp, SkipIf
     *      Stmt...
     * #if (IfStmt->ElseCase) {
         *   BAL ExitIf
         * SkipIf:
         *   SETCC Tmp
         *   BEZ Tmp, SkipElse
         *      Stmt...
         *   BAL ExitElse
         * SkipElse:
     * } else {
         * SkipIf:              ; just a label declaration
     * }
     */

    U64 SkipIf = PVMEmitBranchIfFalse(Compiler, &Tmp);
        PVMCompileStmt(Compiler, IfStmt->IfCase);
    if (IfStmt->ElseCase)
    {
        U64 ExitIf = PVMEmitBranch(Compiler, -1);
        PVMPatchBranch(Compiler, SkipIf, BRANCH_CONDITIONAL);
        PVMCompileStmt(Compiler, IfStmt->ElseCase);
        PVMPatchBranch(Compiler, ExitIf, BRANCH_ALWAYS);
    }
    else 
    {
        PVMPatchBranch(Compiler, SkipIf, BRANCH_CONDITIONAL);
    }
}


static void PVMCompileForStmt(PVMCompiler *Compiler, const AstForStmt *ForStmt)
{
    /* 
     *   I = Assign...
     * CondCheck:
     *   SETCC Stop, CondExpr...
     *   BEZ Stop, ExitFor 
     *      Stmt...
     *   ADDI I, +-1
     *   BAL CondCheck
     * ExitFor:
     */

    /* assignment */
    Operand *I = PVMCompileAssignStmt(Compiler, ForStmt->InitStmt);

    /* TODO: temporary register */
    U64 Location = PVMCurrentChunk(Compiler)->Count;
    Operand Stop = PVMAllocateRegister(Compiler, ForStmt->StopExpr.Type);
    PVMCompileExprInto(Compiler, &Stop, &ForStmt->StopExpr);

    /* conditional */
    PVMEmitSetCC(Compiler, ForStmt->Comparison, &Stop, I, &Stop);
    U64 ExitFor = PVMEmitBranchIfFalse(Compiler, &Stop);
    PVMFreeRegister(Compiler, &Stop);

    /* body */
    PVMCompileStmt(Compiler, ForStmt->Stmt);

    /* increment */
    PVMEmitAddImm(Compiler, I, ForStmt->Imm);
    PVMEmitBranch(Compiler, Location);
    PVMPatchBranch(Compiler, ExitFor, BRANCH_CONDITIONAL);
}


static void PVMCompileWhileStmt(PVMCompiler *Compiler, const AstWhileStmt *WhileStmt)
{
    Operand Register = PVMAllocateRegister(Compiler, WhileStmt->Expr.Type);

    U64 Test = PVMCurrentChunk(Compiler)->Count;
    PVMCompileExprInto(Compiler, &Register, &WhileStmt->Expr);
    U64 Location = PVMEmitBranchIfFalse(Compiler, &Register);
    PVMCompileStmt(Compiler, WhileStmt->Stmt);

    PVMEmitBranch(Compiler, Test);
    PVMPatchBranch(Compiler, Location, BRANCH_CONDITIONAL);
}


static Operand *PVMCompileAssignStmt(PVMCompiler *Compiler, const AstAssignStmt *Assignment)
{
    Operand *LValue = PVMGetLocationOf(Compiler, Assignment->VariableID);
    PVMCompileExprInto(Compiler, LValue, &Assignment->Expr);
    return LValue;
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
                &Left, &Left, &Right
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
        PVMEmitLoad(Compiler, Dest, Factor->As.Integer, Factor->Type);
    } break;
    case FACTOR_REAL:
    {
    } break;
    case FACTOR_VARIABLE:
    {
        Operand *Variable = PVMGetLocationOf(Compiler, Factor->As.Variable.ID);
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










static U64 PVMEmitBranchIfFalse(PVMCompiler *Compiler, Operand *Condition)
{
    if (Condition->InRegister)
    {
        return ChunkWriteCode(PVMCurrentChunk(Compiler), PVM_BRIF_INS(EZ, Condition->RegID, -1));
    }
    else 
    {
        PASCAL_UNREACHABLE("TODO: BranchIfFalse mem");
    }
    return 0;
}

static U64 PVMEmitBranch(PVMCompiler *Compiler, U64 Location)
{
    I64 BrOffset = Location - PVMCurrentChunk(Compiler)->Count - 1;
    return ChunkWriteCode(PVMCurrentChunk(Compiler), PVM_BRAL_INS(BrOffset));
}

static void PVMPatchBranch(PVMCompiler *Compiler, U64 StreamOffset, UInt ImmSize)
{
    CodeChunk *Chunk = PVMCurrentChunk(Compiler);
    U32 Instruction = Chunk->Code[StreamOffset];
    U32 Mask = ((U32)1 << ImmSize) - 1;

    I32 Offset = (Chunk->Count - StreamOffset - 1) & Mask;
    Instruction &= ~Mask;
    Chunk->Code[StreamOffset] = Instruction | Offset;
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

static void PVMEmitLoad(PVMCompiler *Compiler, Operand *Dest, U64 Integer, ParserType IntegerType)
{
    /* TODO: sign type */
    if (IntegerType > Dest->IntegralType)
        IntegerType = Dest->IntegralType;

    if (Dest->InRegister)
    {
        CodeChunk *Current = PVMCurrentChunk(Compiler);

        if (0 == Integer)
        {
            ChunkWriteCode(Current, PVM_IRD_ARITH_INS(LDI, Dest->RegID, 0));
            if (IntegerType == TYPE_I64 || IntegerType == TYPE_U64)
                ChunkWriteCode(Current, PVM_IRD_ARITH_INS(LDZHLI, Dest->RegID, 0));
            return;
        }

        switch (IntegerType)
        {
        case TYPE_U8:
        case TYPE_I8:
        case TYPE_I16:
        {
            ChunkWriteCode(Current, PVM_IRD_ARITH_INS(LDI, Dest->RegID, Integer));
        } break;

        case TYPE_U16:
        {
            ChunkWriteCode(Current, PVM_IRD_ARITH_INS(LDZI, Dest->RegID, Integer));
        } break;
        case TYPE_U32:
        case TYPE_I32:
        {
            ChunkWriteCode(Current, PVM_IRD_ARITH_INS(LDZI, Dest->RegID, Integer));
            ChunkWriteCode(Current, PVM_IRD_ARITH_INS(ORUI, Dest->RegID, Integer >> 16));
        } break;

        case TYPE_U64:
        {
            ChunkWriteCode(Current, PVM_IRD_ARITH_INS(LDZI, Dest->RegID, Integer >> 16));
            ChunkWriteCode(Current, PVM_IRD_ARITH_INS(ORUI, Dest->RegID, Integer));
            ChunkWriteCode(Current, PVM_IRD_ARITH_INS(LDZHLI, Dest->RegID, Integer >> 32));
            if (Integer >> 48)
                ChunkWriteCode(Current, PVM_IRD_ARITH_INS(ORHUI, Dest->RegID, Integer >> 48));
        } break;
        case TYPE_I64:
        {
            ChunkWriteCode(Current, PVM_IRD_ARITH_INS(LDZI, Dest->RegID, Integer >> 16));
            ChunkWriteCode(Current, PVM_IRD_ARITH_INS(ORUI, Dest->RegID, Integer));
            if (Integer >> 48 == 0xFFFF)
            {
                ChunkWriteCode(Current, PVM_IRD_ARITH_INS(LDHLI, Dest->RegID, Integer >> 32));
            }
            else 
            {
                ChunkWriteCode(Current, PVM_IRD_ARITH_INS(LDZHLI, Dest->RegID, Integer >> 32));
                ChunkWriteCode(Current, PVM_IRD_ARITH_INS(ORHUI, Dest->RegID, Integer >> 48));
            }
        } break;

        default: PASCAL_UNREACHABLE("Invalid integer type");
        }
    }
    else 
    {
        PASCAL_UNREACHABLE("TODO Load mem, imm");
    }
}


static void PVMEmitAddImm(PVMCompiler *Compiler, Operand *Dest, I16 Imm)
{
    if (Dest->InRegister)
    {
        ChunkWriteCode(PVMCurrentChunk(Compiler), PVM_IRD_ARITH_INS(ADD, Dest->RegID, Imm));
    }
    else 
    {
        PASCAL_UNREACHABLE("TODO: Addi [mem], i32");
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

static void PVMEmitSetCC(PVMCompiler *Compiler, TokenType Op, Operand *Dest, Operand *Left, Operand *Right)
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



    switch (Dest->IntegralType)
    {
    case TYPE_I64: SIGNEDSET(P, "(Signed) PVMEmitSetCC: PVMPtr: %s is not valid", TokenTypeToStr(Op)); break;
    case TYPE_I32: SIGNEDSET(W, "(Signed) PVMEmitSetCC: PVMWord: %s is not valid", TokenTypeToStr(Op)); break;
    case TYPE_I16: SIGNEDSET(H, "(Signed) PVMEmitSetCC: PVMHalf: %s is not valid", TokenTypeToStr(Op)); break;
    case TYPE_I8:  SIGNEDSET(B, "(Signed) PVMEmitSetCC: PVMByte: %s is not valid", TokenTypeToStr(Op)); break;
    case TYPE_U64: SET(P, "PVMEmitSetCC: PVMPtr: %s is not valid", TokenTypeToStr(Op)); break;
    case TYPE_U32: SET(W, "PVMEmitSetCC: PVMWord: %s is not valid", TokenTypeToStr(Op)); break;
    case TYPE_U16: SET(H, "PVMEmitSetCC: PVMHalf: %s is not valid", TokenTypeToStr(Op)); break;
    case TYPE_U8:  SET(B, "PVMEmitSetCC: PVMByte: %s is not valid", TokenTypeToStr(Op)); break;
    default: PASCAL_UNREACHABLE("Invalid size for setcc: %d", Dest->Size); break;
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

