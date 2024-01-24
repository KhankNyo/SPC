

#include <stdarg.h> /* duh */
#include <inttypes.h> /* PRI* */


#include "Memory.h"
#include "StringView.h"
#include "Tokenizer.h"
#include "Vartab.h"
#include "Variable.h"
#include "IntegralTypes.h"

#include "Compiler/Compiler.h"
#include "Compiler/Builtins.h"
#include "Compiler/Data.h"
#include "Compiler/Error.h"
#include "Compiler/Emitter.h"
#include "Compiler/Expr.h"
#include "Compiler/VarList.h"



#define EMITTER() (&Compiler->Emitter)

typedef enum Condition 
{
    COND_FALSE,
    COND_TRUE,
    COND_UNKNOWN,
} Condition;


/*===============================================================================*/
/*
 *                                   RECOVERY
 */
/*===============================================================================*/


static bool CompilerGetLinesFromCallback(PascalCompiler *Compiler)
{
    if (NULL == Compiler->ReplCallback)
        return false;

    const U8 *Line = Compiler->ReplCallback(Compiler->ReplUserData);
    if (NULL == Line)
        return false;

    Compiler->Lexer = TokenizerInit(Line, ++Compiler->Line);
    ConsumeToken(Compiler);
    return true;
}

static bool NoMoreToken(PascalCompiler *Compiler)
{
    if (IsAtEnd(Compiler))
    {
        if (PASCAL_COMPMODE_REPL == Compiler->Flags.CompMode)
        {
            return Compiler->Panic || !CompilerGetLinesFromCallback(Compiler);
        }
        return true;
    }
    return false;
}


static void CalmDownDog(PascalCompiler *Compiler)
{
    Compiler->Panic = false;

    while (!IsAtStmtEnd(Compiler))
    {
        if (Compiler->Curr.Type == TOKEN_SEMICOLON)
        {
            switch (Compiler->Next.Type)
            {
            case TOKEN_LABEL:
            case TOKEN_CONST:
            case TOKEN_TYPE:
            case TOKEN_VAR:
            case TOKEN_PROCEDURE:
            case TOKEN_FUNCTION:
            case TOKEN_BEGIN:

            case TOKEN_END:
            case TOKEN_IF:
            case TOKEN_FOR:
            case TOKEN_WHILE:
                return;

            default: break;
            }
        }
        else switch (Compiler->Next.Type)
        {
        case TOKEN_END:
        case TOKEN_IF:
        case TOKEN_FOR:
        case TOKEN_WHILE:
            return;
        default: break;
        }
        ConsumeToken(Compiler);
    }
}


static void CalmDownAtBlock(PascalCompiler *Compiler)
{
    Compiler->Panic = false;
    if (PASCAL_COMPMODE_REPL == Compiler->Flags.CompMode)
    {
        while (!IsAtEnd(Compiler))
            ConsumeToken(Compiler);
        return;
    }
    do {
        if (TOKEN_SEMICOLON == Compiler->Curr.Type)
        {
            switch (Compiler->Next.Type)
            {
            case TOKEN_LABEL:
            case TOKEN_CONST:
            case TOKEN_TYPE:
            case TOKEN_VAR:
            case TOKEN_PROCEDURE:
            case TOKEN_FUNCTION:
            case TOKEN_BEGIN:
                return;

            default: break;
            }
        }
        
        ConsumeToken(Compiler);
    } while (!IsAtEnd(Compiler));
}





/*===============================================================================*/
/*
 *                                   STATEMENT
 */
/*===============================================================================*/


static void CompileStmt(PascalCompiler *Compiler);


static void CompileExitStmt(PascalCompiler *Compiler)
{
    PASCAL_NONNULL(Compiler);
    /*
     * Exit;
     * Exit();
     * Exit(expr);
     */
    /* 'exit' consumed */
    Token Keyword = Compiler->Curr;
    CompilerInitDebugInfo(Compiler, &Keyword);

    if (ConsumeIfNextTokenIs(Compiler, TOKEN_LEFT_PAREN))
    {
        const char *ErrorMessage = "Expected ')'.";
        if (IsAtGlobalScope(Compiler))
        {
            if (ConsumeIfNextTokenIs(Compiler, TOKEN_RIGHT_PAREN))
                goto Done;

            VarLocation ReturnValue = PVMSetReturnType(EMITTER(), VarTypeInit(TYPE_I32, sizeof(I32)));
            CompileExprInto(Compiler, &Keyword, &ReturnValue);
            FreeExpr(Compiler, ReturnValue);
            ErrorMessage = "Expected ')' after expression.";
        }
        else 
        {
            const SubroutineData *CurrentSubroutine = Compiler->Subroutine[Compiler->Scope - 1].Current;
            PASCAL_NONNULL(CurrentSubroutine);

            /* has a return value */
            if (NULL != CurrentSubroutine->ReturnType)
            {
                /* ')' is the next token? */
                if (ConsumeIfNextTokenIs(Compiler, TOKEN_RIGHT_PAREN))
                {
                    ErrorAt(Compiler, &Keyword, "Function must return a value.");
                    goto Done;
                }

                VarLocation ReturnValue = PVMSetReturnType(EMITTER(), *CurrentSubroutine->ReturnType);
                CompileExprInto(Compiler, &Keyword, &ReturnValue);
                FreeExpr(Compiler, ReturnValue);
                ErrorMessage = "Expected ')' after expression.";
            }
            else if (!NextTokenIs(Compiler, TOKEN_RIGHT_PAREN))
            {
                /* must be an error: a procedure returning something */
                ErrorAt(Compiler, &Keyword, "Cannot return a value from a procedure.");
            }
        }
        ConsumeOrError(Compiler, TOKEN_RIGHT_PAREN, ErrorMessage);
    }
Done:
    PVMEmitExit(EMITTER());
    CompilerEmitDebugInfo(Compiler, &Keyword);
}



static void CompileBeginStmt(PascalCompiler *Compiler)
{
    /* begin consumed */
    do {
        if (NoMoreToken(Compiler))
        {
            Error(Compiler, "Expected 'end'.");
        }
        CompileStmt(Compiler);
    } while (ConsumeIfNextTokenIs(Compiler, TOKEN_SEMICOLON));
    ConsumeOrError(Compiler, TOKEN_END, "Expected ';' between statements.");
}



static UInt CompileLoopBody(PascalCompiler *Compiler)
{
    UInt PrevBreakCount = Compiler->BreakCount;
    bool WasInLoop = Compiler->InLoop;
    Compiler->InLoop = true;

    CompileStmt(Compiler);

    Compiler->InLoop = WasInLoop;
    return PrevBreakCount;
}

static void CompilerPatchBreaks(PascalCompiler *Compiler, UInt PrevBreakCount)
{
    for (UInt i = PrevBreakCount; i < Compiler->BreakCount; i++)
    {
        PVMPatchBranchToCurrent(EMITTER(), Compiler->Breaks[i]);
    }
    Compiler->BreakCount = PrevBreakCount;
}


