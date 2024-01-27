

#include <inttypes.h>
#include <stdio.h>

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
    PREC_SINGLE = 0, /* single token */
    PREC_EXPR,
    PREC_SIMPLE,
    PREC_TERM,
    PREC_FACTOR,
    PREC_VARIABLE,
    PREC_HIGHEST,
} Precedence;
typedef VarLocation (*InfixParseRoutine)(PascalCompiler *, VarLocation *, bool);
typedef VarLocation (*PrefixParseRoutine)(PascalCompiler *, bool);


#define INVALID_BINARY_OP(pOp, LType, RType) \
    ErrorAt(Compiler, pOp, "Invalid operand for binary operator '"STRVIEW_FMT"': %s and %s", \
        STRVIEW_FMT_ARG((pOp)->Lexeme), \
        IntegralTypeToStr(LType), IntegralTypeToStr(RType)\
    )
#define INVALID_PREFIX_OP(pOp, Type) \
    ErrorAt(Compiler, pOp, "Invalid operand for prefix operator '"STRVIEW_FMT"': %s", \
        STRVIEW_FMT_ARG((pOp)->Lexeme), IntegralTypeToStr(Type)\
    )
#define INVALID_POSTFIX_OP(pOp, Type) \
    ErrorAt(Compiler, pOp, "Invalid operand for postfix operator '"STRVIEW_FMT"': %s", \
        STRVIEW_FMT_ARG((pOp)->Lexeme), IntegralTypeToStr(Type)\
    )

typedef struct PrecedenceRule
{
    PrefixParseRoutine PrefixRoutine;
    InfixParseRoutine InfixRoutine;
    Precedence Prec;
} PrecedenceRule;


static const PrecedenceRule *GetPrecedenceRule(TokenType CurrentTokenType);
static VarLocation ParsePrecedence(PascalCompiler *Compiler, Precedence Prec, bool ShouldCallFunction);
static VarLocation ParseAssignmentLhs(PascalCompiler *Compiler, Precedence Prec, bool ShouldCallFunction);


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
    PASCAL_NONNULL(Compiler);
    return ParsePrecedence(Compiler, PREC_EXPR, true);
}

VarLocation CompileExprIntoReg(PascalCompiler *Compiler)
{
    PASCAL_NONNULL(Compiler);
    VarLocation Expr = CompileExpr(Compiler);
    VarLocation Reg;
    PVMEmitIntoRegLocation(EMITTER(), &Reg, false, &Expr); 
    /* ownership is transfered */
    return Reg;
}

VarLocation CompileVariableExpr(PascalCompiler *Compiler)
{
    PASCAL_NONNULL(Compiler);
    return ParseAssignmentLhs(Compiler, PREC_VARIABLE, true);
}

void CompileExprInto(PascalCompiler *Compiler, const Token *ErrorSpot, VarLocation *Location)
{
    PASCAL_NONNULL(Compiler);
    PASCAL_NONNULL(Location);

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

    StringView LocType, ExprType;
InvalidTypeCombination:
    LocType = VarTypeToStringView(Location->Type);
    ExprType = VarTypeToStringView(Expr.Type);
    if (NULL == ErrorSpot)
    {
        Error(Compiler, "Invalid type combination: "STRVIEW_FMT" and "STRVIEW_FMT".",
            STRVIEW_FMT_ARG(LocType), STRVIEW_FMT_ARG(ExprType)
        );
    }
    else
    {
        ErrorAt(Compiler, ErrorSpot, "Invalid type combination: "STRVIEW_FMT" and "STRVIEW_FMT".",
            STRVIEW_FMT_ARG(LocType), STRVIEW_FMT_ARG(ExprType)
        );
    }
}





static VarType TypeOfIntLit(U64 Int)
{
    if (IN_I32((I64)Int))
        return VarTypeInit(TYPE_I32, 4);
    return VarTypeInit(TYPE_I64, 8);
}


