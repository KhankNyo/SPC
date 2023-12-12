

#include <stdarg.h> /* duh */
#include <inttypes.h> /* PRI* */


#include "Memory.h"
#include "Tokenizer.h"
#include "Vartab.h"
#include "Variable.h"

#include "IntegralTypes.h"
#include "PVMCompiler.h"
#include "PVMEmitter.h"


#define EMITTER() (&Compiler->Emitter)



typedef struct CompilerFrame 
{
    U32 Location;
    VarSubroutine *Current; /* VarLocation never resizes, this is safe */
} CompilerFrame;


typedef struct PVMCompiler 
{
    PascalCompileFlags Flags;

    PVMEmitter Emitter;
    PascalTokenizer Lexer;
    Token Curr, Next;

    PascalGPA InternalAlloc;
    PascalGPA *GlobalAlloc;

    PascalVartab *Locals[PVM_MAX_SCOPE_COUNT];
    PascalVartab *Global;

    struct {
        /* TODO: dynamic */
        Token Array[256];
        U32 Count, Cap;
    } Iden;

    struct {
        VarLocation **Location;
        U32 Count, Cap;
    } Var;

    /* TODO: dynamic */
    CompilerFrame Subroutine[PVM_MAX_SCOPE_COUNT];


    I32 Scope;
    U32 EntryPoint;
    FILE *LogFile;
    bool Error, Panic;
} PVMCompiler;




/* and allocates a register for it, use FreeExpr to free the register */
static VarLocation CompileExpr(PVMCompiler *Compiler);
static void FreeExpr(PVMCompiler *Compiler, VarLocation Expr);
static void CompileExprInto(PVMCompiler *Compiler, const Token *OpToken, VarLocation *Location);


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
 *                                   ERROR
 */
/*===============================================================================*/


static void ConsumeToken(PVMCompiler *Compiler);

static bool NextTokenIs(const PVMCompiler *Compiler, TokenType Type)
{
    return Type == Compiler->Next.Type;
}

static bool ConsumeIfNextIs(PVMCompiler *Compiler, TokenType Type)
{
    if (NextTokenIs(Compiler, Type))
    {
        ConsumeToken(Compiler);
        return true;
    }
    return false;
}



static U32 LineLen(const U8 *s)
{
    U32 i = 0;
    for (; s[i] && s[i] != '\n' && s[i] != '\r'; i++)
    {}
    return i;
}



static void PrintAndHighlightSource(FILE *LogFile, const Token *Tok)
{
    const U8 *LineStart = Tok->Str - Tok->LineOffset + 1;
    char Highlighter = '^';
    U32 Len = LineLen(LineStart);

    fprintf(LogFile, "\n    \"%.*s\"", Len, LineStart);
    fprintf(LogFile, "\n    %*s", Tok->LineOffset, "");
    for (U32 i = 0; i < Tok->Len; i++)
    {
        fputc(Highlighter, LogFile);
    }
}


static void VaListError(PVMCompiler *Compiler, const Token *Tok, const char *Fmt, va_list Args)
{
    Compiler->Error = true;
    if (!Compiler->Panic)
    {
        /* TODO: file name */
        /* sample:
         * [line 5, offset 10]
         *     "function bad(): integer;"
         *               ^^^
         * Error: ...
         */

        Compiler->Panic = true;
        fprintf(Compiler->LogFile, "\n[line %d, offset %d]", 
                Tok->Line, Tok->LineOffset
        );
        PrintAndHighlightSource(Compiler->LogFile, Tok);

        fprintf(Compiler->LogFile, "\n    Error: ");
        vfprintf(Compiler->LogFile, Fmt, Args);
        fputc('\n', Compiler->LogFile);
    }
}

static void ErrorAt(PVMCompiler *Compiler, const Token *Tok, const char *Fmt, ...)
{
    va_list Args;
    va_start(Args, Fmt);
    VaListError(Compiler, Tok, Fmt, Args);
    va_end(Args);
}


#define Error(pCompiler, ...) ErrorAt(pCompiler, &(pCompiler)->Next, __VA_ARGS__)


static bool ConsumeOrError(PVMCompiler *Compiler, TokenType Expected, const char *ErrFmt, ...)
{
    if (!ConsumeIfNextIs(Compiler, Expected))
    {
        va_list Args;
        va_start(Args, ErrFmt);
        VaListError(Compiler, &Compiler->Next, ErrFmt, Args);
        va_end(Args);
        return false;
    }
    return true;
}






/*===============================================================================*/
/*
 *                                   UTILS
 */
/*===============================================================================*/



static U32 CompilerGetSizeOfType(PVMCompiler *Compiler, IntegralType Type, const VarLocation *TypeInfo)
{
    switch (Type)
    {
    case TYPE_INVALID:
    case TYPE_COUNT:
    case TYPE_FUNCTION:
        return 0;

    case TYPE_I8:
    case TYPE_U8:
    case TYPE_BOOLEAN:
        return 1;

    case TYPE_I16:
    case TYPE_U16:
        return 2;

    case TYPE_I32:
    case TYPE_U32:
    case TYPE_F32:
        return 4;

    case TYPE_I64:
    case TYPE_U64:
    case TYPE_F64:
        return 8;

    case TYPE_STRING:
        return sizeof(PascalStr);

    case TYPE_POINTER:
        return sizeof(void*);
    case TYPE_RECORD:
    {
        PASCAL_NONNULL(TypeInfo);
        return TypeInfo->Size;
    } break;
    }

    PASCAL_UNREACHABLE("Handle case %s", IntegralTypeToStr(Type));
    (void)Compiler;
    /* TODO: record and array types */
    return 0;
}





static PVMCompiler CompilerInit(const U8 *Source, 
        PascalCompileFlags Flags,
        PascalVartab *PredefinedIdentifiers, PVMChunk *Chunk, PascalGPA *GlobalAlloc, FILE *LogFile)
{
    PVMCompiler Compiler = {
        .Lexer = TokenizerInit(Source),
        .LogFile = LogFile,
        .Error = false,
        .Panic = false,
        .Scope = 0,
        .Flags = Flags,

        .GlobalAlloc = GlobalAlloc,
        .InternalAlloc = GPAInit(4 * 1024 * 1024),
        .Var = {
            .Cap = 256,
            .Count = 0,
        },
        .Global = PredefinedIdentifiers,
        .Iden = {
            .Cap = STATIC_ARRAY_SIZE(Compiler.Iden.Array),
            .Count = 0,
        },
        .Subroutine = { {0} },
        .Emitter = PVMEmitterInit(Chunk),
    };

    Compiler.Var.Location = GPAAllocate(&Compiler.InternalAlloc, sizeof(VarLocation *) * Compiler.Var.Cap);
    for (U32 i = 0; i < Compiler.Var.Cap; i++)
    {
        Compiler.Var.Location[i] = 
            GPAAllocateZero(Compiler.GlobalAlloc, sizeof Compiler.Var.Location[0][0]);
    }
    return Compiler;
}

static void CompilerDeinit(PVMCompiler *Compiler)
{
    PVMSetEntryPoint(EMITTER(), Compiler->EntryPoint);
    PVMEmitterDeinit(EMITTER());
    GPADeinit(&Compiler->InternalAlloc);
}



static bool IsAtGlobalScope(const PVMCompiler *Compiler)
{
    return 0 == Compiler->Scope;
}


static PascalVartab *CurrentScope(PVMCompiler *Compiler)
{
    if (IsAtGlobalScope(Compiler))
    {
        return Compiler->Global;
    }
    else 
    {
        return Compiler->Locals[Compiler->Scope - 1];
    }
}


static void CompilerPushScope(PVMCompiler *Compiler, PascalVartab *Scope)
{
    Compiler->Locals[Compiler->Scope] = Scope;
    Compiler->Scope++;
}

static PascalVartab *CompilerPopScope(PVMCompiler *Compiler)
{
    /* when popping a scope, we revert Var.Count back to Var.Count of the previous scope,
     * and in the mean time, we'd also need to refill 
     * the vars that were taken by the scope we're about to pop */

    Compiler->Scope--;
    U32 Last = Compiler->Var.Count;
    Compiler->Var.Count = CurrentScope(Compiler)->Count;

    /* refill the variables that have been taken */
    for (U32 i = Compiler->Var.Count; i < Last; i++)
    {
        Compiler->Var.Location[i] = 
            GPAAllocateZero(Compiler->GlobalAlloc, sizeof(Compiler->Var.Location[0][0]));
    }
    return Compiler->Locals[Compiler->Scope];
}


