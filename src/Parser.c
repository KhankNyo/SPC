

#include <stdarg.h>
#include "Parser.h"









static AstVarBlock *ParseVar(PascalParser *Parser);
static AstStmtBlock *ParseBeginEnd(PascalParser *Parser);
static AstFunctionBlock *ParseFunction(PascalParser *Parser);
static AstAssignStmt *ParseAssignStmt(PascalParser *Parser);


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





PascalParser ParserInit(const U8 *Source, PascalArena *Arena, FILE *ErrorFile)
{
    PascalParser Parser = {
        .Lexer = TokenizerInit(Source),
        .Arena = Arena,
        .Error = false,
        .ErrorFile = ErrorFile,
    };
    return Parser;
}


PascalAst ParserGenerateAst(PascalParser *Parser)
{
    Parser->Next = TokenizerGetToken(&Parser->Lexer);
    PascalAst Ast = {
        .Block = ParseBlock(Parser),
    };
    return Ast;
}

void ParserDestroyAst(PascalAst *Ast)
{
    /* The arena owns the Ast, no need to do anything here */
    (void)Ast;
}









AstBlock *ParseBlock(PascalParser *Parser)
{
    if (ConsumeIfNextIs(Parser, TOKEN_FUNCTION))
    {
        return (AstBlock*)ParseFunction(Parser);
    }
    if (ConsumeIfNextIs(Parser, TOKEN_VAR))
    {
        return (AstBlock*)ParseVar(Parser);
    }
    if (ConsumeIfNextIs(Parser, TOKEN_BEGIN))
    {
        return (AstBlock*)ParseBeginEnd(Parser);
    }

    PASCAL_ASSERT(0, "Unimplemented %s in ParseBlock()", TokenTypeToStr(Parser->Curr.Type));
    return NULL;
}