static VarLocation FactorLiteral(PascalCompiler *Compiler, bool ShouldCallFunction)
{
    PASCAL_NONNULL(Compiler);

    /* literals are not functions */
    UNUSED(ShouldCallFunction);

    VarLocation Location = {
        .LocationType = VAR_LIT,
    };

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

    /* get function type */
    const VarType *Type = &Location->Type;
    if (TYPE_POINTER == Type->Integral)
    {
        Type = Type->As.Pointee;
        if (NULL == Type || TYPE_FUNCTION != Type->Integral)
        {
            StringView Type = VarTypeToStringView(Location->Type);
            ErrorAt(Compiler, Callee, "Cannot call "STRVIEW_FMT".", STRVIEW_FMT_ARG(Type));
            return (VarLocation) { 0 };
        }
    }
    const SubroutineData *Subroutine = &Type->As.Subroutine;
    const VarType *ReturnType = Subroutine->ReturnType;
    if (NULL == ReturnType)
    {
        /* no return value, but in expression */
        ErrorAt(Compiler, Callee, "Procedure '"STRVIEW_FMT"' does not have a return value.",
                STRVIEW_FMT_ARG(Callee->Lexeme)
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


    /* 
     * TODO: this is calling convention dependent, 
     * in cdecl we can allocate stack space once for all function call and forget about it, 
     * but in stdcall, the callee cleans up the stack 
     */
    /* did we allocate stack space for arguments? */
    if (Subroutine->StackArgSize)
    {
        /* deallocate stack space */
        PVMEmitStackAllocation(EMITTER(), -Subroutine->StackArgSize);
    }

    /* restore caller regs */
    PVMEmitUnsaveCallerRegs(EMITTER(), ReturnReg, SaveRegs);
    return ReturnValue;
}


static VarLocation FactorCall(PascalCompiler *Compiler, VarLocation *Location, bool ShouldCallFunction)
{
    UNUSED(ShouldCallFunction); /* we are forced to call the function here */
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
                STRVIEW_FMT_ARG(Callee.Lexeme)
            );
            return (VarLocation) { 0 };
        }
        return Opt.ReturnValue;
    }

    VarLocation ReturnValue = CompileCallWithReturnValue(Compiler, Location, &Callee);
    FreeExpr(Compiler, *Location);
    return ReturnValue;
}



