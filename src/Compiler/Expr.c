

#include <inttypes.h>

#include "Compiler/Compiler.h"
#include "Compiler/Data.h"
#include "Compiler/Error.h"
#include "Compiler/Expr.h"
#include "Compiler/Builtins.h"
#include "Compiler/VarList.h"


static const IntegralType sCoercionRules[TYPE_COUNT][TYPE_COUNT] = {
    /*Invalid       I8            I16           I32           I64           U8            U16           U32           U64           F32           F64           Function      Boolean       Pointer       string       record */
    { TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID,TYPE_INVALID,TYPE_INVALID, TYPE_INVALID},         /* Invalid */
    { TYPE_INVALID, TYPE_I32,     TYPE_I32,     TYPE_I32,     TYPE_I64,     TYPE_I32,     TYPE_I32,     TYPE_I32,     TYPE_I64,     TYPE_F32,     TYPE_F64,     TYPE_INVALID, TYPE_INVALID, TYPE_POINTER, TYPE_INVALID,TYPE_INVALID,TYPE_INVALID, TYPE_INVALID},         /* I8 */
    { TYPE_INVALID, TYPE_I32,     TYPE_I32,     TYPE_I32,     TYPE_I64,     TYPE_I32,     TYPE_I32,     TYPE_I32,     TYPE_I64,     TYPE_F32,     TYPE_F64,     TYPE_INVALID, TYPE_INVALID, TYPE_POINTER, TYPE_INVALID,TYPE_INVALID,TYPE_INVALID, TYPE_INVALID},         /* I16 */
    { TYPE_INVALID, TYPE_I32,     TYPE_I32,     TYPE_I32,     TYPE_I64,     TYPE_I32,     TYPE_I32,     TYPE_I32,     TYPE_I64,     TYPE_F32,     TYPE_F64,     TYPE_INVALID, TYPE_INVALID, TYPE_POINTER, TYPE_INVALID,TYPE_INVALID,TYPE_INVALID, TYPE_INVALID},         /* I32 */
    { TYPE_INVALID, TYPE_I64,     TYPE_I64,     TYPE_I64,     TYPE_I64,     TYPE_I64,     TYPE_I64,     TYPE_I64,     TYPE_I64,     TYPE_F32,     TYPE_F64,     TYPE_INVALID, TYPE_INVALID, TYPE_POINTER, TYPE_INVALID,TYPE_INVALID,TYPE_INVALID, TYPE_INVALID},         /* I64 */
    { TYPE_INVALID, TYPE_I32,     TYPE_I32,     TYPE_I32,     TYPE_I64,     TYPE_U32,     TYPE_U32,     TYPE_U32,     TYPE_U64,     TYPE_F32,     TYPE_F64,     TYPE_INVALID, TYPE_INVALID, TYPE_POINTER, TYPE_INVALID,TYPE_INVALID,TYPE_INVALID, TYPE_INVALID},         /* U8 */
    { TYPE_INVALID, TYPE_I32,     TYPE_I32,     TYPE_I32,     TYPE_I64,     TYPE_U32,     TYPE_U32,     TYPE_U32,     TYPE_U64,     TYPE_F32,     TYPE_F64,     TYPE_INVALID, TYPE_INVALID, TYPE_POINTER, TYPE_INVALID,TYPE_INVALID,TYPE_INVALID, TYPE_INVALID},         /* U16 */
    { TYPE_INVALID, TYPE_I32,     TYPE_I32,     TYPE_I32,     TYPE_I64,     TYPE_U32,     TYPE_U32,     TYPE_U32,     TYPE_U64,     TYPE_F32,     TYPE_F64,     TYPE_INVALID, TYPE_INVALID, TYPE_POINTER, TYPE_INVALID,TYPE_INVALID,TYPE_INVALID, TYPE_INVALID},         /* U32 */
    { TYPE_INVALID, TYPE_U64,     TYPE_U64,     TYPE_U64,     TYPE_U64,     TYPE_U64,     TYPE_U64,     TYPE_U64,     TYPE_U64,     TYPE_F32,     TYPE_F64,     TYPE_INVALID, TYPE_INVALID, TYPE_POINTER, TYPE_INVALID,TYPE_INVALID,TYPE_INVALID, TYPE_INVALID},         /* U64 */
    { TYPE_INVALID, TYPE_F32,     TYPE_F32,     TYPE_F32,     TYPE_F32,     TYPE_F32,     TYPE_F32,     TYPE_F32,     TYPE_F32,     TYPE_F32,     TYPE_F64,     TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID,TYPE_INVALID,TYPE_INVALID, TYPE_INVALID},         /* F32 */
    { TYPE_INVALID, TYPE_F64,     TYPE_F64,     TYPE_F64,     TYPE_F64,     TYPE_F64,     TYPE_F64,     TYPE_F64,     TYPE_F64,     TYPE_F64,     TYPE_F64,     TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID,TYPE_INVALID,TYPE_INVALID, TYPE_INVALID},         /* F64 */
    { TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID,TYPE_INVALID,TYPE_INVALID, TYPE_INVALID},         /* Function */
    { TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_BOOLEAN, TYPE_INVALID, TYPE_INVALID,TYPE_INVALID,TYPE_INVALID, TYPE_INVALID},         /* Boolean */
    { TYPE_INVALID, TYPE_POINTER, TYPE_POINTER, TYPE_POINTER, TYPE_POINTER, TYPE_POINTER, TYPE_POINTER, TYPE_POINTER, TYPE_POINTER, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_POINTER, TYPE_INVALID,TYPE_INVALID,TYPE_INVALID, TYPE_INVALID},         /* Pointer */
    { TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_STRING, TYPE_INVALID,TYPE_INVALID, TYPE_INVALID},         /* String */
    { TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID,TYPE_RECORD ,TYPE_INVALID, TYPE_INVALID},         /* Record */
    { TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID,TYPE_INVALID,TYPE_CHAR, TYPE_INVALID},            /* Char */
    { TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID,TYPE_INVALID,TYPE_INVALID, TYPE_INVALID},         /* Array */
};





/*===============================================================================*/
/*
 *                                  EXPRESSION
 */
/*===============================================================================*/

typedef enum Precedence 
{
    PREC_SINGLE = 0,
    PREC_EXPR,
    PREC_SIMPLE,
    PREC_TERM,
    PREC_FACTOR,
    PREC_VARIABLE,
    PREC_HIGHEST,
} Precedence;
typedef VarLocation (*InfixParseRoutine)(PascalCompiler *, VarLocation *);
typedef VarLocation (*PrefixParseRoutine)(PascalCompiler *);


#define INVALID_BINARY_OP(pOp, LType, RType) \
    ErrorAt(Compiler, pOp, "Invalid operand for binary operator '"STRVIEW_FMT"': %s and %s", \
            STRVIEW_FMT_ARG(&(pOp)->Lexeme), \
            IntegralTypeToStr(LType), IntegralTypeToStr(RType)\
    )
#define INVALID_PREFIX_OP(pOp, Type) \
    ErrorAt(Compiler, pOp, "Invalid operand for prefix operator '"STRVIEW_FMT"': %s", \
            STRVIEW_FMT_ARG(&(pOp)->Lexeme), IntegralTypeToStr(Type)\
    )
#define INVALID_POSTFIX_OP(pOp, Type) \
    ErrorAt(Compiler, pOp, "Invalid operand for postfix operator '"STRVIEW_FMT"': %s", \
            STRVIEW_FMT_ARG(&(pOp)->Lexeme), IntegralTypeToStr(Type)\
    )

typedef struct PrecedenceRule
{
    PrefixParseRoutine PrefixRoutine;
    InfixParseRoutine InfixRoutine;
    Precedence Prec;
} PrecedenceRule;

static const PrecedenceRule *GetPrecedenceRule(TokenType CurrentTokenType);
static VarLocation ParsePrecedence(PascalCompiler *Compiler, Precedence Prec);
static VarLocation ParseAssignmentLhs(PascalCompiler *Compiler, Precedence Prec);


