



#include <stdarg.h>
#include "Compiler/Compiler.h"
#include "Compiler/Error.h"
#include "Compiler/VarList.h"
#include "Compiler/Expr.h"




SubroutineParameterList CompileParameterListWithParentheses(PascalCompiler *Compiler, PascalVartab *Scope)
{
    PASCAL_NONNULL(Compiler);
    PASCAL_NONNULL(Scope);

    SubroutineParameterList ParameterList = { 0 };
    if (ConsumeIfNextTokenIs(Compiler, TOKEN_LEFT_PAREN))
    {
        if (!NextTokenIs(Compiler, TOKEN_RIGHT_PAREN))
        {
            ParameterList = CompileParameterList(Compiler, Scope);
        }
        ConsumeOrError(Compiler, TOKEN_RIGHT_PAREN, "Expected ')' after parameter list.");
    }
    return ParameterList;
}

static PascalVar *FindTypename(PascalCompiler *Compiler, const Token *Name)
{
    PASCAL_NONNULL(Compiler);
    PASCAL_NONNULL(Name);
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



static I64 LiteralToI64(PascalCompiler *Compiler, const VarLocation *Location)
{
    IntegralType Type = Location->Type.Integral;
    if (Location->LocationType != VAR_LIT)
    {
        Error(Compiler, "Expected constant expression.");
        return 0;
    }
    if (!IntegralTypeIsOrdinal(Type))
    {
        Error(Compiler, "Expected constant expression have ordinal type.");
        return 0;
    }
    return OrdinalLiteralToI64(Location->As.Literal, Location->Type.Integral);
}

static RangeIndex ConsumeRange(PascalCompiler *Compiler)
{
    PASCAL_NONNULL(Compiler);
    RangeIndex Range = { 0 };
    VarLocation ConstantExpr = CompileExpr(Compiler);

    if (VAR_LIT == ConstantExpr.LocationType)
    {
        Range.Low = LiteralToI64(Compiler, &ConstantExpr);
        ConsumeOrError(Compiler, TOKEN_DOT_DOT, "Expected '..' after expression.");
        VarLocation RangeEnd = CompileExpr(Compiler);
        Range.High = LiteralToI64(Compiler, &RangeEnd);
    }
    else if (VAR_TYPENAME == ConstantExpr.LocationType)
    {
        IntegralType Type = ConstantExpr.Type.Integral;
        if (!IntegralTypeIsOrdinal(Type))
        {
            Error(Compiler, "Expected ordinal type.");
        }
        if (IntegralTypeIsInteger(Type) 
        && (Type == TYPE_I32 || Type == TYPE_I64 
         || Type == TYPE_U32 || Type == TYPE_U64))
        {
            Error(Compiler, "Data element too large.");
        }
        static const RangeIndex Ranges[TYPE_COUNT] = {
            [TYPE_BOOLEAN] = { .Low = 0, .High = 1 },
            [TYPE_I8] = { .Low = INT8_MIN, .High = INT8_MAX },
            [TYPE_CHAR] = { .Low = CHAR_MIN, .High = CHAR_MAX },
            [TYPE_U8] = { .Low = 0, .High = UINT8_MAX },

            [TYPE_I16] = { .Low = INT16_MIN, .High = INT16_MAX },
            [TYPE_U16] = { .Low = 0, .High = UINT16_MAX },
        };
        return Ranges[Type];
    }
    else
    {
        Error(Compiler, "Expected constant expression.");
    }
    return Range;
}



#define ParseTypename(pCompiler, pVartype) ParseAndDefineTypenameInternal(pCompiler, NULL, pVartype)
static bool ParseAndDefineTypenameInternal(PascalCompiler *Compiler, const Token *Identifier, VarType *Out)
{
    PASCAL_NONNULL(Compiler);
    PASCAL_NONNULL(Out);
    if (ConsumeIfNextTokenIs(Compiler, TOKEN_RECORD))
    {
        /* records are different from other types because it can be self-referenced 
         * inside its body via pointers, so we have to define its name and type first.
         * Although only self-reference via pointers is allowed,
         * the programmer can just declare the record recursively, 
         * TODO: disallow that */
        const StringView Name = NULL == Identifier?
            Compiler->EmptyToken.Lexeme
            : Identifier->Lexeme;

        /* initialize the record and predefine it unlike other types */
        PascalVartab RecordScope = VartabInit(&Compiler->InternalAlloc, 16);
        *Out = VarTypeRecord(Name, RecordScope, 0);
        PascalVar *RecordName = DefineIdentifier(Compiler, Identifier, *Out, NULL);

        U32 TotalSize = 0;
        CompilerPushScope(Compiler, &RecordScope);
        TmpIdentifiers SaveList = CompilerSaveTmp(Compiler);
        while (!IsAtEnd(Compiler) && NextTokenIs(Compiler, TOKEN_IDENTIFIER))
        {
            /* packed alignment: 1 */
            U32 NextLine = CompileVarList(Compiler, 0, TotalSize, 1);
            if (TotalSize == NextLine) /* error, could not compile type */
                break;

            /* update size */
            TotalSize = NextLine;
            if (NextTokenIs(Compiler, TOKEN_END) 
            || !ConsumeOrError(Compiler, TOKEN_SEMICOLON, "Expect ';' or 'end' after member declaration."))
            {
                break;
            }
        }
        CompilerUnsaveTmp(Compiler, &SaveList);
        ConsumeOrError(Compiler, TOKEN_END, "Expected 'end' after record definition.");
        CompilerPopScope(Compiler);

        RecordName->Type.Size = TotalSize;
        Out->Size = TotalSize;
        return true;
    }
    else if (ConsumeIfNextTokenIs(Compiler, TOKEN_ARRAY))
    {
        if (ConsumeIfNextTokenIs(Compiler, TOKEN_LEFT_BRACKET))
        {
            RangeIndex Range = ConsumeRange(Compiler);
            ConsumeOrError(Compiler, TOKEN_RIGHT_BRACKET, "Expected ']' after expression.");
            ConsumeOrError(Compiler, TOKEN_OF, "Expected 'of'.");
            VarType Type;
            ParseTypename(Compiler, &Type);
            *Out = VarTypeStaticArray(Range, CompilerCopyType(Compiler, Type));
            return true;
        }
        else
        {
            PASCAL_UNREACHABLE("TODO: dynamic and open array");
        }
    }
    else if (ConsumeIfNextTokenIs(Compiler, TOKEN_FUNCTION))
    {
        PascalVartab Scope = VartabInit(&Compiler->InternalAlloc, PVM_INITIAL_VAR_PER_SCOPE);
        SubroutineParameterList ParameterList = CompileParameterListWithParentheses(Compiler, &Scope);

        ConsumeOrError(Compiler, TOKEN_COLON, "Expeccted ':' after function paramter list.");
        if (!ConsumeOrError(Compiler, TOKEN_IDENTIFIER, "Expected function return type."))
            return false;
        PascalVar *Type = FindTypename(Compiler, &Compiler->Curr);
        if (NULL == Type)
            return false;

        VarType *ReturnType = CompilerCopyType(Compiler, Type->Type);
        VarType Function = VarTypeSubroutine(ParameterList, Scope, ReturnType, 0);
        *Out = VarTypePtr(CompilerCopyType(Compiler, Function));
    }
    else if (ConsumeIfNextTokenIs(Compiler, TOKEN_PROCEDURE))
    {
        PascalVartab Scope = VartabInit(&Compiler->InternalAlloc, PVM_INITIAL_VAR_PER_SCOPE);
        SubroutineParameterList ParameterList = CompileParameterListWithParentheses(Compiler, &Scope);
        VarType Procedure = VarTypeSubroutine(ParameterList, Scope, NULL, 0);
        *Out = VarTypePtr(CompilerCopyType(Compiler, Procedure));
    }
    else if (ConsumeIfNextTokenIs(Compiler, TOKEN_IDENTIFIER))
    {
        /* just copy the type */
        PascalVar *Typename = GetIdenInfo(Compiler, &Compiler->Curr, "Undefined type name.");
        if (NULL == Typename)
            return false;

        /* has a location, so not a type name */
        if (NULL != Typename->Location)
        {
            ErrorAt(Compiler, &Compiler->Curr, "'"STRVIEW_FMT"' is not a type name in this scope.", 
                    STRVIEW_FMT_ARG(&Compiler->Curr.Lexeme)
            );
            return false;
        }
        *Out = Typename->Type;
    }
    else if (ConsumeIfNextTokenIs(Compiler, TOKEN_CARET))
    {
        VarType Pointee;
        /* recursive call */
        ParseTypename(Compiler, &Pointee);
        *Out = VarTypePtr(CompilerCopyType(Compiler, Pointee));
    }
    else 
    {
        Error(Compiler, "Expected type name after '"STRVIEW_FMT"'.",
                STRVIEW_FMT_ARG(&Compiler->Curr.Lexeme)
        );
        return false;
    }

    /* define the type name */
    if (NULL != Identifier)
    {
        DefineIdentifier(Compiler, Identifier, *Out, NULL);
    }
    return true;
}

void ParseAndDefineTypename(PascalCompiler *Compiler, const Token *Identifier)
{
    VarType Dummy;
    ParseAndDefineTypenameInternal(Compiler, Identifier, &Dummy);
}


static bool ParseVarList(PascalCompiler *Compiler, VarType *Out)
{
    /* 
     *  a, b, c: typename;
     *  pa, pb: ^typename;
     *  s1, s2: Record ... end;
     *  a1, a2: Array of typename
     *          Array[~~~]
     */

    PASCAL_NONNULL(Compiler);
    PASCAL_NONNULL(Out);

    /* a, b, c part first */
    CompilerResetTmp(Compiler);
    int Count = 0;
    do {
        ConsumeOrError(Compiler, TOKEN_IDENTIFIER, "Expected variable name.");
        CompilerPushTmp(Compiler, Compiler->Curr);
        Count++;
    } while (ConsumeIfNextTokenIs(Compiler, TOKEN_COMMA));

    if (!ConsumeOrError(Compiler, TOKEN_COLON, 
    "Expected ':' and type name after variable%s.", Count > 1 ? "s" : ""))
    {
        return false;
    }

    /* then parses typename */
    return ParseTypename(Compiler, Out);
}


/* alignment should be pow 2 */
U32 CompileVarList(PascalCompiler *Compiler, UInt BaseRegister, U32 StartAddr, U32 Alignment)
{
    /* 
     *  a, b, c: typename;
     *  pa, pb: ^typename;
     *  s1, s2: Record ... end;
     *  a1, a2: Array of typename
     *          Array[~~~]
     */
    VarType Type;
    if (!ParseVarList(Compiler, &Type))
        return StartAddr;

    UInt VarCount = CompilerGetTmpCount(Compiler);
    U32 AlignedSize = Type.Size;
    U32 Mask = ~(Alignment - 1);
    U32 EndAddr = StartAddr;
    if (Type.Size & ~Mask)
    {
        AlignedSize = (Type.Size + Alignment) & Mask;
    }

    /* defining the variables */
    for (UInt i = 0; i < VarCount; i++)
    {
        VarLocation *Location = CompilerAllocateVarLocation(Compiler);
        DefineIdentifier(Compiler, 
                CompilerGetTmp(Compiler, i),
                Type,
                Location
        );

        Location->Type = Type;
        Location->LocationType = VAR_MEM;
        Location->As.Memory = (VarMemory) {
            .Location = EndAddr,
            .RegPtr.ID = BaseRegister,
        };
        EndAddr += AlignedSize;
    }
    return EndAddr;
}



SubroutineParameterList CompileParameterList(PascalCompiler *Compiler, PascalVartab *Scope)
{
    SubroutineParameterList ParameterList = ParameterListInit();
    PASCAL_NONNULL(Compiler);
    PASCAL_NONNULL(Scope);

    /* '(' is consumed */
    do {
        if (ConsumeIfNextTokenIs(Compiler, TOKEN_VAR))
        {
            PASCAL_UNREACHABLE("TODO: reference parameter.");
        }
        else if (ConsumeIfNextTokenIs(Compiler, TOKEN_CONST))
        {
            PASCAL_UNREACHABLE("TODO: const parameter.");
        }

        /* parameter list is completely different from other variable declaration */
        /* so the code to parse it is unique from others */
        CompilerResetTmp(Compiler);
        do {
            ConsumeOrError(Compiler, TOKEN_IDENTIFIER, "Expected parameter name.");
            CompilerPushTmp(Compiler, Compiler->Curr);
        } while (ConsumeIfNextTokenIs(Compiler, TOKEN_COMMA));
        ConsumeOrError(Compiler, TOKEN_COLON, "Expected ':' or ',' after parameter name.");


        if (NextTokenIs(Compiler, TOKEN_SEMICOLON))
        {
            PASCAL_UNREACHABLE("TODO: untyped parameters.");
        }
        else if (NextTokenIs(Compiler, TOKEN_ARRAY))
        {
            PASCAL_UNREACHABLE("TODO: array parameters.");
        }
        else if (ConsumeOrError(Compiler, TOKEN_IDENTIFIER, "Expected type name."))
        {
            Token Typename = Compiler->Curr;
            PascalVar *TypeInfo = GetIdenInfo(Compiler, &Typename, "Undefined type name.");
            if (NULL == TypeInfo)
                continue; /* error reported */

            UInt IdentifierCount = CompilerGetTmpCount(Compiler);
            for (UInt i = 0; i < IdentifierCount; i++)
            {
                VarLocation *ParameterLocation = CompilerAllocateVarLocation(Compiler);
                PASCAL_NONNULL(ParameterLocation);
                ParameterLocation->Type = TypeInfo->Type;

                /* we don't set the location here since it is up to the caller to do that
                 * (should just be CompileSubroutineBlock()) */
                PascalVar *Param = DefineAtScope(
                        Compiler, Scope, 
                        CompilerGetTmp(Compiler, i), 
                        TypeInfo->Type, 
                        ParameterLocation
                );
                ParameterListPush(&ParameterList, &Compiler->InternalAlloc, Param);
            }
        }
        else 
        {
            /* Error from above */
        }
    } while (ConsumeIfNextTokenIs(Compiler, TOKEN_SEMICOLON));
    return ParameterList;
}



static void CompilePartialArgumentList(PascalCompiler *Compiler, 
        const Token *Callee, const SubroutineParameterList *Parameters,
        I32 *Base, UInt HiddenParamCount)
{
    PASCAL_NONNULL(Compiler);
    PASCAL_NONNULL(Callee);
    PASCAL_NONNULL(Compiler);
    PASCAL_NONNULL(Base);
    PASCAL_ASSERT(Compiler->Flags.CallConv == CALLCONV_MSX64, 
            "TODO: other calling convention"
    );

    UInt ExpectedArgCount = Parameters->Count;
    UInt ArgCount = 0;
    if (!ConsumeIfNextTokenIs(Compiler, TOKEN_RIGHT_PAREN))
    {
        do {
            if (ArgCount < ExpectedArgCount)
            {
                PASCAL_NONNULL(Parameters->Params[ArgCount].Location);
                const VarType *ArgType = &Parameters->Params[ArgCount].Location->Type;

                VarLocation Arg = PVMSetArg(EMITTER(), ArgCount + HiddenParamCount, *ArgType, Base);
                CompileExprInto(Compiler, NULL, &Arg);
                PVMMarkArgAsOccupied(EMITTER(), &Arg);
            }
            else
            {
                FreeExpr(Compiler, CompileExpr(Compiler));
            }
            ArgCount++;
        } while (ConsumeIfNextTokenIs(Compiler, TOKEN_COMMA));
        ConsumeOrError(Compiler, TOKEN_RIGHT_PAREN, "Expected ')' after argument list.");
    }
    if (ArgCount != ExpectedArgCount)
    {
        ErrorAt(Compiler, Callee, 
                "Expected %d arguments but %d were given.", ExpectedArgCount, ArgCount
        );
    }
}

void CompileArgumentList(PascalCompiler *Compiler, 
        const Token *Callee, const SubroutineData *Subroutine, 
        I32 *Base, UInt HiddenParamCount)
{
    PASCAL_NONNULL(Compiler);
    PASCAL_NONNULL(Callee);
    PASCAL_NONNULL(Subroutine);
    PASCAL_NONNULL(Base);

    if (ConsumeIfNextTokenIs(Compiler, TOKEN_LEFT_PAREN)
    || TOKEN_LEFT_PAREN == Compiler->Curr.Type)
    {
        CompilePartialArgumentList(Compiler, 
                Callee, &Subroutine->ParameterList,
                Base, HiddenParamCount
        );
    }
    else if (Subroutine->ParameterList.Count != 0)
    {
        ErrorAt(Compiler, Callee, "Expected %d arguments but none were given.", 
                Subroutine->ParameterList.Count
        );
    }
}


void CompilerEmitCall(PascalCompiler *Compiler, const VarLocation *Location, SaveRegInfo SaveRegs)
{
    PASCAL_NONNULL(Compiler);
    PASCAL_NONNULL(Location);

    if (TYPE_POINTER == Location->Type.Integral)
    {
        /* function ptr was saved among saved caller regs */
        if (VAR_REG == Location->LocationType
        && PVMRegIsSaved(SaveRegs, Location->As.Register.ID))
        {
            /* retreive its location on stack for the call */
            VarLocation SavedFunctionPointer = PVMRetreiveSavedCallerReg(EMITTER(), 
                    SaveRegs, Location->As.Register.ID, Location->Type
            );
            PVMEmitCallPtr(EMITTER(), &SavedFunctionPointer);
        }
        else
        {
            PVMEmitCallPtr(EMITTER(), Location);
        }
    }
    else
    {
        /* call regular function */
        U32 CallSite = PVMEmitCall(EMITTER(), 0);
        PushSubroutineReference(Compiler, &Location->As.SubroutineLocation, CallSite);
    }
}