static VarLocation VariableDeref(PascalCompiler *Compiler, VarLocation *Variable, bool ShouldCallFunction)
{
    UNUSED(ShouldCallFunction);
    PASCAL_NONNULL(Compiler);
    PASCAL_NONNULL(Variable);

    Token Caret = Compiler->Curr;
    if (TYPE_POINTER != Variable->Type.Integral)
    {
        StringView Type = VarTypeToStringView(Variable->Type);
        ErrorAt(Compiler, &Caret, STRVIEW_FMT" is not dereferenceable.", 
            STRVIEW_FMT_ARG(Type)
        );
        return *Variable;
    }
    if (NULL == Variable->Type.As.Pointee)
    {
        ErrorAt(Compiler, &Caret, "Cannot dereference an opaque pointer.");
        return *Variable;
    }

    /* 
     * to dereference, 
     * we create a new var location that represents the pointee in memory 
     */
    VarType PointeeType = *Variable->Type.As.Pointee;
    VarLocation Ptr;
    PVMEmitIntoRegLocation(EMITTER(), &Ptr, true, Variable);
    return VAR_LOCATION_MEM(
        .RegPtr = Ptr.As.Register, 
        0, 
        PointeeType
    );
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

static VarLocation VariableAccess(PascalCompiler *Compiler, VarLocation *Left, bool ShouldCallFunction)
{
    UNUSED(ShouldCallFunction); /* use '(' to actually call a method */
    PASCAL_NONNULL(Compiler);
    PASCAL_NONNULL(Left);

    /* identifier consumed */
    Token Dot = Compiler->Curr;
    ConsumeOrError(Compiler, TOKEN_IDENTIFIER, "Expected member name.");
    if (TYPE_RECORD != Left->Type.Integral)
    {
        StringView Type = VarTypeToStringView(Left->Type);
        ErrorAt(Compiler, &Dot, STRVIEW_FMT" does not have a member.", STRVIEW_FMT_ARG(Type));
        return *Left;
    }

    Token Name = Compiler->Curr;
    VarLocation *Member = FindRecordMember(&Left->Type.As.Record.Field, &Name);
    if (NULL == Member)
    {
        ErrorAt(Compiler, &Name, "'"STRVIEW_FMT"' is not a member of record '"STRVIEW_FMT"'.", 
            STRVIEW_FMT_ARG(Name.Lexeme), STRVIEW_FMT_ARG(Left->Type.As.Record.Name)
        );
        return *Left;
    }

    /* member offset + record offset = location */
    if (VAR_MEM == Left->LocationType)              /* regular record */
    {
        /* location: 
         *      reg: ptr of left 
         *      loc: offset of left + offset of member 
         */
        return VAR_LOCATION_MEM(
            .RegPtr = Left->As.Memory.RegPtr, 
            Left->As.Memory.Location + Member->As.Memory.Location,
            Member->Type
        );
    }
    else if (VAR_REG == Left->LocationType)         /* return register holding the addr of the record */
    {
        /* location: 
         *      reg: left (left is the pointer register)
         *      loc: offset of member 
         * */
        return VAR_LOCATION_MEM(
            .RegPtr = Left->As.Register,
            Member->As.Memory.Location,
            Member->Type
        );
    }
    else if (VAR_TYPENAME == Left->LocationType)    /* returns the member type for sizeof */
    {
        return (VarLocation) {
            .Type = Member->Type,
            .LocationType = VAR_TYPENAME
        };
    }
    else 
    {
        ErrorAt(Compiler, &Dot, "Left side of '.' does not have a valid location.");
    }
    return *Left;
}

static VarLocation ArrayAccess(PascalCompiler *Compiler, VarLocation *Left, bool ShouldCallFunction)
{
    UNUSED(ShouldCallFunction);
    PASCAL_NONNULL(Compiler);
    PASCAL_NONNULL(Left);

    Token LeftBracket = Compiler->Curr;
    VarLocation Index = CompileExpr(Compiler);
    ConsumeOrError(Compiler, TOKEN_RIGHT_BRACKET, "Expected ']' after expression.");
    if (!IntegralTypeIsOrdinal(Index.Type.Integral))
        ErrorAt(Compiler, &LeftBracket, "Array index expression must be an ordinal type.");

    IntegralType VariableType = Left->Type.Integral;
    VarLocation Element = { 0 };
    if (VariableType == TYPE_STATIC_ARRAY)
    {
        /* TODO: range check */
        if (Left->LocationType == VAR_TYPENAME)
        {
            /* FPC allowed array name to be indexable for sizeof:
             * ```
             * type TArray = array[0..5] of integer;
             * begin
             *   writeln(sizeof(TArray) / sizeof(TArray[0]));
             * end.
             * ```
             * To facilitate that, we just return the type name of the array's element
             */
            PASCAL_NONNULL(Left->Type.As.StaticArray.ElementType);
            Element.LocationType = VAR_TYPENAME;
            Element.Type = *Left->Type.As.StaticArray.ElementType;
        }
        else
        {
            Element = PVMEmitLoadArrayElement(EMITTER(), Left, &Index);
        }
    }
    else if (VariableType == TYPE_STRING && Left->LocationType == VAR_MEM)
    {
        VarType ElementType = VarTypeInit(TYPE_CHAR, 1);
        VarType FauxArrayType = VarTypeStaticArray((RangeIndex){.Low = 0, .High = 255}, &ElementType);
        const VarLocation FauxArray = VAR_LOCATION_MEM(
            Left->As.Memory.RegPtr, 
            Left->As.Memory.Location, 
            FauxArrayType
        );
        Element = PVMEmitLoadArrayElement(EMITTER(), &FauxArray, &Index);
    }
    else 
    {
        StringView Type = VarTypeToStringView(Left->Type);
        ErrorAt(Compiler, &LeftBracket, "Cannot index "STRVIEW_FMT".", 
            STRVIEW_FMT_ARG(Type)
        );
    }
    FreeExpr(Compiler, Index);
    return Element;
}


static VarLocation FactorGrouping(PascalCompiler *Compiler, bool ShouldCallFunction)
{
    UNUSED(ShouldCallFunction);

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



static VarLocation FactorVariable(PascalCompiler *Compiler, bool ShouldCallFunction)
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
            VarLocation Expr = FactorGrouping(Compiler, false);
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
    if (TYPE_FUNCTION == Variable->Location->Type.Integral && ShouldCallFunction)
    {
        return FactorCall(Compiler, Variable->Location, true);
    }
    return *Variable->Location;
Error:
    return (VarLocation) { 0 };
}


static VarLocation VariableAddrOf(PascalCompiler *Compiler, bool ShouldCallFunction)
{
    PASCAL_NONNULL(Compiler);
    UNUSED(ShouldCallFunction);

    /* current at the '@' token */
    Token AtSign = Compiler->Curr;
    VarLocation Variable = ParsePrecedence(Compiler, PREC_VARIABLE, false);

    if (Variable.LocationType == VAR_TYPENAME
    || Variable.LocationType == VAR_LIT
    || Variable.LocationType == VAR_REG
    || Variable.LocationType == VAR_FLAG
    || Variable.LocationType == VAR_BUILTIN)
    {
        ErrorAt(Compiler, &AtSign, "Address cannot be taken.");
        return (VarLocation) { 0 };
    }


    VarType PtrType = VarTypePtr(CompilerCopyType(Compiler, Variable.Type));
    VarLocation Ptr = PVMAllocateRegisterLocation(EMITTER(), PtrType);
    if (Variable.LocationType == VAR_MEM)
    {
        PVMEmitLoadAddr(EMITTER(), Ptr.As.Register, Variable.As.Memory);
    }
    else if (Variable.LocationType == VAR_SUBROUTINE)
    {
        Ptr.Type = VarTypePtr(CompilerCopyType(Compiler, Variable.Type));
        U32 CallSite = PVMEmitLoadSubroutineAddr(EMITTER(), Ptr.As.Register, 0);
        PushSubroutineReference(Compiler, Variable.As.SubroutineLocation, CallSite);
    }
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
        Location.Type = VarTypeInit(LiteralType, IntegralTypeSize(LiteralType));
    }
    else
    {
        INVALID_PREFIX_OP(OpToken, LiteralType);
    }
    Location.As.Literal = Literal;
    return Location;
}