static void CompileRepeatUntilStmt(PascalCompiler *Compiler)
{
    /* 'repeat' consumed */
    U32 LoopHead = PVMMarkBranchTarget(EMITTER());
    if (!NextTokenIs(Compiler, TOKEN_UNTIL))
    {
        do {
            CompileStmt(Compiler);
        } while (ConsumeIfNextTokenIs(Compiler, TOKEN_SEMICOLON));
    }

    /* nothing to compile, exit and reports error */
    if (!ConsumeOrError(Compiler, TOKEN_UNTIL, "Expected 'until'."))
        return;

    Token UntilKeyword = Compiler->Curr;
    CompilerInitDebugInfo(Compiler, &UntilKeyword);

    VarLocation Tmp = CompileExpr(Compiler);
    if (TYPE_BOOLEAN != Tmp.Type.Integral)
    {
        ErrorTypeMismatch(Compiler, &UntilKeyword, "until loop condition", "boolean", Tmp.Type.Integral);
    }

    U32 ToHead = PVMEmitBranchIfFalse(EMITTER(), &Tmp);
    PVMPatchBranch(EMITTER(), ToHead, LoopHead);
    FreeExpr(Compiler, Tmp);

    CompilerEmitDebugInfo(Compiler, &UntilKeyword);
}


static void CompileForStmt(PascalCompiler *Compiler)
{
    /* 
     * for i := 0 to 100 do
     * for i := 100 downto 0 do 
     * for i in RangeType do 
     * for i in [RangeType] do
     */
    /* 'for' consumed */
    Token Keyword = Compiler->Curr;
    CompilerInitDebugInfo(Compiler, &Keyword);

    /* for loop variable */
    ConsumeOrError(Compiler, TOKEN_IDENTIFIER, "Expected variable name.");
    PascalVar *Counter = GetIdenInfo(Compiler, &Compiler->Curr, "Undefined variable.");
    /* TODO: these returns make error messages bad, 
     * bc the body of the for loop is not parsed correctly */
    if (NULL == Counter)
        return;
    PASCAL_NONNULL(Counter->Location);
    if (!IntegralTypeIsOrdinal(Counter->Type.Integral))
    {
        ErrorAt(Compiler, &Compiler->Curr, "Variable of type %s cannot be used as a counter.",
                VarTypeToStr(Counter->Type)
        );
        return;
    }

    /* FPC does not allow the address of a counter variable to be taken */
    VarLocation CounterSave = *Counter->Location;
    VarLocation *i = Counter->Location;
    *i = PVMAllocateRegisterLocation(EMITTER(), CounterSave.Type);
    i->As.Register.Persistent = true;

    /* init expression */
    ConsumeOrError(Compiler, TOKEN_COLON_EQUAL, "Expected ':=' after variable name.");
    Token Assignment = Compiler->Curr;
    CompileExprInto(Compiler, &Assignment, i);


    /* for loop inc/dec */
    U32 LoopHead = 0;
    TokenType Op = TOKEN_GREATER_EQUAL;
    Token OpToken = Compiler->Next;
    int Inc = 1;
    if (!ConsumeIfNextTokenIs(Compiler, TOKEN_TO))
    {
        ConsumeOrError(Compiler, TOKEN_DOWNTO, "Expected 'to' or 'downto' after expression.");
        Op = TOKEN_LESS_EQUAL;
        Inc = -1;
    }
    /* stop condition expr */
    U32 LoopExit = 0;
    VarLocation StopCondition = CompileExprIntoReg(Compiler); 
    if (TYPE_INVALID == CoerceTypes(i->Type.Integral, StopCondition.Type.Integral))
    {
        ErrorAt(Compiler, &OpToken, "Incompatible types for 'for' loop: from %s "STRVIEW_FMT" %s.", 
                VarTypeToStr(i->Type),
                STRVIEW_FMT_ARG(&OpToken.Lexeme),
                VarTypeToStr(StopCondition.Type)
        );
    }
    else
    {
        PASCAL_ASSERT(ConvertTypeImplicitly(Compiler, i->Type.Integral, &StopCondition), "");

        LoopHead = PVMMarkBranchTarget(EMITTER());
        PVMEmitSetFlag(EMITTER(), Op, &StopCondition, i);
        LoopExit = PVMEmitBranchOnFalseFlag(EMITTER());
    }
    /* do */
    ConsumeOrError(Compiler, TOKEN_DO, "Expected 'do' after expression.");
    CompilerEmitDebugInfo(Compiler, &Keyword);


    /* loop body */
    /* NOTE: writing to the loop counter variable is legal here but not FreePascal */
    UInt BreakCountBeforeBody = CompileLoopBody(Compiler);

    /* loop increment */
    PVMEmitBranchAndInc(EMITTER(), i->As.Register, Inc, LoopHead);

    /* loop end */
    PVMEmitDebugInfo(EMITTER(), Compiler->Curr.Lexeme.Str, Compiler->Curr.Lexeme.Len, Compiler->Curr.Line);
    PVMPatchBranchToCurrent(EMITTER(), LoopExit);
    CompilerPatchBreaks(Compiler, BreakCountBeforeBody);

    /* move the result of the counter variable */
    PVMEmitMove(EMITTER(), &CounterSave, i);
    PVMFreeRegister(EMITTER(), i->As.Register);
    FreeExpr(Compiler, StopCondition);
    *i = CounterSave;
}


static void CompileWhileStmt(PascalCompiler *Compiler)
{
    PASCAL_NONNULL(Compiler);

    /* 'while' consumed */
    Token Keyword = Compiler->Curr;
    CompilerInitDebugInfo(Compiler, &Keyword);

    /* condition expression */
    U32 LoopHead = PVMMarkBranchTarget(EMITTER());
    VarLocation Tmp = CompileExpr(Compiler);
    if (TYPE_BOOLEAN != Tmp.Type.Integral)
    {
        ErrorTypeMismatch(Compiler, &Keyword, "while loop condition", "boolean", Tmp.Type.Integral);
    }

    Condition Cond = COND_UNKNOWN;
    if (VAR_LIT == Tmp.LocationType)
    {
        Cond = COND_FALSE;
        if (Tmp.As.Literal.Bool)
            Cond = COND_TRUE;
    }

    bool Last = EMITTER()->ShouldEmit;
    if (COND_FALSE == Cond)
        EMITTER()->ShouldEmit = false;

    U32 LoopExit = PVMEmitBranchIfFalse(EMITTER(), &Tmp);
    FreeExpr(Compiler, Tmp);
    /* do */
    ConsumeOrError(Compiler, TOKEN_DO, "Expected 'do' after expression.");
    CompilerEmitDebugInfo(Compiler, &Keyword);


    /* loop body */
    UInt BreakCountBeforeBody = CompileLoopBody(Compiler);


    /* back to loophead */
    PVMEmitBranch(EMITTER(), LoopHead);
    /* patch the exit branch */
    PVMPatchBranchToCurrent(EMITTER(), LoopExit);
    CompilerPatchBreaks(Compiler, BreakCountBeforeBody);

    EMITTER()->ShouldEmit = Last;
}


