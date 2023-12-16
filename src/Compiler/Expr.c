

#include <inttypes.h>

#include "Compiler/Data.h"
#include "Compiler/Error.h"
#include "Compiler/Expr.h"
#include "Compiler/Builtins.h"
#include "Compiler/VarList.h"


static IntegralType sCoercionRules[TYPE_COUNT][TYPE_COUNT] = {
    /*Invalid       I8            I16           I32           I64           U8            U16           U32           U64           F32           F64           Function      Boolean       Pointer       string       record */
    { TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID,TYPE_INVALID},         /* Invalid */
    { TYPE_INVALID, TYPE_I32,     TYPE_I32,     TYPE_I32,     TYPE_I64,     TYPE_I32,     TYPE_I32,     TYPE_I32,     TYPE_I64,     TYPE_F32,     TYPE_F64,     TYPE_INVALID, TYPE_INVALID, TYPE_POINTER, TYPE_INVALID,TYPE_INVALID},         /* I8 */
    { TYPE_INVALID, TYPE_I32,     TYPE_I32,     TYPE_I32,     TYPE_I64,     TYPE_I32,     TYPE_I32,     TYPE_I32,     TYPE_I64,     TYPE_F32,     TYPE_F64,     TYPE_INVALID, TYPE_INVALID, TYPE_POINTER, TYPE_INVALID,TYPE_INVALID},         /* I16 */
    { TYPE_INVALID, TYPE_I32,     TYPE_I32,     TYPE_I32,     TYPE_I64,     TYPE_I32,     TYPE_I32,     TYPE_I32,     TYPE_I64,     TYPE_F32,     TYPE_F64,     TYPE_INVALID, TYPE_INVALID, TYPE_POINTER, TYPE_INVALID,TYPE_INVALID},         /* I32 */
    { TYPE_INVALID, TYPE_I64,     TYPE_I64,     TYPE_I64,     TYPE_I64,     TYPE_I64,     TYPE_I64,     TYPE_I64,     TYPE_I64,     TYPE_F32,     TYPE_F64,     TYPE_INVALID, TYPE_INVALID, TYPE_POINTER, TYPE_INVALID,TYPE_INVALID},         /* I64 */
    { TYPE_INVALID, TYPE_I32,     TYPE_I32,     TYPE_I32,     TYPE_I64,     TYPE_U32,     TYPE_U32,     TYPE_U32,     TYPE_U64,     TYPE_F32,     TYPE_F64,     TYPE_INVALID, TYPE_INVALID, TYPE_POINTER, TYPE_INVALID,TYPE_INVALID},         /* U8 */
    { TYPE_INVALID, TYPE_I32,     TYPE_I32,     TYPE_I32,     TYPE_I64,     TYPE_U32,     TYPE_U32,     TYPE_U32,     TYPE_U64,     TYPE_F32,     TYPE_F64,     TYPE_INVALID, TYPE_INVALID, TYPE_POINTER, TYPE_INVALID,TYPE_INVALID},         /* U16 */
    { TYPE_INVALID, TYPE_I32,     TYPE_I32,     TYPE_I32,     TYPE_I64,     TYPE_U32,     TYPE_U32,     TYPE_U32,     TYPE_U64,     TYPE_F32,     TYPE_F64,     TYPE_INVALID, TYPE_INVALID, TYPE_POINTER, TYPE_INVALID,TYPE_INVALID},         /* U32 */
    { TYPE_INVALID, TYPE_U64,     TYPE_U64,     TYPE_U64,     TYPE_U64,     TYPE_U64,     TYPE_U64,     TYPE_U64,     TYPE_U64,     TYPE_F32,     TYPE_F64,     TYPE_INVALID, TYPE_INVALID, TYPE_POINTER, TYPE_INVALID,TYPE_INVALID},         /* U64 */
    { TYPE_INVALID, TYPE_F32,     TYPE_F32,     TYPE_F32,     TYPE_F32,     TYPE_F32,     TYPE_F32,     TYPE_F32,     TYPE_F32,     TYPE_F32,     TYPE_F64,     TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID,TYPE_INVALID},         /* F32 */
    { TYPE_INVALID, TYPE_F64,     TYPE_F64,     TYPE_F64,     TYPE_F64,     TYPE_F64,     TYPE_F64,     TYPE_F64,     TYPE_F64,     TYPE_F64,     TYPE_F64,     TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID,TYPE_INVALID},         /* F64 */
    { TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID,TYPE_INVALID},         /* Function */
    { TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_BOOLEAN, TYPE_INVALID, TYPE_INVALID,TYPE_INVALID},         /* Boolean */
    { TYPE_INVALID, TYPE_POINTER, TYPE_POINTER, TYPE_POINTER, TYPE_POINTER, TYPE_POINTER, TYPE_POINTER, TYPE_POINTER, TYPE_POINTER, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_POINTER, TYPE_INVALID,TYPE_INVALID},         /* Pointer */
    { TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_STRING, TYPE_INVALID},         /* String */
    { TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID,TYPE_RECORD },         /* Record */
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
typedef VarLocation (*InfixParseRoutine)(PVMCompiler *, VarLocation *);
typedef VarLocation (*PrefixParseRoutine)(PVMCompiler *);


#define INVALID_BINARY_OP(pOp, LType, RType) \
    ErrorAt(Compiler, pOp, "Invalid operand for binary operator '%.*s': %s and %s", \
            (pOp)->Len, (pOp)->Str, \
            IntegralTypeToStr(LType), IntegralTypeToStr(RType)\
    )
#define INVALID_PREFIX_OP(pOp, Type) \
    ErrorAt(Compiler, pOp, "Invalid operand for prefix operator '%.*s': %s", \
            (pOp)->Len, (pOp)->Str, IntegralTypeToStr(Type)\
    )
#define INVALID_POSTFIX_OP(pOp, Type) \
    ErrorAt(Compiler, pOp, "Invalid operand for postfix operator '%.*s': %s", \
            (pOp)->Len, (pOp)->Str, IntegralTypeToStr(Type)\
    )

typedef struct PrecedenceRule
{
    PrefixParseRoutine PrefixRoutine;
    InfixParseRoutine InfixRoutine;
    Precedence Prec;
} PrecedenceRule;

static const PrecedenceRule *GetPrecedenceRule(TokenType CurrentTokenType);
static VarLocation ParsePrecedence(PVMCompiler *Compiler, Precedence Prec);
static VarLocation ParseAssignmentLhs(PVMCompiler *Compiler, Precedence Prec);


void FreeExpr(PVMCompiler *Compiler, VarLocation Expr)
{
    PASCAL_NONNULL(Compiler);

    if (VAR_REG == Expr.LocationType && !Expr.As.Register.Persistent)
    {
        PVMFreeRegister(EMITTER(), Expr.As.Register);
    }
}


VarLocation CompileExpr(PVMCompiler *Compiler)
{
    return ParsePrecedence(Compiler, PREC_EXPR);
}

VarLocation CompileExprIntoReg(PVMCompiler *Compiler)
{
    VarLocation Expr = CompileExpr(Compiler);
    if (VAR_REG != Expr.LocationType)
    {
        VarLocation Reg = PVMAllocateRegister(EMITTER(), Expr.Type);
        PVMEmitMov(EMITTER(), &Reg, &Expr);
        return Reg;
    }
    return Expr;
}

VarLocation CompileVariableExpr(PVMCompiler *Compiler)
{
    return ParseAssignmentLhs(Compiler, PREC_VARIABLE);
}

void CompileExprInto(PVMCompiler *Compiler, const Token *OpToken, VarLocation *Location)
{
    VarLocation Expr = CompileExpr(Compiler);
    if (Compiler->Error)
        return;

    CoerceTypes(Compiler, OpToken, Location->Type, Expr.Type);
    if (Expr.LocationType != VAR_MEM)
        ConvertTypeImplicitly(Compiler, Location->Type, &Expr);

    if (TYPE_POINTER == Location->Type && TYPE_POINTER == Expr.Type)
    {
        if (Location->PointerTo.Var.Type != Expr.PointerTo.Var.Type)
        {
            if (NULL == OpToken)
                OpToken = &Compiler->Curr;

            ErrorAt(Compiler, OpToken, "Cannot assign %s pointer to %s pointer.", 
                    IntegralTypeToStr(Expr.PointerTo.Var.Type),
                    IntegralTypeToStr(Location->PointerTo.Var.Type)
            );

            return;
        }
    }
    PVMEmitMov(EMITTER(), Location, &Expr);
    FreeExpr(Compiler, Expr);
}









static VarLocation FactorLiteral(PVMCompiler *Compiler)
{
    VarLocation Location = {
        .LocationType = VAR_LIT,
    };
    PASCAL_NONNULL(Compiler);

    switch (Compiler->Curr.Type)
    {
    case TOKEN_INTEGER_LITERAL:
    {
        Location.Type = TypeOfIntLit(Compiler->Curr.Literal.Int);
        Location.As.Literal.Int = Compiler->Curr.Literal.Int;
    } break;
    case TOKEN_NUMBER_LITERAL:
    {
        Location.Type = TYPE_F64;
        Location.As.Literal.Flt = Compiler->Curr.Literal.Real;
    } break;
    case TOKEN_TRUE:
    case TOKEN_FALSE:
    {
        Location.Type = TYPE_BOOLEAN;
        Location.As.Literal.Bool = Compiler->Curr.Type != TOKEN_FALSE;
    } break;
    case TOKEN_STRING_LITERAL:
    {
        Location.Type = TYPE_STRING;
        Location.As.Literal.Str = Compiler->Curr.Literal.Str;
    } break;
    default:
    {
        PASCAL_UNREACHABLE("Unreachable, %s literal type is not handled", TokenTypeToStr(Compiler->Curr.Type));
    } break;
    }
    return Location;
}




static VarLocation VariableDeref(PVMCompiler *Compiler, VarLocation *Variable)
{
    PASCAL_NONNULL(Compiler);
    PASCAL_NONNULL(Variable);

    Token Caret = Compiler->Curr;
    if (TYPE_POINTER != Variable->Type)
    {
        ErrorAt(Compiler, &Caret, "%s is not dereferenceable.", 
                IntegralTypeToStr(Variable->Type)
        );
        return *Variable;
    }


    VarLocation Ptr = PVMAllocateRegister(EMITTER(), TYPE_POINTER);

    PVMEmitMov(EMITTER(), &Ptr, Variable);
    Ptr.Type = Variable->Type;
    Ptr.LocationType = VAR_MEM;

    VarLocation Memory = {
        .LocationType = VAR_MEM,
        .Type = Variable->PointerTo.Var.Type,
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

    U32 Hash = VartabHashStr(MemberName->Str, MemberName->Len);
    PascalVar *Member = VartabFindWithHash(Record, MemberName->Str, MemberName->Len, Hash);
    if (NULL == Member)
        return NULL;
    return Member->Location;
}

static VarLocation VariableAccess(PVMCompiler *Compiler, VarLocation *Left)
{
    PASCAL_NONNULL(Compiler);
    PASCAL_NONNULL(Left);

    /* identifier consumed */
    Token Dot = Compiler->Curr;
    ConsumeOrError(Compiler, TOKEN_IDENTIFIER, "Expected member name.");
    if (TYPE_RECORD != Left->Type)
    {
        ErrorAt(Compiler, &Dot, "%s does not have a member.", IntegralTypeToStr(Left->Type));
        return *Left;
    }

    Token Name = Compiler->Curr;
    VarLocation *Member = FindRecordMember(&Left->PointerTo.Record, &Name);
    if (NULL == Member)
    {
        ErrorAt(Compiler, &Name, "'%.*s' is not a member.");
        return *Left;
    }

    VarLocation Value = *Member;
    /* member offset + record offset = location */
    PASCAL_ASSERT(VAR_MEM == Left->LocationType, "TODO: offset by register");
    Value.As.Memory.Location += Left->As.Memory.Location;
    Value.As.Memory.RegPtr = Left->As.Memory.RegPtr;
    return Value;
}


static VarLocation FactorGrouping(PVMCompiler *Compiler)
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


static VarLiteral ConvertLiteralTypeExplicitly(PVMCompiler *Compiler, 
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

static VarLocation ConvertTypeExplicitly(PVMCompiler *Compiler, 
        const Token *Converter, IntegralType To, const VarLocation *Expr)
{
    PASCAL_NONNULL(Compiler);
    PASCAL_NONNULL(Expr);
    PASCAL_NONNULL(Converter);
    if (To == Expr->Type)
        return *Expr;

    VarLocation Converted = { 
        .Type = TYPE_INVALID,
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
        Converted.As.Literal = ConvertLiteralTypeExplicitly(Compiler, Converter, 
                To, &Expr->As.Literal, Expr->Type
        );
        Converted.Type = To;
    } break;
    }

    return Converted;
}


static VarLocation FactorCall(PVMCompiler *Compiler, const Token *Identifier, PascalVar *Variable)
{
    PASCAL_NONNULL(Compiler);
    PASCAL_NONNULL(Identifier);
    PASCAL_NONNULL(Variable);
    PASCAL_NONNULL(Variable->Location);

    VarLocation ReturnValue = { 0 };
    VarLocation *Location = Variable->Location;

    /* builtin function */
    if (VAR_BUILTIN == Location->LocationType)
    {
        OptionalReturnValue Opt = CompileCallToBuiltin(Compiler, Variable->Location->As.BuiltinSubroutine);
        if (!Opt.HasReturnValue)
        {
            ErrorAt(Compiler, Identifier, "Builtin procedure does not have a return value.");
            return ReturnValue;
        }
        return Opt.ReturnValue;
    }

    /* function call */
    VarSubroutine *Callee = &Location->As.Subroutine;
    if (!Callee->HasReturnType)
    {
        ErrorAt(Compiler, Identifier, "Procedure '%.*s' does not have a return value.",
                Identifier->Len, Identifier->Str 
        );
        return ReturnValue;
    }

    /* setup return reg */
    if (TYPE_RECORD == Callee->ReturnType.Type)
    {
        PASCAL_UNREACHABLE("TODO: record return");
    }
    else
    {
        ReturnValue = PVMAllocateRegister(EMITTER(), Callee->ReturnType.Type);
        CompileSubroutineCall(Compiler, Callee, Identifier, &ReturnValue);
        if (TYPE_POINTER == ReturnValue.Type)
        {
            PASCAL_NONNULL(Callee->ReturnType.Location);
            ReturnValue.PointerTo.Var = Callee->ReturnType.Location->PointerTo.Var;
        }
        return ReturnValue;
    }
    return ReturnValue;
}


static VarLocation FactorVariable(PVMCompiler *Compiler)
{
    PASCAL_NONNULL(Compiler);

    /* consumed iden */
    Token Identifier = Compiler->Curr;
    PascalVar *Variable = GetIdenInfo(Compiler, &Identifier, "Undefined identifier.");
    if (NULL == Variable)
        goto Error;

    if (NULL == Variable->Location)
    {
        /* type casting */
        /* typename(expr) */
        if (ConsumeIfNextIs(Compiler, TOKEN_LEFT_PAREN))
        {
            VarLocation Expr = FactorGrouping(Compiler);
            return ConvertTypeExplicitly(Compiler, &Identifier, Variable->Type, &Expr);
        }
        /* returns the type itself for sizeof */
        /* typename */
        VarLocation Type = {
            .Type = Variable->Type,
            .LocationType = VAR_INVALID,
        };
        return Type;
    }
    if (TYPE_FUNCTION == Variable->Location->Type)
    {
        return FactorCall(Compiler, &Identifier, Variable);
    }
    return *Variable->Location;
Error:
    return (VarLocation) { 0 };
}


static VarLocation VariableAddrOf(PVMCompiler *Compiler)
{
    /* Curr is '@' */

    Token AtSign = Compiler->Curr;
    ConsumeOrError(Compiler, TOKEN_IDENTIFIER, "Expected variable name after '@'.");
    Token Identifier = Compiler->Curr;

    PascalVar *Variable = GetIdenInfo(Compiler, &Identifier, "Undefined variable.");
    if (NULL == Variable)
        return (VarLocation) { 0 };


    while (ConsumeIfNextIs(Compiler, TOKEN_DOT))
    {
        PASCAL_UNREACHABLE("TODO: addr of record field");
    }
    VarLocation *Location = Variable->Location;
    PASCAL_NONNULL(Location);
    PASCAL_ASSERT(Variable->Type == Location->Type, "");


    VarLocation Ptr = PVMAllocateRegister(EMITTER(), TYPE_POINTER);
    Ptr.PointerTo.Var = *Variable;
    if (VAR_MEM != Location->LocationType)
    {
        ErrorAt(Compiler, &AtSign, "Cannot take the address of this type of variable.");
        return *Location;
    }
    PVMEmitLoadAddr(EMITTER(), &Ptr, Location);
    return Ptr;
}



static VarLocation NegateLiteral(PVMCompiler *Compiler, 
        const Token *OpToken, VarLiteral Literal, IntegralType LiteralType)
{
    VarLocation Location = {
        .LocationType = VAR_LIT,
        .Type = TYPE_INVALID,
    };
    if (IntegralTypeIsInteger(LiteralType))
    {
        Literal.Int = -Literal.Int;
        Location.Type = TypeOfIntLit(Literal.Int);
    }
    else if (IntegralTypeIsFloat(LiteralType))
    {
        Literal.Flt = -Literal.Flt;
        Location.Type = TYPE_F64;
    }
    else 
    {
        INVALID_PREFIX_OP(OpToken, LiteralType);
    }

    Location.As.Literal = Literal;
    return Location;
}

static VarLocation NotLiteral(PVMCompiler *Compiler, 
        const Token *OpToken, VarLiteral Literal, IntegralType LiteralType)
{
    VarLocation Location = {
        .LocationType = VAR_LIT,
        .Type = TYPE_INVALID,
    };
    if (TYPE_BOOLEAN == LiteralType)
    {
        Literal.Bool = !Literal.Bool;
        Location.Type = TYPE_BOOLEAN;
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


static VarLocation ExprUnary(PVMCompiler *Compiler)
{
    TokenType Operator = Compiler->Curr.Type;
    Token OpToken = Compiler->Curr;
    VarLocation Value = ParsePrecedence(Compiler, PREC_FACTOR);

    if (VAR_INVALID == Value.LocationType)
    {
        ErrorAt(Compiler, &OpToken, "unary operator '%.*s' cannot be applied to expression with no storage.",
                OpToken.Len, OpToken.Str
        );
    }
    else if (VAR_LIT == Value.LocationType)
    {
        switch (Operator)
        {
        case TOKEN_PLUS: break;
        case TOKEN_NOT:
        {
            Value = NotLiteral(Compiler, &OpToken, Value.As.Literal, Value.Type);
        } break;
        case TOKEN_MINUS:
        {
            Value = NegateLiteral(Compiler, &OpToken, Value.As.Literal, Value.Type);
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
            if (!IntegralTypeIsFloat(Value.Type) && !IntegralTypeIsInteger(Value.Type))
                goto TypeMismatch;
            PVMEmitNeg(EMITTER(), &Value, &Value);
        } break;
        case TOKEN_NOT:
        {
            if (!IntegralTypeIsInteger(Value.Type) && TYPE_BOOLEAN != Value.Type)
                goto TypeMismatch;
            PVMEmitNot(EMITTER(), &Value, &Value);
        } break;
        default: goto Unreachable;
        }
    }
    return Value;

TypeMismatch:
    INVALID_PREFIX_OP(&OpToken, Value.Type);
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


static VarLocation LiteralExprBinary(PVMCompiler *Compiler, const Token *OpToken, 
        VarLocation *Left, VarLocation *Right)
{
#define COMMON_BIN_OP(Operator)\
    do {\
        if (IntegralTypeIsFloat(Both)) {\
            Result.As.Literal.Flt = Left->As.Literal.Flt Operator Right->As.Literal.Flt;\
        } else if (IntegralTypeIsInteger(Both)) {\
            Result.As.Literal.Int = Left->As.Literal.Int Operator Right->As.Literal.Int;\
        } else if (TYPE_POINTER == Both) {\
            Result.As.Literal.Ptr.As.UInt = \
            Left->As.Literal.Ptr.As.UInt\
            Operator Right->As.Literal.Ptr.As.UInt;\
        } else break;\
        Result.Type = Both;\
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
        Result.Type = TYPE_BOOLEAN;\
    } while (0)

#define INT_AND_BOOL_OP(Operator) \
    do {\
        if (IntegralTypeIsInteger(Both)) {\
            Result.As.Literal.Int = Left->As.Literal.Int Operator Right->As.Literal.Int;\
            Result.Type = Both;\
        } else if (TYPE_BOOLEAN == Both) {\
            Result.As.Literal.Bool = Left->As.Literal.Bool Operator Right->As.Literal.Bool;\
            Result.Type = TYPE_BOOLEAN;\
        }\
    } while (0)

    PASCAL_NONNULL(Compiler);
    PASCAL_NONNULL(OpToken);
    PASCAL_NONNULL(Left);
    PASCAL_NONNULL(Right);
    PASCAL_ASSERT(VAR_INVALID != Left->LocationType, "Unreachable");
    PASCAL_ASSERT(VAR_INVALID != Right->LocationType, "Unreachable");

    IntegralType Both = CoerceTypes(Compiler, OpToken, Left->Type, Right->Type);
    bool ConversionOk = ConvertTypeImplicitly(Compiler, Both, Left);
    ConversionOk = ConversionOk && ConvertTypeImplicitly(Compiler, Both, Right);
    if (!ConversionOk)
    {
        goto InvalidTypeConversion;
    }

    VarLocation Result = {
        .Type = TYPE_INVALID,
        .LocationType = VAR_LIT,
    };
    PASCAL_ASSERT(VAR_LIT == Left->LocationType, "left is not a literal in %s", __func__);
    PASCAL_ASSERT(VAR_LIT == Right->LocationType, "right is not a literal in %s", __func__);

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

            /* TODO: should we free the right string? */
            //PStrDeinit(&RightStr);
            Result.Type = Both;
        }
    } break;
    case TOKEN_MINUS: 
    {
        COMMON_BIN_OP(-);
    } break;
    case TOKEN_STAR:
    {
        if (IntegralTypeIsInteger(Both))
        {
            Result.As.Literal.Int = Left->As.Literal.Int * Right->As.Literal.Int;
            Result.Type = Both;
        }
        else if (IntegralTypeIsFloat(Both))
        {
            Result.As.Literal.Flt = Left->As.Literal.Flt * Right->As.Literal.Flt;
            Result.Type = Both;
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
            Result.As.Literal.Int = Left->As.Literal.Int / Right->As.Literal.Int;
            Result.Type = Both;
        }
        else if (IntegralTypeIsFloat(Both))
        {
            Result.As.Literal.Flt = Left->As.Literal.Flt / Right->As.Literal.Flt;
            Result.Type = Both;
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
            Result.As.Literal.Int = Left->As.Literal.Int % Right->As.Literal.Int;
            Result.Type = Both;
        }
        else goto InvalidTypeConversion;
    } break;

    case TOKEN_EQUAL:           
    {
        Result.As.Literal.Bool = LiteralEqual(Left->As.Literal, Right->As.Literal, Both);
        Result.Type = TYPE_BOOLEAN;
    } break;
    case TOKEN_LESS_GREATER:
    {
        Result.As.Literal.Bool = !LiteralEqual(Left->As.Literal, Right->As.Literal, Both);
        Result.Type = TYPE_BOOLEAN;
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
            Result.Type = TypeOfIntLit(Result.As.Literal.Int);
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
            }
            else
            {
                Result.As.Literal.Int = Left->As.Literal.Int >> Right->As.Literal.Int;
            }
            Result.Type = TypeOfIntLit(Result.As.Literal.Int);
        }
        else goto InvalidTypeConversion;
    } break;

    case TOKEN_AND:
    {
        PASCAL_ASSERT(TYPE_BOOLEAN != Both, "Unreachable");
        if (IntegralTypeIsInteger(Both))
        {
            Result.As.Literal.Int = Left->As.Literal.Int & Right->As.Literal.Int;
            Result.Type = Both;
        }
        else goto InvalidTypeConversion;
    } break;
    case TOKEN_OR:
    {
        PASCAL_ASSERT(TYPE_BOOLEAN != Both, "Unreachable");
        if (IntegralTypeIsInteger(Both))
        {
            Result.As.Literal.Int = Left->As.Literal.Int | Right->As.Literal.Int;
            Result.Type = Both;
        }
        else goto InvalidTypeConversion;
    } break;
    case TOKEN_XOR:
    {
        if (IntegralTypeIsInteger(Both))
        {
            Result.As.Literal.Int = Left->As.Literal.Int ^ Right->As.Literal.Int;
        }
        else goto InvalidTypeConversion;
    } break;
    default:
    {
        PASCAL_UNREACHABLE("Unreachable operator in %s: %s", __func__, TokenTypeToStr(OpToken->Type));
    } break;
    }

    if (TYPE_INVALID != Result.Type)
    {
        return Result;
    }

InvalidTypeConversion:
    INVALID_BINARY_OP(OpToken, Left->Type, Right->Type);
    return *Left;
#undef COMMON_BIN_OP
#undef COMPARE_OP
#undef INT_AND_BOOL_OP
}


