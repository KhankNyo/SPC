

#include <stdarg.h>

#include "Include/Parser.h"











static Expr ParseExpr(PascalParser *Parser);
static SimpleExpr ParseSimpleExpr(PascalParser *Parser);
static Term ParseTerm(PascalParser *Parser);
static Factor ParseFactor(PascalParser *Parser);

static bool ConsumeIfTokenIsOneOf(PascalParser *Parser, UInt Count, const TokenType Types[]);
static bool ConsumeIfTokenIs(PascalParser *Parser, TokenType Type);
static void ConsumeToken(PascalParser *Parser);
static bool ConsumeOrError(PascalParser *Parser, TokenType Expected, const char *Fmt, ...);
static void Error(PascalParser *Parser, const char *Fmt, va_list VaList);



static void ParserPrintExpr(FILE *f, const Expr *Expression);
static void ParserPrintSimpleExpr(FILE *f, const SimpleExpr *Simple);
static void ParserPrintTerm(FILE *f, const Term *Term);
static void ParserPrintFactor(FILE *f, const Factor *Factor);




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


ParserAst ParserGenerateAst(PascalParser *Parser)
{
    Parser->Next = TokenizerGetToken(&Parser->Lexer);
    Expr Expression = ParseExpr(Parser);

    return (ParserAst) {
        .Expression = Expression,
    };
}

void ParserDestroyAst(ParserAst *Ast)
{
}


void ParserPrintAst(FILE *f, const ParserAst *Ast)
{
    ParserPrintExpr(f, &Ast->Expression);
}











static Expr ParseExpr(PascalParser *Parser)
{
    Expr Expression = {0};
    Expression.SimpleExpression = ParseSimpleExpr(Parser);
    return Expression;
}

static SimpleExpr ParseSimpleExpr(PascalParser *Parser)
{
    SimpleExpr SimpleExpression = {0};
    /* left */
    SimpleExpression.Left = ParseTerm(Parser);
    SimpleExpr **RightExpr = &SimpleExpression.Right;

    /* infix */
    static const TokenType Tokens[] = { TOKEN_MINUS, TOKEN_PLUS, TOKEN_NOT };
    while (ConsumeIfTokenIsOneOf(Parser, STATIC_ARRAY_SIZE(Tokens), Tokens))
    {
        *RightExpr = ArenaAllocate(Parser->Arena, sizeof(**RightExpr));
        (*RightExpr)->InfixOp = Parser->Curr;

        /* right */
        Term Right = ParseTerm(Parser);
        (*RightExpr)->Left = Right;
        RightExpr = &(*RightExpr)->Right;
    }

    return SimpleExpression;
}

static Term ParseTerm(PascalParser *Parser)
{
    Term CurrentTerm = {0};

    /* prefixes */
    static const TokenType PrefixOps[] = { TOKEN_PLUS, TOKEN_MINUS };
    ConsumeIfTokenIsOneOf(Parser, STATIC_ARRAY_SIZE(PrefixOps), PrefixOps);

    /* left oper */
    CurrentTerm.Left = ParseFactor(Parser);
    Term **RightTerm = &CurrentTerm.Right;

    /* infixes */
    static const TokenType InfixOps[] = { TOKEN_STAR, TOKEN_SLASH, TOKEN_DIV, TOKEN_MOD, TOKEN_AND };
    while (ConsumeIfTokenIsOneOf(Parser, STATIC_ARRAY_SIZE(InfixOps), InfixOps))
    {
        *RightTerm = ArenaAllocate(Parser->Arena, sizeof(**RightTerm));
        (*RightTerm)->InfixOp = Parser->Curr;

        /* right oper */
        Factor Right = ParseFactor(Parser);
        (*RightTerm)->Left = Right;
        RightTerm = &(*RightTerm)->Right;
    }
    return CurrentTerm;
}

static Factor ParseFactor(PascalParser *Parser)
{
    Factor Factor = {0};
    if (ConsumeIfTokenIs(Parser, TOKEN_INTEGER_LITERAL))
    {
        Factor.Type = FACTOR_INTEGER;
        Factor.As.Integer = Parser->Curr.Literal.Int;
        return Factor;
    }
    if (ConsumeIfTokenIs(Parser, TOKEN_LEFT_PAREN))
    {
        Expr Expression = ParseExpr(Parser);
        if (ConsumeOrError(Parser, TOKEN_RIGHT_PAREN, "Expected ')' after expression."))
        {
            Factor.Type = FACTOR_GROUP_EXPR;
            Factor.As.Expression = ArenaAllocate(Parser->Arena, sizeof(*Factor.As.Expression));
            *Factor.As.Expression = Expression;
        }
        return Factor;
    }

    return Factor;
}







static bool ConsumeIfTokenIsOneOf(PascalParser *Parser, UInt Count, const TokenType Types[])
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

static bool ConsumeIfTokenIs(PascalParser *Parser, TokenType Type)
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
    if (!ConsumeIfTokenIs(Parser, Expected))
    {
        Parser->Error = true;

        va_list VaList;
        va_start(VaList, Fmt);
        Error(Parser, Fmt, VaList);
        va_end(VaList);
        return false;
    }
    return true;
}


static void Error(PascalParser *Parser, const char *Fmt, va_list VaList)
{
    vfprintf(Parser->ErrorFile, Fmt, VaList);
}





static void ParserPrintExpr(FILE *f, const Expr *Expression)
{
    ParserPrintSimpleExpr(f, &Expression->Left);
    const Expr *CurrentExpr = Expression->Right;
    while (NULL != CurrentExpr)
    {
        fprintf(f, "%.*s ", CurrentExpr->InfixOp.Len, CurrentExpr->InfixOp.Str);
        ParserPrintSimpleExpr(f, &CurrentExpr->Left);
        CurrentExpr = CurrentExpr->Right;
    }
}


static void ParserPrintSimpleExpr(FILE *f, const SimpleExpr *Simple)
{
    ParserPrintTerm(f, &Simple->Left);
    const SimpleExpr *SimpleExpr = Simple->Right;
    while (NULL != SimpleExpr)
    {
        fprintf(f, "%.*s ", SimpleExpr->InfixOp.Len, SimpleExpr->InfixOp.Str);
        ParserPrintTerm(f, &SimpleExpr->Left);
        SimpleExpr = SimpleExpr->Right;
    }
}


static void ParserPrintTerm(FILE *f, const Term *T)
{
    ParserPrintFactor(f, &T->Left);
    const Term *CurrentTerm = T->Right;
    while (NULL != CurrentTerm)
    {
        fprintf(f, "%.*s ", CurrentTerm->InfixOp.Len, CurrentTerm->InfixOp.Str);
        ParserPrintFactor(f, &CurrentTerm->Left);
        CurrentTerm = CurrentTerm->Right;
    }
}

static void ParserPrintFactor(FILE *f, const Factor *Factor)
{
    switch (Factor->Type)
    {
    case FACTOR_GROUP_EXPR:
    {
        fprintf(f, "( ");
        ParserPrintExpr(f, Factor->As.Expression);
        fprintf(f, ") ");
    } break;

    case FACTOR_INTEGER:
    {
        fprintf(f, "%llu ", Factor->As.Integer); 
    } break;

    case FACTOR_REAL:
    {
        fprintf(f, "%f ", Factor->As.Real);
    } break;

    case FACTOR_VARIABLE:
    {
    } break;

    case FACTOR_NOT_EXPR:
    {
    } break;

    case FACTOR_CALL:
    {
    } break;

    case FACTOR_INVALID:
    {
        PASCAL_UNREACHABLE("Invalid factor\n");
    } break;

    }
}

