

#include <stdarg.h> /* duh */
#include <inttypes.h> /* PRI* */


#include "Memory.h"
#include "Tokenizer.h"
#include "Vartab.h"
#include "Variable.h"
#include "IntegralTypes.h"

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



static void CalmDownDog(PVMCompiler *Compiler)
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


static void CalmDownAtBlock(PVMCompiler *Compiler)
{
    Compiler->Panic = false;
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


static void CompileStmt(PVMCompiler *Compiler);


static void CompileExitStmt(PVMCompiler *Compiler)
{
    /* TODO: refactor this piece of shit of a function */
    PASCAL_NONNULL(Compiler);
    /* 'exit' consumed */
    Token Keyword = Compiler->Curr;
    CompilerInitDebugInfo(Compiler, &Keyword);


    /* Exit */
    if (ConsumeIfNextIs(Compiler, TOKEN_LEFT_PAREN))
    {

        if (IsAtGlobalScope(Compiler))
        {
            /* Exit(expr) */
            if (!ConsumeIfNextIs(Compiler, TOKEN_RIGHT_PAREN))
            {
                VarLocation ReturnValue = PVMSetReturnType(EMITTER(), VarTypeInit(TYPE_I32, sizeof(I32)));
                CompileExprInto(Compiler, &Keyword, &ReturnValue);
            }
        }
        else
        {
            const SubroutineData *Subroutine = Compiler->Subroutine[Compiler->Scope - 1].Current;
            if (NULL != Subroutine->ReturnType)
            {
                PASCAL_NONNULL(Subroutine);
                PASCAL_NONNULL(Subroutine->ReturnType);

                /* Exit(expr), must have return value */
                if (NextTokenIs(Compiler, TOKEN_RIGHT_PAREN))
                {
                    ErrorAt(Compiler, &Keyword, "Function must return a value.");
                    goto Done;
                }

                VarLocation ReturnValue = PVMSetReturnType(EMITTER(), 
                        *Subroutine->ReturnType
                );
                if (TYPE_RECORD == ReturnValue.Type.Integral)
                {
                    VarLocation RecordAddr = CompileExpr(Compiler);
                    PASCAL_ASSERT(RecordAddr.LocationType == VAR_MEM, "Record ret");
                    PVMEmitCopy(EMITTER(), &ReturnValue, &RecordAddr);
                }
                else 
                {
                    CompileExprInto(Compiler, &Keyword, &ReturnValue);
                }
            }
            else if (!NextTokenIs(Compiler, TOKEN_RIGHT_PAREN))
            {
                /* Exit(), no return type */
                ErrorAt(Compiler, &Keyword, "Procedure cannot return a value.");
            }
        }

        /* else Exit(), no return type */
        ConsumeOrError(Compiler, TOKEN_RIGHT_PAREN, "Expected ')' after expression.");
    }
Done:
    PVMEmitExit(EMITTER());
    CompilerEmitDebugInfo(Compiler, &Keyword);
}



static void CompileBeginStmt(PVMCompiler *Compiler)
{
    /* begin consumed */
    /* 
     * this complex-looking loop is necessary bc 
     * Pascal allows the last stmt of a begin-end block to not have a semicolon 
     */
    while (!IsAtEnd(Compiler) && !NextTokenIs(Compiler, TOKEN_END))
    {
        CompileStmt(Compiler);
        if (ConsumeIfNextIs(Compiler, TOKEN_END))
            return;
        ConsumeOrError(Compiler, TOKEN_SEMICOLON, "Expected ';' between statements.");
    }
    ConsumeOrError(Compiler, TOKEN_END, "Expected 'end'.");
}



static UInt CompileLoopBody(PVMCompiler *Compiler)
{
    UInt PrevBreakCount = Compiler->BreakCount;
    bool WasInLoop = Compiler->InLoop;
    Compiler->InLoop = true;

    CompileStmt(Compiler);

    Compiler->InLoop = WasInLoop;
    return PrevBreakCount;
}

static void CompilerPatchBreaks(PVMCompiler *Compiler, UInt PrevBreakCount)
{
    for (UInt i = PrevBreakCount; i < Compiler->BreakCount; i++)
    {
        PVMPatchBranchToCurrent(EMITTER(), Compiler->Breaks[i], PATCHTYPE_BRANCH_UNCONDITIONAL);
    }
    Compiler->BreakCount = PrevBreakCount;
}


static void CompileRepeatUntilStmt(PVMCompiler *Compiler)
{
    /* 'repeat' consumed */
    U32 LoopHead = PVMMarkBranchTarget(EMITTER());
    while (!IsAtEnd(Compiler) && !NextTokenIs(Compiler, TOKEN_UNTIL))
    {
        CompileStmt(Compiler);
        if (NextTokenIs(Compiler, TOKEN_UNTIL))
            break;
        ConsumeOrError(Compiler, TOKEN_SEMICOLON, "Expected ';' in between statements.");
    }


    /* nothing to compile, exit and reports error */
    if (!ConsumeOrError(Compiler, TOKEN_UNTIL, "Expected 'until'."))
        return;

    Token UntilKeyword = Compiler->Curr;
    CompilerInitDebugInfo(Compiler, &UntilKeyword);

    VarLocation Tmp = CompileExprIntoReg(Compiler);
    if (TYPE_BOOLEAN != Tmp.Type.Integral)
    {
        ErrorTypeMismatch(Compiler, &UntilKeyword, "until loop condition", "boolean", Tmp.Type.Integral);
    }

    U32 ToHead = PVMEmitBranchIfFalse(EMITTER(), &Tmp);
    PVMPatchBranch(EMITTER(), ToHead, LoopHead, PATCHTYPE_BRANCH_CONDITIONAL);
    FreeExpr(Compiler, Tmp);

    CompilerEmitDebugInfo(Compiler, &UntilKeyword);
}


static void CompileForStmt(PVMCompiler *Compiler)
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
    if (!IntegralTypeIsInteger(Counter->Type.Integral))
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
    if (!ConsumeIfNextIs(Compiler, TOKEN_TO))
    {
        ConsumeOrError(Compiler, TOKEN_DOWNTO, "Expected 'to' or 'downto' after expression.");
        Op = TOKEN_LESS_EQUAL;
        Inc = -1;
    }
    /* stop condition expr */
    U32 LoopExit = 0;
    VarLocation StopCondition = CompileExprIntoReg(Compiler); 
    if (TYPE_INVALID == CoerceTypes(Compiler, &OpToken, i->Type.Integral, StopCondition.Type.Integral))
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
    PVMPatchBranchToCurrent(EMITTER(), LoopExit, PATCHTYPE_BRANCH_FLAG);
    CompilerPatchBreaks(Compiler, BreakCountBeforeBody);

    /* move the result of the counter variable */
    PVMEmitMov(EMITTER(), &CounterSave, i);
    PVMFreeRegister(EMITTER(), i->As.Register);
    FreeExpr(Compiler, StopCondition);
    *i = CounterSave;
}