static VarLocation RuntimeExprBinary(PVMCompiler *Compiler, 
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
    IntegralType Type = CoerceTypes(Compiler, OpToken, Left->Type, Right->Type);
    bool ConversionOk = 
        ConvertTypeImplicitly(Compiler, Type, &Tmp) 
        && ConvertTypeImplicitly(Compiler, Type, &Src);
    if (!ConversionOk) 
    {
        INVALID_BINARY_OP(OpToken, Tmp.Type, Src.Type);
        return *Left;
    }

    VarLocation Dst = PVMAllocateRegister(EMITTER(), Tmp.Type);
    PVMEmitMov(EMITTER(), &Dst, &Tmp);
    switch (OpToken->Type)
    {
    case TOKEN_PLUS: 
    {
        PVMEmitAdd(EMITTER(), &Dst, &Src);
    } break;
    case TOKEN_MINUS:
    {
        PVMEmitSub(EMITTER(), &Dst, &Src);
    } break;

    case TOKEN_STAR:
    {
        PVMEmitMul(EMITTER(), &Dst, &Src);
    } break;
    case TOKEN_DIV:
    case TOKEN_SLASH:
    {
        PVMEmitDiv(EMITTER(), &Dst, &Src);
    } break;
    case TOKEN_MOD:
    {
        PVMEmitMod(EMITTER(), &Dst, &Src);
    } break;

    case TOKEN_SHL:
    case TOKEN_LESS_LESS:
    {
        PVMEmitShl(EMITTER(), &Dst, &Src);
    } break;

    case TOKEN_GREATER_GREATER:
    case TOKEN_SHR:
    {
        PVMEmitShr(EMITTER(), &Dst, &Src);
    } break;

    case TOKEN_LESS:
    case TOKEN_GREATER:
    case TOKEN_LESS_EQUAL:
    case TOKEN_GREATER_EQUAL:
    case TOKEN_LESS_GREATER:
    case TOKEN_EQUAL:
    {
        if (IntegralTypeIsInteger(Dst.Type) 
        && VAR_LIT != Left->LocationType && VAR_LIT != Right->LocationType
        && (IntegralTypeIsSigned(Left->Type) != IntegralTypeIsSigned(Right->Type)))
        {
            ErrorAt(Compiler, OpToken, 
                    "Comparison between integers of different sign (%s and %s) is not allowed.",
                    IntegralTypeToStr(Left->Type), IntegralTypeToStr(Right->Type)
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
        PVMEmitAnd(EMITTER(), &Dst, &Src);
    } break;
    case TOKEN_OR:
    {
        PVMEmitOr(EMITTER(), &Dst, &Src);
    } break;
    case TOKEN_XOR:
    {
        PVMEmitXor(EMITTER(), &Dst, &Src);
    } break;

    default: PASCAL_UNREACHABLE("Unhandled binary op: %.*s", OpToken->Len, OpToken->Str); break;
    }

    Dst.Type = Type;
    FreeExpr(Compiler, Src);
    return Dst;
}


static VarLocation ExprBinary(PVMCompiler *Compiler, VarLocation *Left)
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
        const char *Msg = "Expression on both sides of '%.*s' does not have a storage class.";
        if (LeftInvalid && !RightInvalid)
            Msg = "Expression on the left of '%.*s' does not have a storage class.";
        else if (!LeftInvalid && RightInvalid)
            Msg = "Expression on the right of '%.*s' does not have a storage class.";

        ErrorAt(Compiler, &OpToken, Msg, OpToken.Len, OpToken.Str);
        return *Left;
    }
    else if (VAR_LIT == Left->LocationType && VAR_LIT == Right.LocationType)
    {
        return LiteralExprBinary(Compiler, &OpToken, Left, &Right);
    }
    else 
    {
        return RuntimeExprBinary(Compiler, &OpToken, Left, &Right);
    }
}


