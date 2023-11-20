

#include <stdarg.h>


#include "Memory.h"
#include "Tokenizer.h"
#include "Vartab.h"

#include "IntegralTypes.h"
#include "PVMCompiler.h"
#include "PVMEmitter.h"



typedef struct FunctionData 
{
    U32 ArgCount, Cap;
    Token *Args;
} FunctionData;

typedef struct PVMCompiler 
{
    PascalTokenizer Lexer;
    Token Curr, Next;

    U32 Scope;
    VarID VariableID;

    PascalVartab *Global;
    PascalVartab Locals[PVM_MAX_SCOPE_COUNT];
    PascalGPA Allocator;
    PascalArena Arena;

    struct {
        Token Array[8];
        U32 Count, Cap;
    } Iden;

    PVMEmitter Emitter;

    FILE *LogFile;
    bool Error, Panic;
} PVMCompiler;


static VarLocation sReturnRegister = {
    .LocationType = VAR_REG,
    .IntegralType = TYPE_INVALID, /* each call sets a diff type */
    .As.Reg.ID = PVM_REG_RET,
};

static VarLocation sArgumentRegister[PVM_REG_ARGCOUNT] = {
    [0] = {.LocationType = VAR_REG, .IntegralType = TYPE_INVALID, .As.Reg.ID = PVM_REG_ARG0 },
    [1] = {.LocationType = VAR_REG, .IntegralType = TYPE_INVALID, .As.Reg.ID = PVM_REG_ARG1 },
    [2] = {.LocationType = VAR_REG, .IntegralType = TYPE_INVALID, .As.Reg.ID = PVM_REG_ARG2 },
    [3] = {.LocationType = VAR_REG, .IntegralType = TYPE_INVALID, .As.Reg.ID = PVM_REG_ARG3 },
    [4] = {.LocationType = VAR_REG, .IntegralType = TYPE_INVALID, .As.Reg.ID = PVM_REG_ARG4 },
    [5] = {.LocationType = VAR_REG, .IntegralType = TYPE_INVALID, .As.Reg.ID = PVM_REG_ARG5 },
    [6] = {.LocationType = VAR_REG, .IntegralType = TYPE_INVALID, .As.Reg.ID = PVM_REG_ARG6 },
    [7] = {.LocationType = VAR_REG, .IntegralType = TYPE_INVALID, .As.Reg.ID = PVM_REG_ARG7 },
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



static PVMCompiler CompilerInit(const U8 *Source, CodeChunk *Chunk, FILE *LogFile)
{
    PVMCompiler Compiler = {
        .Lexer = TokenizerInit(Source),
        .LogFile = LogFile,
        .Error = false,
        .Panic = false,
        .Scope = 0,
        .VariableID = 0,

        .Allocator = GPAInit(
                PVM_MAX_SCOPE_COUNT * PVM_MAX_VAR_PER_SCOPE * sizeof(PascalVar)
        ),
        .Arena = ArenaInit(
                256 * sizeof(FunctionData),
                2
        ),
        .Iden = {
            .Cap = 8,
            .Count = 0,
        },
        .Emitter = PVMEmitterInit(Chunk),
    };
    return Compiler;
}

static void CompilerDeinit(PVMCompiler *Compiler)
{
    GPADeinit(&Compiler->Allocator);
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

static bool IsAtEnd(PVMCompiler *Compiler)
{
    return TOKEN_EOF == Compiler->Next.Type;
}

static bool IsAtGlobalScope(PVMCompiler *Compiler)
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
    while (!IsAtEnd(Compiler))
    {
        if (Compiler->Next.Type == TOKEN_SEMICOLON)
        {
            return;
        }
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
    int BeginEncountered = 0;
    while (!IsAtEnd(Compiler))
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
                return;

            case TOKEN_BEGIN:
            {
                if (BeginEncountered)
                    return;
                BeginEncountered++;
            } break;

            default: break;
            }
        }
        ConsumeToken(Compiler);
    }
}

static void CalmDownAtBlock(PVMCompiler *Compiler)
{
    Compiler->Panic = false;
    while (!IsAtEnd(Compiler))
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
                return;

            default: break;
            }
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
    Compiler->Scope--;
}


static PascalVartab *CurrentScope(PVMCompiler *Compiler)
{
    if (0 == Compiler->Scope)
    {
        return Compiler->Global;
    }
    else 
    {
        return &Compiler->Locals[Compiler->Scope - 1];
    }
}






