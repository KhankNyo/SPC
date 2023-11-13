

#include <stdarg.h>
#include "Parser.h"






static ParserType sCoercionRules[TYPE_COUNT][TYPE_COUNT] = {
    /*Invalid       I8            I16           I32           I64           U8            U16           U32           U64           F32           F64           */
    { TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID },   /* Invalid */
    { TYPE_INVALID, TYPE_I32,     TYPE_I32,     TYPE_I32,     TYPE_I64,     TYPE_I32,     TYPE_I32,     TYPE_I64,     TYPE_I64,     TYPE_F32,     TYPE_F64 },       /* I8 */
    { TYPE_INVALID, TYPE_I32,     TYPE_I32,     TYPE_I32,     TYPE_I64,     TYPE_I32,     TYPE_I32,     TYPE_I64,     TYPE_I64,     TYPE_F32,     TYPE_F64 },       /* I16 */
    { TYPE_INVALID, TYPE_I32,     TYPE_I32,     TYPE_I32,     TYPE_I64,     TYPE_I32,     TYPE_I32,     TYPE_I64,     TYPE_I64,     TYPE_F32,     TYPE_F64 },       /* I32 */
    { TYPE_INVALID, TYPE_I64,     TYPE_I64,     TYPE_I64,     TYPE_I64,     TYPE_I64,     TYPE_I64,     TYPE_I64,     TYPE_I64,     TYPE_F32,     TYPE_F64 },       /* I64 */
    { TYPE_INVALID, TYPE_I32,     TYPE_I32,     TYPE_I32,     TYPE_I64,     TYPE_U32,     TYPE_U32,     TYPE_U64,     TYPE_U64,     TYPE_F32,     TYPE_F64 },       /* U8 */
    { TYPE_INVALID, TYPE_I32,     TYPE_I32,     TYPE_I32,     TYPE_I64,     TYPE_U32,     TYPE_U32,     TYPE_U64,     TYPE_U64,     TYPE_F32,     TYPE_F64 },       /* U16 */
    { TYPE_INVALID, TYPE_I64,     TYPE_I64,     TYPE_I64,     TYPE_I64,     TYPE_U32,     TYPE_U64,     TYPE_U64,     TYPE_U64,     TYPE_F32,     TYPE_F64 },       /* U32 */
    { TYPE_INVALID, TYPE_U64,     TYPE_U64,     TYPE_U64,     TYPE_U64,     TYPE_U64,     TYPE_U64,     TYPE_U64,     TYPE_U64,     TYPE_F32,     TYPE_F64 },       /* U64 */
    { TYPE_INVALID, TYPE_F32,     TYPE_F32,     TYPE_F32,     TYPE_F32,     TYPE_F32,     TYPE_F32,     TYPE_F32,     TYPE_F32,     TYPE_F32,     TYPE_F64 },       /* F32 */
    { TYPE_INVALID, TYPE_F64,     TYPE_F64,     TYPE_F64,     TYPE_F64,     TYPE_F64,     TYPE_F64,     TYPE_F64,     TYPE_F64,     TYPE_F64,     TYPE_F64 },       /* F64 */
};




static void ParseType(PascalParser *Parser);
static AstStmtBlock *ParseBeginEnd(PascalParser *Parser);
static AstVarBlock *ParseVar(PascalParser *Parser);
static AstAssignStmt *ParseAssignStmt(PascalParser *Parser);
static AstReturnStmt *ParseReturnStmt(PascalParser *Parser);
static AstFunctionBlock *ParseFunction(PascalParser *Parser);


static ParserType ParserTokenToVarType(PascalParser *Parser, const Token *Type);
static ParserType ParserLookupTypeOfName(PascalParser *Parser, const Token *Name);

static void ParserDefineType(PascalParser *Parser, const Token *TypeName);
static void ParserDefineVariables(PascalParser *Parser, const AstVarList *VarList);

static bool ParserTypeIsDefined(PascalParser *Parser, const Token *Type);
static bool ParserIdentifierIsDefined(PascalParser *Parser, const Token *Identifier);
static ParserType ParserCoerceTypes(PascalParser *Parser, ParserType Left, ParserType Right);