void FreeExpr(PascalCompiler *Compiler, VarLocation Expr)
{
    PASCAL_NONNULL(Compiler);

    if (VAR_REG == Expr.LocationType)
    {
        if (!PVMRegisterIsFree(EMITTER(), Expr.As.Register.ID))
            PVMFreeRegister(EMITTER(), Expr.As.Register);
    }
    else if (VAR_MEM == Expr.LocationType && !Expr.As.Memory.RegPtr.Persistent)
    {
        if (!PVMRegisterIsFree(EMITTER(), Expr.As.Memory.RegPtr.ID))
            PVMFreeRegister(EMITTER(), Expr.As.Memory.RegPtr);
    }
}


VarLocation CompileExpr(PascalCompiler *Compiler)
{
    return ParsePrecedence(Compiler, PREC_EXPR);
}

VarLocation CompileExprIntoReg(PascalCompiler *Compiler)
{
    VarLocation Expr = CompileExpr(Compiler);
    VarLocation Reg;
    PVMEmitIntoRegLocation(EMITTER(), &Reg, false, &Expr); 
    /* ownership is transfered */
    return Reg;
}

VarLocation CompileVariableExpr(PascalCompiler *Compiler)
{
    return ParseAssignmentLhs(Compiler, PREC_VARIABLE);
}

void CompileExprInto(PascalCompiler *Compiler, const Token *ErrorSpot, VarLocation *Location)
{
    VarLocation Expr = CompileExpr(Compiler);
    if (TYPE_INVALID == CoerceTypes(Location->Type.Integral, Expr.Type.Integral)
    || (Expr.LocationType != VAR_MEM && !ConvertTypeImplicitly(Compiler, Location->Type.Integral, &Expr)))
    {
        goto InvalidTypeCombination;
    }

    if (IntegralTypeIsOrdinal(Location->Type.Integral))
    {
        PVMEmitMove(EMITTER(), Location, &Expr);
    }
    else if (TYPE_POINTER == Location->Type.Integral)
    {
        if (!VarTypeEqual(&Location->Type, &Expr.Type))
            goto InvalidTypeCombination;
        PVMEmitMove(EMITTER(), Location, &Expr);
    }
    else if (VarTypeEqual(&Location->Type, &Expr.Type))
    {
        PVMEmitCopy(EMITTER(), Location, &Expr);
    }
    else goto InvalidTypeCombination;

    FreeExpr(Compiler, Expr);
    return;

InvalidTypeCombination:
    if (NULL == ErrorSpot)
    {
        Error(Compiler, "Invalid type combination: %s and %s.",
            VarTypeToStr(Location->Type), VarTypeToStr(Expr.Type)
        );
    }
    else
    {
        ErrorAt(Compiler, ErrorSpot, "Invalid type combination: %s and %s.",
            VarTypeToStr(Location->Type), VarTypeToStr(Expr.Type)
        );
    }
}





static VarType TypeOfIntLit(U64 Int)
{
    if (IN_I32(Int))
        return VarTypeInit(TYPE_I32, 4);
    return VarTypeInit(TYPE_I64, 8);
}

static VarType TypeOfUIntLit(U64 UInt)
{
    if (IN_U32(UInt))
        return VarTypeInit(TYPE_U32, 4);
    return VarTypeInit(TYPE_U64, 8);
}

static VarLocation FactorLiteral(PascalCompiler *Compiler)
{
    VarLocation Location = {
        .LocationType = VAR_LIT,
    };
    PASCAL_NONNULL(Compiler);

    switch (Compiler->Curr.Type)
    {
    case TOKEN_CHAR_LITERAL:
    {
        U8 Chr = Compiler->Curr.Literal.Chr;
        Location.As.Literal.Chr = Chr;
        Location.Type = VarTypeInit(TYPE_CHAR, 1);
    } break;
    case TOKEN_INTEGER_LITERAL:
    {
        U64 Int = Compiler->Curr.Literal.Int;
        Location.As.Literal.Int = Int;
        Location.Type = TypeOfIntLit(Int);
    } break;
    case TOKEN_NUMBER_LITERAL:
    {
        Location.Type = VarTypeInit(TYPE_F64, sizeof(F64));
        Location.As.Literal.Flt = Compiler->Curr.Literal.Real;
    } break;
    case TOKEN_TRUE:
    case TOKEN_FALSE:
    {
        Location.Type = VarTypeInit(TYPE_BOOLEAN, 1);
        Location.As.Literal.Bool = Compiler->Curr.Type != TOKEN_FALSE;
    } break;
    case TOKEN_STRING_LITERAL:
    {
        Location.Type = VarTypeInit(TYPE_STRING, PStrGetLen(&Compiler->Curr.Literal.Str));
        Location.As.Literal.Str = Compiler->Curr.Literal.Str;
    } break;
    default:
    {
        PASCAL_UNREACHABLE("Unreachable, %s literal type is not handled", TokenTypeToStr(Compiler->Curr.Type));
    } break;
    }
    return Location;
}


static VarLocation CompileCallWithReturnValue(PascalCompiler *Compiler, 
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
            return (VarLocation) { 0 };
        }
    }
    const SubroutineData *Subroutine = &Type->As.Subroutine;
    const VarType *ReturnType = Subroutine->ReturnType;
    if (NULL == ReturnType)
    {
        /* no return value, but in expression */
        ErrorAt(Compiler, Callee, "Procedure '"STRVIEW_FMT"' does not have a return value.",
                STRVIEW_FMT_ARG(&Callee->Lexeme)
        );
        return (VarLocation) { 0 };
    }


    VarLocation ReturnValue;
    UInt ReturnReg = NO_RETURN_REG;
    I32 Base = PVMStartArg(EMITTER(), Subroutine->StackArgSize);
    SaveRegInfo SaveRegs;
    if (!VarTypeIsTriviallyCopiable(*ReturnType))
    {
        SaveRegs = PVMEmitSaveCallerRegs(EMITTER(), NO_RETURN_REG);

        /* then first argument will contain return value */
        VarLocation FirstArg = PVMSetArg(EMITTER(), 0, *ReturnType, &Base);
        if (NULL != Compiler->Lhs && VarTypeEqual(ReturnType, &Compiler->Lhs->Type)) 
        {
            /* pass a pointer of lhs as first argument */
            ReturnValue = *Compiler->Lhs;
        }
        else
        {
            /* create a temporary on stack as first argument */
            Compiler->TemporarySize = uMax(Compiler->TemporarySize, ReturnType->Size);
            ReturnValue = PVMCreateStackLocation(EMITTER(), *ReturnType, Compiler->StackSize);
        }
        PASCAL_ASSERT(ReturnValue.LocationType == VAR_MEM, "%s", __func__);
        PVMEmitMove(EMITTER(), &FirstArg, &ReturnValue);
        PVMMarkArgAsOccupied(EMITTER(), &FirstArg);
    }
    else
    {
        /* return value in register */
        ReturnValue = PVMAllocateRegisterLocation(EMITTER(), *ReturnType);
        ReturnReg = ReturnValue.As.Register.ID;

        SaveRegs = PVMEmitSaveCallerRegs(EMITTER(), ReturnReg);
    }


    CompileArgumentList(Compiler, Callee, Subroutine, &Base, Subroutine->HiddenParamCount);
    CompilerEmitCall(Compiler, Location, SaveRegs);


    if (VarTypeIsTriviallyCopiable(*ReturnType))
    {
        VarLocation DefaultReturnReg = PVMSetReturnType(EMITTER(), *ReturnType);
        PVMEmitMove(EMITTER(), &ReturnValue, &DefaultReturnReg);
    }


    /* did we allocate stack space for arguments? */
    /* 
     * TODO: this is calling convention dependent, 
     * in cdecl we can allocate stack space once for all function call and forget about it, 
     * but in stdcall, the callee cleans up the stack 
     */
    if (Subroutine->StackArgSize)
    {
        /* deallocate stack space */
        PVMEmitStackAllocation(EMITTER(), -Subroutine->StackArgSize);
    }

    /* restore caller regs */
    PVMEmitUnsaveCallerRegs(EMITTER(), ReturnReg, SaveRegs);
    return ReturnValue;
}