static void CompileWhileStmt(PVMCompiler *Compiler)
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
    PVMPatchBranchToCurrent(EMITTER(), LoopExit, PATCHTYPE_BRANCH_CONDITIONAL);
    CompilerPatchBreaks(Compiler, BreakCountBeforeBody);

    EMITTER()->ShouldEmit = Last;
}


static void CompileIfStmt(PVMCompiler *Compiler)
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
    if (ConsumeIfNextIs(Compiler, TOKEN_ELSE))
    {
        U32 FromEndIf = 0;
        if (COND_TRUE == IfCond)
            EMITTER()->ShouldEmit = false;
        
        if (COND_UNKNOWN == IfCond)
            FromEndIf = PVMEmitBranch(EMITTER(), 0);

        PVMPatchBranchToCurrent(EMITTER(), FromIf, PATCHTYPE_BRANCH_CONDITIONAL);
        CompilerInitDebugInfo(Compiler, &Compiler->Curr);
        CompilerEmitDebugInfo(Compiler, &Compiler->Curr);

        CompileStmt(Compiler);

        if (COND_UNKNOWN == IfCond)
            PVMPatchBranchToCurrent(EMITTER(), FromEndIf, PATCHTYPE_BRANCH_UNCONDITIONAL);
    }
    else if (COND_UNKNOWN == IfCond)
    {
        PVMPatchBranchToCurrent(EMITTER(), FromIf, PATCHTYPE_BRANCH_CONDITIONAL);
    }
    EMITTER()->ShouldEmit = Last;
}




