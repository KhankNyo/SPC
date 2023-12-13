
#include "Common.h"
#include "Vartab.h"
#include "Compiler/Expr.h"
#include "Compiler/Error.h"
#include "Compiler/Builtins.h"




#define DEFINE_BUILTIN_FN(pScope, LiteralName, FnLoc) \
    VartabSet(pScope, (const U8*)LiteralName, sizeof(LiteralName) - 1, 0, TYPE_FUNCTION, &(FnLoc))

#define DEFINE_BUILTIN_LIT(pScope, Name, Literal, LiteralType) \
    VartabSet(pScope, (const U8*)Name, sizeof(Name) - 1, 0, Literal, LiteralType)

#define PASCAL_BUILTIN(Name, Arg1, Arg2)\
static OptionalReturnValue Compile##Name (PVMCompiler *, const Token *);\
static VarLocation s##Name = {.Type = TYPE_FUNCTION, .LocationType = VAR_BUILTIN, \
    .As.BuiltinSubroutine = Compile##Name};\
static OptionalReturnValue Compile##Name (PVMCompiler *Arg1, const Token *Arg2)



static const U8 sNewlineConstName[] = "\n";
static U32 sNewlineConstHash = 0;



static void CompileSysWrite(PVMCompiler *Compiler, bool Newline)
{
    PVMEmitSaveCallerRegs(EMITTER(), NO_RETURN_REG);
    UInt ArgCount = 0;
    VarLocation File = VAR_LOCATION_LIT(.Ptr.As.Raw = stdout, TYPE_POINTER);
    if (ConsumeIfNextIs(Compiler, TOKEN_LEFT_PAREN))
    {
        if (!NextTokenIs(Compiler, TOKEN_RIGHT_PAREN))
        {
            /* TODO: file argument */
            do {
                VarLocation Arg =  CompileExpr(Compiler);
                PVMEmitPushMultiple(EMITTER(), 2, 
                        &VAR_LOCATION_LIT(.Int = Arg.Type, TYPE_U32), 
                        &Arg
                );
                ArgCount++;
            } while (ConsumeIfNextIs(Compiler, TOKEN_COMMA));
        }
        ConsumeOrError(Compiler, TOKEN_RIGHT_PAREN, "Expected ')' after argument list.");
    }

    /* newline arg */
    if (Newline)
    {
        PascalVar *NewlineLiteral = VartabFindWithHash(Compiler->Global, 
                sNewlineConstName, sizeof(sNewlineConstName) - 1, sNewlineConstHash
        );
        PASCAL_NONNULL(NewlineLiteral);
        PVMEmitPushMultiple(EMITTER(), 2, 
                &VAR_LOCATION_LIT(.Int = TYPE_STRING, TYPE_U32), 
                NewlineLiteral->Location
        );
        ArgCount++;
    }

    /* arg count reg */
    VarLocation ArgCountReg = PVMSetArgType(EMITTER(), 0, TYPE_U32);
    PVMEmitMov(EMITTER(), &ArgCountReg, &VAR_LOCATION_LIT(.Int = ArgCount, ArgCountReg.Type));

    /* file ptr reg */
    VarLocation FilePtrReg = PVMSetArgType(EMITTER(), 1, File.Type);
    PVMEmitMov(EMITTER(), &FilePtrReg, &File);

    /* syscall */
    PVMEmitWrite(EMITTER());
    PVMEmitUnsaveCallerRegs(EMITTER(), NO_RETURN_REG);
}

PASCAL_BUILTIN(Writeln, Compiler, FnName)
{
    OptionalReturnValue None = {.HasReturnValue = false};
    (void)FnName;
    CompileSysWrite(Compiler, true);
    return None;
}

PASCAL_BUILTIN(Write, Compiler, FnName)
{
    OptionalReturnValue None = {.HasReturnValue = false};
    (void)FnName;
    CompileSysWrite(Compiler, false);
    return None;
}

PASCAL_BUILTIN(SizeOf, Compiler, FnName)
{
    OptionalReturnValue Size = {.HasReturnValue = true};

    ConsumeOrError(Compiler, TOKEN_LEFT_PAREN, "Expected '(' after '%.*s'.", FnName->Len, FnName->Str);
    VarLocation Expr = CompileExpr(Compiler);
    ConsumeOrError(Compiler, TOKEN_RIGHT_PAREN, "Expected ')' after expression.");

    Size.ReturnValue = (VarLocation){
        .Type = TYPE_SIZE,
        .LocationType = VAR_LIT,
        .As.Literal.Int = CompilerGetSizeOfType(Compiler, Expr.Type, &Expr),
    };
    FreeExpr(Compiler, Expr);
    return Size;
}


OptionalReturnValue CompileCallToBuiltin(PVMCompiler *Compiler, VarBuiltinRoutine BuiltinCallee)
{
    PASCAL_NONNULL(Compiler);
    PASCAL_NONNULL(BuiltinCallee);

    Token FnName = Compiler->Curr;
    CompilerInitDebugInfo(Compiler, &FnName);
    OptionalReturnValue Opt = BuiltinCallee(Compiler, &FnName);
    CompilerEmitDebugInfo(Compiler, &FnName);

    return Opt;
}



void DefineCrtSubroutines(PVMCompiler *Compiler)
{
    (void)Compiler;
    PASCAL_UNREACHABLE("TODO");
}

void DefineSystemSubroutines(PVMCompiler *Compiler)
{
    PascalVartab *Scope = Compiler->Global;
    sNewlineConstHash = VartabHashStr(sNewlineConstName, sizeof(sNewlineConstName) - 1);

    /* already defined? */
    if (NULL != VartabFindWithHash(Scope, 
    sNewlineConstName, sizeof(sNewlineConstName) - 1, sNewlineConstHash))
    {
        return;
    }

    static VarLocation NewlineConstant = {
        .Type = TYPE_STRING,
        .LocationType = VAR_MEM,
        .As.Memory.Location = 10,
    };
    NewlineConstant.As.Memory = PVMEmitGlobalData(EMITTER(), "\1\n", sizeof("\n"));
    DEFINE_BUILTIN_LIT(Scope, sNewlineConstName, TYPE_STRING, &NewlineConstant);

    DEFINE_BUILTIN_FN(Scope, "WRITELN", sWriteln);
    DEFINE_BUILTIN_FN(Scope, "WRITE", sWrite);
    DEFINE_BUILTIN_FN(Scope, "SIZEOF", sSizeOf);
    //DEFINE_BUILTIN_FN(Scope, "READLN", sReadln);
    //DEFINE_BUILTIN_FN(Scope, "READ", sRead);

}





