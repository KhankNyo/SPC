

#include <stdarg.h>
#include "Parser.h"











static AstExpr ParseExpr(PascalParser *Parser);
static AstSimpleExpr ParseSimpleExpr(PascalParser *Parser);
static AstTerm ParseTerm(PascalParser *Parser);
static AstFactor ParseFactor(PascalParser *Parser);

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
    AstExpr Expression = ParseExpr(Parser);

    return (PascalAst) {
        .Expression = Expression,
    };
}

void ParserDestroyAst(PascalAst *Ast)
{
    /* The arena owns the Ast, no need to do anything here */
    (void)Ast;
}












static AstExpr ParseExpr(PascalParser *Parser)
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


    Error(Parser, "Expected expression\n");
    return Factor;
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
        Parser->Error = true;

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








