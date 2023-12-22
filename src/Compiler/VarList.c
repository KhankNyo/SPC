


#include "Compiler/Error.h"
#include "Compiler/VarList.h"
#include "Compiler/Expr.h"



typedef struct TypeAttribute 
{
    bool Packed;
    VarType Type;
} TypeAttribute;


static bool ParseTypename(PVMCompiler *Compiler, TypeAttribute *Out)
{
    PASCAL_NONNULL(Compiler);
    PASCAL_NONNULL(Out);
    bool Pointer = ConsumeIfNextIs(Compiler, TOKEN_CARET);
    Out->Packed = false;

    if (ConsumeIfNextIs(Compiler, TOKEN_RECORD))
    {
        /* annonymous record */
        TmpIdentifiers SaveList = CompilerSaveTmp(Compiler);
        PascalVar *Record = CompileRecordDefinition(Compiler, &Compiler->EmptyToken);
        CompilerUnsaveTmp(Compiler, &SaveList);
        if (NULL == Record)
            return false;

        Out->Type = Record->Type;
        return true;
    }
    if (ConsumeIfNextIs(Compiler, TOKEN_ARRAY))
    {
        PASCAL_UNREACHABLE("TODO: array");
    }
    if (ConsumeIfNextIs(Compiler, TOKEN_FUNCTION))
    {
        PASCAL_UNREACHABLE("TODO: fnptr");
    }
    if (ConsumeIfNextIs(Compiler, TOKEN_IDENTIFIER))
    {
        PascalVar *Typename = GetIdenInfo(Compiler, &Compiler->Curr, "Undefined type name.");
        if (NULL == Typename)
            return false;
        if (NULL != Typename->Location)
        {
            ErrorAt(Compiler, &Compiler->Curr, "'"STRVIEW_FMT"' is not a type name in this scope.", 
                    STRVIEW_FMT_ARG(&Compiler->Curr.Lexeme)
            );
            return false;
        }

        if (Pointer)
            Out->Type = VarTypePtr(CompilerCopyType(Compiler, Typename->Type));
        else
            Out->Type = Typename->Type;
        return true;
    }
    else 
    {
        Error(Compiler, "Expected type name after '"STRVIEW_FMT"'.",
                STRVIEW_FMT_ARG(&Compiler->Curr.Lexeme)
        );
    }
    return false;
}



static bool ParseVarList(PVMCompiler *Compiler, TypeAttribute *Out)
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
    } while (ConsumeIfNextIs(Compiler, TOKEN_COMMA));

    if (!ConsumeOrError(Compiler, TOKEN_COLON, 
    "Expected ':' and type name after variable%s.", Count > 1 ? "s" : ""))
    {
        return false;
    }

    /* then parses typename */
    return ParseTypename(Compiler, Out);
}


/* alignment should be pow 2 */
U32 CompileVarList(PVMCompiler *Compiler, UInt BaseRegister, U32 StartAddr, U32 Alignment)
{
    /* 
     *  a, b, c: typename;
     *  pa, pb: ^typename;
     *  s1, s2: Record ... end;
     *  a1, a2: Array of typename
     *          Array[~~~]
     */
    TypeAttribute TypeAttr;
    if (!ParseVarList(Compiler, &TypeAttr))
        return StartAddr;

    UInt VarCount = CompilerGetTmpCount(Compiler);
    U32 AlignedSize = TypeAttr.Type.Size;
    U32 Mask = ~(Alignment - 1);
    U32 EndAddr = StartAddr;
    if (TypeAttr.Type.Size & ~Mask)
    {
        AlignedSize = (TypeAttr.Type.Size + Alignment) & Mask;
    }

    /* defining the variables */
    for (UInt i = 0; i < VarCount; i++)
    {
        VarLocation *Location = CompilerAllocateVarLocation(Compiler);
        DefineIdentifier(Compiler, 
                CompilerGetTmp(Compiler, i),
                TypeAttr.Type,
                Location
        );

        Location->Type = TypeAttr.Type;
        Location->LocationType = VAR_MEM;
        Location->As.Memory = (VarMemory) {
            .Location = EndAddr,
            .RegPtr.ID = BaseRegister,
        };
        EndAddr += AlignedSize;
    }
    return EndAddr;
}



SubroutineParameterList CompileParameterList(PVMCompiler *Compiler, PascalVartab *Scope)
{
    SubroutineParameterList ParameterList = ParameterListInit();
    PASCAL_NONNULL(Compiler);
    PASCAL_NONNULL(Scope);

    /* '(' is consumed */
    do {
        if (ConsumeIfNextIs(Compiler, TOKEN_VAR))
        {
            PASCAL_UNREACHABLE("TODO: reference parameter.");
        }
        else if (ConsumeIfNextIs(Compiler, TOKEN_CONST))
        {
            PASCAL_UNREACHABLE("TODO: const parameter.");
        }

        /* parameter list is completely different from other variable declaration */
        /* so the code to parse it is unique from others */
        CompilerResetTmp(Compiler);
        do {
            ConsumeOrError(Compiler, TOKEN_IDENTIFIER, "Expected parameter name.");
            CompilerPushTmp(Compiler, Compiler->Curr);
        } while (ConsumeIfNextIs(Compiler, TOKEN_COMMA));
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
                PascalVar *Param = DefineAtScope(
                        Compiler, Scope, 
                        CompilerGetTmp(Compiler, i), 
                        TypeInfo->Type, 
                        ParameterLocation
                );
                ParameterListPush(&ParameterList, Compiler->GlobalAlloc, Param);
            }
        }
        else 
        {
            /* Error from above */
        }
    } while (ConsumeIfNextIs(Compiler, TOKEN_SEMICOLON));
    return ParameterList;
}




