

#include <stdarg.h>


#include "Memory.h"
#include "Tokenizer.h"
#include "Vartab.h"
#include "Variable.h"

#include "IntegralTypes.h"
#include "PVMCompiler.h"
#include "PVMEmitter.h"


typedef struct PVMCompiler 
{
    PascalTokenizer Lexer;
    Token Curr, Next;

    U32 Scope;

    PascalVartab *Global;
    PascalVartab Locals[PVM_MAX_SCOPE_COUNT];
    PascalGPA Allocator;
    PascalArena Arena;

    struct {
        /* TODO: dynamic */
        Token Array[8];
        U32 Count, Cap;
    } Iden;

    struct {
        VarLocation **Location;
        U32 Count, Cap;
    } Var;
    struct {
        /* TODO: dynamic */
        U32 Location[PVM_MAX_SCOPE_COUNT];
        U32 Count;
    } Frame;

    PVMEmitter Emitter;

    FILE *LogFile;
    bool Error, Panic;
} PVMCompiler;




/* TODO: put this in the emitter, the compiler should not worry about register level details */
static VarLocation sReturnRegister = {
    .LocationType = VAR_REG,
    .Type = TYPE_INVALID, /* each call sets a diff type */
    .As.Reg.ID = PVM_REG_RET,
};

static VarLocation sArgumentRegister[PVM_REG_ARGCOUNT] = {
    [0] = {.LocationType = VAR_REG, .Type = TYPE_INVALID, .As.Reg.ID = PVM_REG_ARG0 },
    [1] = {.LocationType = VAR_REG, .Type = TYPE_INVALID, .As.Reg.ID = PVM_REG_ARG1 },
    [2] = {.LocationType = VAR_REG, .Type = TYPE_INVALID, .As.Reg.ID = PVM_REG_ARG2 },
    [3] = {.LocationType = VAR_REG, .Type = TYPE_INVALID, .As.Reg.ID = PVM_REG_ARG3 },
    [4] = {.LocationType = VAR_REG, .Type = TYPE_INVALID, .As.Reg.ID = PVM_REG_ARG4 },
    [5] = {.LocationType = VAR_REG, .Type = TYPE_INVALID, .As.Reg.ID = PVM_REG_ARG5 },
    [6] = {.LocationType = VAR_REG, .Type = TYPE_INVALID, .As.Reg.ID = PVM_REG_ARG6 },
    [7] = {.LocationType = VAR_REG, .Type = TYPE_INVALID, .As.Reg.ID = PVM_REG_ARG7 },
};





