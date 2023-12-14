


#include "Compiler/Error.h"
#include "Compiler/VarList.h"
#include "Compiler/Expr.h"



typedef struct TypeAttribute 
{
    bool Packed;
    IntegralType Type;
    USize Size;
    union {
        PascalVar Var;
        PascalVartab Record;
    } Pointer;
} TypeAttribute;

static bool ParseTypename(PVMCompiler *Compiler, bool IsParameterList, TypeAttribute *Out)
{
    PASCAL_NONNULL(Compiler);
    PASCAL_NONNULL(Out);
    bool Pointer = ConsumeIfNextIs(Compiler, TOKEN_CARET);
    Out->Packed = false;

    if (ConsumeIfNextIs(Compiler, TOKEN_RECORD))
    {
        /* annonymous record */
        if (IsParameterList)
        {
            ErrorAt(Compiler, &Compiler->Curr, "Record definition is not allowed in parameter list.");
            return false;
        }

        TmpIdentifiers SaveList = CompilerSaveTmp(Compiler);
        PascalVar *Record = CompileRecordDefinition(Compiler, &Compiler->EmptyToken);
        CompilerUnsaveTmp(Compiler, &SaveList);
        if (NULL == Record)
            return false;

        Out->Size = CompilerGetSizeOfType(Compiler, TYPE_RECORD, Record->Location);
        Out->Type = TYPE_RECORD;
        PASCAL_NONNULL(Record->Location);
        Out->Pointer.Record = Record->Location->PointerTo.Record;
        return true;
    }
    if (ConsumeIfNextIs(Compiler, TOKEN_ARRAY))
    {
        PASCAL_UNREACHABLE("TODO: array");
    }
    if (ConsumeIfNextIs(Compiler, TOKEN_IDENTIFIER))
    {
        PascalVar *Typename = GetIdenInfo(Compiler, &Compiler->Curr, "Undefined type name.");
        if (NULL == Typename)
            return false;

        Out->Size = CompilerGetSizeOfType(Compiler, Typename->Type, Typename->Location);
        if (Pointer)
        {
            Out->Type = TYPE_POINTER;
            Out->Pointer.Var = *Typename;
        }
        else if (TYPE_RECORD == Typename->Type)
        {
            PASCAL_NONNULL(Typename->Location);
            Out->Type = TYPE_RECORD;
            Out->Pointer.Record = Typename->Location->PointerTo.Record;
        }
        else if (NULL == Typename->Location)
        {
            Out->Type = Typename->Type;
        }
        else
        {
            /* if the identifier has a location, ie Location is not NULL, 
             * then it's not a type */
            ErrorAt(Compiler, &Compiler->Curr, "'%.*s' is not a type name in this scope.", 
                    Compiler->Curr.Len, Compiler->Curr.Str
            );
            return false;
        }
        return true;
    }
    else 
    {
        Error(Compiler, "Expected type name after '%.*s'.",
                Compiler->Curr.Len, Compiler->Curr.Str
        );
    }
    return false;
}



static bool ParseVarList(PVMCompiler *Compiler, bool IsParameterList, TypeAttribute *Out)
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
    const char *Type = "variable";
    if (IsParameterList)
        Type = "parameter";
    int Count = 0;
    do {
        ConsumeOrError(Compiler, TOKEN_IDENTIFIER, "Expected %s name.", Type);
        CompilerPushTmp(Compiler, Compiler->Curr);
        Count++;
    } while (ConsumeIfNextIs(Compiler, TOKEN_COMMA));

    if (!ConsumeOrError(Compiler, TOKEN_COLON, 
    "Expected ':' and type name after %s%s.", Type, Count > 1 ? "s" : ""))
    {
        return false;
    }

    /* then parses typename */
    return ParseTypename(Compiler, IsParameterList, Out);
}