static AstSimpleExpr ParseSimpleExpr(PascalParser *Parser);
static AstTerm ParseTerm(PascalParser *Parser);
static AstFactor ParseFactor(PascalParser *Parser);


static bool IsAtEnd(const PascalParser *Parser);
static bool NextTokenIs(const PascalParser *Parser, const TokenType Type);
static bool ConsumeIfNextIsOneOf(PascalParser *Parser, UInt Count, const TokenType Types[]);
static bool ConsumeIfNextIs(PascalParser *Parser, TokenType Type);
static void ConsumeToken(PascalParser *Parser);
static bool ConsumeOrError(PascalParser *Parser, TokenType Expected, const char *Fmt, ...);

static void Error(PascalParser *Parser, const char *Fmt, ...);
static void VaListError(PascalParser *Parser, const char *Fmt, va_list VaList);
static void RecoverFromError(PascalParser *Parser);





PascalParser ParserInit(const U8 *Source, PascalArena *Arena, FILE *ErrorFile)
{
    PascalParser Parser = {
        .Lexer = TokenizerInit(Source),
        .Arena = Arena,
        .PanicMode = false,
        .Error = false,
        .ErrorFile = ErrorFile,
        .IdentifiersInScope = VartabInit(1024),
    };
    VartabSet(&Parser.IdentifiersInScope, (const U8*)"INTEGER", 7, TYPE_I16);
    VartabSet(&Parser.IdentifiersInScope, (const U8*)"REAL", 4, TYPE_F32);
    return Parser;
}


PascalAst *ParserGenerateAst(PascalParser *Parser)
{
    Parser->Next = TokenizerGetToken(&Parser->Lexer);
    PascalAst *Ast = ArenaAllocate(Parser->Arena, sizeof(*Ast));
    Ast->Block = ParseBlock(Parser);

    VartabDeinit(&Parser->IdentifiersInScope);

    if (Parser->Error)
    {
        ParserDestroyAst(Ast);
        return NULL;
    }

    return Ast;
}

void ParserDestroyAst(PascalAst *Ast)
{
    /* The arena owns the Ast, no need to do anything here */
    (void)Ast;
}









AstBlock *ParseBlock(PascalParser *Parser)
{
    AstBlock *Block = NULL;
    if (ConsumeIfNextIs(Parser, TOKEN_FUNCTION))
    {
        Block = (AstBlock*)ParseFunction(Parser);
    }
    else if (ConsumeIfNextIs(Parser, TOKEN_PROCEDURE))
    {
        PASCAL_UNREACHABLE("TODO: ParseBlock: TOKEN_PROCEDURE");
    }
    else if (ConsumeIfNextIs(Parser, TOKEN_VAR))
    {
        Block = (AstBlock*)ParseVar(Parser);
    }
    else if (ConsumeIfNextIs(Parser, TOKEN_LABEL))
    {
        PASCAL_UNREACHABLE("TODO: ParserBlock: TOKEN_LABEL");
    }
    else if (ConsumeIfNextIs(Parser, TOKEN_TYPE))
    {
        ParseType(Parser);
    }
    else if (ConsumeIfNextIs(Parser, TOKEN_BEGIN))
    {
        return (AstBlock*)ParseBeginEnd(Parser);
    }
    else if (ConsumeIfNextIs(Parser, TOKEN_PROGRAM))
    {
        PASCAL_UNREACHABLE("TODO: ParseBlock: TOKEN_PROGRAM");
    }


    if (NULL == Block)
    {
        Error(Parser, 
                "Expected 'label', 'const', "
                "'type', 'var', 'procedure', "
                "'function' or 'begin' before a block."
        );
        return NULL;
    }


    if (Parser->Error)
    {
        RecoverFromError(Parser);
    }

    /* consume the end of a block */
    if (!IsAtEnd(Parser))
    {
        Block->Next = ParseBlock(Parser);
    }
    return Block;
}


AstStmt *ParseStmt(PascalParser *Parser)
{
    if (ConsumeIfNextIs(Parser, TOKEN_EXIT)) /* TODO: return by assigning function name */
    {
        return (AstStmt*)ParseReturnStmt(Parser);
    }
    return (AstStmt*)ParseAssignStmt(Parser);
}