static void CompileIfStmt(PascalCompiler *Compiler)
{
    PASCAL_NONNULL(Compiler);

    /* 'if' consumed */
    Token Keyword = Compiler->Curr;
    CompilerInitDebugInfo(Compiler, &Keyword);


    /* condition expression */
    VarLocation Tmp = CompileExpr(Compiler);
    ConsumeOrError(Compiler, TOKEN_THEN, "Expected 'then' after expression.");
    CompilerEmitDebugInfo(Compiler, &Keyword);

    if (TYPE_BOOLEAN != Tmp.Type.Integral)
        ErrorTypeMismatch(Compiler, &Keyword, "if condition", "boolean", Tmp.Type.Integral);

    Condition IfCond = COND_UNKNOWN;
    if (VAR_LIT == Tmp.LocationType)
    {
        IfCond = COND_FALSE;
        if (Tmp.As.Literal.Bool)
            IfCond = COND_TRUE;
    }

    bool Last = EMITTER()->ShouldEmit;
    /* 
     * IF: 
     *      BEZ Tmp, ELSE 
     *      Stmt...
     *      BAL DONE
     * ELSE:
     *      Stmt...
     * DONE:
     * */
    U32 FromIf = 0;
    if (COND_FALSE == IfCond)
        EMITTER()->ShouldEmit = false;

    if (COND_UNKNOWN == IfCond)
        FromIf = PVMEmitBranchIfFalse(EMITTER(), &Tmp);
    FreeExpr(Compiler, Tmp);

    /* if body */
    CompileStmt(Compiler);
    
    EMITTER()->ShouldEmit = Last;
    if (ConsumeIfNextTokenIs(Compiler, TOKEN_ELSE))
    {
        U32 FromEndIf = 0;
        if (COND_TRUE == IfCond)
            EMITTER()->ShouldEmit = false;
        
        if (COND_UNKNOWN == IfCond)
            FromEndIf = PVMEmitBranch(EMITTER(), 0);

        PVMPatchBranchToCurrent(EMITTER(), FromIf);
        CompilerInitDebugInfo(Compiler, &Compiler->Curr);
        CompilerEmitDebugInfo(Compiler, &Compiler->Curr);

        CompileStmt(Compiler);

        if (COND_UNKNOWN == IfCond)
            PVMPatchBranchToCurrent(EMITTER(), FromEndIf);
    }
    else if (COND_UNKNOWN == IfCond)
    {
        PVMPatchBranchToCurrent(EMITTER(), FromIf);
    }
    EMITTER()->ShouldEmit = Last;
}



static void CompileCallWithoutReturnValue(PascalCompiler *Compiler, 
        const VarLocation *Location, const Token *Callee)
{
    PASCAL_NONNULL(Compiler);
    PASCAL_NONNULL(Location);
    PASCAL_NONNULL(Callee);


    const VarType *Type = &Location->Type;
    if (TYPE_POINTER == Type->Integral)
    {
        Type = Type->As.Pointee;
        if (NULL == Type || TYPE_FUNCTION != Type->Integral)
        {
            ErrorAt(Compiler, Callee, "Cannot call %s.", VarTypeToStr(Location->Type));
            return;
        }
    }
    const SubroutineData *Subroutine = &Type->As.Subroutine;
    const VarType *ReturnType = Subroutine->ReturnType;


    /* save caller registers asnd start args */
    VarLocation ReturnValue;
    SaveRegInfo SaveRegs = PVMEmitSaveCallerRegs(EMITTER(), NO_RETURN_REG);
    I32 Base = PVMStartArg(EMITTER(), Subroutine->StackArgSize);
    if (NULL != ReturnType && !VarTypeIsTriviallyCopiable(*ReturnType))
    {
        /* create a temporary on stack as first argument */
        VarLocation FirstArg = PVMSetArg(EMITTER(), 0, *ReturnType, &Base);
        Compiler->TemporarySize = uMax(Compiler->TemporarySize, ReturnType->Size);
        ReturnValue = PVMCreateStackLocation(EMITTER(), *ReturnType, Compiler->StackSize);

        PASCAL_ASSERT(ReturnValue.LocationType == VAR_MEM, "%s", __func__);
        PVMEmitMove(EMITTER(), &FirstArg, &ReturnValue);
        PVMMarkArgAsOccupied(EMITTER(), &FirstArg);
    }


    CompileArgumentList(Compiler, Callee, Subroutine, &Base, Subroutine->HiddenParamCount);
    CompilerEmitCall(Compiler, Location, SaveRegs);


    if (Subroutine->StackArgSize)
    {
        /* deallocate stack space */
        PVMEmitStackAllocation(EMITTER(), -Subroutine->StackArgSize);
    }
    /* restore caller regs */
    PVMEmitUnsaveCallerRegs(EMITTER(), NO_RETURN_REG, SaveRegs);
}


static void CompileCallStmt(PascalCompiler *Compiler, const Token Name, VarLocation *Location)
{
    PASCAL_NONNULL(Compiler);
    PASCAL_NONNULL(Location);
    PASCAL_ASSERT(TYPE_FUNCTION == Location->Type.Integral, "Unreachable, IdenInfo is not a subroutine");

    /* iden consumed */
    /* call the subroutine */
    CompilerInitDebugInfo(Compiler, &Name);
    CompileCallWithoutReturnValue(Compiler, Location, &Name);
    FreeExpr(Compiler, *Location);
    CompilerEmitDebugInfo(Compiler, &Name);
}


static void CompilerEmitAssignment(PascalCompiler *Compiler, const Token *Assignment, 
        VarLocation *Left, const VarLocation *Right)
{
    PASCAL_NONNULL(Compiler);
    PASCAL_NONNULL(Assignment);
    PASCAL_NONNULL(Left);
    PASCAL_NONNULL(Right);

    if (TOKEN_COLON_EQUAL == Assignment->Type)
    {
        PVMEmitMove(EMITTER(), Left, Right);
        return;
    }

    VarLocation Dst;
    bool OwningLeft = PVMEmitIntoRegLocation(EMITTER(), &Dst, true, Left);
    switch (Assignment->Type)
    {
    case TOKEN_PLUS_EQUAL:  PVMEmitAdd(EMITTER(), &Dst, Right); break;
    case TOKEN_MINUS_EQUAL: PVMEmitSub(EMITTER(), &Dst, Right); break;
    case TOKEN_STAR_EQUAL:  PVMEmitMul(EMITTER(), &Dst, Right); break;
    case TOKEN_SLASH_EQUAL: PVMEmitDiv(EMITTER(), &Dst, Right); break;
    case TOKEN_PERCENT_EQUAL: 
    {
        if (IntegralTypeIsInteger(Dst.Type.Integral) 
        && IntegralTypeIsInteger(Right->Type.Integral))
        {
            PVMEmitMod(EMITTER(), &Dst, Right);
        }
        else 
        {
            ErrorAt(Compiler, Assignment, "Cannot perform modulo between %s and %s.",
                    VarTypeToStr(Left->Type), VarTypeToStr(Right->Type)
            );
        }
    } break;
    default: 
    {
        ErrorAt(Compiler, Assignment, "Expected ':=' or other assignment operator.");
    } break;
    }

    if (OwningLeft)
    {
        PVMEmitMove(EMITTER(), Left, &Dst);
        FreeExpr(Compiler, Dst);
    }
}