static VarLocation FactorCall(PascalCompiler *Compiler, VarLocation *Location)
{
    PASCAL_NONNULL(Compiler);
    PASCAL_NONNULL(Location);

    Token Callee = Compiler->Curr;
    /* builtin function */
    if (VAR_BUILTIN == Location->LocationType)
    {
        OptionalReturnValue Opt = CompileCallToBuiltin(Compiler, Location->As.BuiltinSubroutine);
        if (!Opt.HasReturnValue)
        {
            ErrorAt(Compiler, &Callee, "Builtin procedure '"STRVIEW_FMT"' does not have a return value.",
                    STRVIEW_FMT_ARG(&Callee.Lexeme)
            );
            return (VarLocation) { 0 };
        }
        return Opt.ReturnValue;
    }

    VarLocation ReturnValue = CompileCallWithReturnValue(Compiler, Location, &Callee);
    FreeExpr(Compiler, *Location);
    return ReturnValue;
}



static VarLocation VariableDeref(PascalCompiler *Compiler, VarLocation *Variable)
{
    PASCAL_NONNULL(Compiler);
    PASCAL_NONNULL(Variable);

    Token Caret = Compiler->Curr;
    if (TYPE_POINTER != Variable->Type.Integral)
    {
        ErrorAt(Compiler, &Caret, "%s is not dereferenceable.", 
                VarTypeToStr(Variable->Type)
        );
        return *Variable;
    }
    if (NULL == Variable->Type.As.Pointee)
    {
        ErrorAt(Compiler, &Caret, "Cannot dereference an opque pointer.");
        return *Variable;
    }


    VarType Pointee = *Variable->Type.As.Pointee;
    VarLocation Ptr;
    PVMEmitIntoRegLocation(EMITTER(), &Ptr, true, Variable);
    FreeExpr(Compiler, *Variable);

    VarLocation Memory = {
        .LocationType = VAR_MEM,
        .Type = Pointee,
        .As.Memory = {
            .RegPtr = Ptr.As.Register,
            .Location = 0,
        },
    };
    return Memory;
}

static VarLocation *FindRecordMember(PascalVartab *Record, const Token *MemberName)
{
    PASCAL_NONNULL(Record);
    PASCAL_NONNULL(MemberName);

    U32 Hash = VartabHashStr(MemberName->Lexeme.Str, MemberName->Lexeme.Len);
    PascalVar *Member = VartabFindWithHash(Record, MemberName->Lexeme.Str, MemberName->Lexeme.Len, Hash);
    if (NULL == Member)
        return NULL;
    return Member->Location;
}

static VarLocation VariableAccess(PascalCompiler *Compiler, VarLocation *Left)
{
    PASCAL_NONNULL(Compiler);
    PASCAL_NONNULL(Left);

    /* identifier consumed */
    Token Dot = Compiler->Curr;
    ConsumeOrError(Compiler, TOKEN_IDENTIFIER, "Expected member name.");
    if (TYPE_RECORD != Left->Type.Integral)
    {
        ErrorAt(Compiler, &Dot, "%s does not have a member.", VarTypeToStr(Left->Type));
        return *Left;
    }
    else if (VAR_INVALID == Left->LocationType)
    {
        ErrorAt(Compiler, &Dot, "Left expression of '.' does not have a location.");
        return *Left;
    }

    Token Name = Compiler->Curr;
    VarLocation *Member = FindRecordMember(&Left->Type.As.Record.Field, &Name);
    if (NULL == Member)
    {
        ErrorAt(Compiler, &Name, "'"STRVIEW_FMT"' is not a member of record '"STRVIEW_FMT"'.", 
                STRVIEW_FMT_ARG(&Name.Lexeme), STRVIEW_FMT_ARG(&Left->Type.As.Record.Name)
        );
        return *Left;
    }

    /* member offset + record offset = location */
    VarLocation Location = *Member;
    if (VAR_MEM == Left->LocationType)
    {
        VarLocation Value = *Member;
        Value.As.Memory.Location += Left->As.Memory.Location;
        Value.As.Memory.RegPtr = Left->As.Memory.RegPtr;
        return Value;
    }
    else if (VAR_REG == Left->LocationType)
    {
        Location.As.Memory.RegPtr = Left->As.Register;
    }
    else 
    {
        PASCAL_UNREACHABLE("Invalid location type: %d\n", Left->LocationType);
    }
    return Location;
}

static VarLocation ArrayAccess(PascalCompiler *Compiler, VarLocation *Left)
{
    PASCAL_NONNULL(Compiler);
    PASCAL_NONNULL(Left);
    Token LeftBracket = Compiler->Curr;
    /* TODO: typecheck on index */
    VarLocation Index = CompileExpr(Compiler);
    ConsumeOrError(Compiler, TOKEN_RIGHT_BRACKET, "Expected ']' after expression.");

    IntegralType VariableType = Left->Type.Integral;
    VarLocation Element = { 0 };
    if (VariableType == TYPE_STATIC_ARRAY)
    {
        Element = PVMEmitLoadArrayElement(EMITTER(), Left, &Index);
    }
    else if (VariableType == TYPE_STRING)
    {
        PASCAL_UNREACHABLE("");
    }
    else 
    {
        ErrorAt(Compiler, &LeftBracket, "Cannot index expression of type %s.", 
            VarTypeToStr(Left->Type)
        );
    }
    FreeExpr(Compiler, Index);
    return Element;
}


static VarLocation FactorGrouping(PascalCompiler *Compiler)
{
    /* '(' consumed */
    VarLocation Group = CompileExpr(Compiler);
    ConsumeOrError(Compiler, TOKEN_RIGHT_PAREN, "Expected ')' after expression.");
    return Group;
}



static bool JavascriptStrToBool(const PascalStr *Str)
{
    if (0 == PStrGetLen(Str))
        return false;

    const char *Ptr = (const char *)PStrGetConstPtr(Str);
    /* assumes null-terminated */

    while (' ' == *Ptr)
        Ptr++;

    if ('-' == *Ptr) Ptr++;
    while ('0' == *Ptr)
        Ptr++;

    if ('.' == *Ptr) Ptr++;
    while ('0' == *Ptr)
        Ptr++;

    return !('e' == *Ptr || 'E' == *Ptr || '\0' == *Ptr);
}


