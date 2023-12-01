

#include <stdarg.h>


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
    VarSubroutine *Current;
} CompilerFrame;


typedef struct PVMCompiler 
{
    PascalCompileFlags Flags;

    PVMEmitter Emitter;
    PascalTokenizer Lexer;
    Token Curr, Next;

    PascalGPA InternalAlloc;
    PascalGPA *GlobalAlloc;

    PascalVartab Locals[PVM_MAX_SCOPE_COUNT];
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
    CompilerFrame Frame[PVM_MAX_SCOPE_COUNT];


    U32 Scope;
    U32 EntryPoint;
    FILE *LogFile;
    bool Error, Panic;
} PVMCompiler;




/* and allocates a register for it, use FreeExpr to free the register */
static VarLocation CompileExpr(PVMCompiler *Compiler);
static void FreeExpr(PVMCompiler *Compiler, VarLocation Expr);
static void CompileExprInto(PVMCompiler *Compiler, VarLocation *Location);


static IntegralType sCoercionRules[TYPE_COUNT][TYPE_COUNT] = {
    /*Invalid       I8            I16           I32           I64           U8            U16           U32           U64           F32           F64           Function      Boolean       Pointer */
    { TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID},         /* Invalid */
    { TYPE_INVALID, TYPE_I32,     TYPE_I32,     TYPE_I32,     TYPE_I64,     TYPE_I32,     TYPE_I32,     TYPE_I32,     TYPE_I64,     TYPE_F32,     TYPE_F64,     TYPE_INVALID, TYPE_INVALID, TYPE_POINTER},         /* I8 */
    { TYPE_INVALID, TYPE_I32,     TYPE_I32,     TYPE_I32,     TYPE_I64,     TYPE_I32,     TYPE_I32,     TYPE_I32,     TYPE_I64,     TYPE_F32,     TYPE_F64,     TYPE_INVALID, TYPE_INVALID, TYPE_POINTER},         /* I16 */
    { TYPE_INVALID, TYPE_I32,     TYPE_I32,     TYPE_I32,     TYPE_I64,     TYPE_I32,     TYPE_I32,     TYPE_I32,     TYPE_I64,     TYPE_F32,     TYPE_F64,     TYPE_INVALID, TYPE_INVALID, TYPE_POINTER},         /* I32 */
    { TYPE_INVALID, TYPE_I64,     TYPE_I64,     TYPE_I64,     TYPE_I64,     TYPE_I64,     TYPE_I64,     TYPE_I64,     TYPE_I64,     TYPE_F32,     TYPE_F64,     TYPE_INVALID, TYPE_INVALID, TYPE_POINTER},         /* I64 */
    { TYPE_INVALID, TYPE_I32,     TYPE_I32,     TYPE_I32,     TYPE_I64,     TYPE_U32,     TYPE_U32,     TYPE_U32,     TYPE_U64,     TYPE_F32,     TYPE_F64,     TYPE_INVALID, TYPE_INVALID, TYPE_POINTER},         /* U8 */
    { TYPE_INVALID, TYPE_I32,     TYPE_I32,     TYPE_I32,     TYPE_I64,     TYPE_U32,     TYPE_U32,     TYPE_U32,     TYPE_U64,     TYPE_F32,     TYPE_F64,     TYPE_INVALID, TYPE_INVALID, TYPE_POINTER},         /* U16 */
    { TYPE_INVALID, TYPE_I32,     TYPE_I32,     TYPE_I32,     TYPE_I64,     TYPE_U32,     TYPE_U32,     TYPE_U32,     TYPE_U64,     TYPE_F32,     TYPE_F64,     TYPE_INVALID, TYPE_INVALID, TYPE_POINTER},         /* U32 */
    { TYPE_INVALID, TYPE_U64,     TYPE_U64,     TYPE_U64,     TYPE_U64,     TYPE_U64,     TYPE_U64,     TYPE_U64,     TYPE_U64,     TYPE_F32,     TYPE_F64,     TYPE_INVALID, TYPE_INVALID, TYPE_POINTER},         /* U64 */
    { TYPE_INVALID, TYPE_F32,     TYPE_F32,     TYPE_F32,     TYPE_F32,     TYPE_F32,     TYPE_F32,     TYPE_F32,     TYPE_F32,     TYPE_F32,     TYPE_F64,     TYPE_INVALID, TYPE_INVALID, TYPE_INVALID},         /* F32 */
    { TYPE_INVALID, TYPE_F64,     TYPE_F64,     TYPE_F64,     TYPE_F64,     TYPE_F64,     TYPE_F64,     TYPE_F64,     TYPE_F64,     TYPE_F64,     TYPE_F64,     TYPE_INVALID, TYPE_INVALID, TYPE_INVALID},         /* F64 */
    { TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID},         /* Function */
    { TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_BOOLEAN, TYPE_INVALID},         /* Boolean */
    { TYPE_INVALID, TYPE_POINTER, TYPE_POINTER, TYPE_POINTER, TYPE_POINTER, TYPE_POINTER, TYPE_POINTER, TYPE_POINTER, TYPE_POINTER, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_POINTER},         /* Pointer */
};