static U32 DefineIdentifier(PVMCompiler *Compiler, const Token *TypeName, IntegralType Type, void *Data)
{
    U32 ID = Compiler->VariableID++;
    VartabSet(CurrentScope(Compiler), TypeName->Str, TypeName->Len, Type, ID, Data);
    return ID;
}


static PascalVar *GetIdenInfo(PVMCompiler *Compiler, const Token *Identifier, const char *ErrFmt, ...)
{
    U32 Hash = VartabHashStr(Identifier->Str, Identifier->Len);


    PascalVar *Info = VartabFindWithHash(CurrentScope(Compiler), 
            Identifier->Str, Identifier->Len, Hash
    );
    if (NULL == Info)
    {
        Info = VartabFindWithHash(Compiler->Global, 
                Identifier->Str, Identifier->Len, Hash
        );
    }

    if (NULL == Info)
    {
        for (int i = Compiler->Scope - 2; i >= 0; i--)
        {
            Info = VartabFindWithHash(&Compiler->Locals[i], 
                    Identifier->Str, Identifier->Len, Hash
            );
            if (NULL != Info) 
                return Info;
        }

        va_list ArgList;
        va_start(ArgList, ErrFmt);
        VaListError(Compiler, Identifier, ErrFmt, ArgList);
        va_end(ArgList);
        return NULL;
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


static void CompilerResetDataIdentifier(PVMCompiler *Compiler)
{
    Compiler->Iden.Count = 0;
}

static void CompilerPushDataIdentifier(PVMCompiler *Compiler, Token Identifier)
{
    if (Compiler->Iden.Count > Compiler->Iden.Cap)
    {
        PASCAL_UNREACHABLE("TODO: allocator");
    }
    Compiler->Iden.Array[Compiler->Iden.Count] = Identifier;
    Compiler->Iden.Count++;
}

static UInt CompilerGetDataIdentifierCount(const PVMCompiler *Compiler)
{
    return Compiler->Iden.Count;
}

static Token *CompilerGetDataIdentifier(PVMCompiler *Compiler, UInt Idx)
{
    return &Compiler->Iden.Array[Idx];
}

static U32 CompilerGetSizeOfType(PVMCompiler *Compiler, const PascalVar *TypeInfo)
{
    switch (TypeInfo->Type)
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
}


/* returns the size of the whole var list */
static U32 CompileAndDeclareVarList(PVMCompiler *Compiler)
{
    /* assumes next token is id1 */
    /* 
     *  id1, id2: typename
     */
    U32 Size = 0;

    CompilerResetDataIdentifier(Compiler);
    do {
        ConsumeOrError(Compiler, TOKEN_IDENTIFIER, "Expected variable name.");
        CompilerPushDataIdentifier(Compiler, Compiler->Curr);

        if (TOKEN_COLON == Compiler->Next.Type)
            break;
        ConsumeOrError(Compiler, TOKEN_COMMA, "Expected ',' or ':' after variable name.");
    } while (!IsAtEnd(Compiler));

    ConsumeOrError(Compiler, TOKEN_COLON, "Expected ':' before type name.");
    ConsumeOrError(Compiler, TOKEN_IDENTIFIER, "Expected type name after ':'");

    /* next token is now the type name */
    PascalVar *TypeInfo = GetIdenInfo(Compiler, &Compiler->Curr, "Undefined type name.");

    /* define them */
    UInt VarCount = CompilerGetDataIdentifierCount(Compiler);
    for (UInt i = 0; i < VarCount; i++)
    {
        /* TODO: what kind of data does a var have */
        DefineIdentifier(Compiler, 
                CompilerGetDataIdentifier(Compiler, i),
                TypeInfo->Type, 
                NULL
        );
    }

    Size = CompilerGetSizeOfType(Compiler, TypeInfo);
    return Size * VarCount;
}



static IntegralType ParserCoerceTypes(PVMCompiler *Compiler, const Token *Op, IntegralType Left, IntegralType Right)
{
    PASCAL_ASSERT(Left >= TYPE_INVALID && Right >= TYPE_INVALID, "Unreachable");
    if (Left > TYPE_COUNT || Right > TYPE_COUNT)
    {
        PASCAL_UNREACHABLE("Invalid types");
    }

    ParserType Type = sCoercionRules[Left][Right];
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


static bool IsTemporary(const VarLocation *Location)
{
    switch (Location->IntegralType)
    {
    case VAR_TMP_REG:
    case VAR_TMP_STK:
        return true;
    default: return false;
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
    PVMEmitLoad(&Compiler->Emitter, Into, 
            Compiler->Curr.Literal.Int, 
            TypeOfIntLit(Compiler->Curr.Literal.Int)
    );
}

static void FactorGrouping(PVMCompiler *Compiler, VarLocation *Into)
{
    ParsePrecedence(Compiler, PREC_EXPR, Into);
}

static void FactorCall(PVMCompiler *Compiler, VarLocation *Into)
{
    (void)Compiler, (void)Into;
    PASCAL_UNREACHABLE("TODO: call");
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
    VarLocation Right = PVMAllocateRegister(&Compiler->Emitter, Left->IntegralType);
    /* +1 to parse binary oper as left associative */
    ParsePrecedence(Compiler, OperatorRule->Prec + 1, &Right);

    switch (Operator)
    {
    case TOKEN_STAR:
    {
        PVMEmitMul(&Compiler->Emitter, Left, Left, &Right);
    } break;
    case TOKEN_DIV:
    {
        VarLocation Dummy = PVMAllocateRegister(&Compiler->Emitter, Left->IntegralType);
        PVMEmitDiv(&Compiler->Emitter, Left, &Dummy, Left, &Right);
        PVMFreeRegister(&Compiler->Emitter, &Dummy);
    } break;
    case TOKEN_MOD:
    {
        VarLocation Dummy = PVMAllocateRegister(&Compiler->Emitter, Left->IntegralType);
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
    [TOKEN_LEFT_PAREN]      = { FactorGrouping,     FactorCall,     PREC_SINGLE },

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

    VarLocation Tmp, *Target = Into;
    if (!IsTemporary(Into))
    {
        Tmp = PVMAllocateRegister(&Compiler->Emitter, Target->IntegralType);
        Target = &Tmp;
    }
    PrefixRoutine(Compiler, Target);


    /* only parse next op if they have the same or lower precedence */
    while (Prec <= GetPrecedenceRule(Compiler->Next.Type)->Prec)
    {
        ConsumeToken(Compiler); /* the operator */
        const ParseRoutine InfixRoutine = 
            GetPrecedenceRule(Compiler->Curr.Type)->InfixRoutine;

        PASCAL_ASSERT(NULL != InfixRoutine, "null routine in ParsePrecedence");

        InfixRoutine(Compiler, Target);
    }

    if (!IsTemporary(Into))
    {
        PVMEmitMov(&Compiler->Emitter, Into, Target);
        PVMFreeRegister(&Compiler->Emitter, Target);
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
    /* empty block */
    if (ConsumeIfNextIs(Compiler, TOKEN_END))
        return;

    /* begin consumed */
    while (!IsAtEnd(Compiler))
    {
        CompileStmt(Compiler);
        if (ConsumeIfNextIs(Compiler, TOKEN_END))
            break;
        ConsumeOrError(Compiler, TOKEN_SEMICOLON, "Expected ';' between statements.");
    }
}



static void CompileAssignStmt(PVMCompiler *Compiler)
{
    /* iden consumed */
    Token Identifier = Compiler->Curr;

    /* TODO: field access and array indexing */
    ConsumeOrError(Compiler, TOKEN_COLON_EQUAL, "TODO: other assignment operator.");

    PascalVar *IdenInfo = GetIdenInfo(Compiler, &Identifier, "Undefined assignment target");
    if (NULL != IdenInfo)
    {
        VarLocation *AssignmentTarget = PVMGetLocationOf(&Compiler->Emitter, IdenInfo->ID);
        CompileExpr(Compiler, AssignmentTarget);
    }
}

static void CompileCallStmt(PVMCompiler *Compiler)
{
    /* iden consumed */
    /* TODO: calling function from field access */

    /* 
     *  valid:      calling;
     *  valid:      calling();
     *  valid:      calling2(1, 2 + 3);
     */
    UInt i = 0;
    PascalVar *Proc = GetIdenInfo(Compiler, &Compiler->Curr, "Undefined procedure.");
    if (ConsumeIfNextIs(Compiler, TOKEN_LEFT_PAREN))
    {
        if (Compiler->Next.Type != TOKEN_RIGHT_PAREN)
        {
            do {

                if (i < PVM_REG_ARGCOUNT)
                {
                    CompileExpr(Compiler, &sArgumentRegister[i]);
                    i++;
                }
                else
                {
                    /* stack arguments */
                }

                if (TOKEN_RIGHT_PAREN == Compiler->Next.Type)
                    break;
                ConsumeOrError(Compiler, TOKEN_COMMA, "Expected ',' instead.");
            } while (!IsAtEnd(Compiler));
        }
        ConsumeOrError(Compiler, TOKEN_RIGHT_PAREN, "Expected ')' instead.");
    }
    /* TODO: emit code for function call */
}


static void CompileIdenStmt(PVMCompiler *Compiler)
{
    /* iden consumed */

    PascalVar *IdentifierInfo = GetIdenInfo(Compiler, &Compiler->Curr,
            "Undefined identifier."
    );
    if (NULL != IdentifierInfo && TYPE_FUNCTION == IdentifierInfo->Type)
    {
        CompileCallStmt(Compiler);
    }
    else
    {
        CompileAssignStmt(Compiler);
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
        PASCAL_UNREACHABLE("TODO: for");
    } break;
    case TOKEN_REPEAT:
    {
        ConsumeToken(Compiler);
        PASCAL_UNREACHABLE("TODO: repeat");
    } break;
    case TOKEN_WHILE:
    {
        ConsumeToken(Compiler);
        PASCAL_UNREACHABLE("TODO: while");
    } break;
    case TOKEN_CASE:
    {
        ConsumeToken(Compiler);
        PASCAL_UNREACHABLE("TODO: case");
    } break;
    case TOKEN_IF:
    {
        ConsumeToken(Compiler);
        PASCAL_UNREACHABLE("TODO: if");
    } break;
    case TOKEN_BEGIN:
    {
        ConsumeToken(Compiler);
        CompileBeginStmt(Compiler);
    } break;
    case TOKEN_SEMICOLON:
    {
        ConsumeToken(Compiler);
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


static void CompileBlock(PVMCompiler *Compiler);

static void CompileBeginBlock(PVMCompiler *Compiler)
{
    CompileBeginStmt(Compiler);
}


static const char *sFunction = "function";
static void CompileSubroutine(PVMCompiler *Compiler, const char *SubroutineType)
{
    /* 'function' consumed */
    ConsumeOrError(Compiler, TOKEN_IDENTIFIER, "Expected %s name.", SubroutineType);
    Token Name = Compiler->Curr;

    FunctionData *Data = 
    DefineIdentifier(Compiler, &Name, TYPE_FUNCTION, Data);

    CompilerBeginScope(Compiler);
    if (ConsumeIfNextIs(Compiler, TOKEN_LEFT_PAREN))
    {
        if (TOKEN_RIGHT_PAREN != Compiler->Next.Type)
        {
            while (!IsAtEnd(Compiler) && TOKEN_RIGHT_PAREN != Compiler->Next.Type)
            {
                CompileAndDeclareVarList(Compiler);
                if (TOKEN_RIGHT_PAREN == Compiler->Next.Type)
                    break;
                ConsumeOrError(Compiler, TOKEN_SEMICOLON, 
                        "Expected ';' between %s parameters.", SubroutineType
                );
            }
        }
        ConsumeOrError(Compiler, TOKEN_RIGHT_PAREN, "Expected ')' after parameter list.");
    }

    CompileBlock(Compiler);
    CompilerEndScope(Compiler);
}


static void CompileVarBlock(PVMCompiler *Compiler)
{
    /* 
     * var
     *      id1, id2: typename;
     *      id3: typename;
     */

    if (TOKEN_IDENTIFIER != Compiler->Next.Type)
    {
        Error(Compiler, "Var block must have at least 1 declaration.");
        return;
    }

    while (!IsAtEnd(Compiler) && TOKEN_IDENTIFIER == Compiler->Next.Type)
    {
        CompileAndDeclareVarList(Compiler);
        /* TODO: initialization */
        ConsumeOrError(Compiler, TOKEN_SEMICOLON, "Expected ';' after variable declaration.");
    }

    ConsumeOrError(Compiler, TOKEN_SEMICOLON, "Expected ';'");
}







static void CompileBlock(PVMCompiler *Compiler)
{
    while (!IsAtEnd(Compiler) && Compiler->Next.Type != TOKEN_BEGIN)
    {
        switch (Compiler->Next.Type)
        {
        case TOKEN_BEGIN: goto BeginEnd;
        case TOKEN_FUNCTION:
        {
            ConsumeToken(Compiler);
            CompileSubroutine(Compiler, sFunction);
            if (Compiler->Panic)
                CalmDownAtFunction(Compiler);
        } break;
        case TOKEN_PROCEDURE:
        {
            ConsumeToken(Compiler);
            CompileSubroutine(Compiler, "procedure");
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
    ConsumeOrError(Compiler, TOKEN_END, "Expected 'end' instead.");
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





bool PVMCompile(const U8 *Source, CodeChunk *Chunk, FILE *LogFile)
{
    PVMCompiler Compiler = CompilerInit(Source, Chunk, LogFile);
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