static VarLiteral ConvertLiteralTypeExplicitly(PascalCompiler *Compiler, 
        const Token *Converter, IntegralType To, const VarLiteral *Lit, IntegralType LitType)
{
    VarLiteral Converted = { 0 };
    PASCAL_NONNULL(Compiler);
    PASCAL_NONNULL(Lit);
    PASCAL_NONNULL(Converter);
    if (To == LitType)
        return *Lit;
    if (TYPE_FUNCTION == To || TYPE_RECORD == To)
    {
        if (To != LitType)
            goto InvalidTypeConversion;
        return *Lit;
    }

    /* fuck fpc and tp I'm going wild with explicit type conversion */
    if (TYPE_STRING == To)
    {
        static char Buf[1024];
        UInt Len = 0;
        if (IntegralTypeIsInteger(LitType))
        {
            if (IntegralTypeIsSigned(LitType))
                Len = snprintf(Buf, sizeof Buf, "%"PRIi64, Lit->Int);
            else 
                Len = snprintf(Buf, sizeof Buf, "%"PRIu64, Lit->Int);
        }
        else if (IntegralTypeIsFloat(LitType))
            Len = snprintf(Buf, sizeof Buf, "%f", Lit->Flt);
        else if (TYPE_POINTER == LitType)
            Len = snprintf(Buf, sizeof Buf, "%p", Lit->Ptr.As.Raw);
        else if (TYPE_BOOLEAN == LitType)
            Len = snprintf(Buf, sizeof Buf, Lit->Bool? "TRUE" : "FALSE");
        else goto InvalidTypeConversion;

        Converted.Str = PStrCopy((const U8*)Buf, Len);
    }
    else if (TYPE_BOOLEAN == To)
    {
        if (IntegralTypeIsInteger(LitType))
            Converted.Bool = Lit->Int != 0;
        else if (IntegralTypeIsFloat(LitType))
            Converted.Bool = Lit->Flt != 0;
        else if (TYPE_POINTER == LitType)
            Converted.Bool = Lit->Ptr.As.UInt != 0;
        else if (TYPE_STRING == LitType)
            Converted.Bool = JavascriptStrToBool(&Lit->Str);
        else goto InvalidTypeConversion;
    }
    else if (IntegralTypeIsInteger(To))
    {
        if (IntegralTypeIsFloat(LitType))
            Converted.Int = Lit->Flt;
        else if (TYPE_POINTER == LitType)
            Converted.Int = Lit->Ptr.As.UInt;
        else if (TYPE_BOOLEAN == LitType)
            Converted.Int = 0 != Lit->Bool;
        else goto InvalidTypeConversion;
    }
    else if (IntegralTypeIsFloat(To))
    {
        if (IntegralTypeIsInteger(LitType))
            Converted.Flt = Lit->Int; /* truncated */
        else if (TYPE_BOOLEAN == LitType)
            Converted.Flt = 0 != Lit->Bool;
        else if (TYPE_POINTER == LitType)
            Converted.Flt = Lit->Ptr.As.UInt;
        else goto InvalidTypeConversion;
    }


    return Converted;
InvalidTypeConversion:
    ErrorAt(Compiler, Converter, "Cannot convert from %s to %s explicitly.",
            IntegralTypeToStr(LitType), IntegralTypeToStr(To)
    ); 
    return *Lit;
}

static VarLocation ConvertTypeExplicitly(PascalCompiler *Compiler, 
        const Token *Converter, IntegralType To, const VarLocation *Expr)
{
    PASCAL_NONNULL(Compiler);
    PASCAL_NONNULL(Expr);
    PASCAL_NONNULL(Converter);

    if (To == Expr->Type.Integral)
        return *Expr;

    VarLocation Converted = { 
        .LocationType = Expr->LocationType,
    };
    switch (Expr->LocationType)
    {
    case VAR_INVALID:
    case VAR_BUILTIN:
    case VAR_FLAG:
    case VAR_SUBROUTINE:
    {
        PASCAL_UNREACHABLE("");
    } break;

    case VAR_MEM:
    {
        PASCAL_UNREACHABLE("TODO: Convert type implicitly for VAR_MEM");
    } break;
    case VAR_REG:
    {
        PASCAL_UNREACHABLE("TODO: Convert type implicitly for VAR_REG");
    } break;
    case VAR_LIT:
    {
        if (TYPE_RECORD == To)
        {
            PASCAL_UNREACHABLE("Record literal????");
        }

        Converted.As.Literal = ConvertLiteralTypeExplicitly(Compiler, Converter, 
                To, &Expr->As.Literal, Expr->Type.Integral
        );
        Converted.Type.Integral = To;
        Converted.Type = VarTypeInit(To, IntegralTypeSize(To));
    } break;
    }

    return Converted;
}



static VarLocation FactorVariable(PascalCompiler *Compiler)
{
    PASCAL_NONNULL(Compiler);

    /* consumed iden */
    Token Identifier = Compiler->Curr;
    PascalVar *Variable = GetIdenInfo(Compiler, &Identifier, "Undefined identifier.");
    if (NULL == Variable)
        goto Error;

    /* typename that does not have a location */
    if (NULL == Variable->Location)
    {
        /* type casting */
        /* typename(expr) */
        if (ConsumeIfNextTokenIs(Compiler, TOKEN_LEFT_PAREN))
        {
            VarLocation Expr = FactorGrouping(Compiler);
            return ConvertTypeExplicitly(Compiler, &Identifier, Variable->Type.Integral, &Expr);
        }
        /* returns the type itself for sizeof */
        /* typename */
        VarLocation Type = {
            .Type = Variable->Type,
            .LocationType = VAR_TYPENAME,
        };
        return Type;
    }
    /* has a location, function? */
    if (TYPE_FUNCTION == Variable->Location->Type.Integral)
    {
        return FactorCall(Compiler, Variable->Location);
    }
    return *Variable->Location;
Error:
    return (VarLocation) { 0 };
}


static VarLocation VariableAddrOf(PascalCompiler *Compiler)
{
    PASCAL_NONNULL(Compiler);
    /* Curr is '@' */

    Token AtSign = Compiler->Curr;
    ConsumeOrError(Compiler, TOKEN_IDENTIFIER, "Expected variable name after '@'.");
    Token Identifier = Compiler->Curr;

    PascalVar *Variable = GetIdenInfo(Compiler, &Identifier, "Undefined variable.");
    if (NULL == Variable)
        return (VarLocation) { 0 };


    while (ConsumeIfNextTokenIs(Compiler, TOKEN_DOT))
    {
        PASCAL_UNREACHABLE("TODO: addr of record field");
    }

    if (NULL == Variable->Location)
        goto CannotTakeAddress;

    VarType Pointer = VarTypePtr(CompilerCopyType(Compiler, Variable->Type));
    VarLocation Ptr = PVMAllocateRegisterLocation(EMITTER(), Pointer);

    /* address of subroutine */
    if (VAR_SUBROUTINE == Variable->Location->LocationType)
    {
        U32 CallSite = PVMEmitLoadSubroutineAddr(EMITTER(), Ptr.As.Register, 0);
        PushSubroutineReference(Compiler, 
                &Variable->Location->As.SubroutineLocation, 
                CallSite
        );
        return Ptr;
    }

    /* address of memory */
    if (VAR_MEM == Variable->Location->LocationType)
    {
        PASCAL_ASSERT(VarTypeEqual(&Variable->Type, &Variable->Location->Type), "Unreachable");
        PVMEmitLoadAddr(EMITTER(), Ptr.As.Register, Variable->Location->As.Memory);
        return Ptr;
    }

CannotTakeAddress:
    ErrorAt(Compiler, &AtSign, "Cannot take the address this type of variable.");
    return Ptr;
}



static VarLocation NegateLiteral(PascalCompiler *Compiler, 
        const Token *OpToken, VarLiteral Literal, IntegralType LiteralType)
{
    VarLocation Location = {
        .LocationType = VAR_LIT,
    };
    if (IntegralTypeIsInteger(LiteralType))
    {
        Literal.Int = -Literal.Int;
        Location.Type = TypeOfIntLit(Literal.Int);
    }
    else if (IntegralTypeIsFloat(LiteralType))
    {
        Literal.Flt = -Literal.Flt;
        Location.Type.Integral = TYPE_F64;
        Location.Type.Size = sizeof(F64);
    }
    else 
    {
        INVALID_PREFIX_OP(OpToken, LiteralType);
    }

    Location.As.Literal = Literal;
    return Location;
}

static VarLocation NotLiteral(PascalCompiler *Compiler, 
        const Token *OpToken, VarLiteral Literal, IntegralType LiteralType)
{
    VarLocation Location = {
        .LocationType = VAR_LIT,
    };
    if (TYPE_BOOLEAN == LiteralType)
    {
        Literal.Bool = !Literal.Bool;
        Location.Type = VarTypeInit(TYPE_BOOLEAN, 1);
    }
    else if (IntegralTypeIsInteger(LiteralType))
    {
        Literal.Int = ~Literal.Int;
        Location.Type = TypeOfIntLit(Literal.Int);
    }
    else
    {
        INVALID_PREFIX_OP(OpToken, LiteralType);
    }
    Location.As.Literal = Literal;
    return Location;
}