/* alignment should be pow 2 */
U32 CompileVarList(PVMCompiler *Compiler, U32 StartAddr, UInt Alignment, bool Global)
{
    /* 
     *  a, b, c: typename;
     *  pa, pb: ^typename;
     *  s1, s2: Record ... end;
     *  a1, a2: Array of typename
     *          Array[~~~]
     */
    TypeAttribute Type;
    if (!ParseVarList(Compiler, false, &Type))
        return StartAddr;

    UInt VarCount = CompilerGetTmpCount(Compiler);
    U32 AlignedSize = Type.Size;
    U32 Mask = ~(Alignment - 1);
    U32 EndAddr = StartAddr;
    if (Type.Size & ~Mask)
    {
        AlignedSize = (Type.Size + Alignment) & Mask;
    }

    UInt Base = PVM_REG_FP;
    if (Global)
        Base = PVM_REG_GP;
    /* defining the variables */
    for (UInt i = 0; i < VarCount; i++)
    {
        VarLocation *Location = CompilerAllocateVarLocation(Compiler);
        DefineIdentifier(Compiler, 
                CompilerGetTmp(Compiler, i),
                Type.Type,
                Location
        );

        Location->Size = AlignedSize;
        Location->Type = Type.Type;
        Location->LocationType = VAR_MEM;
        Location->As.Memory = (VarMemory) {
            .Location = EndAddr,
            .RegPtr.ID = Base,
        };
        if (TYPE_RECORD == Type.Type)
            Location->PointerTo.Record = Type.Pointer.Record;
        else Location->PointerTo.Var = Type.Pointer.Var;
        EndAddr += AlignedSize;
    }
    return EndAddr;
}



bool CompileParameterList(PVMCompiler *Compiler, VarSubroutine *Subroutine)
{
    /*
     * procedure fn;
     * procedure fn(a, b: integer);
     * procedure fn(a: integer; b: array of integer);
     *
     * */
    PASCAL_NONNULL(Compiler);
    PASCAL_NONNULL(Subroutine);
    PASCAL_ASSERT(!IsAtGlobalScope(Compiler), "Cannot compile param list at global scope");

    Subroutine->ArgCount = 0;
    if (!ConsumeIfNextIs(Compiler, TOKEN_LEFT_PAREN))
        return true;
    if (ConsumeIfNextIs(Compiler, TOKEN_RIGHT_PAREN))
        return true;

    Subroutine->StackArgSize = 0;
    I32 Base = 0;
    do {
        if (ConsumeIfNextIs(Compiler, TOKEN_VAR))
        {
            PASCAL_UNREACHABLE("TODO: passing by ref");
        }
        else if (ConsumeIfNextIs(Compiler, TOKEN_CONST))
        {
            PASCAL_UNREACHABLE("TODO: const param");
        }
        /* parse the parameters */
        TypeAttribute Type;
        if (!ParseVarList(Compiler, true, &Type))
        {
            /* flushes temporary params and pretend nothing happened */
            CompilerResetTmp(Compiler);
            Compiler->Panic = false;
        }

        /* define the parameters */
        UInt VarCount = CompilerGetTmpCount(Compiler);
        for (U32 i = 0; i < VarCount; i++)
        {
            VarLocation *Location = CompilerAllocateVarLocation(Compiler);
            PascalVar *Var = DefineIdentifier(Compiler, 
                    CompilerGetTmp(Compiler, i),
                    Type.Type,
                    Location
            );

            *Location = PVMSetParam(EMITTER(), Subroutine->ArgCount, Type.Type, Type.Size, &Base);
            /* arg will be freed when compilation of function is finished */
            PVMMarkArgAsOccupied(EMITTER(), Location);
            if (TYPE_RECORD == Type.Type)
                Location->PointerTo.Record = Type.Pointer.Record;
            else Location->PointerTo.Var = Type.Pointer.Var;

            SubroutineDataPushParameter(Compiler->GlobalAlloc, Subroutine, Var);
            //if (Subroutine->ArgCount >= PVM_ARGREG_COUNT)
                Subroutine->StackArgSize += Type.Size;
        }

        if (ConsumeIfNextIs(Compiler, TOKEN_RIGHT_PAREN))
            return true;

        if (ConsumeIfNextIs(Compiler, TOKEN_COMMA))
            ErrorAt(Compiler, &Compiler->Curr, "Use ';' to separate parameters.");
        else if (!ConsumeOrError(Compiler, TOKEN_SEMICOLON, "Expected ';' after parameter list."))
            return false;
    } while (!IsAtEnd(Compiler));

    return false;
}


PascalVar *CompileRecordDefinition(PVMCompiler *Compiler, const Token *Name)
{
    PASCAL_NONNULL(Compiler);
    PASCAL_NONNULL(Name);

    /* 'record' consumed */
    PascalVartab RecordScope = VartabInit(Compiler->GlobalAlloc, 16);
    CompilerPushScope(Compiler, &RecordScope);

    U32 TotalSize = 0;
    while (!IsAtEnd(Compiler) && NextTokenIs(Compiler, TOKEN_IDENTIFIER))
    {
        /* packed alignment: 1 */
        U32 NextLine = CompileVarList(Compiler, TotalSize, 1, false);
        if (TotalSize == NextLine)
        {
            return NULL;
        }
        TotalSize = NextLine;

        if (NextTokenIs(Compiler, TOKEN_END))
            break;

        if (!ConsumeOrError(Compiler, TOKEN_SEMICOLON, 
        "Expect ';' or 'end' after member declaration."))
        {
            return NULL;
        }
    }
    ConsumeOrError(Compiler, TOKEN_END, "Expected 'end' after record definition.");

    CompilerPopScope(Compiler);
    VarLocation *Type = CompilerAllocateVarLocation(Compiler);
    Type->Type = TYPE_RECORD;
    Type->LocationType = VAR_INVALID;
    Type->PointerTo.Record = RecordScope;
    Type->Size = TotalSize;
    return DefineIdentifier(Compiler, Name, TYPE_RECORD, Type);
}



