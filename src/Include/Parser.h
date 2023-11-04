#ifndef PASCAL_PARSER_H
#define PASCAL_PARSER_H



#include "Common.h"
#include "Tokenizer.h"
#include "Memory.h"


typedef struct Factor Factor;
typedef struct Term Term;
typedef struct SimpleExpr SimpleExpr;
typedef struct Expr Expr;
typedef enum FactorType 
{
    FACTOR_INVALID = 0,
    FACTOR_GROUP_EXPR,
    FACTOR_NOT_EXPR,
    FACTOR_INTEGER,
    FACTOR_REAL,
    FACTOR_CALL,
    FACTOR_VARIABLE,
} FactorType;


struct Factor 
{
    FactorType Type;
    union {
        U64 Integer;
        F64 Real;
        Expr *Expression;
    } As;
};

struct Term
{
    Token InfixOp;
    union {
        Factor Left, Factor;
    };
    struct Term *Right;
};

struct SimpleExpr
{
    Token PrefixOp;
    union {
        Term Left, Term;
    };
    Token InfixOp;
    struct SimpleExpr *Right;
};

struct Expr 
{
    union {
        SimpleExpr Left, SimpleExpression;
    };
    Token InfixOp;
    struct Expr *Right;
};


typedef struct ParserAst 
{
    Expr Expression;
} ParserAst;




typedef struct PascalParser 
{
    PascalArena *Arena;
    PascalTokenizer Lexer;
    Token Curr, Next;
    bool Error;
    FILE *ErrorFile;
} PascalParser;


PascalParser ParserInit(const U8 *Source, PascalArena *Arena, FILE *ErrorFile);

ParserAst ParserGenerateAst(PascalParser *Parser);
void ParserDestroyAst(ParserAst *Ast);

void ParserPrintAst(FILE *f, const ParserAst *Ast);


#endif /* PASCAL_PARSER_H */

