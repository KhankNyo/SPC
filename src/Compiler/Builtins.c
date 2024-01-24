
#include "Common.h"
#include "Vartab.h"

#include "Compiler/Compiler.h"
#include "Compiler/Expr.h"
#include "Compiler/Error.h"
#include "Compiler/Builtins.h"




#define DEFINE_BUILTIN_FN(pScope, LiteralName, FnLoc) \
    VartabSet(pScope, (const U8*)LiteralName, sizeof(LiteralName) - 1, 0, \
            (VarType) {.Integral = TYPE_FUNCTION}, &(FnLoc))

#define DEFINE_BUILTIN_LIT(pScope, Name, LiteralType, Literal) \
    VartabSet(pScope, (const U8*)Name, sizeof(Name) - 1, 0, \
            VarTypeInit(LiteralType, IntegralTypeSize(LiteralType)), Literal)


#define PASCAL_BUILTIN(Name, Arg1, Arg2)\
static OptionalReturnValue Compile##Name (PascalCompiler *, const Token *);\
static VarLocation s##Name = {.Type = { .Integral = TYPE_FUNCTION }, .LocationType = VAR_BUILTIN, \
    .As.BuiltinSubroutine = Compile##Name};\
static OptionalReturnValue Compile##Name (PascalCompiler *Arg1, const Token *Arg2)



static const U8 sNewlineConstName[] = "\n";
static U32 sNewlineConstHash = 0;



static void CompileSysWrite(PascalCompiler *Compiler, bool Newline)
{
    PASCAL_NONNULL(Compiler);
    SaveRegInfo SaveRegs = PVMEmitSaveCallerRegs(EMITTER(), NO_RETURN_REG);
    UInt ArgCount = 0;

    SaveRegInfo RegList = { 0 };
    if (ConsumeIfNextTokenIs(Compiler, TOKEN_LEFT_PAREN))
    {
        if (!NextTokenIs(Compiler, TOKEN_RIGHT_PAREN))
        {
            /* TODO: file argument */
            do {
                VarLocation Arg =  CompileExpr(Compiler);
                if (TYPE_RECORD == Arg.Type.Integral || TYPE_FUNCTION == Arg.Type.Integral)
                {
                    const char *FnName = Newline? "Writeln" : "Write";
                    Error(Compiler, "%s does accept value of type %s.", FnName, VarTypeToStr(Arg.Type));
                }
                VarLocation ArgType = VAR_LOCATION_LIT(.Int = Arg.Type.Integral, TYPE_U32);
                PVMQueueAndCommitOnFull(EMITTER(), &RegList, &Arg);
                PVMQueueAndCommitOnFull(EMITTER(), &RegList, &ArgType);
                ArgCount++;
            } while (ConsumeIfNextTokenIs(Compiler, TOKEN_COMMA));
        }
        ConsumeOrError(Compiler, TOKEN_RIGHT_PAREN, "Expected ')' after argument list.");
    }

    /* newline arg */
    if (Newline)
    {
        PascalVar *NewlineLiteral = VartabFindWithHash(&Compiler->Global, 
                sNewlineConstName, sizeof(sNewlineConstName) - 1, sNewlineConstHash
        );
        PASCAL_NONNULL(NewlineLiteral);
        PVMQueueAndCommitOnFull(EMITTER(), &RegList, NewlineLiteral->Location);
        PVMQueueAndCommitOnFull(EMITTER(), &RegList, &VAR_LOCATION_LIT(.Int = TYPE_STRING, TYPE_U32));
        ArgCount++;
    }

    /* pushes the queue */
    PVMQueueCommit(EMITTER(), &RegList);
    PVMQueueRefresh(EMITTER(), &RegList);


    /* arg count reg */
    VarLocation ArgCountReg = {
        .Type = VarTypeInit(TYPE_U32, 4),
        .LocationType = VAR_REG,
        .As.Register.ID = 0,
    };
    PVMEmitMove(EMITTER(), &ArgCountReg, &VAR_LOCATION_LIT(.Int = ArgCount, ArgCountReg.Type.Integral));

    /* file ptr reg */
    VarLocation File = VAR_LOCATION_LIT(.Ptr.As.Raw = stdout, TYPE_POINTER);
    VarLocation FilePtrReg = {
        .Type = VarTypeInit(TYPE_POINTER, sizeof(void*)),
        .LocationType = VAR_REG,
        .As.Register.ID = 1,
    };
    PVMEmitMove(EMITTER(), &FilePtrReg, &File);

    /* syscall */
    PVMEmitWrite(EMITTER());
    PVMEmitUnsaveCallerRegs(EMITTER(), NO_RETURN_REG, SaveRegs);
}

PASCAL_BUILTIN(Writeln, Compiler, FnName)
{
    PASCAL_NONNULL(Compiler);
    PASCAL_NONNULL(FnName);
    OptionalReturnValue None = {.HasReturnValue = false};
    CompileSysWrite(Compiler, true);
    return None;
}