static VarLocation ExprUnary(PascalCompiler *Compiler)
{
    TokenType Operator = Compiler->Curr.Type;
    Token OpToken = Compiler->Curr;
    VarLocation Value = ParsePrecedence(Compiler, PREC_FACTOR);

    if (VAR_INVALID == Value.LocationType)
    {
        ErrorAt(Compiler, &OpToken, 
                "unary operator '"STRVIEW_FMT"' cannot be applied to expression with no storage.",
                STRVIEW_FMT_ARG(&OpToken.Lexeme)
        );
    }
    else if (VAR_LIT == Value.LocationType)
    {
        switch (Operator)
        {
        case TOKEN_PLUS: break;
        case TOKEN_NOT:
        {
            Value = NotLiteral(Compiler, &OpToken, Value.As.Literal, Value.Type.Integral);
        } break;
        case TOKEN_MINUS:
        {
            Value = NegateLiteral(Compiler, &OpToken, Value.As.Literal, Value.Type.Integral);
        } break;
        default: goto Unreachable;
        }
    }
    else 
    {
        switch (Operator)
        {
        case TOKEN_PLUS: break;
        case TOKEN_MINUS:
        {
            if (!IntegralTypeIsFloat(Value.Type.Integral) 
            && !IntegralTypeIsInteger(Value.Type.Integral))
                goto TypeMismatch;

            PVMEmitNeg(EMITTER(), &Value, &Value);
        } break;
        case TOKEN_NOT:
        {
            if (!IntegralTypeIsInteger(Value.Type.Integral) 
            && TYPE_BOOLEAN != Value.Type.Integral)
                goto TypeMismatch;

            PVMEmitNot(EMITTER(), &Value, &Value);
        } break;
        default: goto Unreachable;
        }
    }
    return Value;

TypeMismatch:
    INVALID_PREFIX_OP(&OpToken, Value.Type.Integral);
    return Value;

Unreachable: 
    PASCAL_UNREACHABLE("%s is not handled in %s", TokenTypeToStr(Operator), __func__);
    return Value;
}




static bool CompareStringOrder(TokenType CompareType, const PascalStr *s1, const PascalStr *s2)
{
    switch (CompareType)
    {
    case TOKEN_EQUAL:
    {
        return PStrEqu(s1, s2);
    } break;
    case TOKEN_LESS_GREATER:
    {
        return !PStrEqu(s1, s2);
    } break;

    case TOKEN_LESS:
    {
        return PStrIsLess(s1, s2);
    } break;
    case TOKEN_GREATER:
    {
        return PStrIsLess(s2, s1);
    } break;
    case TOKEN_LESS_EQUAL:
    {
        return !PStrIsLess(s2, s1);
    } break;
    case TOKEN_GREATER_EQUAL:
    {
        return !PStrIsLess(s1, s2);
    } break;
    default: PASCAL_UNREACHABLE("Unreachable in: %s: %s", __func__, TokenTypeToStr(CompareType));
    }
    return false;
}




bool LiteralEqual(VarLiteral A, VarLiteral B, IntegralType Type)
{
    if (IntegralTypeIsFloat(Type)) 
        return A.Flt == B.Flt;
    if (IntegralTypeIsInteger(Type))
        return A.Flt == B.Flt;
    if (TYPE_POINTER == Type)
        return A.Ptr.As.Raw == B.Ptr.As.Raw;
    if (TYPE_STRING == Type) 
        return CompareStringOrder(TOKEN_EQUAL, &A.Str, &B.Str);
    if (Type == TYPE_BOOLEAN) 
        return A.Bool == B.Bool;
    return false;
}