static void CompileAssignStmt(PascalCompiler *Compiler, const Token Identifier)
{
    PASCAL_NONNULL(Compiler);
    PASCAL_ASSERT(NULL == Compiler->Lhs, "unreachable");

    CompilerInitDebugInfo(Compiler, &Identifier);

    VarLocation Dst = CompileVariableExpr(Compiler);
    ConsumeToken(Compiler);
    const Token Assignment = Compiler->Curr;
    Compiler->Lhs = &Dst;
    VarLocation Right = CompileExpr(Compiler);


    if (TYPE_RECORD == Dst.Type.Integral)
    {
        if (TOKEN_COLON_EQUAL != Assignment.Type)
        {
            ErrorAt(Compiler, &Assignment, "Expected ':=' instead.");
        }
        if (!VarTypeEqual(&Dst.Type, &Right.Type))
        {
            ErrorAt(Compiler, &Assignment, "Cannot assign %s to %s.",
                    VarTypeToStr(Right.Type), VarTypeToStr(Dst.Type)
            );
        }
        /* CompileExpr handled record return value already */
    }
    else
    {
        if (!ConvertTypeImplicitly(Compiler, Dst.Type.Integral, &Right))
        {
            ErrorAt(Compiler, &Assignment, "Cannot assign expression of type %s to %s.", 
                    VarTypeToStr(Right.Type), VarTypeToStr(Dst.Type)
            );
        }
        if (!Compiler->Panic)
        {
            CompilerEmitAssignment(Compiler, &Assignment, &Dst, &Right);
        }
    }
    FreeExpr(Compiler, Right);
    CompilerEmitDebugInfo(Compiler, &Identifier);

    /* invalidate lhs */
    Compiler->Lhs = NULL;
}


static void CompileIdenStmt(PascalCompiler *Compiler)
{
    PASCAL_NONNULL(Compiler);
    /* iden consumed */
    PascalVar *IdentifierInfo = GetIdenInfo(Compiler, &Compiler->Curr,
            "Undefined identifier."
    );
    if (NULL == IdentifierInfo)
        return;
    
    if (TYPE_FUNCTION == IdentifierInfo->Type.Integral)
    {
        Token Callee = Compiler->Curr;
        /* Pascal's weird return statement */
        if (IdentifierInfo->Str.Len == Callee.Lexeme.Len
        && TokenEqualNoCase(IdentifierInfo->Str.Str, Callee.Lexeme.Str, Callee.Lexeme.Len) 
        && ConsumeIfNextTokenIs(Compiler, TOKEN_COLON_EQUAL))
        {
            PASCAL_UNREACHABLE("TODO: return by assigning to function name");
        }

        PASCAL_NONNULL(IdentifierInfo->Location);
        const VarLocation *Location = IdentifierInfo->Location;
        if (VAR_BUILTIN == Location->LocationType)
        {
            CompileCallToBuiltin(Compiler, Location->As.BuiltinSubroutine);
        }
        else
        {
            CompileCallStmt(Compiler, Callee, IdentifierInfo->Location);
        }
    }
    else
    {
        Token Identifier = Compiler->Curr;
        CompileAssignStmt(Compiler, Identifier);
    }
}


static void CompileCaseStmt(PascalCompiler *Compiler)
{
    PASCAL_NONNULL(Compiler);
    /* case consumed */
    Token Keyword = Compiler->Curr;

    /* expression */
    CompilerInitDebugInfo(Compiler, &Keyword);
    VarLocation Expr = CompileExpr(Compiler);
    ConsumeOrError(Compiler, TOKEN_OF, "Expected 'of' after expression.");
    CompilerEmitDebugInfo(Compiler, &Keyword);


    bool Last = EMITTER()->ShouldEmit;
    bool ExprIsConstant = VAR_LIT == Expr.LocationType;
    bool Emitted = false;

    U32 OutBranch[256];
    U32 CaseCount = 0;
    if (!NextTokenIs(Compiler, TOKEN_END) && !NextTokenIs(Compiler, TOKEN_ELSE))
    {
        do {
            Token CaseConstant = Compiler->Next;
            CompilerInitDebugInfo(Compiler, &CaseConstant);

            /* don't need to free the expr here since 
             * it is only evaluated at compile time and does not use any register */
            VarLocation Constant = CompileExpr(Compiler);
            if (VAR_LIT != Constant.LocationType) 
            {
                /* TODO: expression highlighter */
                Error(Compiler, "Case expression cannot be evaluated at compile time.");
            }
            if (!ConvertTypeImplicitly(Compiler, Expr.Type.Integral, &Constant))
            {
                Error(Compiler, "Cannot convert from %s to %s in case expression.", 
                        VarTypeToStr(Constant.Type), VarTypeToStr(Expr.Type)
                );
                Expr.Type = Constant.Type;
            }
 
            if (ExprIsConstant)
                EMITTER()->ShouldEmit = false;

            ConsumeOrError(Compiler, TOKEN_COLON, "Expected ':' after case entry.");
            PVMEmitSetFlag(EMITTER(), TOKEN_EQUAL, &Expr, &Constant);
            U32 NextCase = PVMEmitBranchOnFalseFlag(EMITTER());
            CompilerEmitDebugInfo(Compiler, &CaseConstant);           

            if (ExprIsConstant)
            {
                bool LiteralEqu = LiteralEqual(Expr.As.Literal, Constant.As.Literal, Expr.Type.Integral);
                EMITTER()->ShouldEmit = LiteralEqu;
                if (LiteralEqu)
                    Emitted = true;
            }

            CompileStmt(Compiler);
            if (ExprIsConstant)
                EMITTER()->ShouldEmit = false;


            /* end of statement */
            OutBranch[CaseCount++] = PVMEmitBranch(EMITTER(), 0);
            PVMPatchBranchToCurrent(EMITTER(), NextCase);
            EMITTER()->ShouldEmit = Last;

            if (NextTokenIs(Compiler, TOKEN_END) || NextTokenIs(Compiler, TOKEN_ELSE))
                break;
            ConsumeOrError(Compiler, TOKEN_SEMICOLON, "Expected ';' between case statements.");
        } while (!IsAtEnd(Compiler) && !NextTokenIs(Compiler, TOKEN_END) && !NextTokenIs(Compiler, TOKEN_ELSE));
    }

    if (ConsumeIfNextTokenIs(Compiler, TOKEN_ELSE))
    {
        if (ExprIsConstant && Emitted)
            EMITTER()->ShouldEmit = false;

        /* for 'else' keyword */
        CompilerInitDebugInfo(Compiler, &Compiler->Curr);
        CompilerEmitDebugInfo(Compiler, &Compiler->Curr);
        do {
            CompileStmt(Compiler);
            if (NextTokenIs(Compiler, TOKEN_END))
                break;
            ConsumeOrError(Compiler, TOKEN_SEMICOLON, "Expected ';' between statement.");
        } while (!IsAtEnd(Compiler) && !NextTokenIs(Compiler, TOKEN_END));
    }

    if (!ExprIsConstant)
    {
        for (U32 i = 0; i < CaseCount; i++)
            PVMPatchBranchToCurrent(EMITTER(), OutBranch[i]);
    }

    ConsumeOrError(Compiler, TOKEN_END, "Expected 'end' after statement.");
    FreeExpr(Compiler, Expr);
    EMITTER()->ShouldEmit = Last;
}