AstStmt *ParseStmt(PascalParser *Parser)
{
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








static AstVarBlock *ParseVar(PascalParser *Parser)
{
    AstVarBlock *BlockDeclaration = ArenaAllocateZero(Parser->Arena, sizeof(*BlockDeclaration));
    BlockDeclaration->Base.Type = AST_BLOCK_VAR;
    AstVarList *Decl = &BlockDeclaration->Decl;

    do {
        /* parses declaration */
        AstVarList *Start = Decl;
        do {
            ConsumeOrError(Parser, TOKEN_IDENTIFIER, "Expected variable name.");

            Decl->TypeName = Parser->Curr;
            if (NextTokenIs(Parser, TOKEN_COMMA))
            {
                Decl->Next = ArenaAllocateZero(Parser->Arena, sizeof(*Decl->Next));
                Decl = Decl->Next;
                ConsumeToken(Parser);
            }
            else break;
        } while (1);


        ConsumeOrError(Parser, TOKEN_COLON, "Expected ':' or ',' after variable name.");
        ConsumeOrError(Parser, TOKEN_IDENTIFIER, "Expected type name after ':'.");
        /* assign type to each variable */
        while (NULL != Start)
        {
            Start->TypeName = Parser->Curr;
            Start = Start->Next;
        }


        ConsumeOrError(Parser, TOKEN_SEMICOLON, "Expected ';' after type name.");
    } while (NextTokenIs(Parser, TOKEN_IDENTIFIER));

    return BlockDeclaration;
}



static AstStmtBlock *ParseBeginEnd(PascalParser *Parser)
{
    AstStmtBlock *Statements = ArenaAllocateZero(Parser->Arena, sizeof(*Statements));
    AstStmtList *CurrStmt = Statements->Statements;
    Statements->Base.Type = AST_BLOCK_STATEMENTS;

    while (!IsAtEnd(Parser) && ConsumeIfNextIs(Parser, TOKEN_END))
    {
        CurrStmt->Statement = ParseStatement(Parser);
        CurrStmt = CurrStmt->Next;
    }

    if (IsAtEnd(Parser))
    {
        Error(Parser, "Unexpected end of file.");
    }
    return Statements;
}



static AstAssignStmt *ParseAssignStmt(PascalParser *Parser)
{
    AstAssignStmt *Assignment = ArenaAllocateZero(Parser->Arena, sizeof(*Assignment));
    Assignment->Base.Type = AST_STMT_ASSIGNMENT;

    Assignment->Variable = Parser->Curr;
    ConsumeOrError(Parser, TOKEN_COLON_EQUAL, "TODO: assignment");
    Assignment->Expr = ParseExpr(Parser);

    return Assignment;
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


















static AstSimpleExpr ParseSimpleExpr(PascalParser *Parser)
{
    AstSimpleExpr SimpleExpression = {0};

    /* prefixes */
    static const TokenType PrefixOps[] = { TOKEN_PLUS, TOKEN_MINUS };
    if (ConsumeIfNextIsOneOf(Parser, STATIC_ARRAY_SIZE(PrefixOps), PrefixOps))
    {
        SimpleExpression.Prefix = Parser->Curr.Type;
    }

    /* left */
    SimpleExpression.Left = ParseTerm(Parser);
    AstOpTerm **Right = &SimpleExpression.Right;

    static const TokenType Ops[] = { TOKEN_MINUS, TOKEN_PLUS, TOKEN_OR };
    while (ConsumeIfNextIsOneOf(Parser, STATIC_ARRAY_SIZE(Ops), Ops))
    {
        (*Right) = ArenaAllocateZero(Parser->Arena, sizeof(**Right));
        (*Right)->Op = Parser->Curr.Type;
        (*Right)->Term = ParseTerm(Parser);
        Right = &(*Right)->Next;
    }
    return SimpleExpression;
}

static AstTerm ParseTerm(PascalParser *Parser)
{
    AstTerm CurrentTerm = {0};
    /* left oper */
    CurrentTerm.Left = ParseFactor(Parser);
    AstOpFactor **Right = &CurrentTerm.Right;

    static const TokenType InfixOps[] = { TOKEN_STAR, TOKEN_SLASH, TOKEN_DIV, TOKEN_MOD, TOKEN_AND };
    while (ConsumeIfNextIsOneOf(Parser, STATIC_ARRAY_SIZE(InfixOps), InfixOps))
    {
        (*Right) = ArenaAllocateZero(Parser->Arena, sizeof(**Right));
        (*Right)->Op = Parser->Curr.Type;
        (*Right)->Factor = ParseFactor(Parser);
        Right = &(*Right)->Next;
    }
    return CurrentTerm;
}

static AstFactor ParseFactor(PascalParser *Parser)
{
    AstFactor Factor = {0};
    if (ConsumeIfNextIs(Parser, TOKEN_INTEGER_LITERAL))
    {
        Factor.Type = FACTOR_INTEGER;
        Factor.As.Integer = Parser->Curr.Literal.Int;
        return Factor;
    }
    if (ConsumeIfNextIs(Parser, TOKEN_NUMBER_LITERAL))
    {
        Factor.Type = FACTOR_REAL;
        Factor.As.Real = Parser->Curr.Literal.Real;
    }
    if (ConsumeIfNextIs(Parser, TOKEN_LEFT_PAREN))
    {
        AstExpr Expression = ParseExpr(Parser);
        if (ConsumeOrError(Parser, TOKEN_RIGHT_PAREN, "Expected ')' after expression."))
        {
            Factor.Type = FACTOR_GROUP_EXPR;
            Factor.As.Expression = ArenaAllocateZero(Parser->Arena, sizeof(*Factor.As.Expression));
            *Factor.As.Expression = Expression;
        }
        return Factor;
    }
    if (ConsumeIfNextIs(Parser, TOKEN_NOT))
    {
        Factor.Type = FACTOR_NOT;
        Factor.As.NotFactor = ArenaAllocateZero(Parser->Arena, sizeof(*Factor.As.NotFactor));
        *Factor.As.NotFactor = ParseFactor(Parser);
        return Factor;
    }


    Error(Parser, "Expected expression");
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
    vfprintf(Parser->ErrorFile, Fmt, VaList);
}








