#ifndef PASCAL_AST_H
#define PASCAL_AST_H

#include "Tokenizer.h"
#include "Common.h"

typedef struct AstFactor AstFactor;
typedef struct AstTerm AstTerm;
typedef struct AstSimpleExpr AstSimpleExpr;
typedef struct AstExpr AstExpr;
typedef enum AstFactorType 
{
    FACTOR_INVALID = 0,
    FACTOR_GROUP_EXPR,
    FACTOR_NOT,
    FACTOR_INTEGER,
    FACTOR_REAL,
    FACTOR_CALL,
    FACTOR_VARIABLE,
} AstFactorType;


struct AstFactor 
{
    AstFactorType Type;
    union {
        U64 Integer;
        F64 Real;
        AstExpr *Expression;
        struct AstFactor *NotFactor;
    } As;
};

struct AstTerm
{
    Token InfixOp;
    union {
        AstFactor Left, Factor;
    };
    struct AstTerm *Right;
};

struct AstSimpleExpr
{
    Token PrefixOp;
    union {
        AstTerm Left, Term;
    };
    Token InfixOp;
    struct AstSimpleExpr *Right;
};

struct AstExpr 
{
    union {
        AstSimpleExpr Left, SimpleExpression;
    };
    Token InfixOp;
    struct AstExpr *Right;
};


typedef struct PascalAst 
{
    AstExpr Expression;
} PascalAst;


void PAstPrint(FILE *f, const PascalAst *Ast);
AstFactor PAstEvaluate(const PascalAst *Ast);


#endif /* PASCAL_AST_H */

