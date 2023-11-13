#ifndef PASCAL_PARSER_H
#define PASCAL_PARSER_H



#include "Common.h"
#include "Tokenizer.h"
#include "Memory.h"
#include "Ast.h"
#include "Vartab.h"


typedef struct PascalParser 
{
    PascalArena *Arena;
    PascalTokenizer Lexer;
    Token Curr, Next;
    bool Error, PanicMode;
    FILE *ErrorFile;
    PascalVartab IdentifiersInScope;
} PascalParser;


PascalParser ParserInit(const U8 *Source, PascalArena *Arena, FILE *ErrorFile);


PascalAst *ParserGenerateAst(PascalParser *Parser);
AstExpr ParseExpr(PascalParser *Parser);

/* ptr owned by the arena */
AstBlock *ParseBlock(PascalParser *Parser);
AstStmt *ParseStatement(PascalParser *Parser);


void ParserDestroyAst(PascalAst *Ast);


#endif /* PASCAL_PARSER_H */