static void CompileCallStmt(PVMCompiler *Compiler, const Token Name, PascalVar *IdenInfo)
{
    PASCAL_NONNULL(Compiler);
    PASCAL_NONNULL(IdenInfo);
    PASCAL_NONNULL(IdenInfo->Location);
    PASCAL_ASSERT(TYPE_FUNCTION == IdenInfo->Type.Integral, "Unreachable, IdenInfo is not a subroutine");

    /* iden consumed */
    /* call the subroutine */
    CompilerInitDebugInfo(Compiler, &Name);
    CompileSubroutineCall(Compiler, IdenInfo->Location, &Name, NULL);
    CompilerEmitDebugInfo(Compiler, &Name);
}


static void CompilerEmitAssignment(PVMCompiler *Compiler, const Token *Assignment, 
        VarLocation *Left, const VarLocation *Right)
{
    PASCAL_NONNULL(Compiler);
    PASCAL_NONNULL(Assignment);
    PASCAL_NONNULL(Left);
    PASCAL_NONNULL(Right);

    if (TOKEN_COLON_EQUAL == Assignment->Type)
    {
        PVMEmitMov(EMITTER(), Left, Right);
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
        PVMEmitMov(EMITTER(), Left, &Dst);
        FreeExpr(Compiler, Dst);
    }
}

static void CompileAssignStmt(PVMCompiler *Compiler, const Token Identifier)
{
    PASCAL_NONNULL(Compiler);
    /* iden consumed */
    CompilerInitDebugInfo(Compiler, &Identifier);


    /* no nested assignment is possible, this is ok */
    Compiler->Lhs = CompileVariableExpr(Compiler);
    VarLocation *Dst = &Compiler->Lhs;
    PASCAL_ASSERT(VAR_INVALID != Dst->LocationType, "Invalid location");


    const Token Assignment = Compiler->Next;
    ConsumeToken(Compiler); /* assignment type */


    VarLocation Right = CompileExpr(Compiler);
    if (TYPE_INVALID == CoerceTypes(Compiler, &Assignment, Dst->Type.Integral, Right.Type.Integral)
    || !ConvertTypeImplicitly(Compiler, Dst->Type.Integral, &Right))
    {
        ErrorAt(Compiler, &Assignment, "Cannot assign expression of type %s to %s.", 
                VarTypeToStr(Right.Type), VarTypeToStr(Dst->Type)
        );
    }
    else if (TYPE_RECORD == Dst->Type.Integral)
    {
        if (!VarTypeEqual(&Dst->Type, &Right.Type))
        {
            ErrorAt(Compiler, &Assignment, "Cannot assign record '"STRVIEW_FMT"' to record '"STRVIEW_FMT"'",
                    STRVIEW_FMT_ARG(&Dst->Type.As.Record.Name), STRVIEW_FMT_ARG(&Right.Type.As.Record.Name)
            );
        }
    }

    /* emit the assignment */
    CompilerEmitAssignment(Compiler, &Assignment, Dst, &Right);
    Compiler->Lhs.LocationType = VAR_INVALID;
    FreeExpr(Compiler, Right);
    CompilerEmitDebugInfo(Compiler, &Identifier);
}