static void CompileBreakStmt(PascalCompiler *Compiler)
{
    /* break; */
    /* break consumed */
    const Token *Keyword = &Compiler->Curr;

    if (!Compiler->InLoop)
    {
        ErrorAt(Compiler, &Compiler->Curr, "Break statment is only allowed in loops.");
    }
    if (Compiler->BreakCount >= STATIC_ARRAY_SIZE(Compiler->Breaks))
    {
        PASCAL_UNREACHABLE("TODO: dynamic array of breaks.");
    }

    CompilerInitDebugInfo(Compiler, Keyword);
    Compiler->Breaks[Compiler->BreakCount++] = PVMEmitBranch(EMITTER(), 0);
    CompilerEmitDebugInfo(Compiler, Keyword);
}

static void CompileStmt(PascalCompiler *Compiler)
{
    PASCAL_NONNULL(Compiler);
    switch (Compiler->Next.Type)
    {
    case TOKEN_GOTO:
    {
        ConsumeToken(Compiler);
        PASCAL_UNREACHABLE("TODO: goto");
    } break;
    case TOKEN_WITH:
    {
        ConsumeToken(Compiler);
        PASCAL_UNREACHABLE("TODO: with");
    } break;
    case TOKEN_BREAK:
    {
        ConsumeToken(Compiler);
        CompileBreakStmt(Compiler);
    } break;
    case TOKEN_FOR:
    {
        ConsumeToken(Compiler);
        CompileForStmt(Compiler);
    } break;
    case TOKEN_REPEAT:
    {
        ConsumeToken(Compiler);
        CompileRepeatUntilStmt(Compiler);
    } break;
    case TOKEN_WHILE:
    {
        ConsumeToken(Compiler);
        CompileWhileStmt(Compiler);
    } break;
    case TOKEN_CASE:
    {
        ConsumeToken(Compiler);
        CompileCaseStmt(Compiler);
    } break;
    case TOKEN_IF:
    {
        ConsumeToken(Compiler);
        CompileIfStmt(Compiler);
    } break;
    case TOKEN_BEGIN:
    {
        ConsumeToken(Compiler);
        CompileBeginStmt(Compiler);
    } break;
    /* TODO: exit should not be a keyword */
    case TOKEN_EXIT:
    {
        ConsumeToken(Compiler);
        CompileExitStmt(Compiler);
    } break;
    case TOKEN_END:
    case TOKEN_UNTIL:
    case TOKEN_SEMICOLON:
    {
        /* null statement */
    } break;
    case TOKEN_EOF:
    {
        if (PASCAL_COMPMODE_REPL != Compiler->Flags.CompMode)
        {
            Error(Compiler, "Unexpected end of file.");
        }
    } break;
    default:
    {
        ConsumeToken(Compiler);
        CompileIdenStmt(Compiler);
    } break;
            /* for good error message */
    case TOKEN_ELSE:
    {
        if (TOKEN_SEMICOLON == Compiler->Curr.Type)
        {
            ErrorAt(Compiler, &Compiler->Curr, "Semicolon is not allowed before 'else'.");
        }
        else
        {
            Error(Compiler, "Unexpected token.");
        }
    } break;
    }

    if (Compiler->Panic)
    {
        CalmDownDog(Compiler);
    }
}







/*===============================================================================*/
/*
 *                                   BLOCKS
 */
/*===============================================================================*/


static bool CompileBlock(PascalCompiler *Compiler);

static void CompileBeginBlock(PascalCompiler *Compiler)
{
    if (IsAtGlobalScope(Compiler))
        Compiler->EntryPoint = PVMGetCurrentLocation(EMITTER());
    CompileBeginStmt(Compiler);
}


typedef struct SubroutineInformation 
{
    PascalVar Name;
    Token NameToken;
    bool FirstDeclaration;

    /* these pointers are owned by VarLocation */
    SubroutineData *Info;
    U32 *Location;
    VarType *Type;
} SubroutineInformation;

static SubroutineInformation ConsumeSubroutineName(PascalCompiler *Compiler, 
        bool IsFunction, const char *SubroutineType)
{
    PASCAL_NONNULL(Compiler);
    PASCAL_NONNULL(SubroutineType);

    ConsumeOrError(Compiler, TOKEN_IDENTIFIER, "Expected %s name.", SubroutineType);
    const Token *Name = &Compiler->Curr;
    PascalVar *Identifier = FindIdentifier(Compiler, Name);

    /* brand new name? */
    if (NULL == Identifier) 
    {
        VarLocation *Location = CompilerAllocateVarLocation(Compiler);
        PASCAL_NONNULL(Location);
        Location->Type = VarTypeInit(TYPE_FUNCTION, sizeof(void*));
        Location->As.SubroutineLocation = SUBROUTINE_INVALID_LOCATION;
        Location->LocationType = VAR_SUBROUTINE;

        Identifier = DefineIdentifier(Compiler, Name, Location->Type, Location);
        PASCAL_NONNULL(Identifier);

        return (SubroutineInformation) {
            .Info = &Location->Type.As.Subroutine,
            .Type = &Location->Type,
            .Location = &Location->As.SubroutineLocation,
            .Name = *Identifier,
            .NameToken = *Name,
            .FirstDeclaration = true,
        };
    }

    PASCAL_NONNULL(Identifier->Location);
    SubroutineInformation Subroutine = { 
        .Info = &Identifier->Location->Type.As.Subroutine,
        .Type = &Identifier->Location->Type,
        .Location = &Identifier->Location->As.SubroutineLocation,
        .Name = *Identifier,
        .NameToken = *Name,
        .FirstDeclaration = false,
    };

    /* name is already defined */
    /* redefinition in repl? */
    if (PASCAL_COMPMODE_REPL == Compiler->Flags.CompMode)
        return Subroutine; /* redefinition is fine in repl */

    /* not a function? */
    if (TYPE_FUNCTION != Identifier->Type.Integral)
    {

        ErrorAt(Compiler, &Subroutine.NameToken, "Redefinition of '"STRVIEW_FMT"' from %s to %s",
                STRVIEW_FMT_ARG(&Name->Lexeme), 
                VarTypeToStr(Identifier->Type), IsFunction? "function" : "procedure"
        );
        return Subroutine;
    }


    /* is subroutined DEFINED? */
    if (SUBROUTINE_INVALID_LOCATION != *Subroutine.Location)
    {
        PASCAL_UNREACHABLE("TODO: function overload");
    }
    /* mismatch between function and subroutine */
    if (IsFunction != (NULL == Subroutine.Info->ReturnType))
    {
        ErrorAt(Compiler, &Subroutine.NameToken, 
                "Definition of %s '"STRVIEW_FMT"' does not match declaration on line %d.",
                SubroutineType, STRVIEW_FMT_ARG(&Subroutine.NameToken.Lexeme), 
                Identifier->Line
        );
    }
    return Subroutine;
}