static VarLocation ExprUnary(PascalCompiler *Compiler, bool ShouldCallFunction)
{
    UNUSED(ShouldCallFunction);
    PASCAL_NONNULL(Compiler);

    TokenType Operator = Compiler->Curr.Type;
    Token OpToken = Compiler->Curr;
    VarLocation Value = ParsePrecedence(Compiler, PREC_FACTOR, true);
    IntegralType Type = Value.Type.Integral;

    VarLocation Ret;
    switch (Operator)
    {
    case TOKEN_PLUS:
    {
        if (!IntegralTypeIsInteger(Type) && !IntegralTypeIsFloat(Type))
            goto InvalidOperand;
        if (VAR_LIT != Value.LocationType 
        && VAR_MEM != Value.LocationType 
        && VAR_REG != Value.LocationType)
            goto InvalidStorage;

        Ret = Value;
    } break;
    case TOKEN_MINUS:
    {
        if (!IntegralTypeIsInteger(Type) && !IntegralTypeIsFloat(Type))
            goto InvalidOperand;

        if (VAR_LIT == Value.LocationType)
        {
            Ret = NegateLiteral(Compiler, &OpToken, Value.As.Literal, Type);
        }
        else if (VAR_MEM == Value.LocationType || VAR_REG == Value.LocationType)
        {
            PVMEmitIntoRegLocation(EMITTER(), &Ret, false, &Value);
            PVMEmitNeg(EMITTER(), Ret.As.Register, Ret.As.Register, Value.Type.Integral);
        }
        else goto InvalidStorage;
    } break;
    case TOKEN_NOT:
    {
        if (!IntegralTypeIsInteger(Type) && TYPE_BOOLEAN != Type)
            goto InvalidOperand;

        /* special boolean case */
        if (VAR_FLAG == Value.LocationType)
        {
            PASCAL_ASSERT(Type == TYPE_BOOLEAN, "Unreachable");
            Ret = Value;
            Ret.As.FlagValueAsIs = !Ret.As.FlagValueAsIs;
        }
        else if (VAR_LIT == Value.LocationType)
        {
            Ret = NotLiteral(Compiler, &OpToken, Value.As.Literal, Type);
        }
        else if (VAR_MEM == Value.LocationType || VAR_REG == Value.LocationType)
        {
            PVMEmitIntoRegLocation(EMITTER(), &Ret, false, &Value);
            PVMEmitNot(EMITTER(), Ret.As.Register, Ret.As.Register, Type);
        }
        else goto InvalidStorage;
    } break;
    default: 
    {
        Ret = (VarLocation){0};
        PASCAL_UNREACHABLE("Unhandled unary case");
    } break;
    }
    return Ret;

InvalidOperand:
    INVALID_PREFIX_OP(&OpToken, Type);
    return Value;
InvalidStorage:
    ErrorAt(Compiler, &OpToken, "Invalid storage class for unary '"STRVIEW_FMT"'.", 
        STRVIEW_FMT_ARG(OpToken.Lexeme)
    );
    return Value;
}




static IntegralType TryCoercingTypes(TokenType Operation, IntegralType Left, IntegralType Right)
{
    switch (Operation)
    {
    case TOKEN_PLUS:
    {
        if (Left == TYPE_STRING 
        || IntegralTypeIsInteger(Left) 
        || IntegralTypeIsFloat(Left))
        {
            return CoerceTypes(Left, Right);
        }
    } break;
    case TOKEN_MINUS:
    {
        if (IntegralTypeIsInteger(Left) 
        || IntegralTypeIsFloat(Left))
        {
            return CoerceTypes(Left, Right);
        }
    } break;
    case TOKEN_STAR:
    case TOKEN_DIV: case TOKEN_SLASH:
    {
        if (IntegralTypeIsInteger(Left) 
        || IntegralTypeIsFloat(Left))
        {
            return CoerceTypes(Left, Right);
        }
    } break;
    case TOKEN_GREATER:
    case TOKEN_LESS:
    case TOKEN_EQUAL:
    case TOKEN_LESS_GREATER: /* <> */
    case TOKEN_GREATER_EQUAL:
    case TOKEN_LESS_EQUAL:
    {
        if (Left == TYPE_RECORD 
        || Left == TYPE_STATIC_ARRAY 
        || Left == TYPE_FUNCTION 
        )
        {
            return TYPE_INVALID;
        }

        /* comparing integers of different signs */
        if (IntegralTypeIsInteger(Left) && IntegralTypeIsInteger(Right)
        && IntegralTypeIsSigned(Left) != IntegralTypeIsSigned(Right))
        {
            return TYPE_INVALID;
        }

        /* returns the common type for comparison */
        return CoerceTypes(Left, Right);
    } break;
    case TOKEN_SHL:
    case TOKEN_SHR:
    case TOKEN_ASR:
    case TOKEN_LESS_LESS: /* << */
    case TOKEN_GREATER_GREATER: /* >> */
    case TOKEN_MOD:
    {
        if (IntegralTypeIsInteger(Left) && IntegralTypeIsInteger(Right))
        {
            return Left;
        }
    } break;
    case TOKEN_AND:
    case TOKEN_OR:
    {
        if (IntegralTypeIsInteger(Left) && IntegralTypeIsInteger(Right))
        {
            return CoerceTypes(Left, Right);
        }
        if (TYPE_BOOLEAN == Left && TYPE_BOOLEAN == Right)
        {
            return TYPE_BOOLEAN;
        }
    } break;
    case TOKEN_XOR:
    {
        if (IntegralTypeIsInteger(Left) && IntegralTypeIsInteger(Right))
        {
            return CoerceTypes(Left, Right);
        }
    } break;
    default: break;
    }
    return TYPE_INVALID;
}