static void CompilerPushSubroutine(PVMCompiler *Compiler, VarSubroutine *Subroutine)
{
    Compiler->Subroutine[Compiler->Scope].Current = Subroutine;
    Compiler->Subroutine[Compiler->Scope].Location = Compiler->Var.Count;
    CompilerPushScope(Compiler, &Subroutine->Scope);
    PASCAL_ASSERT(Compiler->Scope < (I32)STATIC_ARRAY_SIZE(Compiler->Subroutine), "TODO: dynamic nested scope");
}

static void CompilerPopSubroutine(PVMCompiler *Compiler)
{
    CompilerPopScope(Compiler);
}


static VarLocation *CompilerAllocateVarLocation(PVMCompiler *Compiler)
{
    if (Compiler->Var.Count >= Compiler->Var.Cap)
    {
        U32 OldCap = Compiler->Var.Cap;
        Compiler->Var.Cap *= 2;
        Compiler->Var.Location = GPAReallocateArray(&Compiler->InternalAlloc, 
                Compiler->Var.Location, *Compiler->Var.Location, 
                Compiler->Var.Cap
        );
        for (U32 i = OldCap; i < Compiler->Var.Cap; i++)
        {
            Compiler->Var.Location[i] = 
                GPAAllocateZero(Compiler->GlobalAlloc, sizeof Compiler->Var.Location[0][0]);
        }
    }
    return Compiler->Var.Location[Compiler->Var.Count++];
}








static void ConsumeToken(PVMCompiler *Compiler)
{
    Compiler->Curr = Compiler->Next;
    Compiler->Next = TokenizerGetToken(&Compiler->Lexer);
    if (NextTokenIs(Compiler, TOKEN_ERROR))
    {
        Error(Compiler, "%s", Compiler->Next.Literal.Err);
    }
}

static bool IsAtEnd(const PVMCompiler *Compiler)
{
    return NextTokenIs(Compiler, TOKEN_EOF);
}

static bool IsAtStmtEnd(const PVMCompiler *Compiler)
{
    return IsAtEnd(Compiler) || NextTokenIs(Compiler, TOKEN_SEMICOLON);
}







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
    /* TODO: this skips too much */
    Compiler->Panic = false;
    ConsumeToken(Compiler);
    while (!IsAtEnd(Compiler))
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
            return;

        default: break;
        }
        ConsumeToken(Compiler);
    }
}
















/*===============================================================================*/
/*
 *                          VARIABLES AND IDENTIFIERS
 */
/*===============================================================================*/




static PascalVar *FindIdentifier(PVMCompiler *Compiler, const Token *Identifier)
{
    U32 Hash = VartabHashStr(Identifier->Str, Identifier->Len);
    PascalVar *Info = NULL;

    /* iterate from innermost scope to global - 1 */
    for (int i = Compiler->Scope - 1; i >= 0; i--)
    {
        Info = VartabFindWithHash(Compiler->Locals[i], 
                Identifier->Str, Identifier->Len, Hash
        );
        if (NULL != Info)
            return Info;
    }
    Info = VartabFindWithHash(Compiler->Global,
            Identifier->Str, Identifier->Len, Hash
    );
    return Info;
}


static PascalVar *DefineAtScope(PVMCompiler *Compiler, PascalVartab *Scope,
        const Token *Identifier, IntegralType Type, VarLocation *Location)
{
    PascalVar *AlreadyDefined = VartabFindWithHash(Scope, 
            Identifier->Str, Identifier->Len, 
            VartabHashStr(Identifier->Str, Identifier->Len)
    );
    if (AlreadyDefined)
    {
        if (0 == AlreadyDefined->Line)
        {
            ErrorAt(Compiler, Identifier, "'%.*s' is a predefined identifier in this scope.",
                    Identifier->Len, Identifier->Str
            );
        }
        else
        {
            ErrorAt(Compiler, Identifier, "'%.*s' is already defined on line %d in this scope.", 
                    Identifier->Len, Identifier->Str,
                    AlreadyDefined->Line
            );
        }

        return AlreadyDefined;
    }
    return VartabSet(Scope, 
            Identifier->Str, Identifier->Len, Identifier->Line, 
            Type, Location
    );
}

static PascalVar *DefineIdentifier(PVMCompiler *Compiler, 
        const Token *Identifier, IntegralType Type, VarLocation *Location)
{
    return DefineAtScope(Compiler, CurrentScope(Compiler), Identifier, Type, Location);
}

static PascalVar *DefineGlobal(PVMCompiler *Compiler,
        const Token *Identifier, IntegralType Type, VarLocation *Location)
{
    return DefineAtScope(Compiler, Compiler->Global, Identifier, Type, Location);
}



/* reports error if identifier is not found */
static PascalVar *GetIdenInfo(PVMCompiler *Compiler, const Token *Identifier, const char *ErrFmt, ...)
{
    PascalVar *Info = FindIdentifier(Compiler, Identifier);
    if (NULL == Info)
    {
        va_list ArgList;
        va_start(ArgList, ErrFmt);
        VaListError(Compiler, Identifier, ErrFmt, ArgList);
        va_end(ArgList);
    }
    return Info;
}

/* returns the type that both sides should be */
static IntegralType CoerceTypes(PVMCompiler *Compiler, const Token *Op, IntegralType Left, IntegralType Right)
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
        ErrorAt(Compiler, Op, "Invalid combination of %s and %s",
                IntegralTypeToStr(Left), IntegralTypeToStr(Right)
        );
    }
    return TYPE_INVALID;
}


static bool ConvertTypeImplicitly(PVMCompiler *Compiler, IntegralType To, VarLocation *From)
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
                PVMEmitIntToFltTypeConversion(EMITTER(), FloatReg.As.Register, To, From->As.Register, From->Type);
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
    {
        PASCAL_UNREACHABLE("%s", __func__);
    } break;
    }

    From->Type = To;
    return true;
InvalidTypeConversion:
    Error(Compiler, "Cannot convert from %s to %s.", IntegralTypeToStr(From->Type), IntegralTypeToStr(To));
    return false;
}







/*===============================================================================*/
/*
 *                                  HELPERS
 */
/*===============================================================================*/

static void CompilerInitDebugInfo(PVMCompiler *Compiler, const Token *From)
{
    PVMEmitDebugInfo(EMITTER(), From->Str, From->Len, From->Line);
}

static void CompilerEmitDebugInfo(PVMCompiler *Compiler, const Token *From)
{
    U32 LineLen = Compiler->Curr.Str - From->Str + Compiler->Curr.Len;
    if (From->Type == TOKEN_FUNCTION || From->Type == TOKEN_PROCEDURE)
    {
        PVMUpdateDebugInfo(EMITTER(), LineLen, true);
    }
    else
    {
        PVMUpdateDebugInfo(EMITTER(), LineLen, false);
    }
}



static void CompilerResetTmpIdentifier(PVMCompiler *Compiler)
{
    Compiler->Iden.Count = 0;
}

static void CompilerPushTmpIdentifier(PVMCompiler *Compiler, Token Identifier)
{
    if (Compiler->Iden.Count > Compiler->Iden.Cap)
    {
        PASCAL_UNREACHABLE("TODO: allocator");
    }
    Compiler->Iden.Array[Compiler->Iden.Count] = Identifier;
    Compiler->Iden.Count++;
}

static UInt CompilerGetTmpIdentifierCount(const PVMCompiler *Compiler)
{
    return Compiler->Iden.Count;
}

static Token *CompilerGetTmpIdentifier(PVMCompiler *Compiler, UInt Idx)
{
    return &Compiler->Iden.Array[Idx];
}