static U32 CompileLocalParameter(PascalCompiler *Compiler, 
        SubroutineData *Subroutine)
{
    PASCAL_NONNULL(Compiler);
    PASCAL_NONNULL(Subroutine);
    PASCAL_ASSERT(CALLCONV_MSX64 == Compiler->Flags.CallConv, "TODO: other calling convention");

    U32 Location = PVMEmitEnter(EMITTER());
    U32 StackSize = 0;
    U32 HiddenParamCount = Subroutine->HiddenParamCount;
    SubroutineParameterList *ParameterList = &Subroutine->ParameterList;

    /* hidden param */
    if (NULL != Subroutine->ReturnType && !VarTypeIsTriviallyCopiable(*Subroutine->ReturnType))
    {
        PVMMarkRegisterAsAllocated(EMITTER(), 0);
    }

    if (ParameterList->Count)
    {
        PascalVar *Params = ParameterList->Params;
        PASCAL_NONNULL(Params);
        U32 ArgOffset = 0;

        for (UInt i = 0; i < ParameterList->Count; i++)
        {
            PASCAL_NONNULL(ParameterList->Params[i].Location);
            if (i + HiddenParamCount < PVM_ARGREG_COUNT)
            {
                I32 Dummy = 0;
                *Params[i].Location = PVMCreateStackLocation(EMITTER(), 
                        Params[i].Type, StackSize
                );
                StackSize += uRoundUpToMultipleOfPow2(Params[i].Type.Size, PVM_STACK_ALIGNMENT);
                VarLocation Arg = PVMSetArg(EMITTER(), i + HiddenParamCount, Params[i].Type, &Dummy);
                PVMMarkArgAsOccupied(EMITTER(), &Arg);

                if (VarTypeIsTriviallyCopiable(Params[i].Type))
                {
                    PVMEmitMove(EMITTER(), Params[i].Location, &Arg);
                }
                else
                {
                    PVMEmitCopy(EMITTER(), Params[i].Location, &Arg);
                }
                FreeExpr(Compiler, Arg);
            }
            else
            {
                ArgOffset -= Params[i].Type.Size;
                *Params[i].Location = PVMCreateStackLocation(EMITTER(), 
                        Params[i].Type, ArgOffset
                );
            }
        }
    }
    Compiler->StackSize += StackSize;
    return Location;
}





static SubroutineInformation CompileSubroutineDeclaration(PascalCompiler *Compiler, 
        bool IsFunction, const char *SubroutineType)
{
    /* consumes name */
    SubroutineInformation Subroutine = ConsumeSubroutineName(Compiler, 
            IsFunction, SubroutineType
    );

    /* consumes parameter list */
    PascalVartab Scope = VartabInit(&Compiler->InternalAlloc, PVM_INITIAL_VAR_PER_SCOPE);
    SubroutineParameterList ParameterList = CompileParameterListWithParentheses(Compiler, &Scope);
    if (!Subroutine.FirstDeclaration)
    {
        PASCAL_UNREACHABLE("TODO: compare parameter list");
    }

    /* consumes return type */
    const char *BeforeSemicolon;
    const VarType *ReturnType = NULL;
    if (IsFunction)
    {
        BeforeSemicolon = "type name";
        ConsumeOrError(Compiler, TOKEN_COLON, "Expected ':' after parameter list.");
        ConsumeOrError(Compiler, TOKEN_IDENTIFIER, "Expected return type name.");
        PascalVar *ReturnTypeName = GetIdenInfo(Compiler, &Compiler->Curr, "Undefined return type.");
        if (NULL != ReturnTypeName)
        {
            ReturnType = CompilerCopyType(Compiler, ReturnTypeName->Type);
        }
    }
    else
    {
        BeforeSemicolon = "parameter list";
        /* for good error message */
        if (ConsumeIfNextTokenIs(Compiler, TOKEN_COLON))
        {
            if (ConsumeIfNextTokenIs(Compiler, TOKEN_IDENTIFIER))
            {
                Error(Compiler, "Procedure cannot have a return type.");
            }
        }
    }
    ConsumeOrError(Compiler, TOKEN_SEMICOLON, "Expected ';' after %s.", BeforeSemicolon);

    /* sets parameter list for caller,
     * callee's local copy of parameter list 
     * will only be set up during compilation of subroutine's body */
    U32 CalleeStackArgSize = 0;
    for (UInt i = PVM_ARGREG_COUNT; i < ParameterList.Count; i++)
    {
        VarLocation *Parameter = ParameterList.Params[i].Location;
        PASCAL_NONNULL(Parameter);
        CalleeStackArgSize += Parameter->Type.Size;
    }
    *Subroutine.Type = VarTypeSubroutine(ParameterList, Scope, ReturnType, CalleeStackArgSize);
    return Subroutine;
}


static void CompileSubroutineBlock(PascalCompiler *Compiler, const char *SubroutineType)
{
    PASCAL_NONNULL(Compiler);
    PASCAL_NONNULL(SubroutineType);

    /* function/procedure consumed */
    Token Keyword = Compiler->Curr;
    SaveRegInfo PrevScope = PVMEmitterBeginScope(EMITTER());


    /* debug information for function signature */
    CompilerInitDebugInfo(Compiler, &Keyword);
    SubroutineInformation Subroutine = CompileSubroutineDeclaration(Compiler, 
            TOKEN_FUNCTION == Keyword.Type, SubroutineType
    );
    CompilerEmitDebugInfo(Compiler, &Keyword);


    /* forward decl */
    if (ConsumeIfNextTokenIs(Compiler, TOKEN_FORWARD))
    {
        ConsumeOrError(Compiler, TOKEN_SEMICOLON, "Expected ';' after 'forward'.");
    }
    else /* body */
    {
        CompilerPushSubroutine(Compiler, Subroutine.Info);


        /* block */
        U32 PrevStackSize = Compiler->StackSize;
        *Subroutine.Location = CompileLocalParameter(Compiler, Subroutine.Info);
        CompileBlock(Compiler);
        PVMPatchEnter(EMITTER(), *Subroutine.Location, Compiler->StackSize + Compiler->TemporarySize);
        Compiler->StackSize = PrevStackSize;


        /* exit */
        Token End = Compiler->Curr;
        CompilerInitDebugInfo(Compiler, &End);
        PVMEmitExit(EMITTER());
        ConsumeOrError(Compiler, TOKEN_SEMICOLON, "Expected ';' after %s block.", SubroutineType);
        CompilerEmitDebugInfo(Compiler, &End);

        CompilerPopSubroutine(Compiler);
    }
    PVMEmitterEndScope(EMITTER(), PrevScope);
}