AstExpr ParseExpr(PascalParser *Parser)
{
    AstExpr Expression = {0};
    /* leftmost */
    Expression.Left = ParseSimpleExpr(Parser);
    AstOpSimpleExpr **Right = &Expression.Right;


    static const TokenType Ops[] = { 
        TOKEN_LESS, TOKEN_GREATER, 
        TOKEN_LESS_EQUAL, TOKEN_GREATER_EQUAL, 
        TOKEN_LESS_GREATER, TOKEN_EQUAL, TOKEN_IN
    };
    while (ConsumeIfNextIsOneOf(Parser, STATIC_ARRAY_SIZE(Ops), Ops))
    {
        (*Right) = ArenaAllocateZero(Parser->Arena, sizeof(**Right));
        (*Right)->Op = Parser->Curr.Type;
        (*Right)->SimpleExpr = ParseSimpleExpr(Parser);
        Right = &(*Right)->Next;
    }
    return Expression;
}











static void ParseType(PascalParser *Parser)
{
    if (!NextTokenIs(Parser, TOKEN_IDENTIFIER))
    {
        Error(Parser, "Expected identifier.");
        return;
    }

    do {
        ConsumeToken(Parser);
        Token Identifier = Parser->Curr;

        ConsumeOrError(Parser, TOKEN_EQUAL, "Expected '=' after '%.*s'.",
                Identifier.Len, Identifier.Str
        );
        ConsumeOrError(Parser, TOKEN_IDENTIFIER, "Expected type name after '='.");

        //ParserPushTypeName(Parser, &Identifier, &Parser->Curr);

        ConsumeOrError(Parser, TOKEN_SEMICOLON, "Expected ';' after '%.*s'.",
                Parser->Next.Len, Parser->Next.Str
        );
    } while (NextTokenIs(Parser, TOKEN_IDENTIFIER));
}




static AstStmtBlock *ParseBeginEnd(PascalParser *Parser)
{
    AstStmtBlock *Statements = ArenaAllocateZero(Parser->Arena, sizeof(*Statements));
    Statements->Base.Type = AST_BLOCK_STATEMENTS;
    AstStmtList **CurrStmt = &Statements->Statements;

    while (!IsAtEnd(Parser) && !ConsumeIfNextIs(Parser, TOKEN_END))
    {
        *CurrStmt = ArenaAllocateZero(Parser->Arena, sizeof(**CurrStmt));

        (*CurrStmt)->Statement = ParseStmt(Parser);
        ConsumeOrError(Parser, TOKEN_SEMICOLON, "Expected ';' after statement.");

        CurrStmt = &(*CurrStmt)->Next;
    }
    return Statements;
}





static AstVarList *ParseVarList(PascalParser *Parser, AstVarList *List)
{        
    /* parses declaration */
    AstVarList *Start = List;
    do {
        ConsumeOrError(Parser, TOKEN_IDENTIFIER, "Expected variable name.");

        List->Identifier = Parser->Curr;
        if (ConsumeIfNextIs(Parser, TOKEN_COMMA))
        {
            List->Next = ArenaAllocateZero(Parser->Arena, sizeof(*List->Next));
            List = List->Next;
        }
        else break;
    } while (1);


    ConsumeOrError(Parser, TOKEN_COLON, "Expected ':' or ',' after variable name.");
    ConsumeOrError(Parser, TOKEN_IDENTIFIER, "Expected type name after ':'.");

    /* check if type name is already defined */
    if (!ParserTypeIsDefined(Parser, &Parser->Curr))
    {
        Error(Parser, "Unknown type.");
    }

    /* assign type to each variable */
    while (NULL != Start)
    {
        Start->Type = ParserLookupTypeOfName(Parser, &Parser->Curr);
        Start = Start->Next;
    }
    ConsumeOrError(Parser, TOKEN_SEMICOLON, "Expected ';' after type name.");
    return List;
}