static VarLocation LiteralExprBinary(PascalCompiler *Compiler, const Token *OpToken, 
        VarLocation *Left, VarLocation *Right)
{
#define COMMON_BIN_OP(Operator)\
    do {\
        if (IntegralTypeIsFloat(Both)) {\
            Result.As.Literal.Flt = Left->As.Literal.Flt Operator Right->As.Literal.Flt;\
            Result.Type = VarTypeInit(Both, IntegralTypeSize(Both));\
        } else if (IntegralTypeIsInteger(Both)) {\
            Result.As.Literal.Int = Left->As.Literal.Int Operator Right->As.Literal.Int;\
            Result.Type = TypeOfIntLit(Result.As.Literal.Int);\
        } else break;\
    } while (0)

#define COMPARE_OP(Operator) \
    do {\
        if (IntegralTypeIsFloat(Both)) {\
            Result.As.Literal.Bool = Left->As.Literal.Flt Operator Right->As.Literal.Flt;\
        } else if (IntegralTypeIsInteger(Both)) {\
            Result.As.Literal.Bool = Left->As.Literal.Int Operator Right->As.Literal.Int;\
        } else if (TYPE_POINTER == Both) {\
            Result.As.Literal.Bool = Left->As.Literal.Ptr.As.UInt Operator Right->As.Literal.Ptr.As.UInt;\
        } else if (TYPE_STRING == Both) {\
            Result.As.Literal.Bool = CompareStringOrder(OpToken->Type, &Left->As.Literal.Str, &Right->As.Literal.Str);\
        }\
        Result.Type = VarTypeInit(TYPE_BOOLEAN, 1);\
    } while (0)

    PASCAL_NONNULL(Compiler);
    PASCAL_NONNULL(OpToken);
    PASCAL_NONNULL(Left);
    PASCAL_NONNULL(Right);
    PASCAL_ASSERT(VAR_LIT == Left->LocationType, "left is not a literal in %s", __func__);
    PASCAL_ASSERT(VAR_LIT == Right->LocationType, "right is not a literal in %s", __func__);

    IntegralType Both = CoerceTypes(Left->Type.Integral, Right->Type.Integral);
    bool ConversionOk = ConvertTypeImplicitly(Compiler, Both, Left);
    ConversionOk = ConversionOk && ConvertTypeImplicitly(Compiler, Both, Right);
    if (!ConversionOk)
    {
        goto InvalidTypeConversion;
    }

    VarLocation Result = {
        .LocationType = VAR_LIT,
        .Type = VarTypeInit(Both, IntegralTypeSize(Both)),
    };

    switch (OpToken->Type)
    {
    case TOKEN_PLUS:
    { 
        COMMON_BIN_OP(+);
        if (TYPE_STRING == Both) 
        {
            /* shallow copy, Result now owns the string */
            Result.As.Literal.Str = Left->As.Literal.Str;
            PascalStr RightStr = Right->As.Literal.Str;
            PStrAppendStr(&Result.As.Literal.Str, PStrGetPtr(&RightStr), PStrGetLen(&RightStr));

            /* free the right string since it is a tmp literal */
            PStrDeinit(&RightStr);
        }
    } break;
    case TOKEN_MINUS: 
    {
        if (!IntegralTypeIsInteger(Both) || !IntegralTypeIsFloat(Both))
            goto InvalidTypeConversion;

        COMMON_BIN_OP(-);
    } break;
    case TOKEN_STAR:
    {
        if (IntegralTypeIsInteger(Both))
        {
            if (IntegralTypeIsSigned(Both))
            {
                Result.As.Literal.Int = (I64)Left->As.Literal.Int * (I64)Right->As.Literal.Int;
                Result.Type = TypeOfIntLit(Result.As.Literal.Int);
            }
            else 
            {
                Result.As.Literal.Int = Left->As.Literal.Int * Right->As.Literal.Int;
                Result.Type = TypeOfUIntLit(Result.As.Literal.Int);
            }
        }
        else if (IntegralTypeIsFloat(Both))
        {
            Result.As.Literal.Flt = Left->As.Literal.Flt * Right->As.Literal.Flt;
        } 
        else goto InvalidTypeConversion;
    } break;
    case TOKEN_DIV:
    case TOKEN_SLASH:
    {
        if (IntegralTypeIsInteger(Both))
        {
            if (0 == Right->As.Literal.Int)
            {
                ErrorAt(Compiler, OpToken, "Integer division by 0");
                break;
            }

            if (IntegralTypeIsSigned(Both))
            {
                Result.As.Literal.Int = (I64)Left->As.Literal.Int / (I64)Right->As.Literal.Int;
                Result.Type = TypeOfIntLit(Result.As.Literal.Int);
            }
            else 
            {
                Result.As.Literal.Int = Left->As.Literal.Int / Right->As.Literal.Int;
                Result.Type = TypeOfUIntLit(Result.As.Literal.Int);
            }
        }
        else if (IntegralTypeIsFloat(Both))
        {
            Result.As.Literal.Flt = Left->As.Literal.Flt / Right->As.Literal.Flt;
        } 
        else goto InvalidTypeConversion;
    } break;
    case TOKEN_MOD:
    {
        if (IntegralTypeIsInteger(Both))
        {
            if (0 == Right->As.Literal.Int)
            {
                ErrorAt(Compiler, OpToken, "Integer modulo by 0");
                break;
            }

            if (IntegralTypeIsSigned(Both))
            {
                Result.As.Literal.Int = (I64)Left->As.Literal.Int % (I64)Right->As.Literal.Int;
                Result.Type = TypeOfIntLit(Result.As.Literal.Int);
            }
            else 
            {
                Result.As.Literal.Int = Left->As.Literal.Int % Right->As.Literal.Int;
                Result.Type = TypeOfUIntLit(Result.As.Literal.Int);
            }
        }
        else goto InvalidTypeConversion;
    } break;

    case TOKEN_EQUAL:           
    {
        Result.As.Literal.Bool = LiteralEqual(Left->As.Literal, Right->As.Literal, Both);
        Result.Type = VarTypeInit(TYPE_BOOLEAN, 1);
    } break;
    case TOKEN_LESS_GREATER:
    {
        Result.As.Literal.Bool = !LiteralEqual(Left->As.Literal, Right->As.Literal, Both);
        Result.Type = VarTypeInit(TYPE_BOOLEAN, 1);
    } break;
    case TOKEN_LESS:            COMPARE_OP(<); break;
    case TOKEN_GREATER:         COMPARE_OP(>); break;
    case TOKEN_LESS_EQUAL:      COMPARE_OP(<=); break;
    case TOKEN_GREATER_EQUAL:   COMPARE_OP(>=); break;

    /* TODO: issue a warning if shift amount > integer width */
    case TOKEN_SHL:
    case TOKEN_LESS_LESS:
    {
        if (IntegralTypeIsInteger(Both))
        {
            Result.As.Literal.Int = Left->As.Literal.Int << Right->As.Literal.Int;
            Result.Type = TypeOfUIntLit(Result.As.Literal.Int);
        }
        else goto InvalidTypeConversion;
    } break;
    case TOKEN_SHR:
    case TOKEN_GREATER_GREATER:
    {
        if (IntegralTypeIsInteger(Both))
        {
            /* sign bit */
            if (Left->As.Literal.Int >> 63)
            {
                Result.As.Literal.Int = ArithmeticShiftRight(Left->As.Literal.Int, Right->As.Literal.Int);
                Result.Type = TypeOfIntLit(Result.As.Literal.Int);
            }
            else
            {
                Result.As.Literal.Int = Left->As.Literal.Int >> Right->As.Literal.Int;
                Result.Type = TypeOfUIntLit(Result.As.Literal.Int);
            }
        }
        else goto InvalidTypeConversion;
    } break;

    case TOKEN_AND:
    {
        PASCAL_ASSERT(TYPE_BOOLEAN != Both, "Unreachable");
        if (IntegralTypeIsInteger(Both))
        {
            Result.As.Literal.Int = Left->As.Literal.Int & Right->As.Literal.Int;
            Result.Type = TypeOfUIntLit(Result.As.Literal.Int);
        }
        else goto InvalidTypeConversion;
    } break;
    case TOKEN_OR:
    {
        PASCAL_ASSERT(TYPE_BOOLEAN != Both, "Unreachable");
        if (IntegralTypeIsInteger(Both))
        {
            Result.As.Literal.Int = Left->As.Literal.Int | Right->As.Literal.Int;
            Result.Type = TypeOfUIntLit(Result.As.Literal.Int);
        }
        else goto InvalidTypeConversion;
    } break;
    case TOKEN_XOR:
    {
        if (IntegralTypeIsInteger(Both))
        {
            Result.As.Literal.Int = Left->As.Literal.Int ^ Right->As.Literal.Int;
            Result.Type = TypeOfUIntLit(Result.As.Literal.Int);
        }
        else goto InvalidTypeConversion;
    } break;
    default:
    {
        PASCAL_UNREACHABLE("Unreachable operator in %s: %s", __func__, TokenTypeToStr(OpToken->Type));
    } break;
    }

    if (TYPE_INVALID != Result.Type.Integral)
    {
        return Result;
    }

InvalidTypeConversion:
    INVALID_BINARY_OP(OpToken, Left->Type.Integral, Right->Type.Integral);
    return *Left;
#undef COMMON_BIN_OP
#undef COMPARE_OP
}


static VarLocation RuntimeExprBinary(PascalCompiler *Compiler, 
        const Token *OpToken, VarLocation *Left, VarLocation *Right)
{
    PASCAL_NONNULL(Compiler);
    PASCAL_NONNULL(OpToken);
    PASCAL_NONNULL(Left);
    PASCAL_NONNULL(Right);
    PASCAL_ASSERT(VAR_INVALID != Left->LocationType, "Unreachable");
    PASCAL_ASSERT(VAR_INVALID != Right->LocationType, "Unreachable");

    VarLocation Tmp = *Left;
    VarLocation Src = *Right;
    if (VAR_LIT == Left->LocationType)
    {
        Tmp = *Right;
        Src = *Left;
    }

    /* typecheck */
    IntegralType Type = CoerceTypes(Left->Type.Integral, Right->Type.Integral);
    bool ConversionOk = ConvertTypeImplicitly(Compiler, Type, &Tmp) 
        && ConvertTypeImplicitly(Compiler, Type, &Src);
    if (!ConversionOk) 
    {
        INVALID_BINARY_OP(OpToken, Tmp.Type.Integral, Src.Type.Integral);
        return *Left;
    }

    VarLocation Dst;
    switch (OpToken->Type)
    {
    case TOKEN_PLUS: 
    {
        PVMEmitIntoRegLocation(EMITTER(), &Dst, false, &Tmp);
        PVMEmitAdd(EMITTER(), &Dst, &Src);
    } break;
    case TOKEN_MINUS:
    {
        PVMEmitIntoRegLocation(EMITTER(), &Dst, false, &Tmp);
        PVMEmitSub(EMITTER(), &Dst, &Src);
    } break;
    case TOKEN_STAR:
    {
        PVMEmitIntoRegLocation(EMITTER(), &Dst, false, &Tmp);
        PVMEmitMul(EMITTER(), &Dst, &Src);
    } break;
    case TOKEN_DIV:
    case TOKEN_SLASH:
    {
        PVMEmitIntoRegLocation(EMITTER(), &Dst, false, &Tmp);
        PVMEmitDiv(EMITTER(), &Dst, &Src);
    } break;
    case TOKEN_MOD:
    {
        PVMEmitIntoRegLocation(EMITTER(), &Dst, false, &Tmp);
        PVMEmitMod(EMITTER(), &Dst, &Src);
    } break;

    case TOKEN_SHL:
    case TOKEN_LESS_LESS:
    {
        PVMEmitIntoRegLocation(EMITTER(), &Dst, false, &Tmp);
        PVMEmitShl(EMITTER(), &Dst, &Src);
    } break;

    case TOKEN_GREATER_GREATER:
    case TOKEN_SHR:
    {
        PVMEmitIntoRegLocation(EMITTER(), &Dst, false, &Tmp);
        PVMEmitShr(EMITTER(), &Dst, &Src);
    } break;

    case TOKEN_LESS:
    case TOKEN_GREATER:
    case TOKEN_LESS_EQUAL:
    case TOKEN_GREATER_EQUAL:
    case TOKEN_LESS_GREATER:
    case TOKEN_EQUAL:
    {
        /* flag will be set instead of register */
        PVMEmitIntoRegLocation(EMITTER(), &Dst, true, &Tmp);

        if (IntegralTypeIsInteger(Dst.Type.Integral) 
        && VAR_LIT != Left->LocationType && VAR_LIT != Right->LocationType
        && (IntegralTypeIsSigned(Left->Type.Integral) != IntegralTypeIsSigned(Right->Type.Integral)))
        {
            ErrorAt(Compiler, OpToken, 
                    "Comparison between integers of different sign (%s and %s) is not allowed.",
                    VarTypeToStr(Left->Type), VarTypeToStr(Right->Type)
            );
            return *Left;
        }
        VarLocation Cond = PVMEmitSetFlag(EMITTER(), OpToken->Type, &Dst, &Src);
        FreeExpr(Compiler, Dst);
        FreeExpr(Compiler, Src);
        return Cond;
    } break;
    case TOKEN_AND:
    {
        PVMEmitIntoRegLocation(EMITTER(), &Dst, false, &Tmp);
        PVMEmitAnd(EMITTER(), &Dst, &Src);
    } break;
    case TOKEN_OR:
    {
        PVMEmitIntoRegLocation(EMITTER(), &Dst, false, &Tmp);
        PVMEmitOr(EMITTER(), &Dst, &Src);
    } break;
    case TOKEN_XOR:
    {
        PVMEmitIntoRegLocation(EMITTER(), &Dst, false, &Tmp);
        PVMEmitXor(EMITTER(), &Dst, &Src);
    } break;

    default: 
    {
        PASCAL_UNREACHABLE("Unhandled binary op: "STRVIEW_FMT, STRVIEW_FMT_ARG(&OpToken->Lexeme));
        return Src;
    } break;
    }

    FreeExpr(Compiler, Src);
    return Dst;
}