/*===============================================================================*/
/*
 *                                   ERROR
 */
/*===============================================================================*/


static void ConsumeToken(PVMCompiler *Compiler);

static bool ConsumeIfNextIs(PVMCompiler *Compiler, TokenType Type)
{
    if (Type == Compiler->Next.Type)
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
        fputc(Highlighter, LogFile);
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

static void ErrorAt(PVMCompiler *Compiler, const Token *Tok, const char *ErrFmt, ...)
{
    va_list Args;
    va_start(Args, ErrFmt);
    VaListError(Compiler, Tok, ErrFmt, Args);
    va_end(Args);
}

static void Error(PVMCompiler *Compiler, const char *ErrFmt, ...)
{
    va_list Args;
    va_start(Args, ErrFmt);
    VaListError(Compiler, &Compiler->Next, ErrFmt, Args);
    va_end(Args);
}

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



static U32 CompilerGetSizeOfType(PVMCompiler *Compiler, IntegralType Type)
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

    case TYPE_POINTER:
        return sizeof(void*);
    }

    PASCAL_UNREACHABLE("Handle case %s in CompilerGetSizeOfType", IntegralTypeToStr(Type));
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
        .Frame = { {0} },
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


static void CompilerPushSubroutine(PVMCompiler *Compiler, VarSubroutine *Subroutine)
{
    Compiler->Frame[Compiler->Scope].Current = Subroutine;
    Compiler->Frame[Compiler->Scope].Location = Compiler->Var.Count;
    Compiler->Scope++;
}

static VarSubroutine *CompilerPopSubroutine(PVMCompiler *Compiler)
{
    Compiler->Scope--;
    U32 Last = Compiler->Var.Count;
    Compiler->Var.Count = Compiler->Frame[Compiler->Scope].Location;

    /* refill the variables that have been taken */
    for (U32 i = Compiler->Var.Count; i < Last; i++)
    {
        Compiler->Var.Location[i] = 
            GPAAllocateZero(Compiler->GlobalAlloc, sizeof(Compiler->Var.Location[0][0]));
    }
    return Compiler->Frame[Compiler->Scope].Current;
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
    if (Compiler->Next.Type == TOKEN_ERROR)
    {
        Error(Compiler, "%s", Compiler->Next.Literal.Err);
    }
}

static bool IsAtEnd(const PVMCompiler *Compiler)
{
    return TOKEN_EOF == Compiler->Next.Type;
}

static bool IsAtStmtEnd(const PVMCompiler *Compiler)
{
    return IsAtEnd(Compiler) || TOKEN_SEMICOLON == Compiler->Next.Type;
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
        ConsumeToken(Compiler);
    }
}


