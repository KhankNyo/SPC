

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




static VarLocation CompileExpr(PVMCompiler *Compiler);
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
    { TYPE_INVALID, TYPE_I64,     TYPE_I64,     TYPE_I64,     TYPE_I64,     TYPE_U32,     TYPE_U32,     TYPE_U32,     TYPE_U64,     TYPE_F32,     TYPE_F64,     TYPE_INVALID, TYPE_INVALID, TYPE_POINTER},         /* U32 */
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

    (void)Compiler;
    /* TODO: record and array types */
    return 0;
}


static UInt TypeOfIntLit(U64 Integer)
{
    if (IN_I8(Integer))
        return TYPE_I8;
    if (IN_U8(Integer))
        return TYPE_U8;
    if (IN_I16(Integer))
        return TYPE_I16;
    if (IN_U16(Integer))
        return TYPE_U16;
    if (IN_I32(Integer))
        return TYPE_I32;
    if (IN_U32(Integer))
        return TYPE_U32;
    return TYPE_U64;
}





static PVMCompiler CompilerInit(const U8 *Source, 
        PascalVartab *PredefinedIdentifiers, PVMChunk *Chunk, PascalGPA *GlobalAlloc, FILE *LogFile)
{
    PVMCompiler Compiler = {
        .Lexer = TokenizerInit(Source),
        .LogFile = LogFile,
        .Error = false,
        .Panic = false,
        .Scope = 0,

        .GlobalAlloc = GlobalAlloc,
        .InternalAlloc = GPAInit(4 * 1024 * 1024),
        .Var = {
            .Cap = 256,
            .Count = 0,
        },
        .Global = PredefinedIdentifiers,
        .Iden = {
            .Cap = 8,
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
        Compiler->Var.Location = 
            GPAReallocate(&Compiler->InternalAlloc, Compiler->Var.Location, Compiler->Var.Cap);
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

static IntegralType CoerceTypes(PVMCompiler *Compiler, const Token *Op, IntegralType Left, IntegralType Right)
{
    PASCAL_ASSERT(Left >= TYPE_INVALID && Right >= TYPE_INVALID, "Unreachable");
    if (Left > TYPE_COUNT || Right > TYPE_COUNT)
    {
        PASCAL_UNREACHABLE("Invalid types");
    }

    IntegralType Type = sCoercionRules[Left][Right];
    if (TYPE_INVALID == Type)
    {
        goto InvalidTypeCombo;
    }

    if (NULL == Op)
        return Type;

    switch (Op->Type)
    {
    case TOKEN_PLUS:
    case TOKEN_PLUS_EQUAL:
    case TOKEN_MINUS:
    case TOKEN_MINUS_EQUAL:
    case TOKEN_SLASH:
    case TOKEN_SLASH_EQUAL:
    case TOKEN_STAR:
    case TOKEN_STAR_EQUAL:
    case TOKEN_DIV:
    case TOKEN_MOD:
    {
        if (TYPE_BOOLEAN == Left || TYPE_BOOLEAN == Right)
            goto InvalidTypeCombo;
    } break;
    case TOKEN_DOWNTO:
    case TOKEN_TO:
    case TOKEN_LESS:
    case TOKEN_GREATER:
    case TOKEN_LESS_EQUAL:
    case TOKEN_GREATER_EQUAL:
    case TOKEN_LESS_GREATER:
    case TOKEN_EQUAL:
    {
        if (TYPE_BOOLEAN == Left || TYPE_BOOLEAN == Right)
            goto InvalidTypeCombo;
        return TYPE_BOOLEAN;
    } break;
    case TOKEN_AND:
    case TOKEN_OR:
    case TOKEN_NOT:
    {
        if (TYPE_BOOLEAN != Left && TYPE_BOOLEAN != Right)
            goto InvalidTypeCombo;
    } break;
    default: break;
    }

    return Type;

InvalidTypeCombo:
    if (NULL == Op)
    {
        Error(Compiler, "Invalid combination of type %s and %s",
                IntegralTypeToStr(Left), IntegralTypeToStr(Right)
        );
    }
    else 
    {
        ErrorAt(Compiler, Op, "Invalid combination of type %s and %s", 
                IntegralTypeToStr(Left), IntegralTypeToStr(Right)
        );
    }
    return TYPE_INVALID;
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
        Function->Args = GPAReallocate(Allocator, Function->Args, Function->Cap);
    }

    Function->Args[Function->ArgCount++] = Param;
}





static Token *CompilerGetTmpIdentifier(PVMCompiler *Compiler, UInt Idx)
{
    return &Compiler->Iden.Array[Idx];
}


static bool CompileAndDeclareParameter(PVMCompiler *Compiler, U32 VarCount, U32 VarSize, IntegralType VarType, VarSubroutine *Function)
{
    PASCAL_ASSERT(CompilerGetTmpIdentifierCount(Compiler) != 0, 
            "cannot be called outside of CompileAndDeclareVarList()");

    bool HasStackParams = false;
    for (U32 i = 0; i < VarCount; i++)
    {
        VarLocation *Location = CompilerAllocateVarLocation(Compiler);
        PascalVar *Var = DefineIdentifier(Compiler, 
                CompilerGetTmpIdentifier(Compiler, i),
                VarType,
                Location
        );

        /* register args */
        if (Function->ArgCount < PVM_ARGREG_COUNT)
        {
            *Location = Compiler->Emitter.ArgReg[Function->ArgCount];
            Location->As.Register.Type = VarType;

            /* will be freed at the end of the function's scope */
            /* TODO: nested functions */
            PVMMarkRegisterAsAllocated(EMITTER(), Location->As.Register.ID);
        }
        else /* stack args */
        {
            HasStackParams = true;
            Location->LocationType = VAR_MEM;
            Location->As.Memory = PVMQueueStackAllocation(EMITTER(), VarSize, VarType);
        }
        FunctionDataPushParameter(Compiler->GlobalAlloc, Function, Var);
    }
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
        Location->As.Memory = PVMQueueStackAllocation(EMITTER(), VarSize, VarType);
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
        Location->As.Memory = PVMEmitGlobalSpace(EMITTER(), VarSize, VarType);
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

    do {
        CompileAndDeclareVarList(Compiler, CurrentFunction);
    } while (ConsumeIfNextIs(Compiler, TOKEN_SEMICOLON));
    ConsumeOrError(Compiler, TOKEN_RIGHT_PAREN, "Expected ')' after parameter list.");
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



    /* register args */
    do {
        if (ArgCount >= ExpectedArgCount)
            break;

        Compiler->Emitter.ArgReg[ArgCount].As.Register.Type = 
            TypeOfLocation(Subroutine->Args[ArgCount]->Location);
        CompileExprInto(Compiler, &EMITTER()->ArgReg[ArgCount]);
        ArgCount++;
    } while (ArgCount < PVM_ARGREG_COUNT && ConsumeIfNextIs(Compiler, TOKEN_COMMA));

    /* stack args */
    while (!IsAtEnd(Compiler) && TOKEN_RIGHT_PAREN != Compiler->Next.Type)
    {
        PASCAL_UNREACHABLE("TODO: stack args");
    }

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
    PASCAL_ASSERT(Expr.LocationType == VAR_REG, "Cannot free expression.");
    PVMFreeRegister(EMITTER(), Expr.As.Register);
}



static VarLocation FactorIntLit(PVMCompiler *Compiler)
{
    U64 Literal = Compiler->Curr.Literal.Int;
    IntegralType LiteralType = TypeOfIntLit(Literal);
    VarLocation Location = PVMAllocateRegister(EMITTER(), LiteralType);

    PVMEmitLoadImm(EMITTER(), Location.As.Register, Literal);
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
        Compiler->Emitter.ReturnValue.As.Register.Type = Callee->ReturnType;
        PVMEmitMov(EMITTER(), &ReturnValue, &EMITTER()->ReturnValue);
        PVMEmitUnsaveCallerRegs(EMITTER());
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
    VarLocation Group = CompileExpr(Compiler);
    ConsumeOrError(Compiler, TOKEN_RIGHT_PAREN, "Expected ')' after expression.");
    return Group;
}


static VarLocation ExprUnary(PVMCompiler *Compiler)
{
    TokenType Operator = Compiler->Curr.Type;
    VarLocation Value = ParsePrecedence(Compiler, PREC_SIMPLE);

    switch (Operator)
    {
    case TOKEN_PLUS: break;
    case TOKEN_MINUS:
    {
        PVMEmitNeg(EMITTER(), &Value, &Value);
    } break;
    default: PASCAL_UNREACHABLE("Invalid operator for unary"); break;
    }
    return Value;
}

static VarLocation ExprBinary(PVMCompiler *Compiler, VarLocation *Left)
{
    Token OpToken = Compiler->Curr;
    TokenType Operator = OpToken.Type;
    const PrecedenceRule *OperatorRule = GetPrecedenceRule(Compiler->Curr.Type);

    /* +1 to parse binary oper as left associative,
     * ex:  parses  1 + 2 + 3 
     *      as      ((1 + 2) + 3)
     *      not     (1 + (2 + 3)) 
     */
    VarLocation Right = ParsePrecedence(Compiler, OperatorRule->Prec + 1);
    IntegralType Type = CoerceTypes(Compiler, &OpToken, TypeOfLocation(Left), TypeOfLocation(&Right));

    switch (Operator)
    {
    case TOKEN_STAR:
    {
        PVMEmitMul(EMITTER(), Left, &Right);
    } break;
    case TOKEN_DIV:
    {
        PVMEmitDiv(EMITTER(), Left, &Right);
    } break;
    case TOKEN_MOD:
    {
        PVMEmitDiv(EMITTER(), Left, &Right);
    } break;

    case TOKEN_PLUS:
    {
        PVMEmitAdd(EMITTER(), Left, &Right);
    } break;
    case TOKEN_MINUS:
    {
        PVMEmitSub(EMITTER(), Left, &Right);
    } break;

    case TOKEN_LESS:
    case TOKEN_GREATER:
    case TOKEN_LESS_EQUAL:
    case TOKEN_GREATER_EQUAL:
    case TOKEN_EQUAL:
    case TOKEN_LESS_GREATER:
    {
        PVMEmitSetCC(EMITTER(), Operator, Left, &Right);
    } break;

    case TOKEN_AND:
    case TOKEN_OR:

    default: PASCAL_UNREACHABLE("Operator %s is unreachable in ExprBinary", TokenTypeToStr(Operator)); break;
    }

    PVMFreeRegister(EMITTER(), Right.As.Register);
    Left->As.Register.Type = Type;
    return *Left;
}




static const PrecedenceRule sPrecedenceRuleLut[TOKEN_TYPE_COUNT] = 
{
    [TOKEN_INTEGER_LITERAL] = { FactorIntLit,       NULL,           PREC_SINGLE },
    [TOKEN_IDENTIFIER]      = { FactorVariable,     NULL,           PREC_SINGLE },
    [TOKEN_LEFT_PAREN]      = { FactorGrouping,     NULL,           PREC_SINGLE },

    [TOKEN_STAR]            = { NULL,               ExprBinary,     PREC_TERM },
    [TOKEN_DIV]             = { NULL,               ExprBinary,     PREC_TERM },
    [TOKEN_MOD]             = { NULL,               ExprBinary,     PREC_TERM },

    [TOKEN_OR]              = { NULL,               ExprBinary,     PREC_SIMPLE },
    [TOKEN_PLUS]            = { ExprUnary,          ExprBinary,     PREC_SIMPLE },
    [TOKEN_MINUS]           = { ExprUnary,          ExprBinary,     PREC_SIMPLE },

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
    return ParsePrecedence(Compiler, PREC_EXPR);
}

static void CompileExprInto(PVMCompiler *Compiler, VarLocation *Location)
{
    VarLocation Expr = ParsePrecedence(Compiler, PREC_EXPR);
    CoerceTypes(Compiler, NULL, TypeOfLocation(Location), TypeOfLocation(&Expr));
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

            EMITTER()->ReturnValue.As.Register.Type = TYPE_I32;
            CompileExprInto(Compiler, &EMITTER()->ReturnValue);
        }
        else if (Compiler->Frame[Compiler->Scope - 1].Current->HasReturnType)
        {
            /* Exit(expr), must have return type */
            EMITTER()->ReturnValue.As.Register.Type = Compiler->Frame[Compiler->Scope - 1].Current->ReturnType;
            CompileExprInto(Compiler, &EMITTER()->ReturnValue);
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
    *Counter->Location = CompileExpr(Compiler);


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
    PASCAL_ASSERT(TYPE_FUNCTION == IdenInfo->Type, "Unreachable, IdenInfo is not a subroutine in CompileCallStmt()");

    /* iden consumed */
    CompilerInitDebugInfo(Compiler, Callee);

    VarSubroutine *Subroutine = &IdenInfo->Location->As.Subroutine;
    PVMEmitSaveCallerRegs(EMITTER(), NO_RETURN_REG);
    CompileArgumentList(Compiler, Callee, Subroutine);
    PVMEmitCall(EMITTER(), Subroutine);
    PVMEmitUnsaveCallerRegs(EMITTER());

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
        PascalVartab *PredefinedIdentifiers, PVMChunk *Chunk, PascalGPA *GlobalAlloc, FILE *LogFile)
{
    PVMCompiler Compiler = CompilerInit(Source, PredefinedIdentifiers, Chunk, GlobalAlloc, LogFile);
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