bool LiteralEqual(VarLiteral A, VarLiteral B, IntegralType CommonType)
{
    bool Result = false;
    if (IntegralTypeIsFloat(CommonType))
        Result = A.Flt == B.Flt;
    else if (IntegralTypeIsInteger(CommonType))
        Result = A.Int == B.Int;
    else if (TYPE_BOOLEAN == CommonType)
        Result = 0 == (1 & (A.Bool ^ B.Bool));
    else if (TYPE_POINTER == CommonType)
        Result = A.Ptr.As.Raw == B.Ptr.As.Raw;
    else if (TYPE_STRING == CommonType)
        Result = PStrEqu(&A.Str, &B.Str);
    else if (TYPE_CHAR == CommonType)
        Result = A.Chr == B.Chr;
    return Result;
}

static bool LiteralLess(VarLiteral A, VarLiteral B, IntegralType CommonType)
{
    bool Result = false;
    if (IntegralTypeIsFloat(CommonType))
        Result = A.Flt < B.Flt;
    else if (IntegralTypeIsInteger(CommonType))
        Result = A.Int < B.Int;
    else if (TYPE_POINTER == CommonType)
        Result = A.Ptr.As.UInt < B.Ptr.As.UInt;
    else if (TYPE_STRING == CommonType)
        Result = PStrIsLess(&A.Str, &B.Str);
    else if (TYPE_CHAR == CommonType)
        Result = A.Chr < B.Chr;
    return Result;
}

#define LiteralNotEqual(A, B, T)        !LiteralEqual(A, B, T)
#define LiteralGreater(A, B, T)         LiteralLess(B, A, T)
#define LiteralLessOrEqual(A, B, T)     !LiteralGreater(A, B, T)
#define LiteralGreaterOrEqual(A, B, T)  !LiteralLess(A, B, T)



static VarLocation LiteralExprBinary(PascalCompiler *Compiler,
    const Token *OpToken, IntegralType ResultingType,
    const VarLiteral *Left, IntegralType LeftType, 
    const VarLiteral *Right, IntegralType RightType)
{
#define SET_IF(Operand1, Comparison, Operand2) do {\
    return VAR_LOCATION_LIT(\
        .Bool = Literal ## Comparison(*Operand1, *Operand2, ResultingType), \
        TYPE_BOOLEAN\
    );\
} while (0)