static AstVarBlock *ParseVar(PascalParser *Parser)
{
    AstVarBlock *BlockDeclaration = ArenaAllocateZero(Parser->Arena, sizeof(*BlockDeclaration));
    BlockDeclaration->Base.Type = AST_BLOCK_VAR;
    AstVarList *Decl = &BlockDeclaration->Decl;

    Decl = ParseVarList(Parser, Decl);
    while (NextTokenIs(Parser, TOKEN_IDENTIFIER))
    {
        Decl->Next = ArenaAllocateZero(Parser->Arena, sizeof(*Decl));
        Decl = ParseVarList(Parser, Decl->Next);
    }

    ParserDefineVariables(Parser, &BlockDeclaration->Decl);
    return BlockDeclaration;
}





static AstAssignStmt *ParseAssignStmt(PascalParser *Parser)
{
    AstAssignStmt *Assignment = ArenaAllocateZero(Parser->Arena, sizeof(*Assignment));
    Assignment->Base.Type = AST_STMT_ASSIGNMENT;

    ConsumeOrError(Parser, TOKEN_IDENTIFIER, "Expected identifier before ':='");
    if (!ParserIdentifierIsDefined(Parser, &Parser->Curr))
    {
        Error(Parser, "Variable or function is not defined.");
    }
    Assignment->Variable = Parser->Curr;


    ConsumeOrError(Parser, TOKEN_COLON_EQUAL, "TODO: assignment");
    Assignment->Expr = ParseExpr(Parser);

    return Assignment;
}


static AstReturnStmt *ParseReturnStmt(PascalParser *Parser)
{
    AstReturnStmt *RetStmt = ArenaAllocateZero(Parser->Arena, sizeof(*RetStmt));
    RetStmt->Base.Type = AST_STMT_RETURN;
    if (ConsumeIfNextIs(Parser, TOKEN_LEFT_PAREN))
    {
        RetStmt->Expr = ArenaAllocateZero(Parser->Arena, sizeof *RetStmt->Expr);
        *RetStmt->Expr = ParseExpr(Parser);
        ConsumeOrError(Parser, TOKEN_RIGHT_PAREN, "Expected ')' after expression.");
    }
    return RetStmt;
}




static AstFunctionBlock *ParseFunction(PascalParser *Parser)
{
    AstFunctionBlock *Function = ArenaAllocateZero(Parser->Arena, sizeof(*Function));
    Function->Base.Type = AST_BLOCK_FUNCTION;

    ConsumeOrError(Parser, TOKEN_IDENTIFIER, "Expected function name.");
    Function->Identifier = Parser->Curr;


    Token BeforeColon = Function->Identifier;
    if (ConsumeIfNextIs(Parser, TOKEN_LEFT_PAREN))
    {
        BeforeColon = Parser->Curr;
    }

    ConsumeOrError(Parser, TOKEN_COLON, "Expected ':' after '%.*s'.", BeforeColon.Len, BeforeColon.Str);
    ConsumeOrError(Parser, TOKEN_IDENTIFIER, "Expected function return type.");
    ConsumeOrError(Parser, TOKEN_SEMICOLON, "Expected ';' after function return type.");

    Function->Block = ParseBlock(Parser);
    return Function;
}








static ParserType ParserTokenToVarType(PascalParser *Parser, const Token *Type)
{
    U32 *TypeNumber = VartabGet(&Parser->IdentifiersInScope, Type->Str, Type->Len);
    if (NULL == TypeNumber)
        return TYPE_INVALID;
    return *TypeNumber;
}


static ParserType ParserLookupTypeOfName(PascalParser *Parser, const Token *Name)
{
    U32 *Type = VartabGet(&Parser->IdentifiersInScope, Name->Str, Name->Len);
    if (NULL != Type)
        return *Type;
    return TYPE_INVALID;
}


static void ParserDefineType(PascalParser *Parser, const Token *TypeName)
{
    ParserType Type = ParserTokenToVarType(Parser, TypeName);
    PASCAL_ASSERT(Type != TYPE_INVALID, "TODO: handle this");
    VartabSet(&Parser->IdentifiersInScope, TypeName->Str, TypeName->Len, Type);
}


static bool ParserTypeIsDefined(PascalParser *Parser, const Token *Type)
{
    return ParserTokenToVarType(Parser, Type) != TYPE_INVALID;
}


