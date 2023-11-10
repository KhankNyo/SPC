

#include <stdarg.h>
#include "Parser.h"









static AstStmtBlock *ParseBeginEnd(PascalParser *Parser);
static AstVarBlock *ParseVar(PascalParser *Parser);
static AstAssignStmt *ParseAssignStmt(PascalParser *Parser);
static AstReturnStmt *ParseReturnStmt(PascalParser *Parser);
static AstFunctionBlock *ParseFunction(PascalParser *Parser);

static PascalStr *ParserLookupTypeOfName(PascalParser *Parser, const Token *Name);


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
    };
    return Parser;
}


PascalAst *ParserGenerateAst(PascalParser *Parser)
{
    Parser->Next = TokenizerGetToken(&Parser->Lexer);
    PascalAst *Ast = ArenaAllocate(Parser->Arena, sizeof(*Ast));
    Ast->Block = ParseBlock(Parser);
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
    else if (ConsumeIfNextIs(Parser, TOKEN_VAR))
    {
        Block = (AstBlock*)ParseVar(Parser);
    }

    if (NULL == Block)
    {
        Error(Parser, 
                "Expected 'label', 'const', "
                "'type', 'var', 'procedure', "
                "'function' or 'begin' before a block."
        );
    }


    if (Parser->Error)
    {
        RecoverFromError(Parser);
    }

    /* consume the end of a block */
    if (ConsumeIfNextIs(Parser, TOKEN_BEGIN))
    {
        Block->Next = (AstBlock*)ParseBeginEnd(Parser);
    }
    else if (!IsAtEnd(Parser))
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
    /* assign type to each variable */
    while (NULL != Start)
    {
        Start->TypeName = Parser->Curr;
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

    return BlockDeclaration;
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



static AstAssignStmt *ParseAssignStmt(PascalParser *Parser)
{
    AstAssignStmt *Assignment = ArenaAllocateZero(Parser->Arena, sizeof(*Assignment));
    Assignment->Base.Type = AST_STMT_ASSIGNMENT;

    ConsumeOrError(Parser, TOKEN_IDENTIFIER, "Expected identifier before ':='");
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
    ConsumeOrError(Parser, TOKEN_SEMICOLON, "Expected ';' after '%.*s'.", Parser->Curr.Len, Parser->Curr.Str);
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










static PascalStr *ParserLookupTypeOfName(PascalParser *Parser, const Token *Name)
{
    return NULL;
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
    switch (Parser->Next.Type)
    {
    case TOKEN_INTEGER_LITERAL:
    {
        ConsumeToken(Parser);
        Factor.Type = FACTOR_INTEGER;
        Factor.As.Integer = Parser->Curr.Literal.Int;
    } break;
    case TOKEN_NUMBER_LITERAL:
    {
        ConsumeToken(Parser);
        Factor.Type = FACTOR_REAL;
        Factor.As.Real = Parser->Curr.Literal.Real;
    } break;
    case TOKEN_LEFT_PAREN:
    {
        ConsumeToken(Parser);
        AstExpr Expression = ParseExpr(Parser);
        if (ConsumeOrError(Parser, TOKEN_RIGHT_PAREN, "Expected ')' after expression."))
        {
            Factor.Type = FACTOR_GROUP_EXPR;
            Factor.As.Expression = ArenaAllocateZero(Parser->Arena, sizeof(*Factor.As.Expression));
            *Factor.As.Expression = Expression;
        }
    } break;
    case TOKEN_NOT:
    {
        ConsumeToken(Parser);
        Factor.Type = FACTOR_NOT;
        Factor.As.NotFactor = ArenaAllocateZero(Parser->Arena, sizeof(*Factor.As.NotFactor));
        *Factor.As.NotFactor = ParseFactor(Parser);
    } break;
    case TOKEN_IDENTIFIER:
    {
        ConsumeToken(Parser);
        PASCAL_ASSERT(!NextTokenIs(Parser, TOKEN_LEFT_PAREN), "TODO: call expression");

        Factor.Type = FACTOR_VARIABLE;
        Factor.As.Variable.Name = Parser->Curr;
        Factor.As.Variable.Type = ParserLookupTypeOfName(Parser, &Parser->Curr);
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