static IntegralType sCoercionRules[TYPE_COUNT][TYPE_COUNT] = {
    /*Invalid       I8            I16           I32           I64           U8            U16           U32           U64           F32           F64           Function      Boolean*/
    { TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID},         /* Invalid */
    { TYPE_INVALID, TYPE_I32,     TYPE_I32,     TYPE_I32,     TYPE_I64,     TYPE_I32,     TYPE_I32,     TYPE_I32,     TYPE_I64,     TYPE_F32,     TYPE_F64,     TYPE_INVALID, TYPE_INVALID},         /* I8 */
    { TYPE_INVALID, TYPE_I32,     TYPE_I32,     TYPE_I32,     TYPE_I64,     TYPE_I32,     TYPE_I32,     TYPE_I32,     TYPE_I64,     TYPE_F32,     TYPE_F64,     TYPE_INVALID, TYPE_INVALID},         /* I16 */
    { TYPE_INVALID, TYPE_I32,     TYPE_I32,     TYPE_I32,     TYPE_I64,     TYPE_I32,     TYPE_I32,     TYPE_I32,     TYPE_I64,     TYPE_F32,     TYPE_F64,     TYPE_INVALID, TYPE_INVALID},         /* I32 */
    { TYPE_INVALID, TYPE_I64,     TYPE_I64,     TYPE_I64,     TYPE_I64,     TYPE_I64,     TYPE_I64,     TYPE_I64,     TYPE_I64,     TYPE_F32,     TYPE_F64,     TYPE_INVALID, TYPE_INVALID},         /* I64 */
    { TYPE_INVALID, TYPE_I32,     TYPE_I32,     TYPE_I32,     TYPE_I64,     TYPE_U32,     TYPE_U32,     TYPE_U32,     TYPE_U64,     TYPE_F32,     TYPE_F64,     TYPE_INVALID, TYPE_INVALID},         /* U8 */
    { TYPE_INVALID, TYPE_I32,     TYPE_I32,     TYPE_I32,     TYPE_I64,     TYPE_U32,     TYPE_U32,     TYPE_U32,     TYPE_U64,     TYPE_F32,     TYPE_F64,     TYPE_INVALID, TYPE_INVALID},         /* U16 */
    { TYPE_INVALID, TYPE_I64,     TYPE_I64,     TYPE_I64,     TYPE_I64,     TYPE_U32,     TYPE_U32,     TYPE_U32,     TYPE_U64,     TYPE_F32,     TYPE_F64,     TYPE_INVALID, TYPE_INVALID},         /* U32 */
    { TYPE_INVALID, TYPE_U64,     TYPE_U64,     TYPE_U64,     TYPE_U64,     TYPE_U64,     TYPE_U64,     TYPE_U64,     TYPE_U64,     TYPE_F32,     TYPE_F64,     TYPE_INVALID, TYPE_INVALID},         /* U64 */
    { TYPE_INVALID, TYPE_F32,     TYPE_F32,     TYPE_F32,     TYPE_F32,     TYPE_F32,     TYPE_F32,     TYPE_F32,     TYPE_F32,     TYPE_F32,     TYPE_F64,     TYPE_INVALID, TYPE_INVALID},         /* F32 */
    { TYPE_INVALID, TYPE_F64,     TYPE_F64,     TYPE_F64,     TYPE_F64,     TYPE_F64,     TYPE_F64,     TYPE_F64,     TYPE_F64,     TYPE_F64,     TYPE_F64,     TYPE_INVALID, TYPE_INVALID},         /* F64 */
    { TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID},         /* Function */
    { TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_BOOLEAN},         /* Invalid */
};










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


static U32 StrlenDelim(const U8 *s, char Delim)
{
    U32 i = 0;
    for (; s[i] && s[i] != Delim; i++)
    {}
    return i;
}



static PVMCompiler CompilerInit(const U8 *Source, 
        PascalVartab *PredefinedIdentifiers, CodeChunk *Chunk, FILE *LogFile)
{
    PVMCompiler Compiler = {
        .Lexer = TokenizerInit(Source),
        .LogFile = LogFile,
        .Error = false,
        .Panic = false,
        .Scope = 0,

        .Allocator = GPAInit(
                PVM_MAX_SCOPE_COUNT * PVM_MAX_VAR_PER_SCOPE * sizeof(PascalVar)
                + 4096 * 4096
        ),
        .Arena = ArenaInit(
                4096 * 4096,
                2
        ),
        .Var = {
            .Cap = 256,
            .Count = 0,
        },
        .Global = PredefinedIdentifiers,
        .Iden = {
            .Cap = 8,
            .Count = 0,
        },
        .Emitter = PVMEmitterInit(Chunk),
    };

    Compiler.Var.Location = GPAAllocate(&Compiler.Allocator, sizeof(VarLocation *) * Compiler.Var.Cap);
    for (U32 i = 0; i < Compiler.Var.Cap; i++)
    {
        Compiler.Var.Location[i] = ArenaAllocateZero(&Compiler.Arena, sizeof Compiler.Var.Location[0][0]);
    }
    return Compiler;
}

static void CompilerDeinit(PVMCompiler *Compiler)
{
    PVMEmitterDeinit(&Compiler->Emitter);
    GPADeinit(&Compiler->Allocator);
    ArenaDeinit(&Compiler->Arena);
}



static void CompilerStackAllocateFrame(PVMCompiler *Compiler)
{
    Compiler->Frame.Location[Compiler->Frame.Count] = Compiler->Var.Count;
    Compiler->Frame.Count++;
}

static void CompilerStackDeallocateFrame(PVMCompiler *Compiler)
{
    Compiler->Frame.Count--;
}


static VarLocation *CompilerStackAlloc(PVMCompiler *Compiler)
{
    if (Compiler->Var.Count >= Compiler->Var.Cap)
    {
        U32 OldCap = Compiler->Var.Cap;
        Compiler->Var.Cap *= 2;
        Compiler->Var.Location = GPAReallocate(&Compiler->Allocator, Compiler->Var.Location, Compiler->Var.Cap);
        for (U32 i = OldCap; i < Compiler->Var.Cap; i++)
        {
            Compiler->Var.Location[i] = ArenaAllocateZero(&Compiler->Arena, sizeof Compiler->Var.Location[0][0]);
        }
    }
    return Compiler->Var.Location[Compiler->Var.Count++];
}