static VarLocation ExprAnd(PVMCompiler *Compiler, VarLocation *Left)
{
    PASCAL_NONNULL(Compiler);
    PASCAL_NONNULL(Left);

    VarLocation Right;
    Token OpToken = Compiler->Curr;
    if (TYPE_BOOLEAN == Left->Type)
    {
        bool WasEmit = EMITTER()->ShouldEmit;
        if (VAR_LIT == Left->LocationType && !Left->As.Literal.Bool)
        {
            /* false and ...: don't emit right */
            EMITTER()->ShouldEmit = false;
        }

        U32 FromLeft = PVMEmitBranchIfFalse(EMITTER(), Left);

        Right = ParsePrecedence(Compiler, GetPrecedenceRule(OpToken.Type)->Prec + 1);
        IntegralType ResultType = CoerceTypes(Compiler, &OpToken, Left->Type, Right.Type);
        if (TYPE_INVALID == ResultType)
            goto InvalidOperands;
        PASCAL_ASSERT(TYPE_BOOLEAN == ResultType, "");
        PASCAL_ASSERT(TYPE_BOOLEAN == Right.Type, "");

        PVMPatchBranchToCurrent(EMITTER(), FromLeft, BRANCHTYPE_CONDITIONAL);

        EMITTER()->ShouldEmit = WasEmit;
    }
    else if (IntegralTypeIsInteger(Left->Type))
    {
        return ExprBinary(Compiler, Left);
    }
    else 
    {
        ErrorAt(Compiler, &OpToken, "Invalid left operand for 'and': %s",
                IntegralTypeToStr(Left->Type)
        );
    }

    return *Left;
InvalidOperands:
    INVALID_BINARY_OP(&OpToken, Left->Type, Right.Type);
    return *Left;
}