static void CompileVarBlock(PascalCompiler *Compiler)
{
    /* 
     * var
     *      id1, id2: typename;
     *      id3: typename;
     */
    if (!NextTokenIs(Compiler, TOKEN_IDENTIFIER))
    {
        Error(Compiler, "Expected variable name.");
        return;
    }

    U32 BaseRegister = PVM_REG_FP;
    U32 BaseAddr = Compiler->StackSize;
    UInt Alignment = PVM_STACK_ALIGNMENT;
    bool AtGlobalScope = IsAtGlobalScope(Compiler);
    if (AtGlobalScope)
    {
        BaseRegister = PVM_REG_GP;
        BaseAddr = PVMGetGlobalOffset(EMITTER());
        Alignment = PVM_GLOBAL_ALIGNMENT;
    }
    U32 TotalSize = 0;

    while (!IsAtEnd(Compiler) && NextTokenIs(Compiler, TOKEN_IDENTIFIER))
    {
        Token VariableName = Compiler->Next;
        U32 Next = CompileVarList(Compiler, BaseRegister, BaseAddr + TotalSize, Alignment);
        if (Next == TotalSize)
        {
            /* Error encountered */
            return;
        }
        TotalSize = Next - BaseAddr;

        /* initialization */
        if (ConsumeIfNextTokenIs(Compiler, TOKEN_EQUAL))
        {
            PascalVar *Variable = FindIdentifier(Compiler, &VariableName);
            PASCAL_NONNULL(Variable);
            Token EqualSign = Compiler->Curr;
            if (CompilerGetTmpCount(Compiler) > 1)
            {
                ErrorAt(Compiler, &EqualSign, "Only one variable can be initialized at a time.");
            }
            PASCAL_NONNULL(Variable->Location);

            if (AtGlobalScope)
            {
                VarLocation Constant = CompileExpr(Compiler);
                if (VAR_LIT != Constant.LocationType)
                {
                    ErrorAt(Compiler, &EqualSign, 
                            "Can only initialize global variable with constant expression."
                    );
                }
                if (TYPE_INVALID == CoerceTypes(Variable->Type.Integral, Constant.Type.Integral))
                {
                    ErrorAt(Compiler, &EqualSign, "Invalid type combination: %s and %s", 
                            VarTypeToStr(Variable->Type), VarTypeToStr(Constant.Type)
                    );
                    continue;
                }

                PASCAL_ASSERT(ConvertTypeImplicitly(Compiler, Variable->Type.Integral, &Constant), 
                        "Unreachable"
                );

                /* flushes global variable so initialization can be done */
                PVMEmitGlobalAllocation(EMITTER(), TotalSize);
                BaseAddr += TotalSize;
                TotalSize = 0;
                PVMInitializeGlobal(EMITTER(), 
                        Variable->Location, 
                        &Constant.As.Literal, Constant.Type.Size, Constant.Type.Integral
                );
            }
            else /* stack variable */
            {
                CompileExprInto(Compiler, &EqualSign, Variable->Location);
            }
            ConsumeOrError(Compiler, TOKEN_SEMICOLON, "Expected ';' after expression.");
        }
        else
        {
            ConsumeOrError(Compiler, TOKEN_SEMICOLON, "Expected ';' after variable declaration.");
        }

        if (ConsumeIfNextTokenIs(Compiler, TOKEN_VAR) && !NextTokenIs(Compiler, TOKEN_IDENTIFIER))
        {
            Error(Compiler, "Expected variable name.");
            return;
        }
    }

    if (AtGlobalScope)
    {
        PVMEmitGlobalAllocation(EMITTER(), TotalSize);
    }
    else
    {
        Compiler->StackSize += TotalSize;
    }
}





static void CompileTypeBlock(PascalCompiler *Compiler)
{
    /*
     * type
     *  id = id;
     *  id = record ... end;
     *  id = array of record ... end;
     *       array of id;
     */

    if (!NextTokenIs(Compiler, TOKEN_IDENTIFIER))
    {
        Error(Compiler, "Type block must have at least 1 definition.");
        return;
    }

    while (ConsumeIfNextTokenIs(Compiler, TOKEN_IDENTIFIER))
    {
        Token Identifier = Compiler->Curr;
        ConsumeOrError(Compiler, TOKEN_EQUAL, "Expected '=' after identifier.");
        ParseAndDefineTypename(Compiler, &Identifier);
        ConsumeOrError(Compiler, TOKEN_SEMICOLON, "Expected ';' after type definition.");
    }
}


static void CompileConstBlock(PascalCompiler *Compiler)
{
    /* 
     * const
     *  id = constexpr;
     *  id: typename = constexpr;
     */
    /* 'const' consumed */
    if (!NextTokenIs(Compiler, TOKEN_IDENTIFIER))
    {
        Error(Compiler, "Const block must have at least 1 constant.");
        return;
    }

    while (ConsumeIfNextTokenIs(Compiler, TOKEN_IDENTIFIER))
    {
        Token Identifier = Compiler->Curr;
        if (ConsumeIfNextTokenIs(Compiler, TOKEN_COLON))
        {
            PASCAL_UNREACHABLE("TODO: type of const ");
        }

        ConsumeOrError(Compiler, TOKEN_EQUAL, "Expected '='.");
        Token EquSign = Compiler->Curr;

        VarLocation *Literal = CompilerAllocateVarLocation(Compiler);
        *Literal = CompileExpr(Compiler);
        if (VAR_LIT != Literal->LocationType)
        {
            ErrorAt(Compiler, &EquSign, 
                    "Expression on the right of '=' is not a compile-time constant."
            );
        }
        DefineIdentifier(Compiler, &Identifier, Literal->Type, Literal);
        ConsumeOrError(Compiler, TOKEN_SEMICOLON, "Expected ';' after constant.");
    }
}





/* returns true if Begin is encountered */
static bool CompileHeadlessBlock(PascalCompiler *Compiler)
{
    while (!NoMoreToken(Compiler) && !NextTokenIs(Compiler, TOKEN_BEGIN))
    {
        switch (Compiler->Next.Type)
        {
        case TOKEN_BEGIN: 
        {
            ConsumeToken(Compiler); 
            return true;
        }
        case TOKEN_FUNCTION:
        {
            ConsumeToken(Compiler);
            CompileSubroutineBlock(Compiler, "function");
        } break;
        case TOKEN_PROCEDURE:
        {
            ConsumeToken(Compiler);
            CompileSubroutineBlock(Compiler, "procedure");
        } break;
        case TOKEN_VAR:
        {
            ConsumeToken(Compiler);
            CompileVarBlock(Compiler);
        } break;
        case TOKEN_TYPE:
        {
            ConsumeToken(Compiler);
            CompileTypeBlock(Compiler);
        } break;
        case TOKEN_CONST:
        {
            ConsumeToken(Compiler);
            CompileConstBlock(Compiler);
        } break;
        case TOKEN_LABEL:
        {
            ConsumeToken(Compiler);
            PASCAL_UNREACHABLE("TODO: label");
        } break;
        default: 
        {
            if (PASCAL_COMPMODE_REPL != Compiler->Flags.CompMode)
            {
                Error(Compiler, "A block cannot start with '"STRVIEW_FMT"'.", 
                        STRVIEW_FMT_ARG(&Compiler->Next.Lexeme)
                );
            }
            return false;
        } break;
        }

        if (Compiler->Panic)
        {
            CalmDownAtBlock(Compiler);
        }
    }
    return ConsumeIfNextTokenIs(Compiler, TOKEN_BEGIN);
}