static void ParserDefineVariables(PascalParser *Parser, const AstVarList *VarList)
{
    while (NULL != VarList)
    {
        const Token *Identifier = &VarList->Identifier;

        /* TODO: shadowing globals */
        VartabSet(&Parser->IdentifiersInScope, 
                Identifier->Str, Identifier->Len,
                VarList->Type
        );
        VarList = VarList->Next;
    }
}


static bool ParserIdentifierIsDefined(PascalParser *Parser, const Token *Identifier)
{
    return NULL != VartabFind(&Parser->IdentifiersInScope, Identifier->Str, Identifier->Len);
}




static ParserType ParserCoerceTypes(PascalParser *Parser, ParserType Left, ParserType Right)
{
    PASCAL_ASSERT(Left >= TYPE_INVALID && Right >= TYPE_INVALID, "Unreachable");
    if (Left > TYPE_COUNT || Right > TYPE_COUNT)
    {
        Error(Parser, "Invalid combination of type %d and %d", Left, Right);
        return TYPE_INVALID;
    }
    return sCoercionRules[Left][Right];
}













static AstSimpleExpr ParseSimpleExpr(PascalParser *Parser)
{
    AstSimpleExpr SimpleExpression = {0};

    /* prefixes */
    /* TODO: handle prefixes properly */
    static const TokenType PrefixOps[] = { TOKEN_PLUS, TOKEN_MINUS };
    if (ConsumeIfNextIsOneOf(Parser, STATIC_ARRAY_SIZE(PrefixOps), PrefixOps))
    {
        SimpleExpression.Prefix = Parser->Curr.Type;
    }

    /* left */
    SimpleExpression.Left = ParseTerm(Parser);
    ParserType LastType = SimpleExpression.Left.Type;
    AstOpTerm **Right = &SimpleExpression.Right;

    static const TokenType Ops[] = { TOKEN_MINUS, TOKEN_PLUS, TOKEN_OR };
    while (ConsumeIfNextIsOneOf(Parser, STATIC_ARRAY_SIZE(Ops), Ops))
    {
        (*Right) = ArenaAllocateZero(Parser->Arena, sizeof(**Right));
        (*Right)->Op = Parser->Curr.Type;
        (*Right)->Term = ParseTerm(Parser);
        (*Right)->Type = ParserCoerceTypes(Parser, LastType, (*Right)->Term.Type);
        Right = &(*Right)->Next;
    }
    SimpleExpression.Type = LastType;
    return SimpleExpression;
}

static AstTerm ParseTerm(PascalParser *Parser)
{
    AstTerm CurrentTerm = {0};
    /* left oper */
    CurrentTerm.Left = ParseFactor(Parser);
    ParserType LastType = CurrentTerm.Left.Type;
    AstOpFactor **Right = &CurrentTerm.Right;

    static const TokenType InfixOps[] = { TOKEN_STAR, TOKEN_SLASH, TOKEN_DIV, TOKEN_MOD, TOKEN_AND };
    while (ConsumeIfNextIsOneOf(Parser, STATIC_ARRAY_SIZE(InfixOps), InfixOps))
    {
        (*Right) = ArenaAllocateZero(Parser->Arena, sizeof(**Right));
        (*Right)->Op = Parser->Curr.Type;
        (*Right)->Factor = ParseFactor(Parser);
        (*Right)->Type = ParserCoerceTypes(Parser, LastType, (*Right)->Factor.Type);
        Right = &(*Right)->Next;
    }
    CurrentTerm.Type = LastType;
    return CurrentTerm;
}