static VarLocation ExprOr(PVMCompiler *Compiler, VarLocation *Left)
{
    PASCAL_NONNULL(Compiler);
    PASCAL_NONNULL(Left);


    VarLocation Right;
    Token OpToken = Compiler->Curr;
    if (TYPE_BOOLEAN == Left->Type)
    {
        bool WasEmit = EMITTER()->ShouldEmit;
        if (VAR_LIT == Left->LocationType && Left->As.Literal.Bool)
        {
            /* true or ...: don't emit right */
            EMITTER()->ShouldEmit = false;
        }

        U32 FromTrue = PVMEmitBranchIfTrue(EMITTER(), Left);

        Right = ParsePrecedence(Compiler, GetPrecedenceRule(OpToken.Type)->Prec + 1);
        IntegralType ResultType = CoerceTypes(Compiler, &OpToken, Left->Type, Right.Type);
        if (TYPE_INVALID == ResultType)
            goto InvalidOperands;
        PASCAL_ASSERT(TYPE_BOOLEAN == ResultType, "");
        PASCAL_ASSERT(TYPE_BOOLEAN == Right.Type, "");

        PVMPatchBranchToCurrent(EMITTER(), FromTrue, BRANCHTYPE_CONDITIONAL);

        EMITTER()->ShouldEmit = WasEmit;
    }
    else if (IntegralTypeIsInteger(Left->Type))
    {
        return ExprBinary(Compiler, Left);
    }
    else 
    {
        ErrorAt(Compiler, &OpToken, "Invalid left operand for 'or': %s",
                IntegralTypeToStr(Left->Type)
        );
    }

    return *Left;
InvalidOperands:
    INVALID_BINARY_OP(&OpToken, Left->Type, Right.Type);
    return *Left;
}




