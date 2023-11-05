

#include <stdarg.h>
#include "Include/Parser.h"











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
}












static AstExpr ParseExpr(PascalParser *Parser)
{
    AstExpr Expression = {0};
    /* left */
    Expression.Left = ParseSimpleExpr(Parser);
    AstExpr **RightExpr = &Expression.Right;


    /* oper */
    static const TokenType Ops[] = { 
        TOKEN_COMMA, 
        TOKEN_LESS, TOKEN_GREATER, 
        TOKEN_LESS_EQUAL, TOKEN_GREATER_EQUAL, 
        TOKEN_LESS_GREATER, TOKEN_EQUAL
    };
    while (ConsumeIfNextIsOneOf(Parser, STATIC_ARRAY_SIZE(Ops), Ops))
    {
        *RightExpr = ArenaAllocateZero(Parser->Arena, sizeof(**RightExpr));
        (*RightExpr)->InfixOp = Parser->Curr;

        /* right */
        (*RightExpr)->Left = ParseSimpleExpr(Parser);
        RightExpr = &(*RightExpr)->Right;
    }
    return Expression;
}

static AstSimpleExpr ParseSimpleExpr(PascalParser *Parser)
{
    AstSimpleExpr SimpleExpression = {0};

    /* prefixes */
    static const TokenType PrefixOps[] = { TOKEN_PLUS, TOKEN_MINUS };
    ConsumeIfNextIsOneOf(Parser, STATIC_ARRAY_SIZE(PrefixOps), PrefixOps);

    /* left */
    SimpleExpression.Left = ParseTerm(Parser);
    AstSimpleExpr **RightExpr = &SimpleExpression.Right;

    /* infix */
    static const TokenType Ops[] = { TOKEN_MINUS, TOKEN_PLUS, TOKEN_OR };
    while (ConsumeIfNextIsOneOf(Parser, STATIC_ARRAY_SIZE(Ops), Ops))
    {
        *RightExpr = ArenaAllocateZero(Parser->Arena, sizeof(**RightExpr));
        (*RightExpr)->InfixOp = Parser->Curr;

        /* right */
        (*RightExpr)->Left = ParseTerm(Parser);
        RightExpr = &(*RightExpr)->Right;
    }
    return SimpleExpression;
}

static AstTerm ParseTerm(PascalParser *Parser)
{
    AstTerm CurrentTerm = {0};
    /* left oper */
    CurrentTerm.Left = ParseFactor(Parser);
    AstTerm **RightTerm = &CurrentTerm.Right;

    /* infixes */
    static const TokenType InfixOps[] = { TOKEN_STAR, TOKEN_SLASH, TOKEN_DIV, TOKEN_MOD, TOKEN_AND };
    while (ConsumeIfNextIsOneOf(Parser, STATIC_ARRAY_SIZE(InfixOps), InfixOps))
    {
        *RightTerm = ArenaAllocateZero(Parser->Arena, sizeof(**RightTerm));
        (*RightTerm)->InfixOp = Parser->Curr;

        /* right oper */
        (*RightTerm)->Left = ParseFactor(Parser);
        RightTerm = &(*RightTerm)->Right;
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








