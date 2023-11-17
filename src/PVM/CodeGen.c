
#include <string.h> /* memcmp */

#include "Parser.h"
#include "PVM/PVM.h"
#include "PVM/CodeGen.h"


#define PVM_SCOPE_COUNT 128
#define PVM_MAX_LOCAL_COUNT PVM_REG_COUNT
#define BRANCH_ALWAYS 26
#define BRANCH_CONDITIONAL 21

typedef struct VarLocation 
{
    ParserType IntegralType;
    union {
        struct {
            UInt ID;
        } Reg;
        struct {
            UInt Size;
            U32 FPOffset;
        } Mem;
    } As;
    bool InRegister;
} VarLocation;


typedef struct LocalVar 
{
    U32 ID;
    VarLocation Location;
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
    U32 SP[PVM_SCOPE_COUNT];
} PVMCompiler;



static VarLocation sReturnRegister = {
    .IntegralType = TYPE_U64,
    .InRegister = true,
    .As.Reg.ID = PVM_REG_RET,
};

static const PVMArgReg sArgumentRegister[PVM_REG_ARGCOUNT] = {
    PVM_REG_ARG0,
    PVM_REG_ARG1,
    PVM_REG_ARG2,
    PVM_REG_ARG3,
    PVM_REG_ARG4,
    PVM_REG_ARG5,
    PVM_REG_ARG6,
    PVM_REG_ARG7,
};




PVMCompiler PVMCompilerInit(CodeChunk *Code, const PascalAst *Ast);
void PVMCompilerDeinit(PVMCompiler *Compiler);




static void PVMDeclareLocal(PVMCompiler *Compiler, U32 ID, VarLocation Dest);
static VarLocation *PVMGetLocationOf(PVMCompiler *Compiler, U32 LocationID);



static void PVMCompileBlock(PVMCompiler *Compiler, const AstBlock *Block);
static void PVMCompileVarBlock(PVMCompiler *Compiler, const AstVarBlock *Declarations);
static void PVMCompileFunctionBlock(PVMCompiler *Compiler, const AstFunctionBlock *Function);
static void PVMCompileStmtBlock(PVMCompiler *Compiler, const AstStmtBlock *Block);

static void PVMCompileStmt(PVMCompiler *Compiler, const AstStmt *Statement);
static void PVMCompileIfStmt(PVMCompiler *Compiler, const AstIfStmt *IfStmt);
static void PVMCompileBeginEndStmt(PVMCompiler *Compiler, const AstBeginEndStmt *BeginEnd);
static void PVMCompileForStmt(PVMCompiler *Compiler, const AstForStmt *ForStmt);
static void PVMCompileWhileStmt(PVMCompiler *Compiler, const AstWhileStmt *WhileStmt);
static VarLocation *PVMCompileAssignStmt(PVMCompiler *Compiler, const AstAssignStmt *Assignment);
static void PVMCompileReturnStmt(PVMCompiler *Compiler, const AstReturnStmt *RetStmt);



static void PVMCompileExprInto(PVMCompiler *Compiler, VarLocation *Dest, const AstExpr *Expr);
static void PVMCompileSimpleExprInto(PVMCompiler *Compiler, VarLocation *Dest, const AstSimpleExpr *SimpleExpr);
static void PVMCompileTermInto(PVMCompiler *Compiler, VarLocation *Dest, const AstTerm *Term);
static void PVMCompileFactorInto(PVMCompiler *Compiler, VarLocation *Dest, const AstFactor *Factor);
static void PVMCompileArgumentList(PVMCompiler *Compiler, const AstExprList *Args);



