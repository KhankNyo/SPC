#ifndef PASCAL_PARSER_H
#define PASCAL_PARSER_H



#include "Common.h"
#include "Tokenizer.h"
#include "Memory.h"
#include "Vartab.h"


#define PARSER_MAX_SCOPE 8
#define PARSER_VAR_PER_SCOPE 256


#if 0
typedef struct PascalParser 
{
    PascalArena *Arena;
    PascalGPA Allocator;

    PascalTokenizer Lexer;
    Token Curr, Next;
    bool Error, PanicMode;
    FILE *ErrorFile;

    PascalVartab *Global;
    PascalVartab Scope[PARSER_MAX_SCOPE];
    int ScopeCount;
    U32 VariableID;
} PascalParser;


PascalParser ParserInit(const U8 *Source, PascalVartab *PredefinedIdentifiers, PascalArena *Arena, FILE *ErrorFile);
void ParserDestroyAst(PascalAst *Ast);

PascalAst *ParserGenerateAst(PascalParser *Parser);
AstExpr ParseExpr(PascalParser *Parser);

/* ptr owned by the arena */
AstBlock *ParseBlock(PascalParser *Parser);
AstStmt *ParseStatement(PascalParser *Parser);

const char *ParserTypeToStr(ParserType Type);
#endif 




#endif /* PASCAL_PARSER_H */

