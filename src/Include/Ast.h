#ifndef PASCAL_AST_H
#define PASCAL_AST_H


#include "AstExpr.h"



typedef struct AstBlock AstBlock;
typedef struct AstStmt AstStmt;

typedef struct AstVarList AstVarList;
typedef struct AstStmtList AstStmtList;

typedef struct AstFunctionBlock AstFunctionBlock;
typedef struct AstVarBlock AstVarBlock;
typedef struct AstConstBlock AstConstBlock;
typedef struct AstReturnStmt AstReturnStmt;
typedef struct AstStmtBlock AstStmtBlock;


typedef struct AstAssignStmt AstAssignStmt;



typedef enum AstBlockType
{
    AST_BLOCK_INVALID = 0,
    AST_BLOCK_FUNCTION,
    AST_BLOCK_VAR,
    AST_BLOCK_STATEMENTS,
} AstBlockType;

typedef enum AstStmtType 
{
    AST_STMT_INVALID = 0,
    AST_STMT_ASSIGNMENT,
    AST_STMT_RETURN,
} AstStmtType;



/*----------------------------------------- 
 *                  BASE
 *-----------------------------------------*/

struct AstBlock 
{
    AstBlockType Type;
    struct AstBlock *Next;
};

struct AstStmt 
{
    AstStmtType Type;
};




/*----------------------------------------- 
 *                  LISTS
 *-----------------------------------------*/


struct AstVarList
{
    Token Identifier;
    Token TypeName;
    struct AstVarList *Next;
};

struct AstStmtList
{
    AstStmt *Statement;
    struct AstStmtList *Next;
};





/*----------------------------------------- 
 *                  BLOCKS
 *-----------------------------------------*/


struct AstFunctionBlock 
{
    AstBlock Base;
    Token Identifier;
    AstBlock *Block;
};


struct AstVarBlock
{
    AstBlock Base;
    AstVarList Decl;
};

struct AstStmtBlock
{
    AstBlock Base;
    AstStmtList *Statements;
};


/*----------------------------------------- 
 *                 STATEMENTS
 *-----------------------------------------*/

struct AstAssignStmt 
{
    AstStmt Base;
    Token Variable;
    AstExpr Expr;
};

struct AstReturnStmt 
{
    AstStmt Base;
    AstExpr *Expr;
};





typedef struct PascalAst 
{
    AstBlock *Block;
} PascalAst;




void PAstPrint(FILE *f, const PascalAst *Ast);


#endif /* PASCAL_AST_H */