static VarLocation ExprBinary(PascalCompiler *Compiler, VarLocation *Left)
{
    Token OpToken = Compiler->Curr;
    const PrecedenceRule *OperatorRule = GetPrecedenceRule(OpToken.Type);

    /* +1 to parse binary oper as left associative,
     * ex:  parses  1 + 2 + 3 
     *      as      ((1 + 2) + 3)
     *      not     (1 + (2 + 3)) 
     */
    VarLocation Right = ParsePrecedence(Compiler, OperatorRule->Prec + 1);
    bool LeftInvalid = VAR_INVALID == Left->LocationType;
    bool RightInvalid = VAR_INVALID == Right.LocationType;

    if (LeftInvalid || RightInvalid)
    {
        const char *Msg = "Expression on both sides of '"STRVIEW_FMT"' does not have a storage class.";
        if (LeftInvalid && !RightInvalid)
            Msg = "Expression on the left of '"STRVIEW_FMT"' does not have a storage class.";
        else if (!LeftInvalid && RightInvalid)
            Msg = "Expression on the right of '"STRVIEW_FMT"' does not have a storage class.";

        ErrorAt(Compiler, &OpToken, Msg, STRVIEW_FMT_ARG(&OpToken.Lexeme));
    }
    else if (TYPE_RECORD == Left->Type.Integral)
    {
        PASCAL_UNREACHABLE("No binary op on record type.");
    }
    else if (VAR_LIT == Left->LocationType && VAR_LIT == Right.LocationType)
    {
        return LiteralExprBinary(Compiler, &OpToken, Left, &Right);
    }
    else 
    {
        return RuntimeExprBinary(Compiler, &OpToken, Left, &Right);
    }
    return *Left;
}


static VarLocation ExprAnd(PascalCompiler *Compiler, VarLocation *Left)
{
    PASCAL_NONNULL(Compiler);
    PASCAL_NONNULL(Left);

    VarLocation Right = *Left;
    Token OpToken = Compiler->Curr;
    if (TYPE_BOOLEAN == Left->Type.Integral)
    {
        bool WasEmit = EMITTER()->ShouldEmit;
        if (VAR_LIT == Left->LocationType && !Left->As.Literal.Bool)
        {
            /* false and ...: don't emit right */
            EMITTER()->ShouldEmit = false;
        }

        U32 FromLeft = PVMEmitBranchIfFalse(EMITTER(), Left);

        Right = ParsePrecedence(Compiler, GetPrecedenceRule(OpToken.Type)->Prec + 1);
        IntegralType ResultType = CoerceTypes(Left->Type.Integral, Right.Type.Integral);
        if (TYPE_INVALID == ResultType)
            goto InvalidOperands;
        PASCAL_ASSERT(TYPE_BOOLEAN == ResultType, "");
        PASCAL_ASSERT(TYPE_BOOLEAN == Right.Type.Integral, "");

        PVMPatchBranchToCurrent(EMITTER(), FromLeft);

        EMITTER()->ShouldEmit = WasEmit;
    }
    else if (IntegralTypeIsInteger(Left->Type.Integral))
    {
        return ExprBinary(Compiler, Left);
    }
    else 
    {
        ErrorAt(Compiler, &OpToken, "Invalid left operand for 'and': %s",
                VarTypeToStr(Left->Type)
        );
    }

    return Right;
InvalidOperands:
    INVALID_BINARY_OP(&OpToken, Left->Type.Integral, Right.Type.Integral);
    return *Left;
}

static VarLocation ExprOr(PascalCompiler *Compiler, VarLocation *Left)
{
    PASCAL_NONNULL(Compiler);
    PASCAL_NONNULL(Left);


    VarLocation Right = *Left;
    Token OpToken = Compiler->Curr;
    if (TYPE_BOOLEAN == Left->Type.Integral)
    {
        bool WasEmit = EMITTER()->ShouldEmit;
        if (VAR_LIT == Left->LocationType && Left->As.Literal.Bool)
        {
            /* true or ...: don't emit right */
            EMITTER()->ShouldEmit = false;
        }

        U32 FromTrue = PVMEmitBranchIfTrue(EMITTER(), Left);

        Right = ParsePrecedence(Compiler, GetPrecedenceRule(OpToken.Type)->Prec + 1);
        IntegralType ResultType = CoerceTypes(Left->Type.Integral, Right.Type.Integral);
        if (TYPE_INVALID == ResultType)
            goto InvalidOperands;
        PASCAL_ASSERT(TYPE_BOOLEAN == ResultType, "");
        PASCAL_ASSERT(TYPE_BOOLEAN == Right.Type.Integral, "");

        PVMPatchBranchToCurrent(EMITTER(), FromTrue);

        EMITTER()->ShouldEmit = WasEmit;
    }
    else if (IntegralTypeIsInteger(Left->Type.Integral))
    {
        return ExprBinary(Compiler, Left);
    }
    else 
    {
        ErrorAt(Compiler, &OpToken, "Invalid left operand for 'or': %s",
                VarTypeToStr(Left->Type)
        );
    }

    return Right;
InvalidOperands:
    INVALID_BINARY_OP(&OpToken, Left->Type.Integral, Right.Type.Integral);
    return *Left;
}




