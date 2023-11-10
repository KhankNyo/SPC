#ifndef PASCAL_ASTEXPR_H
#define PASCAL_ASTEXPR_H


#include "Tokenizer.h"
#include "Common.h"

typedef struct AstFactor AstFactor;
typedef struct AstOpFactor AstOpFactor;

typedef struct AstTerm AstTerm;
typedef struct AstOpTerm AstOpTerm;

typedef struct AstSimpleExpr AstSimpleExpr;
typedef struct AstOpSimpleExpr AstOpSimpleExpr;

typedef struct AstExpr AstExpr;
typedef struct AstOpExpr AstExOppr;

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
        Token Variable;
    } As;
};

struct AstOpFactor 
{
    TokenType Op;
    AstFactor Factor;
    struct AstOpFactor *Next;
};


struct AstTerm
{
    AstFactor Left;
    AstOpFactor *Right;
};

struct AstOpTerm 
{
    TokenType Op;
    AstTerm Term;
    struct AstOpTerm *Next;
};



struct AstSimpleExpr
{
    TokenType Prefix;
    AstTerm Left;
    AstOpTerm *Right;
};

struct AstOpSimpleExpr
{
    TokenType Op;
    AstSimpleExpr SimpleExpr;
    struct AstOpSimpleExpr *Next;
};



struct AstExpr 
{
    AstSimpleExpr Left;
    AstOpSimpleExpr *Right;
};




#endif /* PASCAL_ASTEXPR_H */