static void ConsumeToken(PVMCompiler *Compiler)
{
    Compiler->Curr = Compiler->Next;
    Compiler->Next = TokenizerGetToken(&Compiler->Lexer);
}

static bool ConsumeIfNextIs(PVMCompiler *Compiler, TokenType Type)
{
    if (Type == Compiler->Next.Type)
    {
        ConsumeToken(Compiler);
        return true;
    }
    return false;
}

static bool IsAtEnd(const PVMCompiler *Compiler)
{
    return TOKEN_EOF == Compiler->Next.Type;
}

static bool IsAtStmtEnd(const PVMCompiler *Compiler)
{
    return IsAtEnd(Compiler) || TOKEN_SEMICOLON == Compiler->Next.Type;
}

static bool IsAtGlobalScope(const PVMCompiler *Compiler)
{
    return 0 == Compiler->Scope;
}





/*===============================================================================*/
/*
 *                                   ERROR
 */
/*===============================================================================*/



static void PrintAndHighlightSource(FILE *LogFile, const Token *Tok)
{
    const U8 *LineStart = Tok->Str - Tok->LineOffset + 1;
    char Highlighter = '^';
    U32 Len = StrlenDelim(LineStart, '\n');

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

        fprintf(Compiler->LogFile, "\nError: ");
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

static void CalmDownAtFunction(PVMCompiler *Compiler)
{
    Compiler->Panic = false;
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

static void CalmDownAtBlock(PVMCompiler *Compiler)
{
    Compiler->Panic = false;
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



static void BeginScope(PVMCompiler *Compiler)
{
    /* mark allocation point */
    CompilerStackAllocateFrame(Compiler);

    /* creates new variable table */
    PascalVartab *NewScope = &Compiler->Locals[Compiler->Scope++];
    PASCAL_ASSERT(Compiler->Scope <= PVM_MAX_SCOPE_COUNT, "Too many scopes");

    if (0 == NewScope->Cap)
    {
        *NewScope = VartabInit(&Compiler->Allocator, PVM_MAX_VAR_PER_SCOPE);
    }
    else
    {
        VartabReset(NewScope);
    }
}

static void EndScope(PVMCompiler *Compiler)
{
    CompilerStackDeallocateFrame(Compiler);
    Compiler->Scope--;
}


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






static PascalVar *DefineIdentifier(PVMCompiler *Compiler, const Token *Identifier, IntegralType Type, VarLocation *Location)
{
    return VartabSet(CurrentScope(Compiler), Identifier->Str, Identifier->Len, Type, Location);
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
        ErrorAt(Compiler, Op, "Invalid combination of type %s and %s", 
                IntegralTypeToStr(Left), IntegralTypeToStr(Right)
        );
        return TYPE_INVALID;
    }

    switch (Op->Type)
    {
    case TOKEN_LESS:
    case TOKEN_GREATER:
    case TOKEN_LESS_EQUAL:
    case TOKEN_GREATER_EQUAL:
    case TOKEN_LESS_GREATER:
    case TOKEN_EQUAL:
    case TOKEN_DOWNTO:
    case TOKEN_TO:
    {
        if (TYPE_INVALID == Type)
            return TYPE_INVALID;
        else 
            return TYPE_BOOLEAN;
    } break;
    default: return Type;
    }
}






/*===============================================================================*/
/*
 *                                  HELPERS
 */
/*===============================================================================*/

static void CompilerInitDebugInfo(PVMCompiler *Compiler, const Token *From)
{
    PVMEmitDebugInfo(&Compiler->Emitter, From->Str, From->Line);
}

static void CompilerEmitDebugInfo(PVMCompiler *Compiler, const Token *From)
{
    U32 LineLen = Compiler->Curr.Str - From->Str + Compiler->Curr.Len;
    PVMUpdateDebugInfo(&Compiler->Emitter, LineLen);
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



static void FunctionDataPushParameter(PascalGPA *Allocator, FunctionVar *Function, const U8 *Str, UInt Len, IntegralType Type)
{
    if (Function->ArgCount == Function->Cap)
    {
        Function->Cap = Function->Cap * 2 + 8;
        Function->Args = GPAReallocate(Allocator, Function->Args, Function->Cap);
    }

    Function->Args[Function->ArgCount++] = (NameAndType) {
        .Name = Str,
        .Len = Len,
        .Type = Type,
    };
}





static Token *CompilerGetTmpIdentifier(PVMCompiler *Compiler, UInt Idx)
{
    return &Compiler->Iden.Array[Idx];
}


/* returns the size of the whole var list */
static U32 CompileAndDeclareVarList(PVMCompiler *Compiler, FunctionVar *Function)
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
        return 0;
    if (!ConsumeOrError(Compiler, TOKEN_IDENTIFIER, "Expected type name after ':'"))
        return 0;


    /* next token is now the type name */
    IntegralType Type = TYPE_INVALID;
    PascalVar *TypeInfo = GetIdenInfo(Compiler, &Compiler->Curr, "Undefined type name.");
    if (NULL != TypeInfo)
        Type = TypeInfo->Type;
    U32 Size = CompilerGetSizeOfType(Compiler, Type);
    UInt VarCount = CompilerGetTmpIdentifierCount(Compiler);


    /* TODO: clean up this piece of scheisse of a function */
    /* define them */
    for (UInt i = 0; i < VarCount; i++)
    {
        VarLocation *Location = CompilerStackAlloc(Compiler);
        Location->Type = Type;
        PascalVar *Var = DefineIdentifier(Compiler, 
                CompilerGetTmpIdentifier(Compiler, i),
                Type,
                Location
        );

        if (NULL != Function)
        {
            FunctionDataPushParameter(&Compiler->Allocator, Function, 
                    Var->Str, Var->Len, Var->Type
            );

            if (Function->ArgCount < PVM_REG_ARGCOUNT)
            {
                Location->LocationType = VAR_REG;
                Location->As.Reg.ID = sArgumentRegister[Function->ArgCount].As.Reg.ID;
            }
            else
            {
                PASCAL_UNREACHABLE("TODO: param list for function");
                Location->LocationType = VAR_LOCAL;
            }
        }
        else if (IsAtGlobalScope(Compiler))
        {
            Location->LocationType = VAR_GLOBAL;
            Location->As.Global = PVMEmitGlobalSpace(&Compiler->Emitter, CompilerGetSizeOfType(Compiler, Type));
        }
        else
        {
            Location->LocationType = VAR_LOCAL;
            Location->As.Local = (LocalVar) {
                .Size = Size,
                .FPOffset = 0,
            };
        }
    }
    return Size * VarCount;
}



static void CompileParameterList(PVMCompiler *Compiler, FunctionVar *CurrentFunction)
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


static void CompileExpr(PVMCompiler *Compiler, VarLocation *Location);
static void CompileArgumentList(PVMCompiler *Compiler, const Token *FunctionName, FunctionVar *Subroutine, const char *SubroutineType)
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
        if (ArgCount < ExpectedArgCount)
        {
            sArgumentRegister[ArgCount].Type = Subroutine->Args[ArgCount].Type;
        }
        CompileExpr(Compiler, &sArgumentRegister[ArgCount]);
        ArgCount++;
        if (TOKEN_RIGHT_PAREN == Compiler->Next.Type)
            break;
        else
            ConsumeOrError(Compiler, TOKEN_COMMA, "Expected ',' between arguments of %s", SubroutineType);
    } while (!IsAtEnd(Compiler) && ArgCount < PVM_REG_ARGCOUNT);

    /* stack args */
    while (!IsAtEnd(Compiler) && TOKEN_RIGHT_PAREN != Compiler->Next.Type)
    {
        PASCAL_UNREACHABLE("TODO: stack args");
    }

    /* proper english */
    ConsumeOrError(Compiler, TOKEN_RIGHT_PAREN, "Expected ')' after argument list.");

CheckArgs:
    if (ArgCount != ExpectedArgCount)
    {
        ErrorAt(Compiler, FunctionName, "Expected %d arguments but got %d instead.", Subroutine->ArgCount, ArgCount);
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
typedef void (*ParseRoutine)(PVMCompiler *, VarLocation *);

typedef struct PrecedenceRule
{
    ParseRoutine PrefixRoutine;
    ParseRoutine InfixRoutine;
    Precedence Prec;
} PrecedenceRule;

static const PrecedenceRule *GetPrecedenceRule(TokenType CurrentTokenType);
static void ParsePrecedence(PVMCompiler *Compiler, Precedence Prec, VarLocation *Into);



static void FactorIntLit(PVMCompiler *Compiler, VarLocation *Into)
{
    /* literal consumed */
    PVMEmitLoad(&Compiler->Emitter, Into, 
            Compiler->Curr.Literal.Int, 
            TypeOfIntLit(Compiler->Curr.Literal.Int)
    );
}

static void FactorVariable(PVMCompiler *Compiler, VarLocation *Into)
{
    /* identifier consumed */
    Token Identifier = Compiler->Curr;
    PascalVar *Variable = GetIdenInfo(Compiler, &Identifier, "Undefined variable.");

    if (TYPE_FUNCTION == Variable->Type)
    {
        PASCAL_ASSERT(Variable->Location->Type == TYPE_FUNCTION, "wtf??");
        PASCAL_ASSERT(Variable->Location->LocationType == VAR_FUNCTION, "wtf????");

        FunctionVar *Subroutine = &Variable->Location->As.Function;
        CompileArgumentList(Compiler, &Identifier, Subroutine, "function");
        if (!Subroutine->HasReturnType)
        {
            ErrorAt(Compiler, &Identifier, "Procedure does not have a return value.");
        }
        else
        {
            PVMEmitCall(&Compiler->Emitter, Subroutine);
            sReturnRegister.Type = Subroutine->ReturnType;
            PVMEmitMov(&Compiler->Emitter, Into, &sReturnRegister);
        }
    }
    else
    {
        PVMEmitMov(&Compiler->Emitter, Into, Variable->Location);
    }
}

static void FactorGrouping(PVMCompiler *Compiler, VarLocation *Into)
{
    /* '(' consumed */
    ParsePrecedence(Compiler, PREC_EXPR, Into);
    ConsumeOrError(Compiler, TOKEN_RIGHT_PAREN, "Expected ')' after expression.");
}


static void ExprUnary(PVMCompiler *Compiler, VarLocation *Into)
{
    TokenType Operator = Compiler->Curr.Type;
    ParsePrecedence(Compiler, PREC_SIMPLE, Into);

    switch (Operator)
    {
    case TOKEN_PLUS: break;
    case TOKEN_MINUS:
    {
        PASCAL_UNREACHABLE("TODO: Neg instruction");
        //PVMEmitNeg(&Compiler->Emitter, Into, Into);
    } break;
    default: PASCAL_UNREACHABLE("Invalid operator for unary"); break;
    }
}

static void ExprBinary(PVMCompiler *Compiler, VarLocation *Left)
{
    TokenType Operator = Compiler->Curr.Type;
    const PrecedenceRule *OperatorRule = GetPrecedenceRule(Compiler->Curr.Type);

    /* TODO: typecheck */
    VarLocation Right = PVMAllocateRegister(&Compiler->Emitter, Left->Type);

    /* +1 to parse binary oper as left associative,
     * ex:  parses  1 + 2 + 3 
     *      as      ((1 + 2) + 3)
     *      not     (1 + (2 + 3)) 
     */
    ParsePrecedence(Compiler, OperatorRule->Prec + 1, &Right);

    switch (Operator)
    {
    case TOKEN_STAR:
    {
        PVMEmitMul(&Compiler->Emitter, Left, Left, &Right);
    } break;
    case TOKEN_DIV:
    {
        VarLocation Dummy = PVMAllocateRegister(&Compiler->Emitter, Left->Type);
        PVMEmitDiv(&Compiler->Emitter, Left, &Dummy, Left, &Right);
        PVMFreeRegister(&Compiler->Emitter, &Dummy);
    } break;
    case TOKEN_MOD:
    {
        VarLocation Dummy = PVMAllocateRegister(&Compiler->Emitter, Left->Type);
        PVMEmitDiv(&Compiler->Emitter, &Dummy, Left, Left, &Right);
        PVMFreeRegister(&Compiler->Emitter, &Dummy);
    } break;

    case TOKEN_PLUS:
    {
        PVMEmitAdd(&Compiler->Emitter, Left, Left, &Right);
    } break;
    case TOKEN_MINUS:
    {
        PVMEmitSub(&Compiler->Emitter, Left, Left, &Right);
    } break;

    case TOKEN_LESS:
    case TOKEN_GREATER:
    case TOKEN_LESS_EQUAL:
    case TOKEN_GREATER_EQUAL:
    case TOKEN_EQUAL:
    case TOKEN_LESS_GREATER:
    {
        PVMEmitSetCC(&Compiler->Emitter, Operator, Left, Left, &Right);
    } break;

    case TOKEN_AND:
    case TOKEN_OR:

    default: PASCAL_UNREACHABLE("Operator %s is unreachable in ExprBinary", TokenTypeToStr(Operator)); break;
    }

    PVMFreeRegister(&Compiler->Emitter, &Right);
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


static void ParsePrecedence(PVMCompiler *Compiler, Precedence Prec, VarLocation *Into)
{
    ConsumeToken(Compiler);
    const ParseRoutine PrefixRoutine = GetPrecedenceRule(Compiler->Curr.Type)->PrefixRoutine;
    if (NULL == PrefixRoutine)
    {
        Error(Compiler, "Expected expression.");
        return;
    }

    VarLocation Target;
    bool IsOwning = PVMEmitIntoReg(&Compiler->Emitter, &Target, Into);
    PrefixRoutine(Compiler, &Target);


    /* only parse next op if they have the same or lower precedence */
    while (Prec <= GetPrecedenceRule(Compiler->Next.Type)->Prec)
    {
        ConsumeToken(Compiler); /* the operator */
        const ParseRoutine InfixRoutine = 
            GetPrecedenceRule(Compiler->Curr.Type)->InfixRoutine;

        PASCAL_ASSERT(NULL != InfixRoutine, "NULL infix routine in ParsePrecedence()");

        InfixRoutine(Compiler, &Target);
    }

    if (IsOwning)
    {
        PVMEmitMov(&Compiler->Emitter, Into, &Target);
        PVMFreeRegister(&Compiler->Emitter, &Target);
    }
}


static void CompileExpr(PVMCompiler *Compiler, VarLocation *Into)
{
    ParsePrecedence(Compiler, PREC_EXPR, Into);
}







/*===============================================================================*/
/*
 *                                   STATEMENT
 */
/*===============================================================================*/


static void CompileStmt(PVMCompiler *Compiler);


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
    ConsumeOrError(Compiler, TOKEN_END, "Expected 'end' after statement.");
}


static void CompileForStmt(PVMCompiler *Compiler)
{
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
    *Counter->Location = PVMAllocateRegister(&Compiler->Emitter, Counter->Type);

    
    /* init expression */
    ConsumeOrError(Compiler, TOKEN_COLON_EQUAL, "Expected ':=' after variable name.");
    CompileExpr(Compiler, Counter->Location);


    /* for loop inc/dec */
    U32 LoopHead = PVMMarkBranchTarget(&Compiler->Emitter);
    VarLocation StopCondition = PVMAllocateRegister(&Compiler->Emitter, Counter->Location->Type);
    TokenType Op = TOKEN_LESS;
    I16 Inc = 1;
    if (!ConsumeIfNextIs(Compiler, TOKEN_TO))
    {
        ConsumeOrError(Compiler, TOKEN_DOWNTO, "Expected 'to' or 'downto' after expression.");
        Op = TOKEN_GREATER;
        Inc = -1;
    }
    CompileExpr(Compiler, &StopCondition);
    PVMEmitSetCC(&Compiler->Emitter, Op, &StopCondition, Counter->Location, &StopCondition);
    U32 LoopExit = PVMEmitBranchIfFalse(&Compiler->Emitter, &StopCondition);
    PVMFreeRegister(&Compiler->Emitter, &StopCondition);

    ConsumeOrError(Compiler, TOKEN_DO, "Expected 'do' after expression.");
    CompilerEmitDebugInfo(Compiler, &Keyword);


    /* loop body */
    CompileStmt(Compiler);


    /* loop increment */
    PVMEmitAddImm(&Compiler->Emitter, Counter->Location, Inc);
    PVMEmitBranch(&Compiler->Emitter, LoopHead);
    PVMPatchBranchToCurrent(&Compiler->Emitter, LoopExit, PVM_CONDITIONAL_BRANCH);

    /* TODO: unhack this */
    PVMFreeRegister(&Compiler->Emitter, Counter->Location);
    *Counter->Location = Save;
}


static void CompileWhileStmt(PVMCompiler *Compiler)
{
    /* 'while' consumed */
    Token Keyword = Compiler->Curr;
    CompilerInitDebugInfo(Compiler, &Keyword);

    /* condition expression */
    /* TODO: should be TYPE_BOOLEAN */
    VarLocation Tmp = PVMAllocateRegister(&Compiler->Emitter, TYPE_U32);
    U32 LoopHead = PVMMarkBranchTarget(&Compiler->Emitter);
        CompileExpr(Compiler, &Tmp);
    U32 LoopExit = PVMEmitBranchIfFalse(&Compiler->Emitter, &Tmp);
    PVMFreeRegister(&Compiler->Emitter, &Tmp);

    ConsumeOrError(Compiler, TOKEN_DO, "Expected 'do' after expression.");
    CompilerEmitDebugInfo(Compiler, &Keyword);


    /* loop body */
        CompileStmt(Compiler);

    /* back to loophead */
    PVMEmitBranch(&Compiler->Emitter, LoopHead);


    /* patch the exit branch */
    PVMPatchBranchToCurrent(&Compiler->Emitter, LoopExit, PVM_CONDITIONAL_BRANCH);
}


static void CompileIfStmt(PVMCompiler *Compiler)
{
    /* 'if' consumed */
    Token Keyword = Compiler->Curr;
    CompilerInitDebugInfo(Compiler, &Keyword);
    

    /* condition expression */
    /* TODO: should be TYPE_BOOLEAN */
    VarLocation Tmp = PVMAllocateRegister(&Compiler->Emitter, TYPE_U32);
    CompileExpr(Compiler, &Tmp);
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

    U32 FromIf = PVMEmitBranchIfFalse(&Compiler->Emitter, &Tmp);
    PVMFreeRegister(&Compiler->Emitter, &Tmp);
        CompileStmt(Compiler);
    if (TOKEN_ELSE == Compiler->Next.Type)
    {
        CompilerInitDebugInfo(Compiler, &Compiler->Next);
        ConsumeToken(Compiler);
        CompilerEmitDebugInfo(Compiler, &Compiler->Curr);

        U32 FromEndIf = PVMEmitBranch(&Compiler->Emitter, 0);
        CompileStmt(Compiler);
        PVMPatchBranchToCurrent(&Compiler->Emitter, FromEndIf, PVM_UNCONDITIONAL_BRANCH);
    }
    PVMPatchBranchToCurrent(&Compiler->Emitter, FromIf, PVM_CONDITIONAL_BRANCH);
}




static void CompileCallStmt(PVMCompiler *Compiler, const Token *Callee, PascalVar *IdenInfo)
{
    PASCAL_ASSERT(NULL != IdenInfo, "Unreachable, IdenInfo is NULL in CompileCallStmt()");
    PASCAL_ASSERT(NULL != IdenInfo->Location, "Unreachable, Location is NULL in CompileCallStmt()");
    PASCAL_ASSERT(TYPE_FUNCTION == IdenInfo->Type, "Unreachable, IdenInfo is not a subroutine in CompileCallStmt()");

    /* iden consumed */
    CompilerInitDebugInfo(Compiler, Callee);
    CompileArgumentList(Compiler, Callee, &IdenInfo->Location->As.Function, 
            IdenInfo->Location->As.Function.HasReturnType?
            "function" : "procedure"
    );
    PVMEmitCall(&Compiler->Emitter, &IdenInfo->Location->As.Function);
    CompilerEmitDebugInfo(Compiler, Callee);
}


static void CompileAssignStmt(PVMCompiler *Compiler, PascalVar *IdenInfo)
{
    /* iden consumed */
    Token Identifier = Compiler->Curr;
    CompilerInitDebugInfo(Compiler, &Identifier);

    /* TODO: field access and array indexing */
    ConsumeOrError(Compiler, TOKEN_COLON_EQUAL, "TODO: other assignment operator.");
    Token Op = Compiler->Curr;
    if (NULL != IdenInfo)
    {
        IntegralType Type = IdenInfo->Type;
        PASCAL_ASSERT(NULL != IdenInfo->Location, "Location must be valid");

        CompileExpr(Compiler, IdenInfo->Location);

        /* typecheck in here */
        CoerceTypes(Compiler, &Op, Type, IdenInfo->Type);
        IdenInfo->Location->Type = Type;
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
        PASCAL_UNREACHABLE("TODO: repeat");
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
    CompileBeginStmt(Compiler);
}


static void CompileSubroutineBlock(PVMCompiler *Compiler, const char *SubroutineType)
{
    /* 'function', 'procedure' consumed */
    VarLocation *Var = CompilerStackAlloc(Compiler);
    Var->LocationType = VAR_FUNCTION;
    Var->Type = TYPE_FUNCTION;
    Var->As.Function = (FunctionVar) {
        .HasReturnType = TOKEN_FUNCTION == Compiler->Curr.Type,
        .Location = PVMGetCurrentLocation(&Compiler->Emitter),
    };
    FunctionVar *Subroutine = &Var->As.Function;


    /* function/proc name */
    ConsumeOrError(Compiler, TOKEN_IDENTIFIER, "Expected %s name.", SubroutineType);
    PascalVar *FunctionInfo = DefineIdentifier(Compiler, &Compiler->Curr, TYPE_FUNCTION, Var);
    PASCAL_ASSERT(FunctionInfo->Location == Var, "Unreachable");


    /* param list */
    BeginScope(Compiler);
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
        ConsumeOrError(Compiler, TOKEN_SEMICOLON, "Expected ';' after parameter list.");
    }


    /* body */
    CompileBlock(Compiler);
    EndScope(Compiler);

    Token End = Compiler->Curr;
    CompilerInitDebugInfo(Compiler, &End);
    ConsumeOrError(Compiler, TOKEN_SEMICOLON, "Expected ';' after %s body.", SubroutineType);
    CompilerEmitDebugInfo(Compiler, &End);
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

    if (!IsAtGlobalScope(Compiler))
    {
        CompilerInitDebugInfo(Compiler, &Keyword);
    }

    USize TotalSize = 0;
    while (!IsAtEnd(Compiler) && TOKEN_IDENTIFIER == Compiler->Next.Type)
    {
        TotalSize += CompileAndDeclareVarList(Compiler, NULL);
        /* TODO: initialization */
        ConsumeOrError(Compiler, TOKEN_SEMICOLON, "Expected ';' after variable declaration.");
    }

    if (!IsAtGlobalScope(Compiler))
    {
        CompilerEmitDebugInfo(Compiler, &Keyword);

        /* TODO: arch change */
        U32 StackSize = TotalSize / sizeof(PVMPtr);
        if (TotalSize % sizeof(PVMPtr))
            StackSize += 1;
        PVMAllocateStackSpace(&Compiler->Emitter, StackSize);
    }
}







static bool CompileBlock(PVMCompiler *Compiler)
{
    while (!IsAtEnd(Compiler) && Compiler->Next.Type != TOKEN_BEGIN)
    {
        switch (Compiler->Next.Type)
        {
        case TOKEN_BEGIN: goto BeginEnd;
        case TOKEN_FUNCTION:
        {
            ConsumeToken(Compiler);
            CompileSubroutineBlock(Compiler, "function");
            if (Compiler->Panic)
                CalmDownAtFunction(Compiler);
        } break;
        case TOKEN_PROCEDURE:
        {
            ConsumeToken(Compiler);
            CompileSubroutineBlock(Compiler, "procedure");
            if (Compiler->Panic)
                CalmDownAtFunction(Compiler);
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

BeginEnd:
    ConsumeOrError(Compiler, TOKEN_BEGIN, "Expected 'begin' instead.");
    CompileBeginBlock(Compiler);
    return !Compiler->Error;
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





bool PVMCompile(const U8 *Source, PascalVartab *PredefinedIdentifiers, CodeChunk *Chunk, FILE *LogFile)
{
    PVMCompiler Compiler = CompilerInit(Source, PredefinedIdentifiers, Chunk, LogFile);
    ConsumeToken(&Compiler);

    if (ConsumeIfNextIs(&Compiler, TOKEN_PROGRAM))
    {
        if (!CompileProgram(&Compiler))
            goto CompileError;
    }
    else if (!CompileBlock(&Compiler))
    {
        goto CompileError;
    }


    CompilerDeinit(&Compiler);
    return true;
CompileError:
    CompilerDeinit(&Compiler);
    return false;
}