static const PrecedenceRule sPrecedenceRuleLut[TOKEN_TYPE_COUNT] = 
{
    [TOKEN_CHAR_LITERAL]    = { FactorLiteral,      NULL,           PREC_SINGLE },
    [TOKEN_INTEGER_LITERAL] = { FactorLiteral,      NULL,           PREC_SINGLE },
    [TOKEN_NUMBER_LITERAL]  = { FactorLiteral,      NULL,           PREC_SINGLE },
    [TOKEN_STRING_LITERAL]  = { FactorLiteral,      NULL,           PREC_SINGLE },
    [TOKEN_TRUE]            = { FactorLiteral,      NULL,           PREC_SINGLE },
    [TOKEN_FALSE]           = { FactorLiteral,      NULL,           PREC_SINGLE },
    [TOKEN_IDENTIFIER]      = { FactorVariable,     NULL,           PREC_SINGLE },

    [TOKEN_CARET]           = { NULL,               VariableDeref,  PREC_VARIABLE },
    [TOKEN_DOT]             = { NULL,               VariableAccess, PREC_VARIABLE },
    [TOKEN_AT]              = { VariableAddrOf,     NULL,           PREC_SINGLE },
    [TOKEN_LEFT_BRACKET]    = { NULL,               ArrayAccess,    PREC_VARIABLE },

    [TOKEN_LEFT_PAREN]      = { FactorGrouping,     FactorCall,     PREC_VARIABLE },
    [TOKEN_NOT]             = { ExprUnary,          NULL,           PREC_FACTOR },

    [TOKEN_STAR]            = { NULL,               ExprBinary,     PREC_TERM },
    [TOKEN_DIV]             = { NULL,               ExprBinary,     PREC_TERM },
    [TOKEN_SLASH]           = { NULL,               ExprBinary,     PREC_TERM },
    [TOKEN_MOD]             = { NULL,               ExprBinary,     PREC_TERM },

    [TOKEN_XOR]             = { NULL,               ExprBinary,     PREC_SIMPLE },
    [TOKEN_PLUS]            = { ExprUnary,          ExprBinary,     PREC_SIMPLE },
    [TOKEN_MINUS]           = { ExprUnary,          ExprBinary,     PREC_SIMPLE },
    [TOKEN_SHR]             = { NULL,               ExprBinary,     PREC_SIMPLE },
    [TOKEN_SHL]             = { NULL,               ExprBinary,     PREC_SIMPLE },
    [TOKEN_LESS_LESS]       = { NULL,               ExprBinary,     PREC_SIMPLE },
    [TOKEN_GREATER_GREATER] = { NULL,               ExprBinary,     PREC_SIMPLE },

    [TOKEN_AND]             = { NULL,               ExprAnd,        PREC_TERM },
    [TOKEN_OR]              = { NULL,               ExprOr,         PREC_SIMPLE },

    [TOKEN_LESS]            = { NULL,               ExprBinary,     PREC_EXPR },
    [TOKEN_GREATER]         = { NULL,               ExprBinary,     PREC_EXPR },
    [TOKEN_LESS_EQUAL]      = { NULL,               ExprBinary,     PREC_EXPR },
    [TOKEN_GREATER_EQUAL]   = { NULL,               ExprBinary,     PREC_EXPR },
    [TOKEN_EQUAL]           = { NULL,               ExprBinary,     PREC_EXPR },
    [TOKEN_LESS_GREATER]    = { NULL,               ExprBinary,     PREC_EXPR },
};


static const PrecedenceRule *GetPrecedenceRule(TokenType CurrentTokenType)
{
    return &sPrecedenceRuleLut[CurrentTokenType];
}

static VarLocation ParseAssignmentLhs(PascalCompiler *Compiler, Precedence Prec)
{
    const PrefixParseRoutine PrefixRoutine = 
        GetPrecedenceRule(Compiler->Curr.Type)->PrefixRoutine;
    if (NULL == PrefixRoutine)
    {
        ErrorAt(Compiler, &Compiler->Curr, "Expected expression.");
        return (VarLocation) { 0 };
    }


    VarLocation Left = PrefixRoutine(Compiler);


    /* only parse next op if they have the same or lower precedence */
    while (Prec <= GetPrecedenceRule(Compiler->Next.Type)->Prec)
    {
        const InfixParseRoutine InfixRoutine = 
            GetPrecedenceRule(Compiler->Next.Type)->InfixRoutine;
        if (NULL == InfixRoutine)
        {
            Error(Compiler, "Expected infix operator.");
            return (VarLocation) { 0 };
        }
        ConsumeToken(Compiler); /* the operator */
        PASCAL_NONNULL(InfixRoutine);
        Left = InfixRoutine(Compiler, &Left);
    }
    return Left;
}

static VarLocation ParsePrecedence(PascalCompiler *Compiler, Precedence Prec)
{
    ConsumeToken(Compiler);
    return ParseAssignmentLhs(Compiler, Prec);
}






/* returns the type that both sides should be */
IntegralType CoerceTypes(IntegralType Left, IntegralType Right)
{
    PASCAL_ASSERT(Left >= TYPE_INVALID && Right >= TYPE_INVALID, "");
    PASCAL_ASSERT(Left < TYPE_COUNT && Right < TYPE_COUNT, "Impossible");

    IntegralType CommonType = sCoercionRules[Left][Right];
    if (TYPE_INVALID != CommonType)
    {
        return CommonType;
    }
    return TYPE_INVALID;
}



bool ConvertRegisterTypeImplicitly(PVMEmitter *Emitter, 
        IntegralType To, VarRegister *From, IntegralType FromType)
{
    if (To == FromType)
        return true;

    if (IntegralTypeIsInteger(To))
    {
        if (IntegralTypeIsInteger(FromType))
        {
            PVMEmitIntegerTypeConversion(Emitter, *From, To, *From, FromType);
        }
        else return false;
    }
    else if (IntegralTypeIsFloat(To))
    {
        if (IntegralTypeIsFloat(FromType))
        {
            PVMEmitFloatTypeConversion(Emitter, *From, To, *From, FromType);
        }
        else if (IntegralTypeIsInteger(FromType))
        {
            VarRegister FloatReg = PVMAllocateRegister(Emitter, To);
            PVMEmitIntToFltTypeConversion(Emitter, FloatReg, To, *From, FromType);
            PVMFreeRegister(Emitter, *From);
            *From = FloatReg;
        }
        else return false;
    }
    else return false;
    return true;
}

bool ConvertTypeImplicitly(PascalCompiler *Compiler, IntegralType To, VarLocation *From)
{
    IntegralType FromType = From->Type.Integral;
    if (To == FromType)
    {
        return true;
    }
    switch (From->LocationType)
    {
    case VAR_LIT:
    {
        if (IntegralTypeIsInteger(To) && IntegralTypeIsInteger(FromType))
            break;

        if (IntegralTypeIsFloat(To))
        {
            if (IntegralTypeIsFloat(FromType))
                break;
            if (IntegralTypeIsInteger(FromType))
            {
                From->As.Literal.Flt = From->As.Literal.Int;
                break;
            }
        }

        goto InvalidTypeConversion;
    } break;
    case VAR_REG:
    {
        if (!ConvertRegisterTypeImplicitly(EMITTER(), To, &From->As.Register, From->Type.Integral))
        {
            goto InvalidTypeConversion;
        }
    } break;
    case VAR_MEM:
    {
        VarRegister OutTarget;
        PVMEmitIntoReg(EMITTER(), &OutTarget, false, From);
        if (!ConvertRegisterTypeImplicitly(EMITTER(), To, &OutTarget, FromType))
            goto InvalidTypeConversion;

        FreeExpr(Compiler, *From);
        From->LocationType = VAR_REG;
        From->As.Register = OutTarget;
    } break;
    case VAR_FLAG:
    {
        if (TYPE_BOOLEAN != To || TYPE_BOOLEAN != FromType)
            goto InvalidTypeConversion;
    } break;
    case VAR_INVALID:
    case VAR_SUBROUTINE: 
    case VAR_BUILTIN:
    {
        PASCAL_UNREACHABLE("");
    } break;
    }

    From->Type = VarTypeInit(To, IntegralTypeSize(To));
    return true;
InvalidTypeConversion:
    Error(Compiler, "Cannot convert from %s to %s.", VarTypeToStr(From->Type), IntegralTypeToStr(To));
    return false;
}




