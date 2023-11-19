
#include <string.h> /* memcmp */

#include "Parser.h"
#include "PVM/PVM.h"
#include "PVM/CodeGen.h"


#define PVM_SCOPE_COUNT 32
#define PVM_LOCAL_COUNT 256
#define BRANCH_ALWAYS 26
#define BRANCH_CONDITIONAL 21
#define PVM_GLOBAL_COUNT 256
#define PVM_VAR_COUNT 256






typedef struct LocalVar 
{
    UInt Size;
    U32 FPOffset;
} LocalVar;
typedef struct GlobalVar 
{
    UInt Size;
    U32 Location;
} GlobalVar;
typedef struct FunctionVar
{
    U32 Location;
    ParserType ReturnType;
    bool HasReturnType;
} FunctionVar;
typedef struct RegisterVar 
{
    U32 ID;
} RegisterVar;

typedef enum VarLocationType 
{
    VAR_INVALID = 0,
    VAR_REG,
    VAR_LOCAL,
    VAR_GLOBAL,
    VAR_FUNCTION,

    VAR_TMP_REG,
    VAR_TMP_STK,
} VarLocationType;

typedef struct VarLocation 
{
    U32 ID;
    VarLocationType LocationType;
    ParserType IntegralType;
    union {
        RegisterVar Reg;
        
        LocalVar Local;
        GlobalVar Global;
        FunctionVar Function;
    } As;
} VarLocation;



typedef struct PVMCompiler
{
    const PascalAst *Ast;
    CodeChunk *Chunk;

    UInt SpilledRegCount;
    U32 RegisterList;
    U32 SavedRegisters[PVM_SCOPE_COUNT];

    UInt CurrentScopeDepth;
    U32 EntryPoint;
    U32 SP;

    U32 GlobalDataSize;
    U32 VarCount;
    U32 GlobalCount;
    VarLocation Vars[PVM_VAR_COUNT];

    bool HasError;
} PVMCompiler;