#define INT_AND_FLT_OP(Op, A, B) do {\
    if (IntegralTypeIsFloat(LeftType)) {\
        Dst.As.Literal.Flt = Left->Flt + Right->Flt;\
    } else if (IntegralTypeIsSigned(LeftType)) {\
        Dst.As.Literal.Int = Left->Int + Right->Int;\
    }\
} while (0)

    TokenType Operator = OpToken->Type;
    VarLocation Dst = VAR_LOCATION_LIT(.Int = 0, ResultingType);
    switch (Operator)
    {
    case TOKEN_GREATER:         SET_IF(Left,    Greater,        Right); break;
    case TOKEN_LESS:            SET_IF(Left,    Less,           Right); break;
    case TOKEN_EQUAL:           SET_IF(Left,    Equal,          Right); break;
    case TOKEN_LESS_GREATER:    SET_IF(Left,    NotEqual,       Right); break; 
    case TOKEN_GREATER_EQUAL:   SET_IF(Left,    GreaterOrEqual, Right); break; 
    case TOKEN_LESS_EQUAL:      SET_IF(Left,    LessOrEqual,    Right); break;

    case TOKEN_MINUS:           INT_AND_FLT_OP(-, Left, Right); break;
    case TOKEN_STAR:            INT_AND_FLT_OP(*, Left, Right); break;
    case TOKEN_PLUS:
    {
        if (TYPE_STRING == ResultingType)
        {
            Dst.As.Literal.Str = Left->Str;
            PStrConcat(&Dst.As.Literal.Str, &Right->Str);
        }
        else INT_AND_FLT_OP(+, Left, Right);
    } break;
    case TOKEN_SLASH:
    case TOKEN_DIV:
    {
        if ((IntegralTypeIsFloat(RightType) && Right->Flt == 0) 
        || (IntegralTypeIsInteger(RightType) && Right->Int == 0))
        {
            ErrorAt(Compiler, OpToken, "Division by 0.");
        }
        else INT_AND_FLT_OP(/, Left, Right);
    } break;
    case TOKEN_MOD: 
    {
        if (0 == Right->Int)
            ErrorAt(Compiler, OpToken, "Modulo by 0.");
        Dst.As.Literal.Int = Left->Int % Right->Int;
    } break;
    case TOKEN_SHL:
    case TOKEN_LESS_LESS:
    {
        Dst.As.Literal.Int = Left->Int << (Right->Int % 64);
        Dst.Type = TypeOfIntLit(Dst.As.Literal.Int);
    } break;
    case TOKEN_SHR:
    case TOKEN_GREATER_GREATER: 
    {
        Dst.As.Literal.Int = Left->Int << (Right->Int % 64);
        Dst.Type = TypeOfIntLit(Dst.As.Literal.Int);
    } break;
    case TOKEN_ASR: 
    {
        Dst.As.Literal.Int = ArithmeticShiftRight(Dst.As.Literal.Int, (Right->Int % 64));
        Dst.Type = TypeOfIntLit(Dst.As.Literal.Int);
    } break;
    case TOKEN_AND:
    {
        if (TYPE_BOOLEAN == ResultingType)
            Dst.As.Literal.Bool = Left->Bool && Right->Bool;
        else Dst.As.Literal.Int = Left->Int & Right->Int;
    } break;
    case TOKEN_OR:
    {
        if (TYPE_BOOLEAN == ResultingType)
            Dst.As.Literal.Bool = Left->Bool || Right->Bool;
        else Dst.As.Literal.Int = Left->Int | Right->Int;
    } break;
    case TOKEN_XOR:
    {
        Dst.As.Literal.Int = Left->Int ^ Right->Int;
    } break;
    default:
    {
        PASCAL_UNREACHABLE("Unhandled binary op: %s\n", TokenTypeToStr(Operator));
    } break;
    }
    return Dst;

#undef SET_IF
#undef INT_AND_FLT_OP
}




static VarLocation RuntimeExprBinary(PascalCompiler *Compiler,
    const Token *OpToken, IntegralType ResultingType,
    /* although this function does not actually modify Dst physically,
     * the content of which will be modified in the code emitted by the emitter */
    VarLocation *Left, VarLocation *Right)
{
#define SET_IF(Operand1, IfWhat, Operand2) do {\
    VarRegister A, B;\
    bool OwningA = PVMEmitIntoReg(EMITTER(), &A, true, Operand1);\
    bool OwningB = PVMEmitIntoReg(EMITTER(), &B, true, Operand2);\
    VarLocation ConditionalCode = PVMEmitSetIf ## IfWhat (EMITTER(), A, B, ResultingType);\
    if (OwningA) PVMFreeRegister(EMITTER(), A);\
    if (OwningB) PVMFreeRegister(EMITTER(), B);\
    return ConditionalCode;\
} while (0)

#define BIN_OP(OpName, Operand1, Operand2) do {\
    PVMEmitIntoRegLocation(EMITTER(), &Dst, false, Operand1);\
    PVMEmit ## OpName (EMITTER(), Dst.As.Register, Operand2);\
} while (0)

    TokenType Operator = OpToken->Type;
    VarLocation Dst = { 0 };
    switch (Operator)
    {
    case TOKEN_GREATER:         SET_IF(Left,    Greater,        Right); break;
    case TOKEN_LESS:            SET_IF(Left,    Less,           Right); break;
    case TOKEN_EQUAL:           SET_IF(Left,    Equal,          Right); break;
    case TOKEN_LESS_GREATER:    SET_IF(Left,    NotEqual,       Right); break; 
    case TOKEN_GREATER_EQUAL:   SET_IF(Left,    GreaterOrEqual, Right); break;
    case TOKEN_LESS_EQUAL:      SET_IF(Left,    LessOrEqual,    Right); break;

    case TOKEN_PLUS:            BIN_OP(Add, Left, Right); break;
    case TOKEN_MINUS:           BIN_OP(Sub, Left, Right); break;
    case TOKEN_STAR:            BIN_OP(Mul, Left, Right); break;
    case TOKEN_SLASH:
    case TOKEN_DIV:             BIN_OP(Div, Left, Right); break;
    case TOKEN_MOD:             BIN_OP(Mod, Left, Right); break;
    case TOKEN_SHL:
    case TOKEN_LESS_LESS:       BIN_OP(Shl, Left, Right); break;
    case TOKEN_SHR:
    case TOKEN_GREATER_GREATER: BIN_OP(Shr, Left, Right); break;
    case TOKEN_ASR:             BIN_OP(Asr, Left, Right); break;
    case TOKEN_AND:             BIN_OP(And, Left, Right); break;
    case TOKEN_OR:              BIN_OP(Or,  Left, Right); break;
    case TOKEN_XOR:             BIN_OP(Xor, Left, Right); break;
    default:
    {
        PASCAL_UNREACHABLE("Unhandled binary op: %s\n", TokenTypeToStr(Operator));
    } break;
    }
    FreeExpr(Compiler, *Right);
    return Dst;