static U64 PVMEmitBranchIfFalse(PVMCompiler *Compiler, VarLocation *Condition);
static void PVMPatchBranch(PVMCompiler *Compiler, U64 StreamOffset, UInt ImmSize); 
static U64 PVMEmitBranch(PVMCompiler *Compiler, U64 Location);
static void PVMEmitMov(PVMCompiler *Compiler, VarLocation *Dest, VarLocation *Src);
static void PVMEmitLoad(PVMCompiler *Compiler, VarLocation *Dest, U64 Integer, ParserType IntegerType);
static void PVMEmitAddImm(PVMCompiler *Compiler, VarLocation *Dest, I16 Imm);
static void PVMEmitAdd(PVMCompiler *Compiler, VarLocation *Dest, VarLocation *Left, VarLocation *Right);
static void PVMEmitSub(PVMCompiler *Compiler, VarLocation *Dest, VarLocation *Left, VarLocation *Right);
static void PVMEmitMul(PVMCompiler *Compiler, VarLocation *Dest, VarLocation *Left, VarLocation *Right);
static void PVMEmitDiv(PVMCompiler *Compiler, VarLocation *Dividend, VarLocation *Remainder, VarLocation *Left, VarLocation *Right);
static void PVMEmitSetCC(
        PVMCompiler *Compiler, TokenType Op, 
        VarLocation *Dest, VarLocation *Left, VarLocation *Right
);
static void PVMEmitAddSp(PVMCompiler *Compiler, I32 Offset);
static void PVMEmitCall(PVMCompiler *Compiler, U32 CalleeID);
static void PVMEmitReturn(PVMCompiler *Compiler);
static void PVMEmitExit(PVMCompiler *Compiler);


static U32 PVMAllocateStackSpace(PVMCompiler *Compiler, UInt Size);
static VarLocation PVMAllocateRegister(PVMCompiler *Compiler, ParserType Type);
static void PVMFreeRegister(PVMCompiler *Compiler, VarLocation *Register);
static bool PVMRegisterIsFree(PVMCompiler *Compiler, UInt RegID);
static void PVMSaveRegister(PVMCompiler *Compiler, UInt RegID);
static CodeChunk *PVMCurrentChunk(PVMCompiler *Compiler);

static void PVMBeginScope(PVMCompiler *Compiler);
static void PVMEndScope(PVMCompiler *Compiler);
static USize PVMSizeOfType(PVMCompiler *Compiler, ParserType Type);





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
