void CompileArgumentList(PVMCompiler *Compiler, 
        const Token *FunctionName, const VarSubroutine *Subroutine)
{
    PASCAL_NONNULL(Compiler);
    PASCAL_NONNULL(FunctionName);
    PASCAL_NONNULL(Subroutine);

    UInt ExpectedArgCount = Subroutine->ArgCount;
    UInt ArgCount = 0;
    /* no args */
    if (!ConsumeIfNextIs(Compiler, TOKEN_LEFT_PAREN))
        goto CheckArgs;
    if (ConsumeIfNextIs(Compiler, TOKEN_RIGHT_PAREN))
        goto CheckArgs;


    PASCAL_ASSERT(Compiler->Flags.CallConv == CALLCONV_MSX64, 
            "TODO: other calling convention"
    );

    I32 Base = PVMStartArg(EMITTER(), Subroutine->StackArgSize);
    do {
        if (ArgCount < ExpectedArgCount)
        {
            const VarLocation *CurrentArg = Subroutine->Args[ArgCount].Location;
            PASCAL_NONNULL(CurrentArg);

            VarLocation Arg = PVMSetArg(EMITTER(), ArgCount, CurrentArg->Type, CurrentArg->Size, &Base);
            if (CurrentArg->Type == TYPE_RECORD)
                Arg.PointerTo.Record = CurrentArg->PointerTo.Record;
            else Arg.PointerTo.Var = CurrentArg->PointerTo.Var;

            CompileExprInto(Compiler, NULL, &Arg);
        }
        else
        {
            FreeExpr(Compiler, CompileExpr(Compiler));
        }
        ArgCount++;
    } while (ConsumeIfNextIs(Compiler, TOKEN_COMMA));

    /* end of arg */
    ConsumeOrError(Compiler, TOKEN_RIGHT_PAREN, "Expected ')' after argument list.");


CheckArgs:
    if (ArgCount != ExpectedArgCount)
    {
        ErrorAt(Compiler, FunctionName, 
                "Expected %d arguments but got %d instead.", Subroutine->ArgCount, ArgCount
        );
    }
}

void CompileSubroutineCall(PVMCompiler *Compiler, 
        VarSubroutine *Callee, const Token *Name, VarLocation *ReturnValue)
{
    PASCAL_NONNULL(Compiler);
    PASCAL_NONNULL(Callee);
    PASCAL_NONNULL(Name);

    U32 CallSite = 0;
    UInt ReturnReg = NO_RETURN_REG;
    SaveRegInfo SaveRegs = { 0 };
    if (NULL != ReturnValue)
    {
        ReturnReg = ReturnValue->As.Register.ID;
        /* call the function */
        SaveRegs = PVMEmitSaveCallerRegs(EMITTER(), ReturnReg);
        CompileArgumentList(Compiler, Name, Callee);
        CallSite = PVMEmitCall(EMITTER(), Callee);

        /* carefully move return reg into the return location */
        VarLocation Tmp = PVMSetReturnType(EMITTER(), ReturnValue->Type);
        PVMEmitMov(EMITTER(), ReturnValue, &Tmp);
    }
    else
    {
        /* calling a procedure, or function without caring about its return value */
        SaveRegs = PVMEmitSaveCallerRegs(EMITTER(), NO_RETURN_REG);
        CompileArgumentList(Compiler, Name, Callee);
        CallSite = PVMEmitCall(EMITTER(), Callee);
    }

    /* deallocate stack args */
    if (Callee->StackArgSize) 
    {
        PVMEmitStackAllocation(EMITTER(), -Callee->StackArgSize);
    }
    PVMEmitUnsaveCallerRegs(EMITTER(), ReturnReg, SaveRegs);
    if (!Callee->Defined)
    {
        SubroutineDataPushRef(Compiler->GlobalAlloc, Callee, CallSite);
    }
}