static const PrecedenceRule sPrecedenceRuleLut[TOKEN_TYPE_COUNT] = 
{
    [TOKEN_INTEGER_LITERAL] = { FactorLiteral,      NULL,           PREC_SINGLE },
    [TOKEN_NUMBER_LITERAL]  = { FactorLiteral,      NULL,           PREC_SINGLE },
    [TOKEN_STRING_LITERAL]  = { FactorLiteral,      NULL,           PREC_SINGLE },
    [TOKEN_TRUE]            = { FactorLiteral,      NULL,           PREC_SINGLE },
    [TOKEN_FALSE]           = { FactorLiteral,      NULL,           PREC_SINGLE },
    [TOKEN_IDENTIFIER]      = { FactorVariable,     NULL,           PREC_SINGLE },

    [TOKEN_CARET]           = { NULL,               VariableDeref,  PREC_VARIABLE },
    [TOKEN_DOT]             = { NULL,               VariableAccess, PREC_VARIABLE },
    [TOKEN_AT]              = { VariableAddrOf,     NULL,           PREC_SINGLE },

    [TOKEN_LEFT_PAREN]      = { FactorGrouping,     NULL,           PREC_FACTOR },
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

static VarLocation ParseAssignmentLhs(PVMCompiler *Compiler, Precedence Prec)
{
    const PrefixParseRoutine PrefixRoutine = GetPrecedenceRule(Compiler->Curr.Type)->PrefixRoutine;
    if (NULL == PrefixRoutine)
    {
        ErrorAt(Compiler, &Compiler->Curr, "Expected expression.");
        return (VarLocation) {
            .LocationType = VAR_INVALID,
        };
    }

    VarLocation Left = PrefixRoutine(Compiler);


    /* only parse next op if they have the same or lower precedence */
    while (Prec <= GetPrecedenceRule(Compiler->Next.Type)->Prec)
    {
        ConsumeToken(Compiler); /* the operator */
        const InfixParseRoutine InfixRoutine = 
            GetPrecedenceRule(Compiler->Curr.Type)->InfixRoutine;

        PASCAL_NONNULL(InfixRoutine);
        Left = InfixRoutine(Compiler, &Left);
    }
    return Left;
}

static VarLocation ParsePrecedence(PVMCompiler *Compiler, Precedence Prec)
{
    ConsumeToken(Compiler);
    return ParseAssignmentLhs(Compiler, Prec);
}






/* returns the type that both sides should be */
IntegralType CoerceTypes(PVMCompiler *Compiler, const Token *Op, IntegralType Left, IntegralType Right)
{
    PASCAL_ASSERT(Left >= TYPE_INVALID && Right >= TYPE_INVALID, "");
    if (Left > TYPE_COUNT || Right > TYPE_COUNT)
        PASCAL_UNREACHABLE("Invalid types");

    IntegralType CommonType = sCoercionRules[Left][Right];
    if (TYPE_INVALID == CommonType)
        goto InvalidTypeCombo;

    return CommonType;
InvalidTypeCombo:
    if (NULL == Op)
    {
        Error(Compiler, "Invalid combination of %s and %s.", 
                IntegralTypeToStr(Left), IntegralTypeToStr(Right)
        );
    }
    else
    {
        ErrorAt(Compiler, Op, "Invalid combination of %s and %s.",
                IntegralTypeToStr(Left), IntegralTypeToStr(Right)
        );
    }
    return TYPE_INVALID;
}


bool ConvertTypeImplicitly(PVMCompiler *Compiler, IntegralType To, VarLocation *From)
{
    if (To == From->Type)
    {
        return true;
    }
    switch (From->LocationType)
    {
    case VAR_LIT:
    {
        if (IntegralTypeIsInteger(To) && IntegralTypeIsInteger(From->Type))
            break;

        if (IntegralTypeIsFloat(To))
        {
            if (IntegralTypeIsFloat(From->Type))
                break;
            if (IntegralTypeIsInteger(From->Type))
            {
                From->As.Literal.Flt = From->As.Literal.Int;
                break;
            }
        }

        goto InvalidTypeConversion;
    } break;
    case VAR_REG:
    {
        if (IntegralTypeIsInteger(To) && IntegralTypeIsInteger(From->Type))
        {
            PVMEmitIntegerTypeConversion(EMITTER(), From->As.Register, To, From->As.Register, From->Type);
            From->Type = To;
            break;
        }
        if (IntegralTypeIsFloat(To))
        {
            if (IntegralTypeIsFloat(From->Type))
            {
                PVMEmitFloatTypeConversion(EMITTER(), From->As.Register, To, From->As.Register, From->Type);
            }
            else if (IntegralTypeIsInteger(From->Type))
            {
                VarLocation FloatReg = PVMAllocateRegister(EMITTER(), To);
                PVMEmitIntToFltTypeConversion(EMITTER(), 
                        FloatReg.As.Register, To, From->As.Register, From->Type
                );
                PVMFreeRegister(EMITTER(), From->As.Register);
                From->As.Register.ID = FloatReg.As.Register.ID;
            }
            else goto InvalidTypeConversion;
            From->Type = To;
            break;
        }
        goto InvalidTypeConversion;
    } break;
    case VAR_MEM:
    {
        From->Type = To;
        return true;
        if (To != From->Type)
        {
            Error(Compiler, "Cannot reinterpret memory of %s as %s.", 
                    IntegralTypeToStr(From->Type), IntegralTypeToStr(To)
            );
            return false;
        }
    } break;
    case VAR_FLAG:
    {
        if (TYPE_BOOLEAN != To)
            goto InvalidTypeConversion;
    } break;
    case VAR_INVALID:
    case VAR_SUBROUTINE: 
    case VAR_BUILTIN:
    {
        PASCAL_UNREACHABLE("");
    } break;
    }

    From->Type = To;
    return true;
InvalidTypeConversion:
    Error(Compiler, "Cannot convert from %s to %s.", IntegralTypeToStr(From->Type), IntegralTypeToStr(To));
    return false;
}