static VarLocation sReturnRegister = {
    .LocationType = VAR_REG,
    .IntegralType = TYPE_U64,
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




static void PVMDeclareLocal(PVMCompiler *Compiler, U32 ID, ParserType IntegralType, LocalVar Local);
static void PVMDeclareGlobal(PVMCompiler *Compiler, U32 ID, ParserType Type, GlobalVar Global);
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
static void PVMCompileCallStmt(PVMCompiler *Compiler, const AstCallStmt *CallStmt);



static void PVMCompileExprInto(PVMCompiler *Compiler, const VarLocation *Dest, const AstExpr *Expr);
static void PVMCompileSimpleExprInto(PVMCompiler *Compiler, const VarLocation *Dest, const AstSimpleExpr *SimpleExpr);
static void PVMCompileTermInto(PVMCompiler *Compiler, const VarLocation *Dest, const AstTerm *Term);
static void PVMCompileFactorInto(PVMCompiler *Compiler, const VarLocation *Dest, const AstFactor *Factor);
static void PVMCompileArgumentList(PVMCompiler *Compiler, const AstExprList *Args);



static U32 PVMEmitCode(PVMCompiler *Compiler, U32 Instruction);
static void PVMEmitGlobal(PVMCompiler *Compiler, GlobalVar Global);
static void PVMEmitDebugInfo(PVMCompiler *Compiler, const AstStmt *BaseStmt);
static bool PVMEmitIntoReg(PVMCompiler *Compiler, VarLocation *Target, const VarLocation *Src);
static U64 PVMEmitBranchIfFalse(PVMCompiler *Compiler, const VarLocation *Condition);
static void PVMPatchBranch(PVMCompiler *Compiler, U32 StreamOffset, U32 Location, UInt ImmSize);
static void PVMPatchBranchToCurrent(PVMCompiler *Compiler, U64 StreamOffset, UInt ImmSize); 
static U64 PVMEmitBranch(PVMCompiler *Compiler, U64 Location);
static void PVMEmitMov(PVMCompiler *Compiler, const VarLocation *Dest, const VarLocation *Src);
static void PVMEmitLoad(PVMCompiler *Compiler, const VarLocation *Dest, U64 Integer, ParserType IntegerType);
static void PVMEmitAddImm(PVMCompiler *Compiler, const VarLocation *Dest, I16 Imm);
static void PVMEmitAdd(PVMCompiler *Compiler, const VarLocation *Dest, const VarLocation *Left, const VarLocation *Right);
static void PVMEmitSub(PVMCompiler *Compiler, const VarLocation *Dest, const VarLocation *Left, const VarLocation *Right);
static void PVMEmitMul(PVMCompiler *Compiler, const VarLocation *Dest, const VarLocation *Left, const VarLocation *Right);
static void PVMEmitDiv(PVMCompiler *Compiler, 
        const VarLocation *Dividend, const VarLocation *Remainder, 
        const VarLocation *Left, const VarLocation *Right
);
static void PVMEmitSetCC(PVMCompiler *Compiler, TokenType Op, 
        const VarLocation *Dest, 
        const VarLocation *Left, const VarLocation *Right
);
static void PVMEmitPush(PVMCompiler *Compiler, UInt RegID);
static void PVMEmitPop(PVMCompiler *Compiler, UInt RegID);
static void PVMEmitAddSp(PVMCompiler *Compiler, I32 Offset);
static void PVMEmitSaveCallerRegs(PVMCompiler *Compiler);
static void PVMEmitCall(PVMCompiler *Compiler, U32 CalleeID);
static void PVMEmitUnsaveCallerRegs(PVMCompiler *Compiler);
static void PVMEmitReturn(PVMCompiler *Compiler);
static void PVMEmitExit(PVMCompiler *Compiler);


static U32 PVMAllocateStackSpace(PVMCompiler *Compiler, UInt Size);

static VarLocation PVMAllocateRegister(PVMCompiler *Compiler, ParserType Type);
static void PVMFreeRegister(PVMCompiler *Compiler, const VarLocation *Register);
static void PVMMarkRegisterAsAllocated(PVMCompiler *Compiler, UInt RegID);
static bool PVMRegisterIsFree(PVMCompiler *Compiler, UInt RegID);
static void PVMSaveRegister(PVMCompiler *Compiler, UInt RegID);

static CodeChunk *PVMCurrentChunk(PVMCompiler *Compiler);

static void PVMBeginScope(PVMCompiler *Compiler);
static void PVMEndScope(PVMCompiler *Compiler);
static USize PVMSizeOfType(PVMCompiler *Compiler, ParserType Type);





bool PVMCompile(CodeChunk *Chunk, const PascalAst *Ast)
{
    PVMCompiler Compiler = PVMCompilerInit(Chunk, Ast);

    /* always is location 0 */
    PVMEmitBranch(&Compiler, 0);
    PVMCompileBlock(&Compiler, Ast->Block);
    PVMPatchBranch(&Compiler, 0, Compiler.EntryPoint, BRANCH_ALWAYS);

    bool HasError = Compiler.HasError;
    if (HasError)
        return false;

    ChunkReserveData(Chunk, Compiler.GlobalDataSize);
    PVMCompilerDeinit(&Compiler);
    return true;
}




PVMCompiler PVMCompilerInit(CodeChunk *Chunk, const PascalAst *Ast)
{
    PVMCompiler Compiler = {
        .Ast = Ast,
        .Chunk = Chunk,
        .HasError = false,
        .RegisterList = 0,
        .SpilledRegCount = 0,
        .GlobalDataSize = 0,
        .VarCount = 0,
        .Vars = { {0} },
        .EntryPoint = 0,
        .SavedRegisters = { 0 },
    };
    return Compiler;
}

void PVMCompilerDeinit(PVMCompiler *Compiler)
{
    PVMEmitExit(Compiler);
}
















static void PVMDeclareLocal(PVMCompiler *Compiler, U32 ID, ParserType Type, LocalVar Local)
{
    U32 VarCount = Compiler->VarCount;
    PASCAL_ASSERT(VarCount < PVM_VAR_COUNT, "TODO: more vars");

    VarLocation *NewLocal = &Compiler->Vars[VarCount];
    NewLocal->ID = ID;
    NewLocal->IntegralType = Type;
    NewLocal->LocationType = VAR_LOCAL;
    NewLocal->As.Local = Local;
    Compiler->VarCount++;
}

static void PVMDeclareGlobal(PVMCompiler *Compiler, U32 ID, ParserType Type, GlobalVar Global)
{
    PASCAL_ASSERT(Compiler->VarCount < PVM_VAR_COUNT, "TODO: more global\n");

    VarLocation *NewGlobal = &Compiler->Vars[Compiler->VarCount];
    NewGlobal->ID = ID;
    NewGlobal->IntegralType = Type;
    NewGlobal->LocationType = VAR_GLOBAL;
    NewGlobal->As.Global = Global;
    Compiler->VarCount++;
    Compiler->GlobalCount++;
}

static void PVMDeclareFunction(PVMCompiler *Compiler, U32 ID, ParserType Type, FunctionVar Function)
{
    PASCAL_ASSERT(Compiler->VarCount < PVM_VAR_COUNT, "TODO: more global\n");

    VarLocation *NewFunction = &Compiler->Vars[Compiler->VarCount];
    NewFunction->ID = ID;
    NewFunction->IntegralType = Type;
    NewFunction->LocationType = VAR_FUNCTION;
    NewFunction->As.Function = Function;
    Compiler->VarCount++;
}

static void PVMDeclareLocalRegister(PVMCompiler *Compiler, U32 ID, ParserType Type, RegisterVar Reg)
{
    PASCAL_ASSERT(Compiler->VarCount < PVM_VAR_COUNT, "TODO: more global\n");

    VarLocation *NewReg = &Compiler->Vars[Compiler->VarCount];
    NewReg->ID = ID;
    NewReg->IntegralType = Type;
    NewReg->LocationType = VAR_REG;
    NewReg->As.Reg = Reg;
    PVMMarkRegisterAsAllocated(Compiler, Reg.ID);
    Compiler->VarCount++;
}



static VarLocation *PVMGetLocationOf(PVMCompiler *Compiler, U32 LocationID)
{
    /* locals */
    for (U32 i = 0; i < Compiler->VarCount; i++)
    {
        if (LocationID == Compiler->Vars[i].ID)
            return &Compiler->Vars[i];
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
        case AST_BLOCK_BEGINEND:
        {
            PVMCompileStmtBlock(Compiler, (const AstStmtBlock*)Block);
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
    if (0 == Compiler->CurrentScopeDepth)
    {
        U32 TotalSize = 0;
        do {
            GlobalVar Global = {
                .Size = PVMSizeOfType(Compiler, I->Type),
                .Location = Compiler->GlobalCount,
            };
            TotalSize += Global.Size;
            PVMDeclareGlobal(Compiler, I->ID, I->Type, Global);

            I = I->Next;
        } while (NULL != I);
        if (TotalSize % sizeof(PVMPtr) != 0)
            TotalSize += sizeof(PVMPtr) - (TotalSize % sizeof(PVMPtr));
        Compiler->GlobalDataSize += TotalSize;
    }
    else /* local */
    {
        U32 TotalSize = 0;
        U32 Offset = Compiler->SP;
        do {
            UInt Size = PVMSizeOfType(Compiler, I->Type);
            LocalVar Local = {
                .Size = Size,
                .FPOffset = Offset + TotalSize,
            };
            PVMDeclareLocal(Compiler, I->ID, I->Type, Local);

            TotalSize += Size / sizeof(PVMPtr);
            /* round up from truncation */
            if (Size % sizeof(PVMPtr) != 0)
                TotalSize += 1;
            I = I->Next;
        } while (NULL != I);

        ChunkWriteDebugInfo(PVMCurrentChunk(Compiler), 
                Declarations->Len, Declarations->Src, Declarations->Line
        );
        PVMAllocateStackSpace(Compiler, TotalSize);
    }
}


static void PVMCompileFunctionBlock(PVMCompiler *Compiler, const AstFunctionBlock *Function)
{
    /* Save location of function */
    FunctionVar GlobalFunction = {
        .Location = PVMCurrentChunk(Compiler)->Count, /* TODO: nested functions */
        .ReturnType = Function->ReturnType,
        .HasReturnType = Function->HasReturnType,
    };
    PVMDeclareFunction(Compiler, Function->ID, TYPE_FUNCTION, GlobalFunction);


    PVMBeginScope(Compiler);
    const AstVarList *I = Function->Params;
    if (NULL != I)
    {
        /* register arguments */
        UInt Arg = 0;
        do {
            RegisterVar RegParam = {
                .ID = sArgumentRegister[Arg],
            };
            PVMDeclareLocalRegister(Compiler, I->ID, I->Type, RegParam);

            I = I->Next;
            Arg++;
        } while (NULL != I && Arg < PVM_REG_ARGCOUNT);

        /* stack arguments */
        while (NULL != I)
        {
            PASCAL_UNREACHABLE("TODO: Handle stack arguments");
            UInt Size = PVMSizeOfType(Compiler, I->Type);
            LocalVar StackParam = {
                .Size = Size,
                .FPOffset = 0, /* TODO: stack param */
            };
            PVMDeclareLocal(Compiler, I->ID, I->Type, StackParam);

            I = I->Next;
        }
    }
    PVMCompileBlock(Compiler, Function->Block);
    PVMEmitReturn(Compiler);
    PVMEndScope(Compiler);
}

static void PVMCompileStmtBlock(PVMCompiler *Compiler, const AstStmtBlock *Block)
{
    Compiler->EntryPoint = PVMCurrentChunk(Compiler)->Count; /* TODO: this is a hack */
    PVMCompileBeginEndStmt(Compiler, Block->BeginEnd);
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
    case AST_STMT_CALL:         PVMCompileCallStmt(Compiler, (const AstCallStmt*)Statement); break;


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

    ChunkWriteDebugInfo(PVMCurrentChunk(Compiler), 
            BeginEnd->EndLen, BeginEnd->EndStr, BeginEnd->EndLine
    );
}

static void PVMCompileIfStmt(PVMCompiler *Compiler, const AstIfStmt *IfStmt)
{
    PVMEmitDebugInfo(Compiler, &IfStmt->Base);
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
        PVMPatchBranchToCurrent(Compiler, SkipIf, BRANCH_CONDITIONAL);
        PVMCompileStmt(Compiler, IfStmt->ElseCase);
        PVMPatchBranchToCurrent(Compiler, ExitIf, BRANCH_ALWAYS);
    }
    else 
    {
        PVMPatchBranchToCurrent(Compiler, SkipIf, BRANCH_CONDITIONAL);
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

    PVMEmitDebugInfo(Compiler, &ForStmt->Base);
    /* assignment */
    VarLocation *I = PVMGetLocationOf(Compiler, ForStmt->VarID);
    PVMCompileExprInto(Compiler, I, &ForStmt->InitExpr);

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
    PVMPatchBranchToCurrent(Compiler, ExitFor, BRANCH_CONDITIONAL);
}


static void PVMCompileWhileStmt(PVMCompiler *Compiler, const AstWhileStmt *WhileStmt)
{
    PVMEmitDebugInfo(Compiler, &WhileStmt->Base);
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
    PVMPatchBranchToCurrent(Compiler, ExitWhile, BRANCH_CONDITIONAL);
}


static VarLocation *PVMCompileAssignStmt(PVMCompiler *Compiler, const AstAssignStmt *Assignment)
{
    PVMEmitDebugInfo(Compiler, &Assignment->Base);
    VarLocation *LValue = PVMGetLocationOf(Compiler, Assignment->VariableID);
    PVMCompileExprInto(Compiler, LValue, &Assignment->Expr);
    return LValue;
}


static void PVMCompileReturnStmt(PVMCompiler *Compiler, const AstReturnStmt *RetStmt)
{
    PVMEmitDebugInfo(Compiler, &RetStmt->Base);
    if (NULL != RetStmt->Expr)
    {
        PVMCompileExprInto(Compiler, &sReturnRegister, RetStmt->Expr);
    }
    PVMEmitReturn(Compiler);
}


static void PVMCompileCallStmt(PVMCompiler *Compiler, const AstCallStmt *CallStmt)
{
    PVMEmitDebugInfo(Compiler, &CallStmt->Base);

    PVMEmitSaveCallerRegs(Compiler);
    PVMCompileArgumentList(Compiler, CallStmt->ArgList);
    PVMEmitCall(Compiler, CallStmt->ProcedureID);
    PVMEmitUnsaveCallerRegs(Compiler);
}









/*
 *================================================================
 *                          EXPRESSIONS
 *================================================================
 */











static void PVMCompileExprInto(PVMCompiler *Compiler, const VarLocation *Dest, const AstExpr *Expr)
{
    const AstOpSimpleExpr *RightSimpleExpr = Expr->Right;
    if (NULL == RightSimpleExpr)
    {
        PVMCompileSimpleExprInto(Compiler, Dest, &Expr->Left);
        return;
    }

    VarLocation Left = PVMAllocateRegister(Compiler, Expr->Left.Type);
    PVMCompileSimpleExprInto(Compiler, &Left, &Expr->Left);

    VarLocation Right = PVMAllocateRegister(Compiler, RightSimpleExpr->Type);
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

static void PVMCompileSimpleExprInto(PVMCompiler *Compiler, const VarLocation *Dest, const AstSimpleExpr *SimpleExpr)
{
    const AstOpTerm *RightTerm = SimpleExpr->Right;
    if (NULL == RightTerm)
    {
        PVMCompileTermInto(Compiler, Dest, &SimpleExpr->Left);
        return;
    }

    VarLocation Left = PVMAllocateRegister(Compiler, SimpleExpr->Left.Type);
    PVMCompileTermInto(Compiler, &Left, &SimpleExpr->Left);

    VarLocation Right = PVMAllocateRegister(Compiler, RightTerm->Type);
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


static void PVMCompileTermInto(PVMCompiler *Compiler, const VarLocation *Dest, const AstTerm *Term)
{
    const AstOpFactor *RightFactor = Term->Right;
    if (NULL == RightFactor)
    {
        PVMCompileFactorInto(Compiler, Dest, &Term->Left);
        return;
    }

    VarLocation Left = PVMAllocateRegister(Compiler, Term->Left.Type);
    PVMCompileFactorInto(Compiler, &Left, &Term->Left);

    VarLocation Right = PVMAllocateRegister(Compiler, RightFactor->Type);
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


static void PVMCompileFactorInto(PVMCompiler *Compiler, const VarLocation *Dest, const AstFactor *Factor)
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
        /* TODO: this is a hack */
        PVMFreeRegister(Compiler, Dest);
        PVMEmitSaveCallerRegs(Compiler);
        PVMMarkRegisterAsAllocated(Compiler, Dest->As.Reg.ID);

        PVMCompileArgumentList(Compiler, Factor->As.Function.ArgList);
        PVMEmitCall(Compiler, Factor->As.Function.ID);
        PVMEmitMov(Compiler, Dest, &sReturnRegister);

        PVMEmitUnsaveCallerRegs(Compiler);
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
    case FACTOR_NOT:
    {
        PASCAL_UNREACHABLE("TODO: FACTOR_NOT case in PVMCompileFactorInto");
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
                .LocationType = VAR_TMP_REG,
                .IntegralType = Current->Expr.Type,
                .As.Reg.ID = sArgumentRegister[ArgCount],
            };
            PVMCompileExprInto(Compiler, &RegArg, &Current->Expr);

            Current = Current->Next;
            ArgCount++;
        } while (NULL != Current && ArgCount < PVM_REG_ARGCOUNT);

        /* stack args */
        while (NULL != Current)
        {
            UInt Size = PVMSizeOfType(Compiler, Current->Expr.Type);
            VarLocation MemArg = {
                .LocationType = VAR_TMP_STK,
                .IntegralType = Current->Expr.Type,
                .As.Local.Size = Size,
                .As.Local.FPOffset = PVMAllocateStackSpace(Compiler, Size),
            };

            PVMCompileExprInto(Compiler, &MemArg, &Current->Expr);
            Current = Current->Next;
            /* no need to inc arg count here */
        }
    }
}







static U32 PVMEmitCode(PVMCompiler *Compiler, U32 Instruction)
{
    return ChunkWriteCode(PVMCurrentChunk(Compiler), Instruction);
}

static void PVMEmitGlobal(PVMCompiler *Compiler, GlobalVar Global)
{
    PASCAL_UNREACHABLE("TODO: emit global");
}


static void PVMEmitDebugInfo(PVMCompiler *Compiler, const AstStmt *BaseStmt)
{
    ChunkWriteDebugInfo(PVMCurrentChunk(Compiler), 
            BaseStmt->Len, 
            BaseStmt->Src,
            BaseStmt->Line
    );
}

static bool PVMEmitIntoReg(PVMCompiler *Compiler, VarLocation *Target, const VarLocation *Src)
{
    switch (Src->LocationType)
    {
    case VAR_INVALID: PASCAL_UNREACHABLE("VAR_INVALID encountered"); break;

    case VAR_TMP_REG:
    case VAR_REG: 
    {
        *Target = *Src;
        return false;
    } break;
    case VAR_TMP_STK:
    case VAR_LOCAL:
    {
        *Target = PVMAllocateRegister(Compiler, Src->IntegralType);
        PVMEmitCode(Compiler, PVM_IRD_MEM_INS(LDRS, Target->As.Reg.ID, Src->As.Local.FPOffset));
        return true;
    } break;
    case VAR_GLOBAL:
    {
        *Target = PVMAllocateRegister(Compiler, Src->IntegralType);
        PVMEmitCode(Compiler, PVM_IRD_MEM_INS(LDG, Target->As.Reg.ID, Src->As.Global.Location));
        return true;
    } break;
    /* loads pointer to function */
    case VAR_FUNCTION:
    {
        PASCAL_UNREACHABLE("TODO: EmitIntoReg function pointer");
    } break;
    }
    return false;
}


static U64 PVMEmitBranchIfFalse(PVMCompiler *Compiler, const VarLocation *Condition)
{
    VarLocation Target;
    bool IsOwning = PVMEmitIntoReg(Compiler, &Target, Condition);
    U64 CurrentOffset = PVMEmitCode(Compiler, PVM_BRIF_INS(EZ, Target.As.Reg.ID, -1));
    if (IsOwning)
    {
        PVMFreeRegister(Compiler, &Target);
    }
    return CurrentOffset;
}

static U64 PVMEmitBranch(PVMCompiler *Compiler, U64 Location)
{
    I64 BrOffset = Location - PVMCurrentChunk(Compiler)->Count - 1;
    return PVMEmitCode(Compiler, PVM_BRAL_INS(BrOffset));
}

static void PVMPatchBranch(PVMCompiler *Compiler, U32 StreamOffset, U32 Location, UInt ImmSize)
{
    CodeChunk *Chunk = PVMCurrentChunk(Compiler);
    U32 Instruction = Chunk->Code[StreamOffset];
    U32 Mask = ((U32)1 << ImmSize) - 1;

    U32 Offset = Location - StreamOffset - 1;
    Chunk->Code[StreamOffset] = (Instruction & ~Mask) | (Offset & Mask);
}

static void PVMPatchBranchToCurrent(PVMCompiler *Compiler, U64 StreamOffset, UInt ImmSize)
{
    PVMPatchBranch(Compiler, StreamOffset, PVMCurrentChunk(Compiler)->Count, ImmSize);
}


static void PVMEmitMov(PVMCompiler *Compiler, const VarLocation *Dest, const VarLocation *Src)
{
    VarLocation Tmp = { 0 };
    bool IsOwning = PVMEmitIntoReg(Compiler, &Tmp, Src);
    switch (Dest->LocationType)
    {
    case VAR_INVALID:
    case VAR_FUNCTION:
    {
        PASCAL_UNREACHABLE("PVMEmitMov: Invalid dest for mov");
    } break;

    case VAR_REG:
    case VAR_TMP_REG:
    {
        if (Dest->As.Reg.ID != Tmp.As.Reg.ID)
        {
            PVMEmitCode(Compiler, PVM_IDAT_TRANSFER_INS(MOV, Dest->As.Reg.ID, Tmp.As.Reg.ID));
        }
    } break;
    case VAR_LOCAL:
    case VAR_TMP_STK:
    {
        PVMEmitCode(Compiler, PVM_IRD_MEM_INS(STRS, Tmp.As.Reg.ID, Dest->As.Local.FPOffset));
    } break;
    case VAR_GLOBAL:
    {
        PVMEmitCode(Compiler, PVM_IRD_MEM_INS(STG, Tmp.As.Reg.ID, Dest->As.Global.Location));
    } break;
    }

    if (IsOwning)
    {
        PVMFreeRegister(Compiler, &Tmp);
    }
}

static void PVMEmitLoad(PVMCompiler *Compiler, const VarLocation *Dest, U64 Integer, ParserType IntegerType)
{
    /* TODO: sign type */
    if (IntegerType > Dest->IntegralType)
        IntegerType = Dest->IntegralType;

    CodeChunk *Current = PVMCurrentChunk(Compiler);

    
    VarLocation Target;
    bool IsOwning = false;
    switch (Dest->LocationType)
    {
    case VAR_REG:
    case VAR_TMP_REG:
    {
        Target = *Dest;
    } break;
    default:
    {
        IsOwning = true;
        Target = PVMAllocateRegister(Compiler, Dest->IntegralType);
    } break;
    }

    /* fast zeroing idiom */
    if (0 == Integer)
    {
        ChunkWriteCode(Current, PVM_IRD_ARITH_INS(LDI, Target.As.Reg.ID, 0));
        if (IntegerType == TYPE_I64 || IntegerType == TYPE_U64)
            ChunkWriteCode(Current, PVM_IRD_ARITH_INS(LDZHLI, Target.As.Reg.ID, 0));
    }
    else /* load the integer */
    {
        switch (IntegerType)
        {
        case TYPE_U8:
        case TYPE_I8:
        case TYPE_I16:
        {
            ChunkWriteCode(Current, PVM_IRD_ARITH_INS(LDI, Target.As.Reg.ID, Integer));
        } break;
        case TYPE_U16:
        {
            ChunkWriteCode(Current, PVM_IRD_ARITH_INS(LDZI, Target.As.Reg.ID, Integer));
        } break;
        case TYPE_U32:
        case TYPE_I32:
        {
            ChunkWriteCode(Current, PVM_IRD_ARITH_INS(LDZI, Target.As.Reg.ID, Integer));
            ChunkWriteCode(Current, PVM_IRD_ARITH_INS(ORUI, Target.As.Reg.ID, Integer >> 16));
        } break;
        case TYPE_U64:
        {
            ChunkWriteCode(Current, PVM_IRD_ARITH_INS(LDZI, Target.As.Reg.ID, Integer >> 16));
            ChunkWriteCode(Current, PVM_IRD_ARITH_INS(ORUI, Target.As.Reg.ID, Integer));
            ChunkWriteCode(Current, PVM_IRD_ARITH_INS(LDZHLI, Target.As.Reg.ID, Integer >> 32));
            if (Integer >> 48)
                ChunkWriteCode(Current, PVM_IRD_ARITH_INS(ORHUI, Target.As.Reg.ID, Integer >> 48));
        } break;
        case TYPE_I64:
        {
            ChunkWriteCode(Current, PVM_IRD_ARITH_INS(LDZI, Target.As.Reg.ID, Integer >> 16));
            ChunkWriteCode(Current, PVM_IRD_ARITH_INS(ORUI, Target.As.Reg.ID, Integer));
            if (Integer >> 48 == 0xFFFF)
            {
                ChunkWriteCode(Current, PVM_IRD_ARITH_INS(LDHLI, Target.As.Reg.ID, Integer >> 32));
            }
            else 
            {
                ChunkWriteCode(Current, PVM_IRD_ARITH_INS(LDZHLI, Target.As.Reg.ID, Integer >> 32));
                ChunkWriteCode(Current, PVM_IRD_ARITH_INS(ORHUI, Target.As.Reg.ID, Integer >> 48));
            }
        } break;
        default: PASCAL_UNREACHABLE("Invalid integer type: %d", IntegerType);
        }
    }

    if (IsOwning)
    {
        PVMEmitMov(Compiler, Dest, &Target);
        PVMFreeRegister(Compiler, &Target);
    }
}


static void PVMEmitAddImm(PVMCompiler *Compiler, const VarLocation *Dest, I16 Imm)
{
    VarLocation Target;
    bool IsOwning = PVMEmitIntoReg(Compiler, &Target, Dest);

    PVMEmitCode(Compiler, PVM_IRD_ARITH_INS(ADD, Target.As.Reg.ID, Imm));

    if (IsOwning)
    {
        PVMEmitMov(Compiler, Dest, &Target);
        PVMFreeRegister(Compiler, &Target);
    }
}


static void PVMEmitAdd(PVMCompiler *Compiler, const VarLocation *Dest, const VarLocation *Left, const VarLocation *Right)
{
    VarLocation Target, LeftReg, RightReg;
    bool IsOwningTarget, IsOwningLeft, IsOwningRight;
    IsOwningTarget = PVMEmitIntoReg(Compiler, &Target, Dest);
    IsOwningLeft = PVMEmitIntoReg(Compiler, &LeftReg, Left);
    IsOwningRight = PVMEmitIntoReg(Compiler, &RightReg, Right);

    PVMEmitCode(Compiler, PVM_IDAT_ARITH_INS(ADD, Target.As.Reg.ID, LeftReg.As.Reg.ID, RightReg.As.Reg.ID, 0));

    if (IsOwningTarget)
    {
        PVMEmitMov(Compiler, Dest, &Target);
        PVMFreeRegister(Compiler, &Target);
    }
    if (IsOwningLeft)
        PVMFreeRegister(Compiler, &LeftReg);
    if (IsOwningRight)
        PVMFreeRegister(Compiler, &RightReg);
}


static void PVMEmitSub(PVMCompiler *Compiler, const VarLocation *Dest, const VarLocation *Left, const VarLocation *Right)
{
    VarLocation Target, LeftReg, RightReg;
    bool IsOwningTarget, IsOwningLeft, IsOwningRight;
    IsOwningTarget = PVMEmitIntoReg(Compiler, &Target, Dest);
    IsOwningLeft = PVMEmitIntoReg(Compiler, &LeftReg, Left);
    IsOwningRight = PVMEmitIntoReg(Compiler, &RightReg, Right);

    PVMEmitCode(Compiler, PVM_IDAT_ARITH_INS(SUB, Target.As.Reg.ID, LeftReg.As.Reg.ID, RightReg.As.Reg.ID, 0));

    if (IsOwningTarget)
    {
        PVMEmitMov(Compiler, Dest, &Target);
        PVMFreeRegister(Compiler, &Target);
    }
    if (IsOwningLeft)
        PVMFreeRegister(Compiler, &LeftReg);
    if (IsOwningRight)
        PVMFreeRegister(Compiler, &RightReg);
}

static void PVMEmitMul(PVMCompiler *Compiler, const VarLocation *Dest, const VarLocation *Left, const VarLocation *Right)
{
    VarLocation Target, LeftReg, RightReg;
    bool IsOwningTarget, IsOwningLeft, IsOwningRight;
    IsOwningTarget = PVMEmitIntoReg(Compiler, &Target, Dest);
    IsOwningLeft = PVMEmitIntoReg(Compiler, &LeftReg, Left);
    IsOwningRight = PVMEmitIntoReg(Compiler, &RightReg, Right);

    PVMEmitCode(Compiler, 
            PVM_IDAT_SPECIAL_INS(MUL, 
                Target.As.Reg.ID, LeftReg.As.Reg.ID, RightReg.As.Reg.ID, 
                false, 0
            )
    );

    if (IsOwningTarget)
    {
        PVMEmitMov(Compiler, Dest, &Target);
        PVMFreeRegister(Compiler, &Target);
    }
    if (IsOwningLeft)
        PVMFreeRegister(Compiler, &LeftReg);
    if (IsOwningRight)
        PVMFreeRegister(Compiler, &RightReg);
}

static void PVMEmitDiv(PVMCompiler *Compiler, const VarLocation *Dividend, const VarLocation *Remainder, const VarLocation *Left, const VarLocation *Right)
{
    VarLocation Target, TargetRemainder, LeftReg, RightReg;
    bool IsOwningTarget, IsOwningRemainder, IsOwningLeft, IsOwningRight;
    IsOwningTarget = PVMEmitIntoReg(Compiler, &Target, Dividend);
    IsOwningRemainder = PVMEmitIntoReg(Compiler, &TargetRemainder, Remainder);
    IsOwningLeft = PVMEmitIntoReg(Compiler, &LeftReg, Left);
    IsOwningRight = PVMEmitIntoReg(Compiler, &RightReg, Right);

    PVMEmitCode(Compiler, 
            PVM_IDAT_SPECIAL_INS(DIV, 
                Target.As.Reg.ID, LeftReg.As.Reg.ID, RightReg.As.Reg.ID, 
                false, TargetRemainder.As.Reg.ID
            )
    );

    if (IsOwningTarget)
    {
        PVMEmitMov(Compiler, Dividend, &Target);
        PVMFreeRegister(Compiler, &Target);
    }
    if (IsOwningRemainder)
    {
        PVMEmitMov(Compiler, Remainder, &TargetRemainder);
        PVMFreeRegister(Compiler, &TargetRemainder);
    }
    if (IsOwningLeft)
        PVMFreeRegister(Compiler, &LeftReg);
    if (IsOwningRight)
        PVMFreeRegister(Compiler, &RightReg);
}

static void PVMEmitSetCC(PVMCompiler *Compiler, TokenType Op, const VarLocation *Dest, const VarLocation *Left, const VarLocation *Right)
{
    VarLocation Target, LeftReg, RightReg;
    bool IsOwningTarget, IsOwningLeft, IsOwningRight;

    IsOwningTarget = PVMEmitIntoReg(Compiler, &Target, Dest);
    IsOwningLeft = PVMEmitIntoReg(Compiler, &LeftReg, Left);
    IsOwningRight = PVMEmitIntoReg(Compiler, &RightReg, Right);

#define SET(Size, ...)\
    do {\
        switch (Op) {\
        case TOKEN_EQUAL:           PVMEmitCode(Compiler, PVM_IDAT_CMP_INS(SEQ ## Size, Target.As.Reg.ID, LeftReg.As.Reg.ID, RightReg.As.Reg.ID)); break;\
        case TOKEN_LESS_GREATER:    PVMEmitCode(Compiler, PVM_IDAT_CMP_INS(SNE ## Size, Target.As.Reg.ID, LeftReg.As.Reg.ID, RightReg.As.Reg.ID)); break;\
        case TOKEN_LESS:            PVMEmitCode(Compiler, PVM_IDAT_CMP_INS(SLT ## Size, Target.As.Reg.ID, LeftReg.As.Reg.ID, RightReg.As.Reg.ID)); break;\
        case TOKEN_GREATER:         PVMEmitCode(Compiler, PVM_IDAT_CMP_INS(SGT ## Size, Target.As.Reg.ID, LeftReg.As.Reg.ID, RightReg.As.Reg.ID)); break;\
        case TOKEN_GREATER_EQUAL:   PVMEmitCode(Compiler, PVM_IDAT_CMP_INS(SLT ## Size, Target.As.Reg.ID, RightReg.As.Reg.ID, LeftReg.As.Reg.ID)); break;\
        case TOKEN_LESS_EQUAL:      PVMEmitCode(Compiler, PVM_IDAT_CMP_INS(SGT ## Size, Target.As.Reg.ID, RightReg.As.Reg.ID, LeftReg.As.Reg.ID)); break;\
        default: {\
            PASCAL_UNREACHABLE(__VA_ARGS__);\
        } break;\
        }\
    }while(0)

#define SIGNEDSET(Size, ...)\
    do {\
        switch (Op) {\
        case TOKEN_EQUAL:           PVMEmitCode(Compiler, PVM_IDAT_CMP_INS(SEQ ## Size, Target.As.Reg.ID, LeftReg.As.Reg.ID, RightReg.As.Reg.ID)); break;\
        case TOKEN_LESS_GREATER:    PVMEmitCode(Compiler, PVM_IDAT_CMP_INS(SNE ## Size, Target.As.Reg.ID, LeftReg.As.Reg.ID, RightReg.As.Reg.ID)); break;\
        case TOKEN_LESS:            PVMEmitCode(Compiler, PVM_IDAT_CMP_INS(SSLT ## Size, Target.As.Reg.ID, LeftReg.As.Reg.ID, RightReg.As.Reg.ID)); break;\
        case TOKEN_GREATER:         PVMEmitCode(Compiler, PVM_IDAT_CMP_INS(SSGT ## Size, Target.As.Reg.ID, LeftReg.As.Reg.ID, RightReg.As.Reg.ID)); break;\
        case TOKEN_GREATER_EQUAL:   PVMEmitCode(Compiler, PVM_IDAT_CMP_INS(SSLT ## Size, Target.As.Reg.ID, RightReg.As.Reg.ID, LeftReg.As.Reg.ID)); break;\
        case TOKEN_LESS_EQUAL:      PVMEmitCode(Compiler, PVM_IDAT_CMP_INS(SSGT ## Size, Target.As.Reg.ID, RightReg.As.Reg.ID, LeftReg.As.Reg.ID)); break;\
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
    default: PASCAL_UNREACHABLE("Invalid type for setcc: %s", ParserTypeToStr(Dest->IntegralType)); break;
    }

    if (IsOwningTarget)
    {
        PVMEmitMov(Compiler, Dest, &Target);
        PVMFreeRegister(Compiler, &Target);
    }
    if (IsOwningLeft)
        PVMFreeRegister(Compiler, &LeftReg);
    if (IsOwningRight)
        PVMFreeRegister(Compiler, &RightReg);

#undef SET
#undef SIGNEDSET
}



static void PVMEmitPush(PVMCompiler *Compiler, UInt RegID)
{
    if (RegID >= 16)
    {
        RegID -= 16;
        PVMEmitCode(Compiler, PVM_IRD_MEM_INS(PSHU, 0, 1 << RegID));
    }
    else 
    {
        PVMEmitCode(Compiler, PVM_IRD_MEM_INS(PSHL, 0, 1 << RegID));
    }
}


static void PVMEmitPop(PVMCompiler *Compiler, UInt RegID)
{
    if (RegID >= 16)
    {
        RegID -= 16;
        PVMEmitCode(Compiler, PVM_IRD_MEM_INS(POPU, 0, 1 << RegID));
    }
    else 
    {
        PVMEmitCode(Compiler, PVM_IRD_MEM_INS(POPL, 0, 1 << RegID));
    }
}


static void PVMEmitAddSp(PVMCompiler *Compiler, I32 Offset)
{
    if (INT21_MIN <= Offset && Offset <= INT21_MAX)
    {
        PVMEmitCode(Compiler, PVM_ADDSP_INS(Offset));
    }
    else 
    {
        PASCAL_UNREACHABLE("Immediate is too large to add to sp");
    }
}


static void PVMEmitSaveCallerRegs(PVMCompiler *Compiler)
{
    /* check all locals in current scope and update their location */
    if (0 == Compiler->RegisterList)
        return;

    UInt RegList = 0;
    for (UInt i = 0; i < 16; i++)
    {
        if (!PVMRegisterIsFree(Compiler, i))
        {
            RegList |= (UInt)1 << i;
        }
    }

    Compiler->SavedRegisters[Compiler->CurrentScopeDepth] = RegList;
    for (UInt i = 0; i < Compiler->VarCount; i++)
    {
        VarLocation *Var = &Compiler->Vars[i];
        if (VAR_REG == Var->LocationType 
        || VAR_TMP_REG == Var->LocationType)
        {
            /* update location */
        }
    }
    PVMEmitCode(Compiler, PVM_IRD_MEM_INS(PSHL, 0, RegList));
}

static void PVMEmitCall(PVMCompiler *Compiler, U32 CalleeID)
{
    /* TODO: function is assumed to have been defined before a call,
     *  though they can be forward declared without defined */

    VarLocation *Function = PVMGetLocationOf(Compiler, CalleeID);
    PASCAL_ASSERT(NULL != Function, "TODO: forward decl");
    U32 CurrentLocation = PVMCurrentChunk(Compiler)->Count;
    PVMEmitCode(Compiler, PVM_BSR_INS(Function->As.Function.Location - CurrentLocation - 1));
}

static void PVMEmitUnsaveCallerRegs(PVMCompiler *Compiler)
{
    if (Compiler->SavedRegisters[Compiler->CurrentScopeDepth] == 0)
        return;

    /* pop all caller save regs */
    PVMEmitCode(Compiler, PVM_IRD_MEM_INS(POPL, 0, 
            Compiler->SavedRegisters[Compiler->CurrentScopeDepth]
    ));

    for (UInt i = 0; i < Compiler->VarCount; i++)
    {
        if (Compiler->Vars[i].LocationType == VAR_TMP_STK)
        {
        }
    }
    Compiler->SavedRegisters[Compiler->CurrentScopeDepth] = 0;
}


static void PVMEmitReturn(PVMCompiler *Compiler)
{
    PVMAllocateStackSpace(Compiler, -Compiler->SP);
    PVMEmitCode(Compiler, PVM_RET_INS);
}


static void PVMEmitExit(PVMCompiler *Compiler)
{
    PVMEmitCode(Compiler, PVM_SYS_INS(EXIT));
}






static U32 PVMAllocateStackSpace(PVMCompiler *Compiler, UInt Slots)
{
    U32 CurrentSP = Compiler->SP;
    Compiler->SP += Slots;
    if (Slots != 0)
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
            PVMMarkRegisterAsAllocated(Compiler, i);
            return (VarLocation) {
                .LocationType = VAR_TMP_REG,
                .IntegralType = Type,
                .As.Reg.ID = i,
            };
        }
    }


    /* spill register */
    UInt Reg = Compiler->SpilledRegCount % PVM_REG_COUNT;
    Compiler->SpilledRegCount++;
    PVMEmitPush(Compiler, Reg);

    return (VarLocation) {
        .LocationType = VAR_TMP_REG,
        .IntegralType = Type,
        .As.Reg.ID = Reg,
    };
}


static void PVMFreeRegister(PVMCompiler *Compiler, const VarLocation *Register)
{
    UInt Reg = (Compiler->SpilledRegCount - 1) % PVM_REG_COUNT;

    if (Compiler->SpilledRegCount > 0 && Reg == Register->As.Reg.ID)
    {
        Compiler->SpilledRegCount--;
        PVMEmitPop(Compiler, Reg);
    }
    else
    {
        Compiler->RegisterList &= ~(1u << Register->As.Reg.ID);
    }
}


static void PVMMarkRegisterAsAllocated(PVMCompiler *Compiler, UInt RegID)
{
    Compiler->RegisterList |= (U32)1 << RegID;
}

static bool PVMRegisterIsFree(PVMCompiler *Compiler, UInt RegID)
{
    return ((Compiler->RegisterList >> RegID) & 1u) == 0;
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
    Compiler->CurrentScopeDepth++;
}

static void PVMEndScope(PVMCompiler *Compiler)
{
    PVMAllocateStackSpace(Compiler, Compiler->SP);
    Compiler->RegisterList = 0; /* free all regs */
    Compiler->CurrentScopeDepth--;
}

static USize PVMSizeOfType(PVMCompiler *Compiler, ParserType Type)
{
    switch (Type)
    {
    case TYPE_I8:
    case TYPE_U8:
    case TYPE_BOOLEAN:
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