static void SubroutineDataPushParameter(PascalGPA *Allocator, VarSubroutine *Subroutine, PascalVar *Param)
{
    if (Subroutine->ArgCount == Subroutine->Cap)
    {
        Subroutine->Cap = Subroutine->Cap * 2 + 8;
        Subroutine->Args = GPAReallocateArray(Allocator, 
                Subroutine->Args, *Subroutine->Args, 
                Subroutine->Cap
        );
    }
    Subroutine->Args[Subroutine->ArgCount++] = *Param;
}

static void SubroutineDataPushRef(PascalGPA *Allocator, VarSubroutine *Subroutine, U32 CallSite)
{
    if (Subroutine->RefCount == Subroutine->RefCap)
    {
        Subroutine->Cap = Subroutine->Cap*2 + 8;
        Subroutine->References = GPAReallocateArray(Allocator,
                Subroutine->References, *Subroutine->References,
                Subroutine->Cap
        );
    }
    Subroutine->References[Subroutine->RefCount++] = CallSite;
}

static void CompilerResolveSubroutineCalls(PVMCompiler *Compiler, VarSubroutine *Subroutine, U32 SubroutineLocation)
{
    for (U32 i = 0; i < Subroutine->RefCount; i++)
    {
        PVMPatchBranch(EMITTER(), 
                Subroutine->References[i], 
                SubroutineLocation, 
                BRANCHTYPE_UNCONDITIONAL
        );
    }
    GPADeallocate(Compiler->GlobalAlloc, Subroutine->References);
    Subroutine->References = NULL;
    Subroutine->RefCount = 0;
    Subroutine->RefCap = 0;
}





static U32 CompileAndDeclareParameter(PVMCompiler *Compiler, 
        U32 VarCount, U32 VarSize, IntegralType VarType, PascalVar TypeInfo, VarSubroutine *Subroutine)
{
    PASCAL_ASSERT(CompilerGetTmpIdentifierCount(Compiler) != 0, 
            "cannot be called outside of %s", __func__
    );

    /* TODO: let the emitter determine rounded size */
    VarSize += sizeof(PVMGPR);
    if (VarSize % sizeof(PVMGPR))
        VarSize &= ~(sizeof(PVMGPR) - 1);

    Subroutine->StackArgSize = 0;
    for (U32 i = 0; i < VarCount; i++)
    {
        VarLocation *Location = CompilerAllocateVarLocation(Compiler);
        PascalVar *Var = DefineIdentifier(Compiler, 
                CompilerGetTmpIdentifier(Compiler, i),
                VarType,
                Location
        );

        *Location = PVMSetParamType(EMITTER(), Subroutine->ArgCount, VarType);
        PVMMarkArgAsOccupied(EMITTER(), Location);
        if (Subroutine->ArgCount > PVM_ARGREG_COUNT)
        {
            Subroutine->StackArgSize += VarSize;
        }
        if (TYPE_RECORD == VarType && NULL != TypeInfo.Location)
            Location->Record = TypeInfo.Location->Record;
        else
            Location->PointsAt = TypeInfo;
        Location->Type = VarType;
        Location->Size = VarSize;
        SubroutineDataPushParameter(Compiler->GlobalAlloc, Subroutine, Var);
    }
    return Subroutine->StackArgSize;
}

static U32 CompileAndDeclareLocal(PVMCompiler *Compiler, 
        U32 VarCount, U32 VarSize, IntegralType VarType, PascalVar TypeInfo)
{
    PASCAL_ASSERT(VarCount > 0, "");

    for (U32 i = 0; i < VarCount; i++)
    {
        VarLocation *Location = CompilerAllocateVarLocation(Compiler);
        DefineIdentifier(Compiler, 
                CompilerGetTmpIdentifier(Compiler, i),
                VarType,
                Location
        );

        Location->LocationType = VAR_MEM;
        if (TYPE_RECORD == VarType && NULL != TypeInfo.Location)
        {
            Location->Record = TypeInfo.Location->Record;
        }
        else
        {
            Location->PointsAt = TypeInfo;
        }
        Location->Type = VarType;
        Location->Size = VarSize;
        Location->As.Memory = PVMQueueStackAllocation(EMITTER(), VarSize);
    }
    return VarCount * VarSize;
}


static bool CompileAndDeclareGlobal(PVMCompiler *Compiler, 
        U32 VarCount, U32 VarSize, IntegralType VarType, PascalVar TypeInfo)
{
    PASCAL_ASSERT(VarCount > 0, "");

    for (U32 i = 0; i < VarCount; i++)
    {
        VarLocation *Location = CompilerAllocateVarLocation(Compiler);
        DefineGlobal(Compiler,
                CompilerGetTmpIdentifier(Compiler, i),
                VarType,
                Location
        );

        Location->LocationType = VAR_MEM;
        Location->Type = VarType;
        if (TYPE_RECORD == VarType && NULL != TypeInfo.Location)
            Location->Record = TypeInfo.Location->Record;
        else
            Location->PointsAt = TypeInfo;
        Location->Size = VarSize;
        Location->As.Memory = PVMEmitGlobalSpace(EMITTER(), VarSize);
    }
    return 0;
}


static U32 CompileAndDeclareVarList(PVMCompiler *Compiler, VarSubroutine *Function);


static PascalVar *CompileRecordDefinition(PVMCompiler *Compiler, const Token *Name)
{
    /* 'record' consumed */
    PascalVartab RecordScope = VartabInit(Compiler->GlobalAlloc, 16);
    CompilerPushScope(Compiler, &RecordScope);

    U32 TotalSize = 0;
    while (!IsAtEnd(Compiler) && NextTokenIs(Compiler, TOKEN_IDENTIFIER))
    {
        CompilerResetTmpIdentifier(Compiler);
        /* get field names */
        do {
            ConsumeOrError(Compiler, TOKEN_IDENTIFIER, "Expected field name.");
            CompilerPushTmpIdentifier(Compiler, Compiler->Curr);
        } while (ConsumeIfNextIs(Compiler, TOKEN_COMMA));

        if (!ConsumeOrError(Compiler, TOKEN_COLON, "Expected ':' or ',' after field name."))
            break;
        
        IntegralType VarType = TYPE_INVALID;
        PascalVar TypeInfo = { 0 };
        /* pointer */
        bool IsPointer = ConsumeIfNextIs(Compiler, TOKEN_CARET);
        /* record */
        if (ConsumeIfNextIs(Compiler, TOKEN_RECORD))
        {
            PASCAL_UNREACHABLE("TODO: annonymous records inside records");
        }
        /* typename */
        else if (ConsumeOrError(Compiler, TOKEN_IDENTIFIER, 
                "Expected type name after '%.*s'", Compiler->Curr.Len, Compiler->Curr.Str))
        {
            /* typename */
            PascalVar *Type = GetIdenInfo(Compiler, &Compiler->Curr, "Undefined type name.");
            if (NULL != Type)
            {
                TypeInfo = *Type;
                VarType = TypeInfo.Type;
            }
        }
        /* Not a type name, error reported */
        else break;

        U32 VarSize = CompilerGetSizeOfType(Compiler, VarType, TypeInfo.Location);
        if (IsPointer)
        {
            VarSize = sizeof(void*);
            VarType = TYPE_POINTER;
        }

        /* define each identifier inside record */
        /* TODO: case of statement in here */
        U32 Count = CompilerGetTmpIdentifierCount(Compiler);
        for (U32 i = 0; i < Count; i++)
        {
            Token *Name = CompilerGetTmpIdentifier(Compiler, i);
            VarLocation *Location = CompilerAllocateVarLocation(Compiler);
            DefineIdentifier(Compiler, Name, VarType, Location);


            if (TYPE_RECORD == VarType && NULL != TypeInfo.Location)
                Location->Record = TypeInfo.Location->Record;
            else
                Location->PointsAt = TypeInfo;
            Location->Type = VarType;
            Location->LocationType = VAR_MEM;
            Location->Size = VarSize;

            /* TODO: currently the record is packed */
            Location->As.Memory.Location = TotalSize;
            TotalSize += VarSize;
        }



        if (NextTokenIs(Compiler, TOKEN_END)
        || !ConsumeOrError(Compiler, TOKEN_SEMICOLON, "Expect ';' or 'end' after member declaration."))
            break;
    }
    ConsumeOrError(Compiler, TOKEN_END, "Expected 'end' after record definition.");

    CompilerPopScope(Compiler);
    VarLocation *Type = CompilerAllocateVarLocation(Compiler);
    Type->Type = TYPE_RECORD;
    Type->LocationType = VAR_INVALID;
    Type->Record = RecordScope;
    Type->Size = TotalSize;

    return DefineIdentifier(Compiler, Name, TYPE_RECORD, Type);
}