#undef SET_IF
#undef BIN_OP
}


static VarLocation ExprBinary(PascalCompiler *Compiler, VarLocation *Left, bool ShouldCallFunction)
{
    PASCAL_NONNULL(Compiler);
    PASCAL_NONNULL(Left);
    UNUSED(ShouldCallFunction);

    /* Left OpToken Right 
     * ^^^^ we are here so far 
     */

    Token OpToken = Compiler->Curr;
    const Precedence Prec = GetPrecedenceRule(OpToken.Type)->Prec;
    bool CallFunction = true; /* if the right expr evaluates to a function reference, call it */
    VarLocation Right = ParsePrecedence(
        Compiler, Prec + 1, CallFunction
    );  /* +1 for left associative */


    if (VAR_REG != Left->LocationType
    && VAR_LIT != Left->LocationType
    && VAR_MEM != Left->LocationType) 
    {
        ErrorAt(Compiler, &OpToken, 
            "Expression to the left of '"STRVIEW_FMT"' does not have a storage class."
        );
        return *Left;
    }
    if (VAR_REG != Right.LocationType 
    && VAR_LIT != Right.LocationType
    && VAR_MEM != Right.LocationType)
    {
        ErrorAt(Compiler, &OpToken, 
            "Expression to the right of '"STRVIEW_FMT"' does not have a storage class."
        );
        return Right;
    }


    /* type conversion */
    IntegralType ResultingType = TryCoercingTypes(OpToken.Type, 
        Left->Type.Integral, Right.Type.Integral
    );
    if (ResultingType == TYPE_INVALID)
    {
        INVALID_BINARY_OP(&OpToken, Left->Type.Integral, Right.Type.Integral);
        return Right;
    }
    if (!ConvertTypeImplicitly(Compiler, ResultingType, Left))
    {
        StringView LeftType = VarTypeToStringView(Left->Type);
        ErrorAt(Compiler, &OpToken, 
            "Cannot convert from %s to "STRVIEW_FMT" implicitly on the left of the '"STRVIEW_FMT"' operator.",
            IntegralTypeToStr(ResultingType), 
            STRVIEW_FMT_ARG(LeftType), 
            STRVIEW_FMT_ARG(OpToken.Lexeme)
        );
        return *Left;
    }
    if (!ConvertTypeImplicitly(Compiler, ResultingType, &Right))
    {
        StringView RightType = VarTypeToStringView(Right.Type);
        ErrorAt(Compiler, &OpToken, 
            "Cannot convert from %s to "STRVIEW_FMT" implicitly on the right of the '"STRVIEW_FMT"' operator.",
            IntegralTypeToStr(ResultingType), 
            STRVIEW_FMT_ARG(RightType), 
            STRVIEW_FMT_ARG(OpToken.Lexeme)
        );
        return Right;
    }


    if (VAR_LIT == Left->LocationType && VAR_LIT == Right.LocationType)
    {
        return LiteralExprBinary(Compiler, 
            &OpToken, ResultingType, 
            &Left->As.Literal, Left->Type.Integral, 
            &Right.As.Literal, Right.Type.Integral
        );
    }
    else
    {
        return RuntimeExprBinary(Compiler, &OpToken, ResultingType, Left, &Right);
    }
}

static VarLocation ExprAnd(PascalCompiler *Compiler, VarLocation *Left, bool ShouldCallFunction)
{
    UNUSED(ShouldCallFunction);
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

        Right = ParsePrecedence(Compiler, GetPrecedenceRule(OpToken.Type)->Prec + 1, true);
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
        return ExprBinary(Compiler, Left, false);
    }
    else 
    {
        StringView LeftType = VarTypeToStringView(Left->Type);
        ErrorAt(Compiler, &OpToken, "Invalid left operand for 'and': "STRVIEW_FMT,
            STRVIEW_FMT_ARG(LeftType)
        );
    }

    return Right;
InvalidOperands:
    INVALID_BINARY_OP(&OpToken, Left->Type.Integral, Right.Type.Integral);
    return *Left;
}