static void CompileIdenStmt(PVMCompiler *Compiler)
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
        && ConsumeIfNextIs(Compiler, TOKEN_COLON_EQUAL))
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
            CompileCallStmt(Compiler, Callee, IdentifierInfo);
        }
    }
    else
    {
        Token Identifier = Compiler->Curr;
        CompileAssignStmt(Compiler, Identifier);
    }
}


static void CompileCaseStmt(PVMCompiler *Compiler)
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
            PVMPatchBranchToCurrent(EMITTER(), NextCase, PATCHTYPE_BRANCH_CONDITIONAL);
            EMITTER()->ShouldEmit = Last;

            if (NextTokenIs(Compiler, TOKEN_END) || NextTokenIs(Compiler, TOKEN_ELSE))
                break;
            ConsumeOrError(Compiler, TOKEN_SEMICOLON, "Expected ';' between case statements.");
        } while (!IsAtEnd(Compiler) && !NextTokenIs(Compiler, TOKEN_END) && !NextTokenIs(Compiler, TOKEN_ELSE));
    }

    if (ConsumeIfNextIs(Compiler, TOKEN_ELSE))
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
            PVMPatchBranchToCurrent(EMITTER(), OutBranch[i], PATCHTYPE_BRANCH_UNCONDITIONAL);
    }

    ConsumeOrError(Compiler, TOKEN_END, "Expected 'end' after statement.");
    FreeExpr(Compiler, Expr);
    EMITTER()->ShouldEmit = Last;
}


static void CompileBreakStmt(PVMCompiler *Compiler)
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