static AstFactor ParseFactor(PascalParser *Parser)
{
    AstFactor Factor = {0};
    switch (Parser->Next.Type)
    {
    case TOKEN_INTEGER_LITERAL:
    {
        ConsumeToken(Parser);
        Factor.FactorType = FACTOR_INTEGER;
        Factor.Type = TYPE_I16;
        Factor.As.Integer = Parser->Curr.Literal.Int;
    } break;
    case TOKEN_NUMBER_LITERAL:
    {
        ConsumeToken(Parser);
        Factor.FactorType = FACTOR_REAL;
        Factor.Type = TYPE_F32;
        Factor.As.Real = Parser->Curr.Literal.Real;
    } break;
    case TOKEN_LEFT_PAREN:
    {
        ConsumeToken(Parser);
        AstExpr Expression = ParseExpr(Parser);
        if (ConsumeOrError(Parser, TOKEN_RIGHT_PAREN, "Expected ')' after expression."))
        {
            Factor.FactorType = FACTOR_GROUP_EXPR;
            Factor.Type = Expression.Type;
            Factor.As.Expression = ArenaAllocateZero(Parser->Arena, sizeof(*Factor.As.Expression));
            *Factor.As.Expression = Expression;
        }
    } break;
    case TOKEN_NOT:
    {
        ConsumeToken(Parser);
        Factor.FactorType = FACTOR_NOT;
        Factor.As.NotFactor = ArenaAllocateZero(Parser->Arena, sizeof(*Factor.As.NotFactor));
        *Factor.As.NotFactor = ParseFactor(Parser);
        Factor.Type = Factor.As.NotFactor->Type;
    } break;
    case TOKEN_IDENTIFIER:
    {
        ConsumeToken(Parser);
        if (!ParserIdentifierIsDefined(Parser, &Parser->Curr))
        {
            Error(Parser, "Undefined Identifier.");
        }

        if (NextTokenIs(Parser, TOKEN_LEFT_PAREN))
        {
            PASCAL_UNREACHABLE("TODO: call expression");
        }
        Factor.FactorType = FACTOR_VARIABLE;
        Factor.As.Variable.Name = Parser->Curr;
        Factor.Type = ParserLookupTypeOfName(Parser, &Parser->Curr);
    } break;

    default: 
    {
        Error(Parser, "Expected expression");
    } break;

    }
    return Factor;
}




static bool IsAtEnd(const PascalParser *Parser)
{
    return Parser->Next.Type == TOKEN_EOF;
}

static bool NextTokenIs(const PascalParser *Parser, const TokenType Type)
{
    return Parser->Next.Type == Type;
}

static bool ConsumeIfNextIsOneOf(PascalParser *Parser, UInt Count, const TokenType Types[])
{
    for (UInt i = 0; i < Count; i++)
    {
        if (Parser->Next.Type == Types[i])
        {
            ConsumeToken(Parser);
            return true;
        }
    }
    return false;
}

static bool ConsumeIfNextIs(PascalParser *Parser, TokenType Type)
{
    if (Parser->Next.Type == Type)
    {
        ConsumeToken(Parser);
        return true;
    }
    return false;
}

static void ConsumeToken(PascalParser *Parser)
{
    Parser->Curr = Parser->Next;
    Parser->Next = TokenizerGetToken(&Parser->Lexer);
}


static bool ConsumeOrError(PascalParser *Parser, TokenType Expected, const char *Fmt, ...)
{
    if (!ConsumeIfNextIs(Parser, Expected))
    {
        va_list VaList;
        va_start(VaList, Fmt);
        VaListError(Parser, Fmt, VaList);
        va_end(VaList);
        return false;
    }
    return true;
}






static void Error(PascalParser *Parser, const char *Fmt, ...)
{
    va_list VaList;
    va_start(VaList, Fmt);
    VaListError(Parser, Fmt, VaList);
    va_end(VaList);
}

static void VaListError(PascalParser *Parser, const char *Fmt, va_list VaList)
{
    Parser->Error = true;
    if (!Parser->PanicMode)
    {
        Parser->PanicMode = true;
        fprintf(Parser->ErrorFile, "Parser [line %d]: '%.*s'\n    ", 
                Parser->Next.Line, Parser->Curr.Len, Parser->Curr.Str);
        vfprintf(Parser->ErrorFile, Fmt, VaList);
        fputc('\n', Parser->ErrorFile);
    }
}


static void RecoverFromError(PascalParser *Parser)
{
    Parser->PanicMode = false;
    while (!IsAtEnd(Parser))
    {
        /* keywords before a block */
        switch (Parser->Next.Type)
        {
        case TOKEN_LABEL:
        case TOKEN_CONST:
        case TOKEN_TYPE:
        case TOKEN_VAR:
        case TOKEN_PROCEDURE:
        case TOKEN_FUNCTION:
        case TOKEN_BEGIN:
            return;

        default: 
        {
            ConsumeToken(Parser);
        } break;

        }
    }
}