static VarLocation ExprOr(PascalCompiler *Compiler, VarLocation *Left, bool ShouldCallFunction)
{
    UNUSED(ShouldCallFunction);
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

        Right = ParsePrecedence(Compiler, GetPrecedenceRule(OpToken.Type)->Prec + 1, true);
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
        return ExprBinary(Compiler, Left, true);
    }
    else 
    {
        StringView LeftType = VarTypeToStringView(Left->Type);
        ErrorAt(Compiler, &OpToken, "Invalid left operand for 'or': "STRVIEW_FMT".",
            STRVIEW_FMT_ARG(LeftType)
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
    [TOKEN_AT]              = { VariableAddrOf,     NULL,           PREC_SINGLE },

    [TOKEN_CARET]           = { NULL,               VariableDeref,  PREC_VARIABLE },
    [TOKEN_DOT]             = { NULL,               VariableAccess, PREC_VARIABLE },
    [TOKEN_LEFT_BRACKET]    = { NULL,               ArrayAccess,    PREC_VARIABLE },
    [TOKEN_LEFT_PAREN]      = { FactorGrouping,     FactorCall,     PREC_VARIABLE },

    [TOKEN_NOT]             = { ExprUnary,          NULL,           PREC_FACTOR },

    [TOKEN_STAR]            = { NULL,               ExprBinary,     PREC_TERM },
    [TOKEN_DIV]             = { NULL,               ExprBinary,     PREC_TERM },
    [TOKEN_SLASH]           = { NULL,               ExprBinary,     PREC_TERM },
    [TOKEN_MOD]             = { NULL,               ExprBinary,     PREC_TERM },
    [TOKEN_AND]             = { NULL,               ExprAnd,        PREC_TERM },

    [TOKEN_XOR]             = { NULL,               ExprBinary,     PREC_SIMPLE },
    [TOKEN_PLUS]            = { ExprUnary,          ExprBinary,     PREC_SIMPLE },
    [TOKEN_MINUS]           = { ExprUnary,          ExprBinary,     PREC_SIMPLE },
    [TOKEN_SHR]             = { NULL,               ExprBinary,     PREC_SIMPLE },
    [TOKEN_SHL]             = { NULL,               ExprBinary,     PREC_SIMPLE },
    [TOKEN_ASR]             = { NULL,               ExprBinary,     PREC_SIMPLE },
    [TOKEN_LESS_LESS]       = { NULL,               ExprBinary,     PREC_SIMPLE },
    [TOKEN_GREATER_GREATER] = { NULL,               ExprBinary,     PREC_SIMPLE },
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

static VarLocation ParseAssignmentLhs(PascalCompiler *Compiler, Precedence Prec, bool ShouldCallFunction)
{
    const PrefixParseRoutine PrefixRoutine = 
        GetPrecedenceRule(Compiler->Curr.Type)->PrefixRoutine;
    if (NULL == PrefixRoutine)
    {
        ErrorAt(Compiler, &Compiler->Curr, "Expected expression.");
        return (VarLocation) { 0 };
    }


    VarLocation Left = PrefixRoutine(Compiler, ShouldCallFunction);


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
        Left = InfixRoutine(Compiler, &Left, ShouldCallFunction);
    }
    return Left;
}

static VarLocation ParsePrecedence(PascalCompiler *Compiler, Precedence Prec, bool ShouldCallFunction)
{
    PASCAL_NONNULL(Compiler);
    ConsumeToken(Compiler);
    return ParseAssignmentLhs(Compiler, Prec, ShouldCallFunction);
}






/* returns the type that both sides should be */
IntegralType CoerceTypes(IntegralType Left, IntegralType Right)
{
    PASCAL_ASSERT(Left >= TYPE_INVALID && Right >= TYPE_INVALID, "");
    PASCAL_ASSERT(Left < TYPE_COUNT && Right < TYPE_COUNT, "Impossible");
    return sCoercionRules[Left][Right];
}



static bool ConvertRegisterTypeImplicitly(PVMEmitter *Emitter, 
        IntegralType To, VarRegister *From, IntegralType FromType)
{
    PASCAL_NONNULL(Emitter);
    PASCAL_NONNULL(From);
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
    PASCAL_NONNULL(Compiler);
    PASCAL_NONNULL(From);

    IntegralType FromType = From->Type.Integral;
    if (To == FromType)
        return true;

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
                if (IntegralTypeIsSigned(FromType))
                    From->As.Literal.Flt = (F64)(I64)From->As.Literal.Int;
                else
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
        return false;
    } break;
    }

    From->Type = VarTypeInit(To, IntegralTypeSize(To));
    return true;

    StringView FromTypeStr;
InvalidTypeConversion:
    FromTypeStr = VarTypeToStringView(From->Type);
    Error(Compiler, 
        "Cannot convert from "STRVIEW_FMT" to %s.", 
        STRVIEW_FMT_ARG(FromTypeStr),  
        IntegralTypeToStr(To)
    );
    return false;
}




