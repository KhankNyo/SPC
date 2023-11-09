#ifndef PASCAL_AST_H
#define PASCAL_AST_H


#include "AstExpr.h"



typedef struct AstBlock AstBlock;
typedef struct AstFunction AstFunction;
typedef struct AstStatement AstStatement;
typedef struct AstDeclaration AstDeclaration;

typedef enum AstBlockType
{
    AST_BLOCK_FUNCTION,
} AstBlockType;

struct AstBlock 
{
    AstBlockType Type;
    AstBlock *Next;
};


struct AstFunction 
{
    AstBlock Base;
    Token Identifier;
};

struct AstStatement 
{
};

struct AstDeclaration 
{
};




typedef struct PascalAst 
{
    AstBlock *Block;
} PascalAst;

void PAstPrint(FILE *f, const PascalAst *Ast);


#endif /* PASCAL_AST_H */