/* returns whether or not the var list needs to be stack allocated */
static U32 CompileAndDeclareVarList(PVMCompiler *Compiler, VarSubroutine *Function)
{
    /* assumes next token is id1 */
    /* 
     *  id1, id2: typename
     */

    CompilerResetTmpIdentifier(Compiler);
    do {
        ConsumeOrError(Compiler, TOKEN_IDENTIFIER, "Expected variable name.");
        CompilerPushTmpIdentifier(Compiler, Compiler->Curr);
    } while (ConsumeIfNextIs(Compiler, TOKEN_COMMA));
    if (!ConsumeOrError(Compiler, TOKEN_COLON, "Expected ':' or ',' after variable name."))
        return false;


    IntegralType VarType = TYPE_INVALID;
    PascalVar ValidType = { 0 };
    /* pointer */
    bool IsPointer = ConsumeIfNextIs(Compiler, TOKEN_CARET);
    /* record */
    if (ConsumeIfNextIs(Compiler, TOKEN_RECORD))
    {
        PASCAL_UNREACHABLE("TODO: annonymous records");
    }
    /* typename */
    else if (ConsumeOrError(Compiler, TOKEN_IDENTIFIER, 
            "Expected type name after '%.*s'", Compiler->Curr.Len, Compiler->Curr.Str))
    {
        /* typename */
        PascalVar *TypeInfo = GetIdenInfo(Compiler, &Compiler->Curr, "Undefined type name.");
        if (NULL != TypeInfo)
        {
            ValidType = *TypeInfo;
            VarType = TypeInfo->Type;
        }
    }
    else return false;


    U32 VarSize = CompilerGetSizeOfType(Compiler, VarType, ValidType.Location);
    if (IsPointer)
    {
        VarSize = sizeof(void*);
        VarType = TYPE_POINTER;
    }
    UInt VarCount = CompilerGetTmpIdentifierCount(Compiler);


    /* dispatch to the appropriate routine to handle space allocation */
    if (NULL != Function)
    {
        return CompileAndDeclareParameter(Compiler, VarCount, VarSize, VarType, ValidType, Function);
    }
    else if (!IsAtGlobalScope(Compiler))
    {
        return CompileAndDeclareLocal(Compiler, VarCount, VarSize, VarType, ValidType);
    }

    return CompileAndDeclareGlobal(Compiler, VarCount, VarSize, VarType, ValidType);
}



static void CompileParameterList(PVMCompiler *Compiler, VarSubroutine *CurrentFunction)
{
    PASCAL_NONNULL(Compiler);
    PASCAL_NONNULL(CurrentFunction);
    PASCAL_ASSERT(!IsAtGlobalScope(Compiler), "Cannot compile param list at global scope");
    /* assumes '(' or some kind of terminator is the next token */

    CurrentFunction->ArgCount = 0;

    if (!ConsumeIfNextIs(Compiler, TOKEN_LEFT_PAREN))
        return;

    if (ConsumeIfNextIs(Compiler, TOKEN_RIGHT_PAREN))
        return;

    bool StackArgs = false;
    do {
        StackArgs = CompileAndDeclareVarList(Compiler, CurrentFunction);
    } while (ConsumeIfNextIs(Compiler, TOKEN_SEMICOLON));

    ConsumeOrError(Compiler, TOKEN_RIGHT_PAREN, "Expected ')' after parameter list.");
    if (StackArgs)
    {
        PVMCommitStackAllocation(EMITTER());
    }
}


