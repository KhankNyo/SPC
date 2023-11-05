#ifndef PASCAL_PARSER_H
#define PASCAL_PARSER_H



#include "Common.h"
#include "Tokenizer.h"
#include "Memory.h"
#include "Ast.h"


typedef struct PascalParser 
{
    PascalArena *Arena;
    PascalTokenizer Lexer;
    Token Curr, Next;
    bool Error;
    FILE *ErrorFile;
} PascalParser;


PascalParser ParserInit(const U8 *Source, PascalArena *Arena, FILE *ErrorFile);

PascalAst ParserGenerateAst(PascalParser *Parser);
void ParserDestroyAst(PascalAst *Ast);


#endif /* PASCAL_PARSER_H */