static bool CompileBlock(PascalCompiler *Compiler)
{
    if (!CompileHeadlessBlock(Compiler))
    {
        Error(Compiler, "Expected 'Begin'.");
        return false;
    }
    else
    {
        CompileBeginBlock(Compiler);
        return !Compiler->Error;
    }
}




static bool CompileProgram(PascalCompiler *Compiler)
{
    /* 'program' consumed */

    /* program iden; */
    ConsumeOrError(Compiler, TOKEN_IDENTIFIER, "Expected identifier.");
    if (ConsumeIfNextTokenIs(Compiler, TOKEN_LEFT_PAREN))
    {
        /* TODO: what are these idens for:
         * program hello(identifier1, iden2, hi);
         *               ^^^^^^^^^^^  ^^^^^  ^^
         */
        do {
            ConsumeOrError(Compiler, TOKEN_IDENTIFIER, "Expected identifier.");
        } while (ConsumeIfNextTokenIs(Compiler, TOKEN_COMMA));
        ConsumeOrError(Compiler, TOKEN_RIGHT_PAREN, "Expected ')' instead");
    }
    ConsumeOrError(Compiler, TOKEN_SEMICOLON, "Expected ';' instead.");


    /* uses iden, iden2; */
    if (ConsumeIfNextTokenIs(Compiler, TOKEN_USES))
    {
        do {
            ConsumeOrError(Compiler, TOKEN_IDENTIFIER, "Expected identifier");
            Token Unit = Compiler->Curr;

            /* TODO: other libraries */
            /* including crt */
            if (Unit.Lexeme.Len == Compiler->Builtins.Crt.Len
            && TokenEqualNoCase(Unit.Lexeme.Str, Compiler->Builtins.Crt.Str, Unit.Lexeme.Len))
            {
                DefineCrtSubroutines(Compiler);
            }
        } while (ConsumeIfNextTokenIs(Compiler, TOKEN_COMMA));
    }


    CompileBlock(Compiler);
    return !Compiler->Error && ConsumeOrError(Compiler, TOKEN_DOT, "Expected '.' instead.");
}











PascalCompiler PascalCompilerInit(
        PascalCompileFlags Flags, const PascalVartab *PredefinedIdentifiers, 
        FILE *LogFile, PVMChunk *OutChunk)
{
    PascalCompiler Compiler = {
        .Flags = Flags,
        .Builtins = {
            .Crt = STRVIEW_INIT_CSTR("crt", 3),
            .System = STRVIEW_INIT_CSTR("system", 6),
        },
        .InternalAlloc = GPAInit(4 * 1024 * 1024),
        .InternalArena = ArenaInit(512*sizeof(VarLocation), 2),
        .Locals = { NULL },
        .Scope = 0,
        .StackSize = 0,

        .Idens = { .Cap = STATIC_ARRAY_SIZE(Compiler.Idens.Array) },
        .Subroutine = { 0 },

        .Lhs = NULL,
        .Breaks = { 0 },
        .BreakCount = 0,
        .InLoop = false,

        .SubroutineReferences = { 0 },
        .EntryPoint = 0,

        .Error = false,
        .LogFile = LogFile,
        .Panic = false,

        .ReplCallback = NULL,
        .ReplUserData = NULL,

        .Line = 1,
    };

    Compiler.Emitter = PVMEmitterInit(OutChunk);
    Compiler.InternalAlloc.CoalesceOnFree = true;
    if (NULL == PredefinedIdentifiers)
    {
        Compiler.Global = VartabPredefinedIdentifiers(&Compiler.InternalAlloc, 1024);
    }
    else
    {
        Compiler.Global = VartabClone(&Compiler.InternalAlloc, PredefinedIdentifiers);
    }
    DefineSystemSubroutines(&Compiler);
    return Compiler;
}

void PascalCompilerDeinit(PascalCompiler *Compiler)
{
    GPADeinit(&Compiler->InternalAlloc);
    ArenaDeinit(&Compiler->InternalArena);
    PVMEmitterDeinit(EMITTER());
}

void PascalCompilerReset(PascalCompiler *Compiler, bool PreserveFunctions)
{
    Compiler->SubroutineReferences.Count = 0;
    Compiler->Curr = (Token) { 0 };
    Compiler->Next = (Token) { 0 };
    Compiler->Panic = false;
    Compiler->Error = false;
    Compiler->StackSize = 0;
    Compiler->BreakCount = 0;
    Compiler->InLoop = false;
    Compiler->Line++;
    memset(Compiler->Locals, 0, sizeof Compiler->Locals);
    PVMEmitterReset(EMITTER(), PreserveFunctions);
}



bool PascalCompileRepl(
        PascalCompiler *Compiler, const U8 *Line,
        PascalReplLineCallbackFn Callback, void *Data)
{
    Compiler->Lexer = TokenizerInit(Line, Compiler->Line);
    ConsumeToken(Compiler);

    Compiler->ReplCallback = Callback;
    Compiler->ReplUserData = Data;

    U32 LastEntry = Compiler->EntryPoint;
    CompileBlock(Compiler);
    ConsumeOrError(Compiler, TOKEN_DOT, "Expected '.' at end.");

    if (Compiler->Error)
    {
        Compiler->EntryPoint = LastEntry;
        return false;
    }
    ResolveSubroutineReferences(Compiler);
    PVMSetEntryPoint(EMITTER(), Compiler->EntryPoint);
    PVMEmitExit(EMITTER());
    return !Compiler->Error;
}


bool PascalCompileProgram(PascalCompiler *Compiler, const U8 *Source)
{
    Compiler->Lexer = TokenizerInit(Source, 1);
    ConsumeToken(Compiler);
    bool NoError = false;
    if (ConsumeIfNextTokenIs(Compiler, TOKEN_PROGRAM))
    {
        NoError = CompileProgram(Compiler);
        ResolveSubroutineReferences(Compiler);
        PVMSetEntryPoint(EMITTER(), Compiler->EntryPoint);
        PVMEmitExit(EMITTER());
        return NoError;
    }
    else if (ConsumeIfNextTokenIs(Compiler, TOKEN_UNIT))
    {
        PASCAL_UNREACHABLE("TODO: Unit");
    }
    else
    {
        Error(Compiler, "Expected 'program' or 'unit' at the start of file.");
    }
    return NoError;
}