PASCAL_BUILTIN(Write, Compiler, FnName)
{
    PASCAL_NONNULL(Compiler);
    PASCAL_NONNULL(FnName);
    OptionalReturnValue None = {.HasReturnValue = false};
    CompileSysWrite(Compiler, false);
    return None;
}

PASCAL_BUILTIN(SizeOf, Compiler, FnName)
{
    PASCAL_NONNULL(Compiler);
    PASCAL_NONNULL(FnName);
    OptionalReturnValue Size = {.HasReturnValue = true};

    VarLocation Expr = CompileExpr(Compiler);
    Size.ReturnValue = VAR_LOCATION_LIT(.Int = Expr.Type.Size, TYPE_SIZE);
    FreeExpr(Compiler, Expr);
    return Size;
}

PASCAL_BUILTIN(Ord, Compiler, FnName)
{
    PASCAL_NONNULL(Compiler);
    PASCAL_NONNULL(FnName);
    OptionalReturnValue Opt = {
        .HasReturnValue = true, 
        .ReturnValue = CompileExpr(Compiler),
    };
    VarLocation *RetVal = &Opt.ReturnValue;
    if (!IntegralTypeIsOrdinal(RetVal->Type.Integral))
    {
        ErrorAt(Compiler, FnName, "Cannot convert %s to integer.", VarTypeToStr(RetVal->Type));
        return Opt;
    }

    switch (RetVal->LocationType)
    {
    case VAR_LIT:
    {
        if (IntegralTypeIsSigned(RetVal->Type.Integral))
            RetVal->Type = VarTypeInit(TYPE_I64, 8);
        else RetVal->Type = VarTypeInit(TYPE_U64, 8);
        RetVal->As.Literal.Int = OrdinalLiteralToI64(RetVal->As.Literal, RetVal->Type.Integral);
    } break;
    case VAR_MEM: 
    {
        VarLocation Tmp = *RetVal;
        (void)PVMEmitIntoRegLocation(EMITTER(), RetVal, true, &Tmp);
        FreeExpr(Compiler, Tmp);
    } FALLTHROUGH;
    case VAR_REG:
    {
        PASCAL_STATIC_ASSERT(TYPE_I32 + 1 == TYPE_I64, "");

        IntegralType Type = IntegralTypeIsSigned(RetVal->Type.Integral) 
            ? TYPE_I32 : TYPE_U32;
        U32 Size = 4;
        if (sizeof(I32) < RetVal->Type.Size)
        {
            RetVal->Type.Integral += 1;
            Size = 8;
        }
        RetVal->Type = VarTypeInit(Type, Size);
    } break;
    default:
    {
        PASCAL_UNREACHABLE("Invalid location type");
    } break;
    }
    return Opt;
}


OptionalReturnValue CompileCallToBuiltin(PascalCompiler *Compiler, VarBuiltinRoutine BuiltinCallee)
{
    PASCAL_NONNULL(Compiler);
    PASCAL_NONNULL(BuiltinCallee);

    Token FnName = Compiler->Curr;
    CompilerInitDebugInfo(Compiler, &FnName);
    OptionalReturnValue Opt = BuiltinCallee(Compiler, &FnName);
    CompilerEmitDebugInfo(Compiler, &FnName);

    return Opt;
}



void DefineCrtSubroutines(PascalCompiler *Compiler)
{
    (void)Compiler;
    PASCAL_UNREACHABLE("TODO: crt subroutines");
}

void DefineSystemSubroutines(PascalCompiler *Compiler)
{
    PascalVartab *Scope = &Compiler->Global;
    sNewlineConstHash = VartabHashStr(sNewlineConstName, sizeof(sNewlineConstName) - 1);

    /* already defined? */
    if (NULL != VartabFindWithHash(Scope, 
    sNewlineConstName, sizeof(sNewlineConstName) - 1, sNewlineConstHash))
    {
        return;
    }

    static VarLocation NewlineConstant = {
        .Type.Integral = TYPE_STRING,
        .Type.Size = sizeof(PascalStr),
        .LocationType = VAR_MEM,
    };
    static VarLocation NilConstant = {
        .As.Literal.Ptr.As.Raw = NULL,
        .LocationType = VAR_LIT,
    };
    NewlineConstant.As.Memory = PVMEmitGlobalData(EMITTER(), "\1\n", sizeof("\n"));
    NilConstant.Type = VarTypePtr(NULL);
    DEFINE_BUILTIN_LIT(Scope, sNewlineConstName, TYPE_STRING, &NewlineConstant);
    VartabSet(Scope, (const U8*)"nil", 3, 0, NilConstant.Type, &NilConstant);

    DEFINE_BUILTIN_FN(Scope, "WRITELN", sWriteln);
    DEFINE_BUILTIN_FN(Scope, "WRITE", sWrite);
    DEFINE_BUILTIN_FN(Scope, "SIZEOF", sSizeOf);
    DEFINE_BUILTIN_FN(Scope, "ORD", sOrd);
    //DEFINE_BUILTIN_FN(Scope, "READLN", sReadln);
    //DEFINE_BUILTIN_FN(Scope, "READ", sRead);

}