static void CalmDownAtBlock(PVMCompiler *Compiler)
{
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


static PascalVartab *CurrentScope(PVMCompiler *Compiler)
{
    if (IsAtGlobalScope(Compiler))
    {
        return Compiler->Global;
    }
    else 
    {
        return &Compiler->Locals[Compiler->Scope - 1];
    }
}



static void BeginScope(PVMCompiler *Compiler, VarSubroutine *Subroutine)
{
    /* creates new variable table */
    PascalVartab *NewScope = &Compiler->Locals[Compiler->Scope];
    PASCAL_ASSERT(Compiler->Scope <= PVM_MAX_SCOPE_COUNT, "Too many scopes");

    *NewScope = VartabInit(Compiler->GlobalAlloc, PVM_INITIAL_VAR_PER_SCOPE);

    /* mark allocation point */
    CompilerPushSubroutine(Compiler, Subroutine);
    PVMEmitterBeginScope(EMITTER());
}

static void EndScope(PVMCompiler *Compiler)
{
    PascalVartab *SubroutineScope = CurrentScope(Compiler);
    VarSubroutine *Subroutine = CompilerPopSubroutine(Compiler);
    Subroutine->Scope = *SubroutineScope;
    PVMEmitterEndScope(EMITTER());
}






static PascalVar *DefineIdentifier(PVMCompiler *Compiler, 
        const Token *Identifier, IntegralType Type, VarLocation *Location)
{
    return VartabSet(CurrentScope(Compiler), Identifier->Str, Identifier->Len, Type, Location);
}

static PascalVar *DefineGlobal(PVMCompiler *Compiler,
        const Token *Identifier, IntegralType Type, VarLocation *Location)
{
    return VartabSet(Compiler->Global, Identifier->Str, Identifier->Len, Type, Location);
}


static PascalVar *GetIdenInfo(PVMCompiler *Compiler, const Token *Identifier, const char *ErrFmt, ...)
{
    U32 Hash = VartabHashStr(Identifier->Str, Identifier->Len);

    PascalVar *Info = NULL;
    for (int i = Compiler->Scope - 1; i >= 0; i--)
    {
        Info = VartabFindWithHash(&Compiler->Locals[i],
                Identifier->Str, Identifier->Len, Hash
        );
        if (NULL != Info)
            return Info;
    }
    Info = VartabFindWithHash(Compiler->Global,
            Identifier->Str, Identifier->Len, Hash
    );
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
    PASCAL_ASSERT(Left >= TYPE_INVALID && Right >= TYPE_INVALID, "Unreachable");
    if (Left > TYPE_COUNT || Right > TYPE_COUNT)
        PASCAL_UNREACHABLE("Invalid types");

    IntegralType CommonType = sCoercionRules[Left][Right];
    if (TYPE_INVALID == CommonType)
        goto InvalidTypeCombo;

    return CommonType;
InvalidTypeCombo:
    if (NULL == Op)
    {
        Error(Compiler, "Invalid combination of %s and %s.");
    }
    else
    {
        ErrorAt(Compiler, Op, "Invalid combination of %s and %s");
    }
    return TYPE_INVALID;
}


static bool ConvertTypeImplicitly(PVMCompiler *Compiler, IntegralType To, VarLocation *From)
{
    switch (From->LocationType)
    {
    case VAR_LIT:
    {
        if (To == From->Type 
        || (IntegralTypeIsInteger(To) && IntegralTypeIsInteger(From->Type)))
        {
            break;
        }

        if (IntegralTypeIsFloat(To))
        {
            if (IntegralTypeIsFloat(From->Type))
            {
                break;
            }
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
        if (To == From->Type)
        {
            break;
        }
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



static void FunctionDataPushParameter(PascalGPA *Allocator, VarSubroutine *Function, PascalVar *Param)
{
    if (Function->ArgCount == Function->Cap)
    {
        Function->Cap = Function->Cap * 2 + 8;
        Function->Args = GPAReallocateArray(Allocator, 
                Function->Args, *Function->Args, 
                Function->Cap
        );
    }

    Function->Args[Function->ArgCount++] = Param;
}





static Token *CompilerGetTmpIdentifier(PVMCompiler *Compiler, UInt Idx)
{
    return &Compiler->Iden.Array[Idx];
}


static bool CompileAndDeclareParameter(PVMCompiler *Compiler, 
        U32 VarCount, U32 VarSize, IntegralType VarType, VarSubroutine *Function)
{
    PASCAL_ASSERT(CompilerGetTmpIdentifierCount(Compiler) != 0, 
            "cannot be called outside of CompileAndDeclareVarList()");

    bool HasStackParams = false;
    VarSize += sizeof(PVMGPR);
    if (VarSize % sizeof(PVMGPR))
        VarSize &= ~(sizeof(PVMGPR) - 1);

    Function->StackArgSize = 0;
    for (U32 i = 0; i < VarCount; i++)
    {
        VarLocation *Location = CompilerAllocateVarLocation(Compiler);
        PascalVar *Var = DefineIdentifier(Compiler, 
                CompilerGetTmpIdentifier(Compiler, i),
                VarType,
                Location
        );

        *Location = PVMSetParamType(EMITTER(), VarType);
        PVMMarkArgAsOccupied(EMITTER(), Location);
        /* register args */
        if (Function->ArgCount < PVM_ARGREG_COUNT)
        {
            /* TODO: nested functions */
        }
        else /* stack args, pascal LTR calling convention, TODO: cdecl */
        {
            HasStackParams = true;
			Function->StackArgSize += VarSize;
        }
        Location->Type = VarType;
        FunctionDataPushParameter(Compiler->GlobalAlloc, Function, Var);
    }
	/* align func args */
    return HasStackParams;
}

static bool CompileAndDeclareLocal(PVMCompiler *Compiler, U32 VarCount, U32 VarSize, IntegralType VarType)
{
    PASCAL_ASSERT(VarCount > 0, "Unreachable");

    for (U32 i = 0; i < VarCount; i++)
    {
        VarLocation *Location = CompilerAllocateVarLocation(Compiler);
        DefineIdentifier(Compiler, 
                CompilerGetTmpIdentifier(Compiler, i),
                VarType,
                Location
        );

        Location->LocationType = VAR_MEM;
        Location->Type = VarType;
        Location->As.Memory = PVMQueueStackAllocation(EMITTER(), VarSize);
    }
    return true;
}


static bool CompileAndDeclareGlobal(PVMCompiler *Compiler, U32 VarCount, U32 VarSize, IntegralType VarType)
{
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
        Location->As.Memory = PVMEmitGlobalSpace(EMITTER(), VarSize);
    }
    return false;
}

/* returns the size of the whole var list */
static bool CompileAndDeclareVarList(PVMCompiler *Compiler, VarSubroutine *Function)
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


    /* typename */
    if (!ConsumeOrError(Compiler, TOKEN_COLON, "Expected ':' or ',' after variable name."))
        return false;
    if (!ConsumeOrError(Compiler, TOKEN_IDENTIFIER, "Expected type name after ':'"))
        return false;


    /* next token is now the type name */
    IntegralType Type = TYPE_INVALID;
    PascalVar *TypeInfo = GetIdenInfo(Compiler, &Compiler->Curr, "Undefined type name.");
    if (NULL != TypeInfo)
    {
        Type = TypeInfo->Type;
    }
    U32 Size = CompilerGetSizeOfType(Compiler, Type);
    UInt VarCount = CompilerGetTmpIdentifierCount(Compiler);


    /* dispatch to the appropriate routine to handle space allocation */
    if (NULL != Function)
    {
        return CompileAndDeclareParameter(Compiler, VarCount, Size, Type, Function);
    }
    else if (!IsAtGlobalScope(Compiler))
    {
        return CompileAndDeclareLocal(Compiler, VarCount, Size, Type);
    }

    return CompileAndDeclareGlobal(Compiler, VarCount, Size, Type);
}



static void CompileParameterList(PVMCompiler *Compiler, VarSubroutine *CurrentFunction)
{
    PASCAL_ASSERT(!IsAtGlobalScope(Compiler), "Cannot compile param list at global scope");
    PASCAL_ASSERT(NULL != CurrentFunction, "CompileParameterList() does not accept NULL");
    /* assumes '(' or some kind of terminator is the next token */

    CurrentFunction->ArgCount = 0;

    if (!ConsumeIfNextIs(Compiler, TOKEN_LEFT_PAREN))
        return;

    if (ConsumeIfNextIs(Compiler, TOKEN_RIGHT_PAREN))
        return;

    bool StackArgs = false;
    EMITTER()->NumArgs = 0;
    do {
        StackArgs = CompileAndDeclareVarList(Compiler, CurrentFunction);
    } while (ConsumeIfNextIs(Compiler, TOKEN_SEMICOLON));

    ConsumeOrError(Compiler, TOKEN_RIGHT_PAREN, "Expected ')' after parameter list.");
    if (StackArgs)
    {
        PVMCommitStackAllocation(EMITTER());
    }
}


static void CompileArgumentList(PVMCompiler *Compiler, const Token *FunctionName, VarSubroutine *Subroutine)
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
    EMITTER()->NumArgs = 0;
    EMITTER()->StackSpace += Subroutine->StackArgSize;
    PVMAllocateStack(EMITTER(), Subroutine->StackArgSize);
    do {
        if (ArgCount < ExpectedArgCount)
        {
            VarLocation Arg = PVMSetArgType(EMITTER(), Subroutine->Args[ArgCount]->Location->Type);
			CompileExprInto(Compiler, &Arg);
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














/*===============================================================================*/
/*
 *                                  EXPRESSION
 * TODO: typecheck
 * */
/*===============================================================================*/

typedef enum Precedence 
{
    PREC_SINGLE = 0,
    PREC_EXPR,
    PREC_SIMPLE,
    PREC_TERM,
    PREC_FACTOR,
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
	if (VAR_REG == Expr.LocationType)
	{
		PVMFreeRegister(EMITTER(), Expr.As.Register);
	}
}



static VarLocation FactorLiteral(PVMCompiler *Compiler)
{
    VarLocation Location = {
        .LocationType = VAR_LIT,
    };

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


static VarLocation FactorVariable(PVMCompiler *Compiler)
{
    /* identifier consumed */
    Token Identifier = Compiler->Curr;
    PascalVar *Variable = GetIdenInfo(Compiler, &Identifier, "Undefined variable.");
    if (NULL == Variable)
        goto Error;


    if (TYPE_FUNCTION == Variable->Type)
    {
        PASCAL_ASSERT(Variable->Location->LocationType == VAR_SUBROUTINE, "wtf????");

        VarSubroutine *Callee = &Variable->Location->As.Subroutine;
        if (!Callee->HasReturnType)
        {
            ErrorAt(Compiler, &Identifier, "Procedure does not have a return value.");
            goto Error;
        }

        /* setup return reg */
        VarLocation ReturnValue = PVMAllocateRegister(EMITTER(), Callee->ReturnType);
        UInt ReturnReg = ReturnValue.As.Register.ID;

        /* call the function */
        PVMEmitSaveCallerRegs(EMITTER(), ReturnReg);
        CompileArgumentList(Compiler, &Identifier, Callee);
        PVMEmitCall(EMITTER(), Callee);

        /* carefully move return reg into the return location */
        Compiler->Emitter.ReturnValue.Type = Callee->ReturnType;
        PVMEmitMov(EMITTER(), &ReturnValue, &EMITTER()->ReturnValue);
        PVMEmitUnsaveCallerRegs(EMITTER(), ReturnReg);
		if (Callee->StackArgSize) 
		{
            PVMAllocateStack(EMITTER(), -Callee->StackArgSize);
		}
        return ReturnValue;
    }
    else
    {
        VarLocation Register = PVMAllocateRegister(EMITTER(), Variable->Type);
        PVMEmitMov(EMITTER(), &Register, Variable->Location);
        return Register;
    }

Error:
    return (VarLocation) {
        .LocationType = VAR_INVALID,
    };
}

static VarLocation FactorGrouping(PVMCompiler *Compiler)
{
    /* '(' consumed */
    VarLocation Group = ParsePrecedence(Compiler, PREC_EXPR);
    ConsumeOrError(Compiler, TOKEN_RIGHT_PAREN, "Expected ')' after expression.");
    return Group;
}


static VarLocation NegateLiteral(PVMCompiler *Compiler, const Token *OpToken, VarLiteral Literal, IntegralType LiteralType)
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
        ErrorAt(Compiler, OpToken, "Cannot be applied to value of tpe %s", 
                IntegralTypeToStr(LiteralType)
        );
    }

    Location.As.Literal = Literal;
    return Location;
}

static VarLocation NotLiteral(PVMCompiler *Compiler, const Token *OpToken, VarLiteral Literal, IntegralType LiteralType)
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
        case TOKEN_MINUS:
        {
            Value = NegateLiteral(Compiler, &OpToken, Value.As.Literal, Value.Type);
        } break;
        case TOKEN_NOT:
        {
            Value = NotLiteral(Compiler, &OpToken, Value.As.Literal, Value.Type);
        } break;
        default: goto Unreachable;
        }
    }
    else if (IntegralTypeIsInteger(Value.Type))
    {
        switch (Operator)
        {
        case TOKEN_PLUS: break;
        case TOKEN_MINUS:
        {
            PVMEmitNeg(EMITTER(), &Value, &Value);
        } break;
        case TOKEN_NOT:
        {
            PVMEmitNot(EMITTER(), &Value, &Value);
        } break;
        default: goto Unreachable;
        }
    }
    else if (TYPE_BOOLEAN == Value.Type)
    {
        PASCAL_UNREACHABLE("TODO boolean in unary");
    }
    return Value;

Unreachable: 
    PASCAL_UNREACHABLE("Invalid operator for unary");
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
    PASCAL_ASSERT(VAR_LIT == Left->LocationType, "left is not a literal in %s", "LiteralExprBinary");
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
    } break;
    case TOKEN_SHR:
    case TOKEN_GREATER_GREATER:
    {
        if (IntegralTypeIsInteger(Both))
        {
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
    } break;

    case TOKEN_AND: INT_AND_BOOL_OP(&); break;
    case TOKEN_OR:  INT_AND_BOOL_OP(|); break;
    case TOKEN_XOR: INT_AND_BOOL_OP(^); break;
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
    VarLocation Dst = *Left;
    VarLocation Src = *Right;
    if (VAR_LIT == Left->LocationType)
    {
        Dst = *Right;
        Src = *Left;
    }

    /* TODO: CoerceTypes is kinda useless */
    /* TODO: Coerce types now returns the type from an expression,
     * cast both sides of the operand to that type? 
     * */
    IntegralType Type = CoerceTypes(Compiler, OpToken, Left->Type, Right->Type);
    bool ConversionOk = 
        ConvertTypeImplicitly(Compiler, Type, &Dst) 
        && ConvertTypeImplicitly(Compiler, Type, &Src);
    if (!ConversionOk) goto TypeMismatch;

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
        Dst = PVMEmitSetCC(EMITTER(), OpToken->Type, &Dst, &Src);
    } break;

    /* TODO: lazy/bool eval */
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

TypeMismatch:
    ErrorAt(Compiler, OpToken, "Invalid operands: %s and %s\n", 
            IntegralTypeToStr(Dst.Type), IntegralTypeToStr(Src.Type)
    );
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




static const PrecedenceRule sPrecedenceRuleLut[TOKEN_TYPE_COUNT] = 
{
    [TOKEN_INTEGER_LITERAL] = { FactorLiteral,      NULL,           PREC_SINGLE },
    [TOKEN_NUMBER_LITERAL]  = { FactorLiteral,      NULL,           PREC_SINGLE },
    [TOKEN_STRING_LITERAL]  = { FactorLiteral,      NULL,           PREC_SINGLE },
    [TOKEN_TRUE]            = { FactorLiteral,      NULL,           PREC_SINGLE },
    [TOKEN_FALSE]           = { FactorLiteral,      NULL,           PREC_SINGLE },
    [TOKEN_IDENTIFIER]      = { FactorVariable,     NULL,           PREC_SINGLE },

    [TOKEN_LEFT_PAREN]      = { FactorGrouping,     NULL,           PREC_FACTOR },
    [TOKEN_NOT]             = { ExprUnary,          NULL,           PREC_FACTOR },

    [TOKEN_STAR]            = { NULL,               ExprBinary,     PREC_TERM },
    [TOKEN_DIV]             = { NULL,               ExprBinary,     PREC_TERM },
    [TOKEN_SLASH]           = { NULL,               ExprBinary,     PREC_TERM },
    [TOKEN_MOD]             = { NULL,               ExprBinary,     PREC_TERM },
    [TOKEN_AND]             = { NULL,               ExprBinary,     PREC_TERM },

    [TOKEN_OR]              = { NULL,               ExprBinary,     PREC_SIMPLE },
    [TOKEN_XOR]             = { NULL,               ExprBinary,     PREC_SIMPLE },
    [TOKEN_PLUS]            = { ExprUnary,          ExprBinary,     PREC_SIMPLE },
    [TOKEN_MINUS]           = { ExprUnary,          ExprBinary,     PREC_SIMPLE },
    [TOKEN_SHR]             = { NULL,               ExprBinary,     PREC_SIMPLE },
    [TOKEN_SHL]             = { NULL,               ExprBinary,     PREC_SIMPLE },
    [TOKEN_LESS_LESS]       = { NULL,               ExprBinary,     PREC_SIMPLE },
    [TOKEN_GREATER_GREATER] = { NULL,               ExprBinary,     PREC_SIMPLE },

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


static VarLocation ParsePrecedence(PVMCompiler *Compiler, Precedence Prec)
{
    ConsumeToken(Compiler);
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

        PASCAL_ASSERT(NULL != InfixRoutine, "NULL infix routine in ParsePrecedence()");
        Left = InfixRoutine(Compiler, &Left);
    }
    return Left;
}


static VarLocation CompileExpr(PVMCompiler *Compiler)
{
    VarLocation Expr = ParsePrecedence(Compiler, PREC_EXPR);
    if (Compiler->Error || VAR_LIT !=  Expr.LocationType)
        return Expr;

    VarLocation Place = PVMAllocateRegister(EMITTER(), Expr.Type);
    PVMEmitMov(EMITTER(), &Place, &Expr);
    FreeExpr(Compiler, Expr);
    return Place;
}

static void CompileExprInto(PVMCompiler *Compiler, VarLocation *Location)
{
    VarLocation Expr = ParsePrecedence(Compiler, PREC_EXPR);
    if (Compiler->Error)
        return;

    CoerceTypes(Compiler, NULL, Location->Type, Expr.Type);
    if (IntegralTypeIsInteger(Location->Type) && IntegralTypeIsFloat(Expr.Type))
    {
        ConvertTypeImplicitly(Compiler, Location->Type, &Expr);
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



static void CompileExitStmt(PVMCompiler *Compiler)
{
    /* 'exit' consumed */
    Token Keyword = Compiler->Curr;
    CompilerInitDebugInfo(Compiler, &Keyword);


    /* Exit */
    if (ConsumeIfNextIs(Compiler, TOKEN_LEFT_PAREN))
    {
        if (IsAtGlobalScope(Compiler))
        {
            /* Exit() */
            if (ConsumeIfNextIs(Compiler, TOKEN_RIGHT_PAREN))
                goto Done;

            EMITTER()->ReturnValue.Type = TYPE_I32;
            CompileExprInto(Compiler, &EMITTER()->ReturnValue);
        }
        else if (Compiler->Frame[Compiler->Scope - 1].Current->HasReturnType)
        {
            /* Exit(expr), must have return value */
            if (TOKEN_RIGHT_PAREN == Compiler->Next.Type)
            {
                Error(Compiler, "Function must return a value.");
                goto Done;
            }

            EMITTER()->ReturnValue.Type = Compiler->Frame[Compiler->Scope - 1].Current->ReturnType;
            CompileExprInto(Compiler, &EMITTER()->ReturnValue);
        }
        else if (TOKEN_RIGHT_PAREN != Compiler->Next.Type)
        {
            /* Exit(), no return type */
            Error(Compiler, "Procedure cannot return a value.");
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
    while (!IsAtEnd(Compiler) && TOKEN_END != Compiler->Next.Type)
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
    while (!IsAtEnd(Compiler) && TOKEN_UNTIL != Compiler->Next.Type)
    {
        CompileStmt(Compiler);
        if (TOKEN_UNTIL == Compiler->Next.Type)
            break;
        ConsumeOrError(Compiler, TOKEN_SEMICOLON, "Expected ';' in between statements.");
    }


    /* nothing to compile, exit and reports error */
    if (!ConsumeOrError(Compiler, TOKEN_UNTIL, "Expected 'until'."))
        return;

    Token UntilKeyword = Compiler->Curr;
    CompilerInitDebugInfo(Compiler, &UntilKeyword);

    VarLocation Tmp = CompileExpr(Compiler);
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
     * for j := 100 downto 0 do 
     */
    /* 'for' consumed */
    Token Keyword = Compiler->Curr;
    CompilerInitDebugInfo(Compiler, &Keyword);


    /* for loop variable */
    ConsumeOrError(Compiler, TOKEN_IDENTIFIER, "Expected variable name.");
    PascalVar *Counter = GetIdenInfo(Compiler, &Compiler->Curr, "Undefined variable.");
    /* TODO: better way to do this
     *  This is a hack to make the for loop variable a register,
     *  this won't work if the loop body take the addr of the counter variable
     * */
    VarLocation Save = *Counter->Location;
    /* init expression */
    ConsumeOrError(Compiler, TOKEN_COLON_EQUAL, "Expected ':=' after variable name.");
    *Counter->Location = PVMAllocateRegister(EMITTER(), Save.Type);
    CompileExprInto(Compiler, Counter->Location);


    /* for loop inc/dec */
    U32 LoopHead = PVMMarkBranchTarget(EMITTER());
    TokenType Op = TOKEN_GREATER;
    I16 Inc = 1;
    if (!ConsumeIfNextIs(Compiler, TOKEN_TO))
    {
        ConsumeOrError(Compiler, TOKEN_DOWNTO, "Expected 'to' or 'downto' after expression.");
        Op = TOKEN_LESS;
        Inc = -1;
    }
    /* stop condition expr */
    VarLocation StopCondition = CompileExpr(Compiler);
    PVMEmitSetCC(EMITTER(), Op, &StopCondition, Counter->Location);
    U32 LoopExit = PVMEmitBranchIfFalse(EMITTER(), &StopCondition);
    FreeExpr(Compiler, StopCondition);
    /* do */
    ConsumeOrError(Compiler, TOKEN_DO, "Expected 'do' after expression.");
    CompilerEmitDebugInfo(Compiler, &Keyword);


    /* loop body */
    CompileStmt(Compiler);


    /* loop increment */
    PVMEmitAddImm(EMITTER(), Counter->Location, Inc);
    PVMEmitBranch(EMITTER(), LoopHead);
    PVMPatchBranchToCurrent(EMITTER(), LoopExit, BRANCHTYPE_CONDITIONAL);


    /* TODO: unhack this */
    FreeExpr(Compiler, *Counter->Location);
    *Counter->Location = Save;
}


static void CompileWhileStmt(PVMCompiler *Compiler)
{
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
    if (TOKEN_ELSE == Compiler->Next.Type)
    {
        ConsumeToken(Compiler);
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




static void CompileCallStmt(PVMCompiler *Compiler, const Token *Callee, PascalVar *IdenInfo)
{
    PASCAL_ASSERT(NULL != IdenInfo, "Unreachable, IdenInfo is NULL in CompileCallStmt()");
    PASCAL_ASSERT(NULL != IdenInfo->Location, "Unreachable, Location is NULL in CompileCallStmt()");
    PASCAL_ASSERT(TYPE_FUNCTION == IdenInfo->Type, 
            "Unreachable, IdenInfo is not a subroutine in CompileCallStmt()"
    );

    /* iden consumed */
    CompilerInitDebugInfo(Compiler, Callee);

    VarSubroutine *Subroutine = &IdenInfo->Location->As.Subroutine;
    PVMEmitSaveCallerRegs(EMITTER(), NO_RETURN_REG);
    CompileArgumentList(Compiler, Callee, Subroutine);
    PVMEmitCall(EMITTER(), Subroutine);
    PVMEmitUnsaveCallerRegs(EMITTER(), NO_RETURN_REG);
	if (Subroutine->StackArgSize) 
	{
        PVMAllocateStack(EMITTER(), -Subroutine->StackArgSize);
	}

    CompilerEmitDebugInfo(Compiler, Callee);
}


static void CompileAssignStmt(PVMCompiler *Compiler, PascalVar *IdenInfo)
{
    /* iden consumed */
    Token Identifier = Compiler->Curr;
    CompilerInitDebugInfo(Compiler, &Identifier);


    /* TODO: field access and array indexing */
    ConsumeOrError(Compiler, TOKEN_COLON_EQUAL, "TODO: other assignment operator.");


    if (NULL != IdenInfo)
    {
        PASCAL_ASSERT(NULL != IdenInfo->Location, "Location must be valid");
        PASCAL_ASSERT(IdenInfo->Type != TYPE_INVALID, "DeclVarblock should've handled this");
        PASCAL_ASSERT(VAR_INVALID != IdenInfo->Location->LocationType, "??");

        CompileExprInto(Compiler, IdenInfo->Location);
    }


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
        CompileCallStmt(Compiler, &Callee, IdentifierInfo);
    }
    else
    {
        CompileAssignStmt(Compiler, IdentifierInfo);
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
    case TOKEN_EXIT:
    {
        ConsumeToken(Compiler);
        CompileExitStmt(Compiler);
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
    /* 'function', 'procedure' consumed */
    VarLocation *Var = CompilerAllocateVarLocation(Compiler);
    Var->LocationType = VAR_SUBROUTINE;
    Var->As.Subroutine = (VarSubroutine) {
        .HasReturnType = TOKEN_FUNCTION == Compiler->Curr.Type,
        .Location = PVMGetCurrentLocation(EMITTER()),
    };
    VarSubroutine *Subroutine = &Var->As.Subroutine;


    /* begin function decl */
    Token Keyword = Compiler->Curr;
    CompilerInitDebugInfo(Compiler, &Keyword);


    /* function/proc name */
    ConsumeOrError(Compiler, TOKEN_IDENTIFIER, "Expected %s name.", SubroutineType);
    PascalVar *FunctionInfo = DefineIdentifier(Compiler, &Compiler->Curr, TYPE_FUNCTION, Var);
    PASCAL_ASSERT(FunctionInfo->Location == Var, "Unreachable");


    /* param list */
    BeginScope(Compiler, Subroutine);
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
        && TOKEN_IDENTIFIER == Compiler->Next.Type)
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


    /* body */
    CompileBlock(Compiler);
    EndScope(Compiler);


    /* 'end' at the end of function */
    Token End = Compiler->Curr;
    CompilerInitDebugInfo(Compiler, &End);
    PVMEmitExit(EMITTER());
    CompilerEmitDebugInfo(Compiler, &End);
    ConsumeOrError(Compiler, TOKEN_SEMICOLON, "Expected ';' after %s body.", SubroutineType);
}



static void CompileVarBlock(PVMCompiler *Compiler)
{
    /* 
     * var
     *      id1, id2: typename;
     *      id3: typename;
     */
    Token Keyword = Compiler->Curr;

    if (TOKEN_IDENTIFIER != Compiler->Next.Type)
    {
        Error(Compiler, "Var block must have at least 1 declaration.");
        return;
    }

    bool HasStackAllocation = false;
    while (!IsAtEnd(Compiler) && TOKEN_IDENTIFIER == Compiler->Next.Type)
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





/* returns true if Begin is encountered */
static bool CompileHeadlessBlock(PVMCompiler *Compiler)
{
    while (!IsAtEnd(Compiler) && Compiler->Next.Type != TOKEN_BEGIN)
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
            PASCAL_UNREACHABLE("TODO: type");
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

