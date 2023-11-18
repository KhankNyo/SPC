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
typedef struct AstIfStmt AstIfStmt;
typedef struct AstForStmt AstForStmt;
typedef struct AstWhileStmt AstWhileStmt;
typedef struct AstBeginEndStmt AstBeginEndStmt;



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
    AST_STMT_BEGINEND,
    AST_STMT_IF,
    AST_STMT_FOR,
    AST_STMT_WHILE,
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
    const U8 *Src;
    UInt Len;
    U32 Line;
};




/*----------------------------------------- 
 *                  LISTS
 *-----------------------------------------*/


struct AstVarList
{
    U32 ID;
    ParserType Type;
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
    AstVarList *Params;
    AstBlock *Block;
    U32 ID;
    int ArgCount;
    ParserType ReturnType;
    bool HasReturnType;
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

struct AstBeginEndStmt 
{
    AstStmt Base;
    AstStmtList *Statements;
};

struct AstIfStmt 
{
    AstStmt Base;
    AstExpr Condition;
    AstStmt *IfCase;
    AstStmt *ElseCase;
};

struct AstForStmt
{
    AstStmt Base;
    AstAssignStmt *InitStmt;
    AstExpr StopExpr;
    AstStmt *Stmt;
    TokenType Comparison;
    int Imm;
};

struct AstWhileStmt 
{
    AstStmt Base;
    AstExpr Expr;
    AstStmt *Stmt;
};

struct AstAssignStmt 
{
    AstStmt Base;

    /* TODO: assigning to things other than variables */
    ParserType LhsType;
    U32 VariableID;
    TokenType TypeOfAssignment;
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