static void CompileStmt(PVMCompiler *Compiler)
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
    case TOKEN_SEMICOLON:
    {
        /* no statement */
        /* will be consumed by whoever called this function */
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
            ErrorAt(Compiler, &Compiler->Curr, "Is not allowed before 'else'.");
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


static bool CompileBlock(PVMCompiler *Compiler);

static void CompileBeginBlock(PVMCompiler *Compiler)
{
    if (IsAtGlobalScope(Compiler))
        Compiler->EntryPoint = PVMGetCurrentLocation(EMITTER());
    CompileBeginStmt(Compiler);
}


static PascalVar *CompileSubroutineName(
        PVMCompiler *Compiler, const Token *Name, bool HasReturnType, const char *SubroutineType)
{
    PascalVar *SubroutineInfo = FindIdentifier(Compiler, Name);
    if (NULL == SubroutineInfo)
    {
        VarLocation *Location = CompilerAllocateVarLocation(Compiler);
        Location->As.SubroutineLocation = SUBROUTINE_INVALID_LOCATION;
        Location->Type = VarTypeInit(TYPE_FUNCTION, sizeof(void*));
        Location->LocationType = VAR_SUBROUTINE;
        return DefineIdentifier(Compiler, Name, Location->Type, Location);
    }
    /* redefinition in repl is fine */
    if (COMPMODE_REPL == Compiler->Flags.CompMode)
        return SubroutineInfo;

    if (TYPE_FUNCTION != SubroutineInfo->Type.Integral)
    {
        ErrorAt(Compiler, Name, "'"STRVIEW_FMT"' of type %s begin redefined as a %s.",
                STRVIEW_FMT_ARG(&Name->Lexeme), VarTypeToStr(SubroutineInfo->Type),
                HasReturnType? "function" : "procedure"
        );
        return SubroutineInfo;
    }


    /* subroutine is already declared */
    VarLocation *Location = SubroutineInfo->Location;
    SubroutineData *Subroutine = &Location->Type.As.Subroutine;
    PASCAL_NONNULL(Location);


    /* redefinition error */
    if (Location->As.SubroutineLocation != SUBROUTINE_INVALID_LOCATION)
    {
        ErrorAt(Compiler, Name, "Redefinition of %s '"STRVIEW_FMT"' that was defined on line %d.",
                SubroutineType, STRVIEW_FMT_ARG(&Name->Lexeme), SubroutineInfo->Line
        );
    }
    /* function/procedure mismatch */
    if (HasReturnType != (NULL == Subroutine->ReturnType))
    {
        ErrorAt(Compiler, Name, "Definition of %s '"STRVIEW_FMT"' does not match declaration on line %d.",
                SubroutineType, STRVIEW_FMT_ARG(&Name->Lexeme), SubroutineInfo->Line
        );
    }
    return SubroutineInfo;
}


static SubroutineParameterList CompileParameterListWithParentheses(PVMCompiler *Compiler, PascalVartab *Scope)
{
    SubroutineParameterList ParameterList = { 0 };
    if (ConsumeIfNextIs(Compiler, TOKEN_LEFT_PAREN))
    {
        if (!NextTokenIs(Compiler, TOKEN_RIGHT_PAREN))
        {
            ParameterList = CompileParameterList(Compiler, Scope);
        }
        ConsumeOrError(Compiler, TOKEN_RIGHT_PAREN, "Expected ')' after parameter list.");
    }
    return ParameterList;
}


static void CompileSubroutineBlock(PVMCompiler *Compiler, const char *SubroutineType)
{
    PASCAL_NONNULL(Compiler);
    PASCAL_NONNULL(SubroutineType);


    /* 'function', 'procedure' consumed */
    Token Keyword = Compiler->Curr;
    bool ShouldHaveReturnType = TOKEN_FUNCTION == Keyword.Type;
    CompilerInitDebugInfo(Compiler, &Keyword);


    /* function/proc name */
    ConsumeOrError(Compiler, TOKEN_IDENTIFIER, "Expected %s name.", SubroutineType);
    Token Name = Compiler->Curr;
    PascalVar *SubroutineInfo = CompileSubroutineName(
            Compiler, &Name, ShouldHaveReturnType, SubroutineType
    );
    VarLocation *Location = SubroutineInfo->Location;
    SubroutineData *Subroutine = &SubroutineInfo->Type.As.Subroutine;
    PASCAL_NONNULL(SubroutineInfo);
    PASCAL_NONNULL(Location);
    

    /* param list */
    /* TODO: param list check */
    PascalVartab Scope = VartabInit(Compiler->GlobalAlloc, PVM_INITIAL_VAR_PER_SCOPE);
    SubroutineParameterList ParameterList = CompileParameterListWithParentheses(Compiler, &Scope);


    /* return type and semicolon */
    VarType *ReturnType = NULL;
    I32 StackBase = 0;
    UInt RecordReturn = 0; 
    if (ShouldHaveReturnType)
    {
        ConsumeOrError(Compiler, TOKEN_COLON, "Expected ':' before function return type.");
        ConsumeOrError(Compiler, TOKEN_IDENTIFIER, "Expected function return type.");
        PascalVar *ReturnTypeInfo = GetIdenInfo(Compiler, &Compiler->Curr, "Undefined return type.");
        if (NULL != ReturnTypeInfo)
        {
            RecordReturn = TYPE_RECORD == ReturnTypeInfo->Type.Integral; 

            /* location in the variable table is volatile, 
             * so we need to copy the return type */
            ReturnType = CompilerCopyType(Compiler, ReturnTypeInfo->Type);
            PASCAL_NONNULL(ReturnType);

            /* first arg is the addr of the returning struct, 
             * TODO: other calling convention */
            PVMSetParam(EMITTER(), 0, *ReturnType, &StackBase);
        }
        ConsumeOrError(Compiler, TOKEN_SEMICOLON, "Expected ';' after function return type.");
    }
    else
    {
        /* for good error message */
        if (ConsumeIfNextIs(Compiler, TOKEN_COLON))
        {
            if (ConsumeIfNextIs(Compiler, TOKEN_IDENTIFIER))
            {
                Error(Compiler, "Procedure cannot have a return type.");
            }
        }
        ConsumeOrError(Compiler, TOKEN_SEMICOLON, "Expected ';' after parameter list.");
    }


    /* finalize the location of parameters */
    /* end function decl */
    U32 StackArgSize = 0;
    for (UInt i = 0; i < ParameterList.Count; i++)
    {
        VarLocation *Parameter = ParameterList.Params[i].Location;
        PASCAL_NONNULL(Parameter);
        *Parameter = PVMSetParam(EMITTER(), i + RecordReturn, Parameter->Type, &StackBase);
        if (i + RecordReturn >= PVM_ARGREG_COUNT)
        {
            StackArgSize += Parameter->Type.Size;
        }
    }
    CompilerEmitDebugInfo(Compiler, &Keyword);
    Location->Type = VarTypeSubroutine(ParameterList, Scope, ReturnType, StackArgSize);
    SubroutineInfo->Type = Location->Type;


    /* forward declaration */
    if (ConsumeIfNextIs(Compiler, TOKEN_FORWARD))
    {
        ConsumeOrError(Compiler, TOKEN_SEMICOLON, "Expected ';' after '"STRVIEW_FMT"'", 
                STRVIEW_FMT_ARG(&Compiler->Curr.Lexeme)
        );
    }
    else /* body */
    {
        /* begin subroutine scope */
        CompilerPushSubroutine(Compiler, Subroutine);
        /* TODO: uncomment this when implementing closure */
        //SaveRegInfo PrevScope = PVMEmitterBeginScope(EMITTER());
        Location->As.SubroutineLocation = PVMGetCurrentLocation(EMITTER());


        /* body */
        U32 EnterLocation = PVMEmitEnter(EMITTER());
        CompileBlock(Compiler);
        PVMPatchEnter(EMITTER(), EnterLocation, Compiler->StackSize);
        Compiler->StackSize = 0;


        /* emit the exit instruction and associate it with the 'end' token */
        Token End = Compiler->Curr;
        CompilerInitDebugInfo(Compiler, &End);
        PVMEmitExit(EMITTER());
        CompilerEmitDebugInfo(Compiler, &End);
        ConsumeOrError(Compiler, TOKEN_SEMICOLON, "Expected ';' after %s body.", SubroutineType);


        /* force free all argument registers */
        UInt ArgregCount = PVM_ARGREG_COUNT;
        if (ArgregCount > Subroutine->ParameterList.Count)
            ArgregCount = Subroutine->ParameterList.Count;
        for (UInt i = 0; i < ArgregCount; i++)
        {
            VarRegister RegisterParameter = Subroutine->ParameterList.Params[i].Location->As.Register;
            PVMMarkRegisterAsFreed(EMITTER(), RegisterParameter.ID);
        }

        CompilerPopSubroutine(Compiler);
        //PVMEmitterEndScope(EMITTER(), PrevScope);
    }
}




static void CompileVarBlock(PVMCompiler *Compiler)
{
    /* 
     * var
     *      id1, id2: typename;
     *      id3: typename;
     */
    //Token Keyword = Compiler->Curr;
    if (!NextTokenIs(Compiler, TOKEN_IDENTIFIER))
    {
        Error(Compiler, "Expected variable name.");
        return;
    }

    bool AtGlobalScope = false;
    U32 BaseRegister = PVM_REG_FP;
    U32 BaseAddr = Compiler->StackSize;
    if (IsAtGlobalScope(Compiler))
    {
        AtGlobalScope = true;
        BaseRegister = PVM_REG_GP;
        BaseAddr = PVMGetGlobalOffset(EMITTER());
    }
    U32 TotalSize = 0;
    UInt Alignment = sizeof(U32);

    while (!IsAtEnd(Compiler) && NextTokenIs(Compiler, TOKEN_IDENTIFIER))
    {
        Token VariableName = Compiler->Next;
        U32 Next = CompileVarList(Compiler, BaseRegister, BaseAddr + TotalSize, Alignment);
        if (Next == TotalSize)
        {
            DBG_PRINT("E\n");
            /* Error encountered */
            return;
        }
        TotalSize = Next - BaseAddr;

        /* initialization */
        if (ConsumeIfNextIs(Compiler, TOKEN_EQUAL))
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
                if (TYPE_INVALID 
                == CoerceTypes(Compiler, &EqualSign, Variable->Type.Integral, Constant.Type.Integral))
                {
                    continue; /* error reported */
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
            else 
            {
                CompileExprInto(Compiler, &EqualSign, Variable->Location);
            }
            ConsumeOrError(Compiler, TOKEN_SEMICOLON, "Expected ';' after expression.");
        }
        else
        {
            ConsumeOrError(Compiler, TOKEN_SEMICOLON, "Expected ';' after variable declaration.");
        }

        if (ConsumeIfNextIs(Compiler, TOKEN_VAR) && !NextTokenIs(Compiler, TOKEN_IDENTIFIER))
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




static PascalVar *FindTypeName(PVMCompiler *Compiler, const Token *Name)
{
    PascalVar *Type = GetIdenInfo(Compiler, Name, "Undefined type name.");
    if (NULL == Type)
        return NULL;
    if (NULL != Type->Location)
    {
        ErrorAt(Compiler, Name, "'"STRVIEW_FMT"' is not a type name.",
                STRVIEW_FMT_ARG(&Name->Lexeme)
        );
        return NULL;
    }
    return Type;
}


static void CompileTypeBlock(PVMCompiler *Compiler)
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

    while (ConsumeIfNextIs(Compiler, TOKEN_IDENTIFIER))
    {
        Token Identifier = Compiler->Curr;
        if (!ConsumeOrError(Compiler, TOKEN_EQUAL, "Expected '=' instead."))
            return;

        if (ConsumeIfNextIs(Compiler, TOKEN_IDENTIFIER))
        {
            const Token TypeName = Compiler->Curr;
            PascalVar *Type = GetIdenInfo(Compiler, &TypeName, "Undefined type name.");
                DefineIdentifier(Compiler, &Identifier, Type->Type, NULL);
        }
        else if (ConsumeIfNextIs(Compiler, TOKEN_CARET))
        {
            if (!ConsumeOrError(Compiler, TOKEN_IDENTIFIER, "Expected type name."))
                continue;
            PascalVar *Type = FindTypeName(Compiler, &Compiler->Curr);
            if (NULL == Type)
                continue;

            VarType Pointer = VarTypePtr(CompilerCopyType(Compiler, Type->Type));
            DefineIdentifier(Compiler, &Identifier, Pointer, NULL);
        }
        else if (ConsumeIfNextIs(Compiler, TOKEN_RECORD))
        {
            CompileRecordDefinition(Compiler, &Identifier);
        }
        else if (ConsumeIfNextIs(Compiler, TOKEN_FUNCTION))
        {
            PascalVartab Scope = VartabInit(Compiler->GlobalAlloc, PVM_INITIAL_VAR_PER_SCOPE);
            SubroutineParameterList ParameterList = CompileParameterListWithParentheses(Compiler, &Scope);

            ConsumeOrError(Compiler, TOKEN_COLON, "Expeccted ':' after function paramter list.");
            if (!ConsumeOrError(Compiler, TOKEN_IDENTIFIER, "Expected function return type."))
                continue;
            PascalVar *Type = FindTypeName(Compiler, &Compiler->Curr);
            if (NULL == Type)
                continue;

            VarType *ReturnType = CompilerCopyType(Compiler, Type->Type);
            VarType Function = VarTypeSubroutine(ParameterList, Scope, ReturnType, 0);
            VarType FunctionPointer = VarTypePtr(CompilerCopyType(Compiler, Function));
            DefineIdentifier(Compiler, &Identifier, FunctionPointer, NULL);
        }
        else if (ConsumeIfNextIs(Compiler, TOKEN_PROCEDURE))
        {
            PascalVartab Scope = VartabInit(Compiler->GlobalAlloc, PVM_INITIAL_VAR_PER_SCOPE);
            SubroutineParameterList ParameterList = CompileParameterListWithParentheses(Compiler, &Scope);
            VarType Procedure = VarTypeSubroutine(ParameterList, Scope, NULL, 0);
            VarType ProcedurePointer = VarTypePtr(CompilerCopyType(Compiler, Procedure));
            DefineIdentifier(Compiler, &Identifier, ProcedurePointer, NULL);
        }
        else
        {
            PASCAL_UNREACHABLE("TODO: more type.");
        }

        ConsumeOrError(Compiler, TOKEN_SEMICOLON, "Expected ';' after type definition.");
    }
}


static void CompileConstBlock(PVMCompiler *Compiler)
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

    while (ConsumeIfNextIs(Compiler, TOKEN_IDENTIFIER))
    {
        Token Identifier = Compiler->Curr;
        if (ConsumeIfNextIs(Compiler, TOKEN_COLON))
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
static bool CompileHeadlessBlock(PVMCompiler *Compiler)
{
    while (!IsAtEnd(Compiler) && !NextTokenIs(Compiler, TOKEN_BEGIN))
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
            if (COMPMODE_REPL != Compiler->Flags.CompMode)
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
    return ConsumeIfNextIs(Compiler, TOKEN_BEGIN);
}



static bool CompileBlock(PVMCompiler *Compiler)
{
    if (CompileHeadlessBlock(Compiler))
    {
        CompileBeginBlock(Compiler);
        return !Compiler->Error;
    }
    Error(Compiler, "Expected 'Begin'.");
    return false;
}




static bool CompileProgram(PVMCompiler *Compiler)
{
    ConsumeOrError(Compiler, TOKEN_IDENTIFIER, "Expected identifier.");
    if (ConsumeIfNextIs(Compiler, TOKEN_LEFT_PAREN))
    {
        /* TODO: what are these idens for:
         * program hello(identifier1, iden2, hi);
         *               ^^^^^^^^^^^  ^^^^^  ^^
         */
        do {
            ConsumeOrError(Compiler, TOKEN_IDENTIFIER, "Expected identifier.");
        } while (ConsumeIfNextIs(Compiler, TOKEN_COMMA));
        ConsumeOrError(Compiler, TOKEN_RIGHT_PAREN, "Expected ')' instead");
    }

    ConsumeOrError(Compiler, TOKEN_SEMICOLON, "Expected ';' instead.");
    if (ConsumeIfNextIs(Compiler, TOKEN_USES))
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
        } while (ConsumeIfNextIs(Compiler, TOKEN_COMMA));
    }


    CompileBlock(Compiler);
    ConsumeOrError(Compiler, TOKEN_DOT, "Expected '.' instead.");
    return !Compiler->Error;
}





bool PascalCompile(const U8 *Source, 
        PascalCompileFlags Flags,
        PascalVartab *PredefinedIdentifiers, PascalGPA *GlobalAlloc, FILE *LogFile,
        PVMChunk *Chunk)
{
    PVMCompiler Compiler = CompilerInit(Source, Flags, PredefinedIdentifiers, Chunk, GlobalAlloc, LogFile);
    ConsumeToken(&Compiler);

    switch (Compiler.Next.Type)
    {
    case TOKEN_PROGRAM:
    {
        ConsumeToken(&Compiler);
        CompileProgram(&Compiler);
    } break;
    case TOKEN_UNIT:
    {
        PASCAL_UNREACHABLE("TODO: Unit");
    } break;
    default:
    {
        if (COMPMODE_REPL != Flags.CompMode)
        {
            Error(&Compiler, "Expected 'Program' or 'Unit' at the beginning of file.");
            goto Out;
        }

        if (CompileHeadlessBlock(&Compiler))
        {
            CompileBeginBlock(&Compiler);
        }
        else 
        {
            Compiler.EntryPoint = PVMGetCurrentLocation(&Compiler.Emitter);
            while (!IsAtEnd(&Compiler))
            {
                CompileStmt(&Compiler);
                if (!IsAtEnd(&Compiler))
                    ConsumeOrError(&Compiler, TOKEN_SEMICOLON, "Expected ';' between statement.");
            }
        }
    } break;
    }

Out:
    CompilerDeinit(&Compiler);
    return !Compiler.Error;
}