PascalVar *CompileRecordDefinition(PVMCompiler *Compiler, const Token *Name)
{
    PASCAL_NONNULL(Compiler);
    PASCAL_NONNULL(Name);

    U32 TotalSize = 0;
    /* 'record' consumed */
    PascalVartab RecordScope = VartabInit(Compiler->GlobalAlloc, 16);
    VarType Record = VarTypeRecord(Name->Lexeme, RecordScope, TotalSize);
    PascalVar *RecordType = DefineIdentifier(Compiler, Name, Record, NULL);
    PASCAL_NONNULL(RecordType);
    CompilerPushScope(Compiler, &RecordScope);

    while (!IsAtEnd(Compiler) && NextTokenIs(Compiler, TOKEN_IDENTIFIER))
    {
        /* packed alignment: 1 */
        U32 NextLine = CompileVarList(Compiler, 0, TotalSize, 1);
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
    RecordType->Type.Size = TotalSize;
    return RecordType;
}



void CompilePartialArgumentList(PVMCompiler *Compiler, 
        const Token *FunctionName, const SubroutineParameterList *Parameters,
        U32 StackArgSize, bool RecordReturnType)
{
    PASCAL_NONNULL(Compiler);
    PASCAL_NONNULL(FunctionName);
    PASCAL_NONNULL(Compiler);

    UInt ExpectedArgCount = Parameters->Count;
    UInt ArgCount = 0;
    /* no args */
    if (ConsumeIfNextIs(Compiler, TOKEN_RIGHT_PAREN))
    {
        PASCAL_ASSERT(Compiler->Flags.CallConv == CALLCONV_MSX64, 
                "TODO: other calling convention"
        );

        I32 Base = PVMStartArg(EMITTER(), StackArgSize);
        UInt RecordArg = 0 != RecordReturnType;
        do {
            if (ArgCount < ExpectedArgCount)
            {
                const VarLocation *CurrentArg = Parameters->Params[ArgCount].Location;
                PASCAL_NONNULL(CurrentArg);

                VarLocation Arg = PVMSetArg(EMITTER(), ArgCount + RecordArg, CurrentArg->Type, &Base);
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
    }
    if (ArgCount != ExpectedArgCount)
    {
        ErrorAt(Compiler, FunctionName, 
                "Expected %d arguments but %d were given.", ExpectedArgCount, ArgCount
        );
    }
}

void CompileArgumentList(PVMCompiler *Compiler, 
        const Token *FunctionName, const SubroutineData *Subroutine)
{
    PASCAL_NONNULL(Compiler);
    PASCAL_NONNULL(FunctionName);
    PASCAL_NONNULL(Subroutine);
    if (ConsumeIfNextIs(Compiler, TOKEN_LEFT_PAREN))
    {
        bool RecordReturnType = Subroutine->ReturnType 
            && TYPE_RECORD == Subroutine->ReturnType->Integral;

        CompilePartialArgumentList(Compiler, 
                FunctionName, &Subroutine->ParameterList, Subroutine->StackArgSize, 
                RecordReturnType
        );
    }
    else if (Subroutine->ParameterList.Count != 0)
    {
        ErrorAt(Compiler, FunctionName, "Expected %d arguments but none were given.", 
                Subroutine->ParameterList.Count
        );
    }
}

void CompileSubroutineCall(PVMCompiler *Compiler, 
        VarLocation *Callee, const Token *Name, VarLocation *ReturnValue)
{
    PASCAL_NONNULL(Compiler);
    PASCAL_NONNULL(Callee);
    PASCAL_NONNULL(Name);

    U32 CallSite = SUBROUTINE_INVALID_LOCATION;
    UInt ReturnReg = NO_RETURN_REG;
    SaveRegInfo SaveRegs = { 0 };

    bool CallingFunctionPointer = TYPE_POINTER == Callee->Type.Integral;
    SubroutineData *CalleeData = &Callee->Type.As.Subroutine;
    U32 SubroutineLocation = Callee->As.SubroutineLocation;

    /* return value is not discarded */
    if (NULL != ReturnValue)
    {
        ReturnReg = ReturnValue->As.Register.ID;
        /* call the function */
        SaveRegs = PVMEmitSaveCallerRegs(EMITTER(), ReturnReg);
        CompileArgumentList(Compiler, Name, CalleeData);
        if (CallingFunctionPointer)
            PVMEmitCallPtr(EMITTER(), Callee);
        else
            CallSite = PVMEmitCall(EMITTER(), SubroutineLocation);

        /* carefully move return reg into the return location */
        VarLocation Tmp = PVMSetReturnType(EMITTER(), ReturnValue->Type);
        PVMEmitMov(EMITTER(), ReturnValue, &Tmp);
    }
    else
    {
        /* calling a procedure, or function without caring about its return value */
        SaveRegs = PVMEmitSaveCallerRegs(EMITTER(), NO_RETURN_REG);
        CompileArgumentList(Compiler, Name, CalleeData);
        if (CallingFunctionPointer)
            PVMEmitCallPtr(EMITTER(), Callee);
        else
            CallSite = PVMEmitCall(EMITTER(), SubroutineLocation);
    }


    if (!CallingFunctionPointer)
    {
        PushSubroutineReference(Compiler, 
                &Callee->As.SubroutineLocation, 
                CallSite, 
                PATCHTYPE_BRANCH_UNCONDITIONAL
        );
    }

    /* deallocate stack args */
    if (CalleeData->StackArgSize) 
    {
        PVMEmitStackAllocation(EMITTER(), -CalleeData->StackArgSize);
    }
    PVMEmitUnsaveCallerRegs(EMITTER(), ReturnReg, SaveRegs);
}