static void PVMDeclareLocal(PVMCompiler *Compiler, U32 ID, VarLocation Dest)
{
    unsigned CurrScope = Compiler->CurrScopeDepth;
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

static VarLocation *PVMGetLocationOf(PVMCompiler *Compiler, U32 LocationID)
{
    for (int i = Compiler->CurrScopeDepth; i >= 0; i--)
    {
        for (int k = 0; k < Compiler->LocalCount[i]; k++)
        {
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
    do {
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
        {
            PASCAL_UNREACHABLE("Invalid block");
        } break;
        }
        Block = Block->Next;
    } while (NULL != Block);
}




static void PVMCompileVarBlock(PVMCompiler *Compiler, const AstVarBlock *Declarations)
{
    const AstVarList *I = &Declarations->Decl;

    /* TODO: global variables */
    U32 TotalSize = 0;
    U32 Offset = Compiler->SP[Compiler->CurrScopeDepth];
    do {
        UInt Size = PVMSizeOfType(Compiler, I->Type);
        VarLocation Local = {
            .InRegister = false,
            .As.Mem.Size = Size,
            .As.Mem.FPOffset = Offset + TotalSize,
            .IntegralType = I->Type,
        };
        PVMDeclareLocal(Compiler, I->ID, Local);

        TotalSize += Size / sizeof(PVMPtr) + 1;
        I = I->Next;
    } while (NULL != I);

    PVMAllocateStackSpace(Compiler, TotalSize);
}


static void PVMCompileFunctionBlock(PVMCompiler *Compiler, const AstFunctionBlock *Function)
{
    //PVMBindVariableToCurrentLocation(Compiler, Function->Identifier);
    /* TODO: assumes that all registers are freed when this function is called */
    const AstVarList *I = Function->Params;
    if (NULL != I)
    {
        PASCAL_UNREACHABLE("TODO: Handle stack arguments");

        /* register arguments */
        UInt Arg = 0;
        do {
            VarLocation Param = {
                .As.Reg.ID = sArgumentRegister[Arg],
                .InRegister = true,
                .IntegralType = I->Type,
            };
            PVMDeclareLocal(Compiler, I->ID, Param);

            I = I->Next;
            Arg++;
        } while (NULL != I && Arg < PVM_REG_ARGCOUNT);

        /* stack arguments */
        while (NULL != I)
        {
            UInt Size = PVMSizeOfType(Compiler, I->Type);
            VarLocation Param = {
                .As.Mem.Size = Size,
                .As.Mem.FPOffset = 0,
                .InRegister = false,
                .IntegralType = I->Type,
            };
            PVMDeclareLocal(Compiler, I->ID, Param);

            I = I->Next;
        }
    }
    /* TODO: parser should verify arg count */

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
    VarLocation Tmp = PVMAllocateRegister(Compiler, IfStmt->Condition.Type);
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
    if (NULL != IfStmt->ElseCase)
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
    VarLocation *I = PVMCompileAssignStmt(Compiler, ForStmt->InitStmt);

    /* TODO: temporary register */
    U64 Location = PVMCurrentChunk(Compiler)->Count;
    VarLocation Stop = PVMAllocateRegister(Compiler, ForStmt->StopExpr.Type);
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
    VarLocation Tmp = PVMAllocateRegister(Compiler, WhileStmt->Expr.Type);

    /* condition test */
    U64 CondTest = PVMCurrentChunk(Compiler)->Count;
    PVMCompileExprInto(Compiler, &Tmp, &WhileStmt->Expr);
    U64 ExitWhile = PVMEmitBranchIfFalse(Compiler, &Tmp);

    PVMFreeRegister(Compiler, &Tmp);

    /* statement */
    PVMCompileStmt(Compiler, WhileStmt->Stmt);

    /* branch to loophead to test for cond */
    PVMEmitBranch(Compiler, CondTest);
    PVMPatchBranch(Compiler, ExitWhile, BRANCH_CONDITIONAL);
}


static VarLocation *PVMCompileAssignStmt(PVMCompiler *Compiler, const AstAssignStmt *Assignment)
{
    VarLocation *LValue = PVMGetLocationOf(Compiler, Assignment->VariableID);
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











static void PVMCompileExprInto(PVMCompiler *Compiler, VarLocation *Dest, const AstExpr *Expr)
{
    const AstOpSimpleExpr *RightSimpleExpr = Expr->Right;
    if (NULL == RightSimpleExpr)
    {
        PVMCompileSimpleExprInto(Compiler, Dest, &Expr->Left);
        return;
    }

    VarLocation Left = PVMAllocateRegister(Compiler, Expr->Type);
    PVMCompileSimpleExprInto(Compiler, &Left, &Expr->Left);

    VarLocation Right = PVMAllocateRegister(Compiler, Expr->Type);
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

static void PVMCompileSimpleExprInto(PVMCompiler *Compiler, VarLocation *Dest, const AstSimpleExpr *SimpleExpr)
{
    const AstOpTerm *RightTerm = SimpleExpr->Right;
    if (NULL == RightTerm)
    {
        PVMCompileTermInto(Compiler, Dest, &SimpleExpr->Left);
        return;
    }

    VarLocation Left = PVMAllocateRegister(Compiler, SimpleExpr->Type);
    PVMCompileTermInto(Compiler, &Left, &SimpleExpr->Left);

    VarLocation Right = PVMAllocateRegister(Compiler, SimpleExpr->Type);
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


static void PVMCompileTermInto(PVMCompiler *Compiler, VarLocation *Dest, const AstTerm *Term)
{
    const AstOpFactor *RightFactor = Term->Right;
    if (NULL == RightFactor)
    {
        PVMCompileFactorInto(Compiler, Dest, &Term->Left);
        return;
    }

    VarLocation Left = PVMAllocateRegister(Compiler, Term->Type);
    PVMCompileFactorInto(Compiler, &Left, &Term->Left);

    VarLocation Right = PVMAllocateRegister(Compiler, Term->Type);
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
            VarLocation Dummy = PVMAllocateRegister(Compiler, RightFactor->Type);
            PVMEmitDiv(Compiler, &Left, &Dummy, &Left, &Right);
            PVMFreeRegister(Compiler, &Dummy);
        } break;

        case TOKEN_MOD:
        {
            VarLocation Dummy = PVMAllocateRegister(Compiler, RightFactor->Type);
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


static void PVMCompileFactorInto(PVMCompiler *Compiler, VarLocation *Dest, const AstFactor *Factor)
{
    switch (Factor->FactorType)
    {
    case FACTOR_INTEGER:
    {
        PVMEmitLoad(Compiler, Dest, Factor->As.Integer, Factor->Type);
    } break;
    case FACTOR_REAL:
    {
        PASCAL_UNREACHABLE("TODO: FACTOR_REAL case in PVMCompileFactorInto");
    } break;
    case FACTOR_CALL:
    {
        PVMCompileArgumentList(Compiler, Factor->As.CallArgList);
        PVMEmitCall(Compiler, Factor->As.VarID);
        PVMEmitMov(Compiler, Dest, &sReturnRegister);
    } break;
    case FACTOR_VARIABLE:
    {
        VarLocation *Variable = PVMGetLocationOf(Compiler, Factor->As.VarID);
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


static void PVMCompileArgumentList(PVMCompiler *Compiler, const AstExprList *Args)
{
    /* TODO:
     * save regs that needed to be saved
     * put args into regs
     */
    const AstExprList *Current = Args;
    UInt ArgCount = 0;
    if (NULL != Args)
    {
        /* register args */
        do {
            VarLocation RegArg = {
                .InRegister = true,
                .IntegralType = Current->Expr.Type,
                .As.Reg.ID = sArgumentRegister[ArgCount],
            };
            if (!PVMRegisterIsFree(Compiler, RegArg.As.Reg.ID))
            {
                PVMSaveRegister(Compiler, RegArg.As.Reg.ID);
            }
            PVMCompileExprInto(Compiler, &RegArg, &Current->Expr);
            Current = Current->Next;
        } while (NULL != Current && ArgCount < PVM_REG_ARGCOUNT);

        /* stack args */
        while (NULL != Current)
        {
            UInt Size = PVMSizeOfType(Compiler, Current->Expr.Type);
            VarLocation MemArg = {
                .InRegister = false,
                .IntegralType = Current->Expr.Type,
                .As.Mem.Size = Size,
                .As.Mem.FPOffset = PVMAllocateStackSpace(Compiler, Size),
            };
            PVMCompileExprInto(Compiler, &MemArg, &Current->Expr);
            Current = Current->Next;
        }
    }
}









static U64 PVMEmitBranchIfFalse(PVMCompiler *Compiler, VarLocation *Condition)
{
    if (Condition->InRegister)
    {
        return ChunkWriteCode(PVMCurrentChunk(Compiler), PVM_BRIF_INS(EZ, Condition->As.Reg.ID, -1));
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


static void PVMEmitMov(PVMCompiler *Compiler, VarLocation *Dest, VarLocation *Src)
{
    if (Dest->InRegister && Src->InRegister)
    {
        ChunkWriteCode(PVMCurrentChunk(Compiler), PVM_IDAT_TRANSFER_INS(MOV, Dest->As.Reg.ID, Src->As.Reg.ID));
    }
    else if (Dest->InRegister && !Src->InRegister)
    {
        PASCAL_ASSERT(Src->As.Mem.Size < 8, "TODO: records");
        ChunkWriteCode(PVMCurrentChunk(Compiler), PVM_IRD_MEM_INS(LDRS, Dest->As.Reg.ID, Src->As.Mem.FPOffset));
    }
    else if (!Dest->InRegister && Src->InRegister)
    {
        PASCAL_ASSERT(Dest->As.Mem.Size < 8, "TODO: records");
        ChunkWriteCode(PVMCurrentChunk(Compiler), PVM_IRD_MEM_INS(STRS, Src->As.Reg.ID, Dest->As.Mem.FPOffset));
    }
    else /* (!Dest->InRegister && !Src->InRegister) */
    {
        PASCAL_ASSERT(Dest->As.Mem.Size < 8, "TODO: records");
        PASCAL_ASSERT(Src->As.Mem.Size < 8, "TODO: records");
        VarLocation Tmp = PVMAllocateRegister(Compiler, Dest->IntegralType);
        /* 
         * ldrs tmp, [src]
         * strs tmp, [dest]
         */
        ChunkWriteCode(PVMCurrentChunk(Compiler), PVM_IRD_MEM_INS(LDRS, Tmp.As.Reg.ID, Src->As.Mem.FPOffset));
        ChunkWriteCode(PVMCurrentChunk(Compiler), PVM_IRD_MEM_INS(STRS, Tmp.As.Reg.ID, Dest->As.Mem.FPOffset));
    }
}

static void PVMEmitLoad(PVMCompiler *Compiler, VarLocation *Dest, U64 Integer, ParserType IntegerType)
{
    /* TODO: sign type */
    if (IntegerType > Dest->IntegralType)
        IntegerType = Dest->IntegralType;

    CodeChunk *Current = PVMCurrentChunk(Compiler);

    
    VarLocation *Target = Dest;
    VarLocation Tmp;
    /* load into a reg first if dest is not in reg */
    if (!Dest->InRegister)
    {
        Tmp = PVMAllocateRegister(Compiler, IntegerType);
        Target = &Tmp;
    }


    /* fast zeroing idiom */
    if (0 == Integer)
    {
        ChunkWriteCode(Current, PVM_IRD_ARITH_INS(LDI, Target->As.Reg.ID, 0));
        if (IntegerType == TYPE_I64 || IntegerType == TYPE_U64)
            ChunkWriteCode(Current, PVM_IRD_ARITH_INS(LDZHLI, Target->As.Reg.ID, 0));
    }
    else /* load the integer */
    {
        switch (IntegerType)
        {
        case TYPE_U8:
        case TYPE_I8:
        case TYPE_I16:
        {
            ChunkWriteCode(Current, PVM_IRD_ARITH_INS(LDI, Target->As.Reg.ID, Integer));
        } break;
        case TYPE_U16:
        {
            ChunkWriteCode(Current, PVM_IRD_ARITH_INS(LDZI, Target->As.Reg.ID, Integer));
        } break;
        case TYPE_U32:
        case TYPE_I32:
        {
            ChunkWriteCode(Current, PVM_IRD_ARITH_INS(LDZI, Target->As.Reg.ID, Integer));
            ChunkWriteCode(Current, PVM_IRD_ARITH_INS(ORUI, Target->As.Reg.ID, Integer >> 16));
        } break;
        case TYPE_U64:
        {
            ChunkWriteCode(Current, PVM_IRD_ARITH_INS(LDZI, Target->As.Reg.ID, Integer >> 16));
            ChunkWriteCode(Current, PVM_IRD_ARITH_INS(ORUI, Target->As.Reg.ID, Integer));
            ChunkWriteCode(Current, PVM_IRD_ARITH_INS(LDZHLI, Target->As.Reg.ID, Integer >> 32));
            if (Integer >> 48)
                ChunkWriteCode(Current, PVM_IRD_ARITH_INS(ORHUI, Target->As.Reg.ID, Integer >> 48));
        } break;
        case TYPE_I64:
        {
            ChunkWriteCode(Current, PVM_IRD_ARITH_INS(LDZI, Target->As.Reg.ID, Integer >> 16));
            ChunkWriteCode(Current, PVM_IRD_ARITH_INS(ORUI, Target->As.Reg.ID, Integer));
            if (Integer >> 48 == 0xFFFF)
            {
                ChunkWriteCode(Current, PVM_IRD_ARITH_INS(LDHLI, Target->As.Reg.ID, Integer >> 32));
            }
            else 
            {
                ChunkWriteCode(Current, PVM_IRD_ARITH_INS(LDZHLI, Target->As.Reg.ID, Integer >> 32));
                ChunkWriteCode(Current, PVM_IRD_ARITH_INS(ORHUI, Target->As.Reg.ID, Integer >> 48));
            }
        } break;
        default: PASCAL_UNREACHABLE("Invalid integer type");
        }
    }

    if (!Dest->InRegister)
    {
        PVMEmitMov(Compiler, Dest, Target);
        PVMFreeRegister(Compiler, Target);
    }
}


static void PVMEmitAddImm(PVMCompiler *Compiler, VarLocation *Dest, I16 Imm)
{
    if (Dest->InRegister)
    {
        ChunkWriteCode(PVMCurrentChunk(Compiler), PVM_IRD_ARITH_INS(ADD, Dest->As.Reg.ID, Imm));
    }
    else 
    {
        PASCAL_UNREACHABLE("TODO: Addi [mem], i32");
    }
}


static void PVMEmitAdd(PVMCompiler *Compiler, VarLocation *Dest, VarLocation *Left, VarLocation *Right)
{
    if (Dest->InRegister && Left->InRegister && Right->InRegister)
    {
        ChunkWriteCode(PVMCurrentChunk(Compiler), PVM_IDAT_ARITH_INS(ADD, Dest->As.Reg.ID, Left->As.Reg.ID, Right->As.Reg.ID, 0));
    }
    else 
    {
        PASCAL_UNREACHABLE("TODO Add dest, left, right");
    }
}


static void PVMEmitSub(PVMCompiler *Compiler, VarLocation *Dest, VarLocation *Left, VarLocation *Right)
{
    if (Dest->InRegister && Left->InRegister && Right->InRegister)
    {
        ChunkWriteCode(PVMCurrentChunk(Compiler), PVM_IDAT_ARITH_INS(SUB, Dest->As.Reg.ID, Left->As.Reg.ID, Right->As.Reg.ID, 0));
    }
    else 
    {
        PASCAL_UNREACHABLE("TODO Sub dest, left, right");
    }
}

static void PVMEmitMul(PVMCompiler *Compiler, VarLocation *Dest, VarLocation *Left, VarLocation *Right)
{
    if (Dest->InRegister && Left->InRegister && Right->InRegister)
    {
        ChunkWriteCode(PVMCurrentChunk(Compiler), PVM_IDAT_SPECIAL_INS(MUL, Dest->As.Reg.ID, Left->As.Reg.ID, Right->As.Reg.ID, true, 0));
    }
    else 
    {
        PASCAL_UNREACHABLE("TODO Mul dest, left, right");
    }
}

static void PVMEmitDiv(PVMCompiler *Compiler, VarLocation *Dividend, VarLocation *Remainder, VarLocation *Left, VarLocation *Right)
{
    if (Dividend->InRegister && Remainder->InRegister && Left->InRegister && Right->InRegister)
    {
        ChunkWriteCode(PVMCurrentChunk(Compiler), PVM_IDAT_SPECIAL_INS(DIV, Dividend->As.Reg.ID, Left->As.Reg.ID, Right->As.Reg.ID, true, Remainder->As.Reg.ID));
    }
    else 
    {
        PASCAL_UNREACHABLE("TODO Div dest, left, right, rem");
    }
}

static void PVMEmitSetCC(PVMCompiler *Compiler, TokenType Op, VarLocation *Dest, VarLocation *Left, VarLocation *Right)
{
    PASCAL_ASSERT(Dest->InRegister && Left->InRegister && Right->InRegister, "TODO: SetCC Dest, Left, Right");
#define SET(Size, ...)\
    do {\
        switch (Op) {\
        case TOKEN_EQUAL:           ChunkWriteCode(PVMCurrentChunk(Compiler), PVM_IDAT_CMP_INS(SEQ ## Size, Dest->As.Reg.ID, Left->As.Reg.ID, Right->As.Reg.ID)); break;\
        case TOKEN_LESS_GREATER:    ChunkWriteCode(PVMCurrentChunk(Compiler), PVM_IDAT_CMP_INS(SNE ## Size, Dest->As.Reg.ID, Left->As.Reg.ID, Right->As.Reg.ID)); break;\
        case TOKEN_LESS:            ChunkWriteCode(PVMCurrentChunk(Compiler), PVM_IDAT_CMP_INS(SLT ## Size, Dest->As.Reg.ID, Left->As.Reg.ID, Right->As.Reg.ID)); break;\
        case TOKEN_GREATER:         ChunkWriteCode(PVMCurrentChunk(Compiler), PVM_IDAT_CMP_INS(SGT ## Size, Dest->As.Reg.ID, Left->As.Reg.ID, Right->As.Reg.ID)); break;\
        case TOKEN_GREATER_EQUAL:   ChunkWriteCode(PVMCurrentChunk(Compiler), PVM_IDAT_CMP_INS(SLT ## Size, Dest->As.Reg.ID, Right->As.Reg.ID, Left->As.Reg.ID)); break;\
        case TOKEN_LESS_EQUAL:      ChunkWriteCode(PVMCurrentChunk(Compiler), PVM_IDAT_CMP_INS(SGT ## Size, Dest->As.Reg.ID, Right->As.Reg.ID, Left->As.Reg.ID)); break;\
        default: {\
            PASCAL_UNREACHABLE(__VA_ARGS__);\
        } break;\
        }\
    }while(0)

#define SIGNEDSET(Size, ...)\
    do {\
        switch (Op) {\
        case TOKEN_EQUAL:           ChunkWriteCode(PVMCurrentChunk(Compiler), PVM_IDAT_CMP_INS(SEQ ## Size, Dest->As.Reg.ID, Left->As.Reg.ID, Right->As.Reg.ID)); break;\
        case TOKEN_LESS_GREATER:    ChunkWriteCode(PVMCurrentChunk(Compiler), PVM_IDAT_CMP_INS(SNE ## Size, Dest->As.Reg.ID, Left->As.Reg.ID, Right->As.Reg.ID)); break;\
        case TOKEN_LESS:            ChunkWriteCode(PVMCurrentChunk(Compiler), PVM_IDAT_CMP_INS(SSLT ## Size, Dest->As.Reg.ID, Left->As.Reg.ID, Right->As.Reg.ID)); break;\
        case TOKEN_GREATER:         ChunkWriteCode(PVMCurrentChunk(Compiler), PVM_IDAT_CMP_INS(SSGT ## Size, Dest->As.Reg.ID, Left->As.Reg.ID, Right->As.Reg.ID)); break;\
        case TOKEN_GREATER_EQUAL:   ChunkWriteCode(PVMCurrentChunk(Compiler), PVM_IDAT_CMP_INS(SSLT ## Size, Dest->As.Reg.ID, Right->As.Reg.ID, Left->As.Reg.ID)); break;\
        case TOKEN_LESS_EQUAL:      ChunkWriteCode(PVMCurrentChunk(Compiler), PVM_IDAT_CMP_INS(SSGT ## Size, Dest->As.Reg.ID, Right->As.Reg.ID, Left->As.Reg.ID)); break;\
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
    default: PASCAL_UNREACHABLE("Invalid size for setcc: %d", Dest->As.Mem.Size); break;
    }

#undef SET
#undef SIGNEDSET
}



static void PVMEmitAddSp(PVMCompiler *Compiler, I32 Offset)
{
    /* TODO: put these in a macro */
    if (-((1 << 21) - 1) <= Offset && Offset <= ((1 << 21) - 1))
    {
        ChunkWriteCode(PVMCurrentChunk(Compiler), PVM_ADDSP_INS(Offset));
    }
    else 
    {
        PASCAL_UNREACHABLE("Immediate is too large to add to sp");
    }
}


static void PVMEmitCall(PVMCompiler *Compiler, U32 CalleeID)
{
    PASCAL_UNREACHABLE("TODO: EmitCall()");
}


static void PVMEmitReturn(PVMCompiler *Compiler)
{
    ChunkWriteCode(PVMCurrentChunk(Compiler), PVM_RET_INS);
}


static void PVMEmitExit(PVMCompiler *Compiler)
{
    ChunkWriteCode(PVMCurrentChunk(Compiler), PVM_SYS_INS(EXIT));
}






static U32 PVMAllocateStackSpace(PVMCompiler *Compiler, UInt Slots)
{
    UInt CurrentScope = Compiler->CurrScopeDepth;
    U32 CurrentSP = Compiler->SP[CurrentScope];

    Compiler->SP[CurrentScope] += Slots;
    PVMEmitAddSp(Compiler, (I32)Slots);
    return CurrentSP;
}


static VarLocation PVMAllocateRegister(PVMCompiler *Compiler, ParserType Type)
{
    PASCAL_ASSERT(Type != TYPE_INVALID, "Compiler received invalid type");
    for (UInt i = 0; i < PVM_REG_COUNT; i++)
    {
        /* found free reg */
        if (PVMRegisterIsFree(Compiler, i))
        {
            /* mark reg as allocated */
            Compiler->RegisterList[Compiler->CurrScopeDepth] |= 1u << i;
            return (VarLocation) {
                .As.Reg.ID = i,
                .InRegister = true,
                .IntegralType = Type,
            };
        }
    }
    PASCAL_UNREACHABLE("TODO: Register spilling");
    return (VarLocation) { 0 };
}

static void PVMFreeRegister(PVMCompiler *Compiler, VarLocation *Register)
{
    Compiler->RegisterList[Compiler->CurrScopeDepth] &= ~(1u << Register->As.Reg.ID);
}

static bool PVMRegisterIsFree(PVMCompiler *Compiler, UInt RegID)
{
    return ((Compiler->RegisterList[Compiler->CurrScopeDepth] >> RegID) & 1u) == 0;
}

static void PVMSaveRegister(PVMCompiler *Compiler, UInt RegID)
{
    PASCAL_UNREACHABLE("TODO: save register");
}


static CodeChunk *PVMCurrentChunk(PVMCompiler *Compiler)
{
    return Compiler->Chunk;
}




static void PVMBeginScope(PVMCompiler *Compiler)
{
    UInt CurrentScope = Compiler->CurrScopeDepth;

    /* copy registers from the last scope */
    Compiler->RegisterList[CurrentScope + 1] = Compiler->RegisterList[CurrentScope];
    Compiler->SP[CurrentScope + 1] = Compiler->SP[CurrentScope];
    Compiler->CurrScopeDepth++;
}

static void PVMEndScope(PVMCompiler *Compiler)
{
    PVMAllocateStackSpace(Compiler, -Compiler->SP[Compiler->CurrScopeDepth]);
    Compiler->CurrScopeDepth--;
}

static USize PVMSizeOfType(PVMCompiler *Compiler, ParserType Type)
{
    switch (Type)
    {
    case TYPE_I8:
    case TYPE_U8:
        return sizeof(U8);

    case TYPE_I16:
    case TYPE_U16:
        return sizeof(U16);

    case TYPE_I32:
    case TYPE_U32:
    case TYPE_F32:
        return 4;

    case TYPE_I64:
    case TYPE_U64:
    case TYPE_F64:
        return 8;

    case TYPE_COUNT:
    case TYPE_INVALID:
    {
        PASCAL_UNREACHABLE("Invalid type in PVMSizeOfType(): %d", Type);
    } break;
    case TYPE_FUNCTION:
    {
        PASCAL_UNREACHABLE("TODO: Type Function in PVMSizeOfType()");
    } break;

    }
    return 0;
}
