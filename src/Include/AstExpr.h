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

typedef struct AstExprList AstExprList;

typedef enum ParserType 
{
    TYPE_INVALID = 0,
    TYPE_I8, TYPE_I16, TYPE_I32, TYPE_I64,
    TYPE_U8, TYPE_U16, TYPE_U32, TYPE_U64,
    TYPE_F32, TYPE_F64, 
    TYPE_FUNCTION,
    TYPE_BOOLEAN,
    TYPE_COUNT,
} ParserType;


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
    ParserType Type;
    AstFactorType FactorType;
    union {
        U64 Integer;
        F64 Real;
        U32 VarID;

        AstExpr *Expression;
        struct {
            U32 ID;
            AstExprList *ArgList;
        } Function;
        struct AstFactor *NotFactor;
    } As;
};

struct AstOpFactor 
{
    ParserType Type;
    TokenType Op;
    AstFactor Factor;
    struct AstOpFactor *Next;
};


struct AstTerm
{
    ParserType Type;
    AstFactor Left;
    AstOpFactor *Right;
};

struct AstOpTerm 
{
    ParserType Type;
    TokenType Op;
    AstTerm Term;
    struct AstOpTerm *Next;
};



struct AstSimpleExpr
{
    ParserType Type;
    bool Negated;
    AstTerm Left;
    AstOpTerm *Right;
};

struct AstOpSimpleExpr
{
    ParserType Type;
    TokenType Op;
    AstSimpleExpr SimpleExpr;
    struct AstOpSimpleExpr *Next;
};



struct AstExpr 
{
    ParserType Type;
    AstSimpleExpr Left;
    AstOpSimpleExpr *Right;
};


struct AstExprList 
{
    AstExpr Expr;
    AstExprList *Next;
};




#endif /* PASCAL_ASTEXPR_H */