static void CompileArgumentList(PVMCompiler *Compiler, const Token *FunctionName, const VarSubroutine *Subroutine)
{
    UInt ExpectedArgCount = Subroutine->ArgCount;
    UInt ArgCount = 0;
    if (!ConsumeIfNextIs(Compiler, TOKEN_LEFT_PAREN))
        goto CheckArgs;
    /* no args */
    if (ConsumeIfNextIs(Compiler, TOKEN_RIGHT_PAREN))
        goto CheckArgs;



    PASCAL_ASSERT(Compiler->Flags.CallConv == CALLCONV_MSX64, 
            "TODO: other calling convention"
    );


    /* TODO: compiler should not be the one doing this */
    EMITTER()->StackSpace += Subroutine->StackArgSize;
    PVMAllocateStack(EMITTER(), Subroutine->StackArgSize);
    do {
        if (ArgCount < ExpectedArgCount)
        {
            const VarLocation *CurrentArg = Subroutine->Args[ArgCount].Location;
            PASCAL_NONNULL(CurrentArg);

            VarLocation Arg = PVMSetArgType(EMITTER(), ArgCount, CurrentArg->Type);
            Arg.PointsAt = CurrentArg->PointsAt;

            CompileExprInto(Compiler, NULL, &Arg);
            PVMMarkArgAsOccupied(EMITTER(), &Arg);
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

static void CompilerCallSubroutine(PVMCompiler *Compiler, VarSubroutine *Callee, const Token *Name, VarLocation *ReturnValue)
{
    U32 CallSite = 0;
    /* calling a function */
    if (NULL != ReturnValue)
    {
        UInt ReturnReg = ReturnValue->As.Register.ID;

        /* call the function */
        PVMEmitSaveCallerRegs(EMITTER(), ReturnReg);
        CompileArgumentList(Compiler, Name, Callee);
        CallSite = PVMEmitCall(EMITTER(), Callee);

        /* carefully move return reg into the return location */
        VarLocation Tmp = PVMSetReturnType(EMITTER(), ReturnValue->Type);
        PVMEmitMov(EMITTER(), ReturnValue, &Tmp);
        PVMEmitUnsaveCallerRegs(EMITTER(), ReturnReg);
    }
    /* calling a procedure, or function without caring about its return value */
    else
    {
        PVMEmitSaveCallerRegs(EMITTER(), NO_RETURN_REG);
        CompileArgumentList(Compiler, Name, Callee);
        CallSite = PVMEmitCall(EMITTER(), Callee);
        PVMEmitUnsaveCallerRegs(EMITTER(), NO_RETURN_REG);
    }

    /* deallocate stack args */
    if (Callee->StackArgSize) 
    {
        PVMAllocateStack(EMITTER(), -Callee->StackArgSize);
    }
    if (!Callee->Defined)
    {
        SubroutineDataPushRef(Compiler->GlobalAlloc, Callee, CallSite);
    }
}












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

typedef struct PrecedenceRule
{
    PrefixParseRoutine PrefixRoutine;
    InfixParseRoutine InfixRoutine;
    Precedence Prec;
} PrecedenceRule;

static const PrecedenceRule *GetPrecedenceRule(TokenType CurrentTokenType);
static VarLocation ParsePrecedence(PVMCompiler *Compiler, Precedence Prec);


static void FreeExpr(PVMCompiler *Compiler, VarLocation Expr)
{
    PASCAL_NONNULL(Compiler);

    if (VAR_REG == Expr.LocationType && !Expr.As.Register.Persistent)
    {
        PVMFreeRegister(EMITTER(), Expr.As.Register);
    }
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
        .Type = Variable->PointsAt.Type,
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
    VarLocation *Member = FindRecordMember(&Left->Record, &Name);
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


static VarLiteral ConvertLiteralTypeExplicitly(PVMCompiler *Compiler, const Token *Converter, IntegralType To, const VarLiteral *Lit, IntegralType LitType)
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

static VarLocation ConvertTypeExplicitly(PVMCompiler *Compiler, const Token *Converter, IntegralType To, const VarLocation *Expr)
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
    case VAR_FLAG:
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
    case VAR_SUBROUTINE:
    {
    } break;
    }

    return Converted;
}


static VarLocation FactorVariable(PVMCompiler *Compiler)
{
    PASCAL_NONNULL(Compiler);

    /* consumed iden */
    Token Identifier = Compiler->Curr;
    PascalVar *Variable = GetIdenInfo(Compiler, &Identifier, "Undefined identifier.");
    if (NULL == Variable)
        goto Error;

    VarLocation *Location = Variable->Location;
    /* type casting */
    if (NULL == Location)
    {
        /* typename(expr) */
        ConsumeOrError(Compiler, TOKEN_LEFT_PAREN, "Expected '(' after type name.");
        VarLocation Expr = FactorGrouping(Compiler);
        return ConvertTypeExplicitly(Compiler, &Identifier, Variable->Type, &Expr);
    }
    /* function call */
    if (TYPE_FUNCTION == Location->Type)
    {
        VarSubroutine *Callee = &Location->As.Subroutine;
        PASCAL_NONNULL(Callee);
        /* TODO: if the callee is NULL, then maybe treat it as a built-in function */
        if (!Callee->HasReturnType)
        {
            ErrorAt(Compiler, &Identifier, "Procedure does not have a return value.");
            goto Error;
        }

        /* setup return reg */
        VarLocation ReturnValue = PVMAllocateRegister(EMITTER(), Callee->ReturnType);
        CompilerCallSubroutine(Compiler, Callee, &Identifier, &ReturnValue);
        return ReturnValue;
    }
    return *Location;
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
    Ptr.PointsAt = *Variable;
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
        ErrorAt(Compiler, OpToken, "Cannot be applied to value of type %s", 
                IntegralTypeToStr(LiteralType)
        );
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
        ErrorAt(Compiler, OpToken, "Cannot be applied to value of type %s",
                IntegralTypeToStr(LiteralType)
        );
    }
    Location.As.Literal = Literal;
    return Location;
}


static VarLocation ExprUnary(PVMCompiler *Compiler)
{
    TokenType Operator = Compiler->Curr.Type;
    Token OpToken = Compiler->Curr;
    VarLocation Value = ParsePrecedence(Compiler, PREC_FACTOR);

    if (VAR_LIT == Value.LocationType)
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
    ErrorAt(Compiler, &OpToken, "Operator is not valid for %s", IntegralTypeToStr(Value.Type));
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


static VarLocation LiteralExprBinary(PVMCompiler *Compiler, const Token *OpToken, 
        VarLocation *Left, VarLocation *Right
        )
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

    case TOKEN_EQUAL:           COMPARE_OP(==); break;
    case TOKEN_LESS:            COMPARE_OP(<); break;
    case TOKEN_GREATER:         COMPARE_OP(>); break;
    case TOKEN_LESS_GREATER:    COMPARE_OP(!=); break;
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
        if (IntegralTypeIsInteger(Left->Type) && IntegralTypeIsInteger(Right->Type))
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

    case TOKEN_AND: INT_AND_BOOL_OP(&); break;
    case TOKEN_OR:  INT_AND_BOOL_OP(|); break;
    case TOKEN_XOR:
    {
        if (IntegralTypeIsInteger(Left->Type) && IntegralTypeIsInteger(Right->Type))
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
    ErrorAt(Compiler, OpToken, "Invalid combination of type %s and %s", 
            IntegralTypeToStr(Left->Type), IntegralTypeToStr(Right->Type)
    );
    return *Left;
#undef COMMON_BIN_OP
#undef COMPARE_OP
#undef INT_AND_BOOL_OP
}


static VarLocation RuntimeExprBinary(PVMCompiler *Compiler, const Token *OpToken, VarLocation *Left, VarLocation *Right)
{
    PASCAL_NONNULL(Compiler);
    PASCAL_NONNULL(OpToken);
    PASCAL_NONNULL(Left);
    PASCAL_NONNULL(Right);

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
        ErrorAt(Compiler, OpToken, "Invalid operands: %s and %s", 
                IntegralTypeToStr(Tmp.Type), IntegralTypeToStr(Src.Type)
        );
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
            ErrorAt(Compiler, OpToken, "Comparison between integers of different sign (%s and %s) is not allowed.",
                    IntegralTypeToStr(Left->Type), IntegralTypeToStr(Right->Type)
            );
            return *Left;
        }
        VarLocation Cond = PVMEmitSetFlag(EMITTER(), OpToken->Type, &Dst, &Src);
        FreeExpr(Compiler, Dst);
        FreeExpr(Compiler, Src);
        return Cond;
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

    if (VAR_LIT != Left->LocationType || VAR_LIT != Right.LocationType)
    {
        return RuntimeExprBinary(Compiler, &OpToken, Left, &Right);
    }
    else 
    {
        return LiteralExprBinary(Compiler, &OpToken, Left, &Right);
    }
}


static VarLocation ExprAnd(PVMCompiler *Compiler, VarLocation *Left)
{
    VarLocation Right;
    Token OpToken = Compiler->Curr;
    if (TYPE_BOOLEAN == Left->Type)
    {
        U32 FromFalse = PVMEmitBranchIfFalse(EMITTER(), Left);
        Right = ParsePrecedence(Compiler, GetPrecedenceRule(OpToken.Type)->Prec + 1);

        IntegralType ResultType = CoerceTypes(Compiler, &OpToken, Left->Type, Right.Type);
        if (TYPE_INVALID == ResultType)
            goto InvalidOperands;
        PASCAL_ASSERT(TYPE_BOOLEAN == ResultType, "");
        PASCAL_ASSERT(TYPE_BOOLEAN == Right.Type, "");

        PVMEmitAnd(EMITTER(), Left, &Right);
        PVMPatchBranchToCurrent(EMITTER(), FromFalse, BRANCHTYPE_CONDITIONAL);
    }
    else if (IntegralTypeIsInteger(Left->Type))
    {
        Right = ParsePrecedence(Compiler, GetPrecedenceRule(OpToken.Type)->Prec + 1);
        if (!IntegralTypeIsInteger(Right.Type))
        {
            goto InvalidOperands;
        }
        IntegralType ResultType = CoerceTypes(Compiler, &OpToken, Left->Type, Right.Type);
        PASCAL_ASSERT(TYPE_INVALID != ResultType, "");
        PASCAL_ASSERT(ConvertTypeImplicitly(Compiler, ResultType, Left), "");
        PASCAL_ASSERT(ConvertTypeImplicitly(Compiler, ResultType, &Right), "");

        PVMEmitAnd(EMITTER(), Left, &Right);
        FreeExpr(Compiler, Right);
    }
    else 
    {
        ErrorAt(Compiler, &OpToken, "Invalid left operand: %s",
                IntegralTypeToStr(Left->Type)
        );
    }

    return *Left;
InvalidOperands:
    ErrorAt(Compiler, &OpToken, "Invalid operands: %s and %s", 
            IntegralTypeToStr(Left->Type), IntegralTypeToStr(Right.Type)
    );
    return *Left;
}

static VarLocation ExprOr(PVMCompiler *Compiler, VarLocation *Left)
{
    VarLocation Right;
    Token OpToken = Compiler->Curr;
    if (TYPE_BOOLEAN == Left->Type)
    {
        U32 FromTrue = PVMEmitBranchIfTrue(EMITTER(), Left);
        Right = ParsePrecedence(Compiler, GetPrecedenceRule(OpToken.Type)->Prec + 1);

        IntegralType ResultType = CoerceTypes(Compiler, &OpToken, Left->Type, Right.Type);
        if (TYPE_INVALID == ResultType)
            goto InvalidOperands;
        PASCAL_ASSERT(TYPE_BOOLEAN == ResultType, "");
        PASCAL_ASSERT(TYPE_BOOLEAN == Right.Type, "");

        PVMEmitOr(EMITTER(), Left, &Right);
        PVMPatchBranchToCurrent(EMITTER(), FromTrue, BRANCHTYPE_CONDITIONAL);
    }
    else if (IntegralTypeIsInteger(Left->Type))
    {
        Right = ParsePrecedence(Compiler, GetPrecedenceRule(OpToken.Type)->Prec + 1);
        if (!IntegralTypeIsInteger(Right.Type))
        {
            goto InvalidOperands;
        }
        IntegralType ResultType = CoerceTypes(Compiler, &OpToken, Left->Type, Right.Type);
        PASCAL_ASSERT(TYPE_INVALID != ResultType, "");
        PASCAL_ASSERT(ConvertTypeImplicitly(Compiler, ResultType, Left), "");
        PASCAL_ASSERT(ConvertTypeImplicitly(Compiler, ResultType, &Right), "");

        PVMEmitOr(EMITTER(), Left, &Right);
        FreeExpr(Compiler, Right);
    }
    else 
    {
        ErrorAt(Compiler, &OpToken, "Invalid left operand: %s",
                IntegralTypeToStr(Left->Type)
        );
    }

    return *Left;
InvalidOperands:
    ErrorAt(Compiler, &OpToken, "Invalid operands: %s and %s", 
            IntegralTypeToStr(Left->Type), IntegralTypeToStr(Right.Type)
    );
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


static VarLocation CompileExpr(PVMCompiler *Compiler)
{
    return ParsePrecedence(Compiler, PREC_EXPR);
}

static VarLocation CompileExprIntoReg(PVMCompiler *Compiler)
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

static void CompileExprInto(PVMCompiler *Compiler, const Token *OpToken, VarLocation *Location)
{
    VarLocation Expr = CompileExpr(Compiler);
    if (Compiler->Error)
        return;

    CoerceTypes(Compiler, OpToken, Location->Type, Expr.Type);
    if (Expr.LocationType != VAR_MEM)
        ConvertTypeImplicitly(Compiler, Location->Type, &Expr);

    if (TYPE_POINTER == Location->Type && TYPE_POINTER == Expr.Type)
    {
        if (Location->PointsAt.Type != Expr.PointsAt.Type)
        {
            if (NULL == OpToken)
                OpToken = &Compiler->Curr;
            ErrorAt(Compiler, OpToken, "Cannot assign %s pointer to %s pointer.", 
                    IntegralTypeToStr(Expr.PointsAt.Type),
                    IntegralTypeToStr(Location->PointsAt.Type)
            );
        }
    }
    PVMEmitMov(EMITTER(), Location, &Expr);
    FreeExpr(Compiler, Expr);
}










/*===============================================================================*/
/*
 *                                   STATEMENT
 */
/*===============================================================================*/


static void CompileStmt(PVMCompiler *Compiler);




static void CompileWriteStmt(PVMCompiler *Compiler, bool NewLine)
{
    /* 'write(ln)' consumed */
    Token Keyword = Compiler->Curr;
    CompilerInitDebugInfo(Compiler, &Keyword);
    U32 ArgCount = 0;


    /* register args */
    PVMEmitSaveCallerRegs(EMITTER(), NO_RETURN_REG);

    /* arguments */
    if (ConsumeIfNextIs(Compiler, TOKEN_LEFT_PAREN))
    {
        if (!ConsumeIfNextIs(Compiler, TOKEN_RIGHT_PAREN))
        {
            do {
                VarLocation Arg = CompileExpr(Compiler);
                VarLocation ArgType = {
                    .Type = TYPE_U32,
                    .LocationType = VAR_LIT,
                    .As.Literal.Int = Arg.Type,
                };
                PVMEmitPushMultiple(EMITTER(), 2, &ArgType, &Arg);

                ArgCount++;
            } while (ConsumeIfNextIs(Compiler, TOKEN_COMMA));
            ConsumeOrError(Compiler, TOKEN_RIGHT_PAREN, "Expected ')' after argument list.");
        }
    }

    if (NewLine)
    {
        static const VarLocation NewLineLiteral = {
            .Type = TYPE_STRING,
            .LocationType = VAR_LIT,
            .As.Literal.Str = {
                .Len = 1,
                .Text = "\n",
            },
        };
        static const VarLocation LiteralType = {
            .Type = TYPE_U32,
            .LocationType = VAR_LIT,
            .As.Literal.Int = TYPE_STRING,
        };
        PVMEmitPushMultiple(EMITTER(), 2, &LiteralType, &NewLineLiteral);
        ArgCount++;
    }

    /* argcount */
    const VarLocation Argc = {
        .Type = TypeOfIntLit(ArgCount),
        .LocationType = VAR_LIT,
        .As.Literal.Int = ArgCount,
    };
    VarLocation ArgCountReg = PVMSetArgType(EMITTER(), PVM_ARGREG_0, Argc.Type);
    PVMEmitMov(EMITTER(), &ArgCountReg, &Argc);
    PVMMarkRegisterAsAllocated(EMITTER(), PVM_ARGREG_0);

    /* TODO:*/
    /* file */
    const VarLocation File = {
        .Type = TYPE_POINTER,
        .LocationType = VAR_LIT,
        .As.Literal.Ptr = NULL,
    };
    VarLocation FileReg = PVMSetArgType(EMITTER(), PVM_ARGREG_1, File.Type);
    PVMEmitMov(EMITTER(), &FileReg, &File);
    PVMMarkRegisterAsAllocated(EMITTER(), PVM_ARGREG_1);

    /* call to write */
    PVMEmitWrite(EMITTER());
    PVMEmitUnsaveCallerRegs(EMITTER(), NO_RETURN_REG);

    CompilerEmitDebugInfo(Compiler, &Keyword);
}


static void CompileExitStmt(PVMCompiler *Compiler)
{
    /* 'exit' consumed */
    Token Keyword = Compiler->Curr;
    CompilerInitDebugInfo(Compiler, &Keyword);


    /* Exit */
    if (ConsumeIfNextIs(Compiler, TOKEN_LEFT_PAREN))
    {
        const VarSubroutine *CurrentSubroutine = Compiler->Subroutine[Compiler->Scope - 1].Current;

        if (IsAtGlobalScope(Compiler))
        {
            /* Exit() */
            if (ConsumeIfNextIs(Compiler, TOKEN_RIGHT_PAREN))
                goto Done;

            VarLocation ReturnValue = PVMSetReturnType(EMITTER(), TYPE_I32);
            CompileExprInto(Compiler, &Keyword, &ReturnValue);
        }
        else if (CurrentSubroutine->HasReturnType)
        {
            /* Exit(expr), must have return value */
            if (NextTokenIs(Compiler, TOKEN_RIGHT_PAREN))
            {
                ErrorAt(Compiler, &Keyword, "Function must return a value.");
                goto Done;
            }

            VarLocation ReturnValue = PVMSetReturnType(EMITTER(), CurrentSubroutine->ReturnType);
            CompileExprInto(Compiler, &Keyword, &ReturnValue);
        }
        else if (!NextTokenIs(Compiler, TOKEN_RIGHT_PAREN))
        {
            /* Exit(), no return type */
            ErrorAt(Compiler, &Keyword, "Procedure cannot return a value.");
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
    PVMPatchBranch(EMITTER(), 
            PVMEmitBranchIfFalse(EMITTER(), &Tmp), 
            LoopHead,
            BRANCHTYPE_CONDITIONAL
    );
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
    if (NULL == Counter)
        return;
    PASCAL_NONNULL(Counter->Location);
    if (!IntegralTypeIsInteger(Counter->Type))
    {
        ErrorAt(Compiler, &Compiler->Curr, "Variable of type %s cannot be used as a counter",
                IntegralTypeToStr(Counter->Type)
        );
        return;
    }

    /* FPC does not allow the address of a counter variable to be taken */
    VarLocation CounterSave = *Counter->Location;
    VarLocation *i = Counter->Location;
    *i = PVMAllocateRegister(EMITTER(), Counter->Type);
    i->As.Register.Persistent = true;

    /* init expression */
    ConsumeOrError(Compiler, TOKEN_COLON_EQUAL, "Expected ':=' after variable name.");
    Token Assignment = Compiler->Curr;
    CompileExprInto(Compiler, &Assignment, i);


    /* for loop inc/dec */
    U32 LoopHead = 0;
    TokenType Op = TOKEN_GREATER;
    Token OpToken = Compiler->Next;
    int Inc = 1;
    if (!ConsumeIfNextIs(Compiler, TOKEN_TO))
    {
        ConsumeOrError(Compiler, TOKEN_DOWNTO, "Expected 'to' or 'downto' after expression.");
        Op = TOKEN_LESS;
        Inc = -1;
    }
    /* stop condition expr */
    U32 LoopExit = 0;
    VarLocation StopCondition = CompileExprIntoReg(Compiler); 
    if (TYPE_INVALID == CoerceTypes(Compiler, &OpToken, i->Type, StopCondition.Type))
    {
        ErrorAt(Compiler, &OpToken, "Incompatible types from %s %.*s %s.", 
                IntegralTypeToStr(i->Type),
                OpToken.Len, OpToken.Str, 
                IntegralTypeToStr(StopCondition.Type)
        );
    }
    else
    {
        PASCAL_ASSERT(ConvertTypeImplicitly(Compiler, i->Type, &StopCondition), "");

        LoopHead = PVMMarkBranchTarget(EMITTER());
        PVMEmitSetFlag(EMITTER(), Op, &StopCondition, i);
        LoopExit = PVMEmitBranchOnFalseFlag(EMITTER());
    }
    /* do */
    ConsumeOrError(Compiler, TOKEN_DO, "Expected 'do' after expression.");
    CompilerEmitDebugInfo(Compiler, &Keyword);


    /* loop body */
    /* TODO: what if the body try to write to the loop variable */
    CompileStmt(Compiler);


    /* loop increment */
    PVMEmitBranchAndInc(EMITTER(), i->As.Register, Inc, LoopHead);
    PVMPatchBranchToCurrent(EMITTER(), LoopExit, BRANCHTYPE_FLAG);


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
    U32 LoopExit = PVMEmitBranchIfFalse(EMITTER(), &Tmp);
    FreeExpr(Compiler, Tmp);
    /* do */
    ConsumeOrError(Compiler, TOKEN_DO, "Expected 'do' after expression.");
    CompilerEmitDebugInfo(Compiler, &Keyword);


    /* loop body */
    CompileStmt(Compiler);


    /* back to loophead */
    PVMEmitBranch(EMITTER(), LoopHead);
    /* patch the exit branch */
    PVMPatchBranchToCurrent(EMITTER(), LoopExit, BRANCHTYPE_CONDITIONAL);
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

    /* 
     * IF: 
     *      BEZ Tmp, ELSE 
     *      Stmt...
     *      BAL DONE
     * ELSE:
     *      Stmt...
     * DONE:
     * */
    U32 FromIf = PVMEmitBranchIfFalse(EMITTER(), &Tmp);
    FreeExpr(Compiler, Tmp);
    /* if body */
    CompileStmt(Compiler);
    if (ConsumeIfNextIs(Compiler, TOKEN_ELSE))
    {
        CompilerInitDebugInfo(Compiler, &Compiler->Curr);
        CompilerEmitDebugInfo(Compiler, &Compiler->Curr);

        U32 FromEndIf = PVMEmitBranch(EMITTER(), 0);
        PVMPatchBranchToCurrent(EMITTER(), FromIf, BRANCHTYPE_CONDITIONAL);
        CompileStmt(Compiler);
        PVMPatchBranchToCurrent(EMITTER(), FromEndIf, BRANCHTYPE_UNCONDITIONAL);
    }
    else
    {
        PVMPatchBranchToCurrent(EMITTER(), FromIf, BRANCHTYPE_CONDITIONAL);
    }
}




static void CompileCallStmt(PVMCompiler *Compiler, const Token Name, PascalVar *IdenInfo)
{
    PASCAL_NONNULL(Compiler);
    PASCAL_NONNULL(IdenInfo);
    PASCAL_NONNULL(IdenInfo->Location);
    PASCAL_ASSERT(TYPE_FUNCTION == IdenInfo->Type, "Unreachable, IdenInfo is not a subroutine");

    /* iden consumed */
    CompilerInitDebugInfo(Compiler, &Name);

    /* call the subroutine */
    VarSubroutine *Callee = &IdenInfo->Location->As.Subroutine;
    CompilerCallSubroutine(Compiler, Callee, &Name, NULL);

    CompilerEmitDebugInfo(Compiler, &Name);
}


static void CompilerEmitAssignment(PVMCompiler *Compiler, const Token *Assignment, 
        VarLocation *Left, const VarLocation *Right)
{
    VarLocation Dst = *Left;
    if (TOKEN_COLON_EQUAL == Assignment->Type)
    {
        PVMEmitMov(EMITTER(), Left, Right);
        return;
    }

    if (VAR_REG != Left->LocationType)
    {
        PVMEmitIntoReg(EMITTER(), Left, Left);
    }
    switch (Assignment->Type)
    {
    case TOKEN_PLUS_EQUAL:  PVMEmitAdd(EMITTER(), Left, Right); break;
    case TOKEN_MINUS_EQUAL: PVMEmitSub(EMITTER(), Left, Right); break;
    case TOKEN_STAR_EQUAL:  PVMEmitMul(EMITTER(), Left, Right); break;
    case TOKEN_SLASH_EQUAL: PVMEmitDiv(EMITTER(), Left, Right); break;
    case TOKEN_PERCENT_EQUAL: 
    {
        if (IntegralTypeIsInteger(Left->Type) && IntegralTypeIsInteger(Right->Type))
        {
            PVMEmitMod(EMITTER(), Left, Right);
        }
        else 
        {
            ErrorAt(Compiler, Assignment, "Cannot perform modulo between %s and %s.",
                    IntegralTypeToStr(Left->Type), IntegralTypeToStr(Right->Type)
            );
        }
    } break;
    default: 
    {
        ErrorAt(Compiler, Assignment, "Expected ':=' or other assignment operator.");
    } break;
    }


    PVMEmitMov(EMITTER(), &Dst, Left);
    /* value of left is in register, TODO: this is a hack */
    if (memcmp(&Dst, Left, sizeof Dst) != 0)
    {
        FreeExpr(Compiler, *Left);
    }
    *Left = Dst;
}

static void CompileAssignStmt(PVMCompiler *Compiler, const Token Identifier)
{
    /* iden consumed */
    CompilerInitDebugInfo(Compiler, &Identifier);

    VarLocation Dst = ParseAssignmentLhs(Compiler, PREC_VARIABLE);
    PASCAL_ASSERT(VAR_INVALID != Dst.LocationType, "Invalid location in AssignStmt.");

    const Token Assignment = Compiler->Next;
    ConsumeToken(Compiler); /* assignment type */

    VarLocation Right = CompileExpr(Compiler);
    if (TYPE_INVALID == CoerceTypes(Compiler, &Assignment, Dst.Type, Right.Type)
    || !ConvertTypeImplicitly(Compiler, Dst.Type, &Right))
    {
        ErrorAt(Compiler, &Assignment, "Cannot assign expression of type %s to %s.", 
                IntegralTypeToStr(Right.Type), IntegralTypeToStr(Dst.Type)
        );
        goto Exit;
    }
    if (TYPE_POINTER == Dst.Type && TYPE_POINTER == Right.Type 
    && Dst.PointsAt.Type != Right.PointsAt.Type)
    {
        ErrorAt(Compiler, &Assignment, "Cannot assign %s pointer to %s pointer.", 
                IntegralTypeToStr(Right.PointsAt.Type),
                IntegralTypeToStr(Dst.PointsAt.Type)
        );
        goto Exit;
    }

    /* emit the assignment */
    CompilerEmitAssignment(Compiler, &Assignment, &Dst, &Right);

Exit:
    FreeExpr(Compiler, Right);
    CompilerEmitDebugInfo(Compiler, &Identifier);
}


static void CompileIdenStmt(PVMCompiler *Compiler)
{
    /* iden consumed */

    PascalVar *IdentifierInfo = GetIdenInfo(Compiler, &Compiler->Curr,
            "Undefined identifier."
    );
    if (NULL != IdentifierInfo && TYPE_FUNCTION == IdentifierInfo->Type)
    {
        Token Callee = Compiler->Curr;
        /* Pascal's weird return statement */
        if (IdentifierInfo->Len == Callee.Len && TokenEqualNoCase(IdentifierInfo->Str, Callee.Str, Callee.Len) 
        && ConsumeIfNextIs(Compiler, TOKEN_COLON_EQUAL))
        {
            PASCAL_UNREACHABLE("TODO: return by assigning to function name");
            Compiler->Emitter.ReturnValue.Type = IdentifierInfo->Location->As.Subroutine.ReturnType;
            CompileExprInto(Compiler, NULL, &Compiler->Emitter.ReturnValue);
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



static void CompileStmt(PVMCompiler *Compiler)
{
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
        PASCAL_UNREACHABLE("TODO: case");
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
    case TOKEN_WRITELN:
    {
        ConsumeToken(Compiler);
        CompileWriteStmt(Compiler, true);
    } break;
    case TOKEN_WRITE:
    {
        ConsumeToken(Compiler);
        CompileWriteStmt(Compiler, false);
    } break;
    case TOKEN_SEMICOLON:
    {
        /* no statement */
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
    Compiler->EntryPoint = PVMGetCurrentLocation(EMITTER());
    CompileBeginStmt(Compiler);
}


static void CompileSubroutineBlock(PVMCompiler *Compiler, const char *SubroutineType)
{
    /* TODO: factor this */
    /* 'function', 'procedure' consumed */
    Token Keyword = Compiler->Curr;
    CompilerInitDebugInfo(Compiler, &Keyword);
    U32 Location = PVMGetCurrentLocation(EMITTER());


    /* function/proc name */
    ConsumeOrError(Compiler, TOKEN_IDENTIFIER, "Expected %s name.", SubroutineType);
    Token Name = Compiler->Curr;
    PascalVar *SubroutineInfo = FindIdentifier(Compiler, &Name);
    VarSubroutine *Subroutine = NULL;
    /* subroutine is already declared */
    if (NULL != SubroutineInfo)
    {
        PASCAL_NONNULL(SubroutineInfo->Location);
        Subroutine = &SubroutineInfo->Location->As.Subroutine;
        /* redefinition error */
        if (Subroutine->Defined)
        {
            ErrorAt(Compiler, &Name, "Redefinition of %s '%.*s' that was defined on line %d.",
                    SubroutineType, Name.Len, Name.Str, Subroutine->Line
            );
        }
    }
    /* new subroutine declaration */
    else 
    {
        VarLocation *Var = CompilerAllocateVarLocation(Compiler);
        Var->LocationType = VAR_SUBROUTINE;
        Var->Type = TYPE_FUNCTION;
        Var->As.Subroutine = (VarSubroutine) {
            .HasReturnType = TOKEN_FUNCTION == Keyword.Type,
            .Location = Location,
            .Defined = false,
        };
        Subroutine = &Var->As.Subroutine;

        SubroutineInfo = DefineIdentifier(Compiler, &Name, TYPE_FUNCTION, Var);
        Subroutine->Scope = VartabInit(Compiler->GlobalAlloc, PVM_INITIAL_VAR_PER_SCOPE);
    }


    /* begin subroutine scope */
    CompilerPushSubroutine(Compiler, Subroutine);
    PVMEmitterBeginScope(EMITTER());
    /* param list */
    CompileParameterList(Compiler, Subroutine);


    /* return type and semicolon */
    if (Subroutine->HasReturnType)
    {
        ConsumeOrError(Compiler, TOKEN_COLON, "Expected ':' before function return type.");
        ConsumeOrError(Compiler, TOKEN_IDENTIFIER, "Expected function return type.");
        PascalVar *ReturnTypeInfo = GetIdenInfo(Compiler, &Compiler->Curr, "Undefined return type.");
        if (NULL != ReturnTypeInfo)
            Subroutine->ReturnType = ReturnTypeInfo->Type;
        ConsumeOrError(Compiler, TOKEN_SEMICOLON, "Expected ';' after return type.");
    }
    else
    {
        if (ConsumeIfNextIs(Compiler, TOKEN_COLON) 
        && NextTokenIs(Compiler, TOKEN_IDENTIFIER))
        {
            Error(Compiler, "Procedure does not have a return type.");
        }
        else
        {
            ConsumeOrError(Compiler, TOKEN_SEMICOLON, "Expected ';' after parameter list.");
        }
    }


    /* end function decl */
    CompilerEmitDebugInfo(Compiler, &Keyword);


    /* forward declaration */
    if (ConsumeIfNextIs(Compiler, TOKEN_FORWARD))
    {
        ConsumeOrError(Compiler, TOKEN_SEMICOLON, "Expected ';' after '%.*s'", Compiler->Curr.Len, Compiler->Curr.Str);
    }
    else /* body */
    {
        CompilerResolveSubroutineCalls(Compiler, Subroutine, Location);
        Subroutine->Defined = true;

        Subroutine->Line = Name.Line;
        PVMEmitSaveFrame(EMITTER());
        CompileBlock(Compiler);

        /* emit the exit instruction,
         * associate it with the 'end' token */
        Token End = Compiler->Curr;
        CompilerInitDebugInfo(Compiler, &End);
        PVMEmitExit(EMITTER());
        CompilerEmitDebugInfo(Compiler, &End);
        ConsumeOrError(Compiler, TOKEN_SEMICOLON, "Expected ';' after %s body.", SubroutineType);
    }

    CompilerPopSubroutine(Compiler);
    PVMEmitterEndScope(EMITTER());
}



static void CompileVarBlock(PVMCompiler *Compiler)
{
    /* 
     * var
     *      id1, id2: typename;
     *      id3: typename;
     */
    Token Keyword = Compiler->Curr;

    if (!NextTokenIs(Compiler, TOKEN_IDENTIFIER))
    {
        Error(Compiler, "Var block must have at least 1 declaration.");
        return;
    }

    bool HasStackAllocation = false;
    while (!IsAtEnd(Compiler) && NextTokenIs(Compiler, TOKEN_IDENTIFIER))
    {
        HasStackAllocation = CompileAndDeclareVarList(Compiler, NULL);
        /* TODO: initialization */
        ConsumeOrError(Compiler, TOKEN_SEMICOLON, "Expected ';' after variable declaration.");
    }

    if (HasStackAllocation)
    {
        CompilerInitDebugInfo(Compiler, &Keyword);
        CompilerEmitDebugInfo(Compiler, &Keyword);
        PVMCommitStackAllocation(EMITTER());
    }
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
            if (NULL == Type)
                continue;
            if (NULL != Type->Location)
            {
                ErrorAt(Compiler, &TypeName, "'%.*s' is not a type name.",
                        TypeName.Len, TypeName.Str
                );
                continue;
            }

            DefineIdentifier(Compiler, &Identifier, Type->Type, NULL);
        }
        else if (ConsumeIfNextIs(Compiler, TOKEN_RECORD))
        {
            CompileRecordDefinition(Compiler, &Identifier);
        }
        else
        {
            PASCAL_UNREACHABLE("TODO: more type.");
        }

        ConsumeOrError(Compiler, TOKEN_SEMICOLON, "Expected ';' after type definition.");
    }
}





/* returns true if Begin is encountered */
static bool CompileHeadlessBlock(PVMCompiler *Compiler)
{
    while (!IsAtEnd(Compiler) && !NextTokenIs(Compiler, TOKEN_BEGIN))
    {
        switch (Compiler->Next.Type)
        {
        case TOKEN_BEGIN: return true;
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
            PASCAL_UNREACHABLE("TODO: const");
        } break;
        case TOKEN_LABEL:
        {
            ConsumeToken(Compiler);
            PASCAL_UNREACHABLE("TODO: label");
        } break;
        default: 
        {
            Error(Compiler, 
                  "Expected 'function', 'procedure', "
                  "'var', 'type', 'const', or 'label' before a block."
            );
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

    CompileBlock(Compiler);
    ConsumeOrError(Compiler, TOKEN_DOT, "Expected '.' instead.");
    return !Compiler->Error;
}





bool PVMCompile(const U8 *Source, 
        PascalCompileFlags Flags,
        PascalVartab *PredefinedIdentifiers, PVMChunk *Chunk, PascalGPA *GlobalAlloc, FILE *LogFile)
{
    PVMCompiler Compiler = CompilerInit(Source, Flags, PredefinedIdentifiers, Chunk, GlobalAlloc, LogFile);
    ConsumeToken(&Compiler);

    if (ConsumeIfNextIs(&Compiler, TOKEN_PROGRAM))
    {
        CompileProgram(&Compiler);
    }
    else if (CompileHeadlessBlock(&Compiler))
    {
        CompileBeginBlock(&Compiler);
    }

    CompilerDeinit(&Compiler);
    return !Compiler.Error;
}

