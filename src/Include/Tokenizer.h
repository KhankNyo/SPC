#ifndef PASCAL_TOKENIZER_H
#define PASCAL_TOKENIZER_H


#include "Common.h"
#include "PascalString.h"

typedef struct PascalTokenizer 
{
    const U8 *Start, *Curr;
    UInt Line;
} PascalTokenizer;


typedef enum TokenType 
{
    TOKEN_EOF = 0,
    TOKEN_ERROR,

    /* keywords */
    TOKEN_AND, TOKEN_ARRAY, TOKEN_ASM,
    TOKEN_BEGIN, TOKEN_BREAK, 
    TOKEN_CASE, TOKEN_CONST, TOKEN_CONSTRUCTOR, TOKEN_CONTINUE, 
    TOKEN_DESTRUCTOR, TOKEN_DIV, TOKEN_DO, TOKEN_DOWNTO,
    TOKEN_ELSE, TOKEN_END, 
    TOKEN_FALSE, TOKEN_FILE, TOKEN_FOR, TOKEN_FUNCTION, 
    TOKEN_GOTO,
    TOKEN_IF, TOKEN_IMPLEMENTATION, TOKEN_IN, TOKEN_INLINE, TOKEN_INTERFACE, 
    TOKEN_LABEL, 
    TOKEN_MODE, 
    TOKEN_NIL, TOKEN_NOT,
    TOKEN_OBJECT, TOKEN_OF, TOKEN_ON, TOKEN_OPERATOR, TOKEN_OR, 
    TOKEN_PACKED, TOKEN_PROCEDURE, TOKEN_PROGRAM, 
    TOKEN_RECORD, TOKEN_REPEAT, 
    TOKEN_SET, TOKEN_SHL, TOKEN_SHR, TOKEN_STRING, 
    TOKEN_THEN, TOKEN_TRUE, TOKEN_TYPE, 
    TOKEN_UNIT, TOKEN_UNTIL, TOKEN_USES, 
    TOKEN_VAR, 
    TOKEN_WHILE, TOKEN_WITH, 
    TOKEN_XOR,

    /* symbols */
    TOKEN_PLUS, TOKEN_MINUS, TOKEN_STAR, TOKEN_SLASH, 
    TOKEN_PLUS_EQUAL, TOKEN_MINUS_EQUAL, TOKEN_STAR_EQUAL, TOKEN_SLASH_EQUAL,
    TOKEN_STAR_STAR,
    TOKEN_EQUAL, TOKEN_LESS, TOKEN_GREATER, TOKEN_LESS_GREATER,
    TOKEN_LESS_LESS, TOKEN_GREATER_GREATER,
    TOKEN_DOT, TOKEN_COMMA, TOKEN_COLON, TOKEN_SEMICOLON,
    TOKEN_COLON_EQUAL,
    TOKEN_LEFT_BRACKET, TOKEN_RIGHT_BRACKET, 
    TOKEN_LEFT_PAREN, TOKEN_RIGHT_PAREN, 
    TOKEN_CARET, TOKEN_AT, TOKEN_DOLLAR, TOKEN_HASHTAG, TOKEN_AMPERSAND, TOKEN_PERCENTAGE,

    /* others */
    TOKEN_NUMBER_LITERAL, TOKEN_HEX_LITERAL, TOKEN_INTEGER_LITERAL, 
    TOKEN_STRING_LITERAL, 
    TOKEN_IDENTIFIER
} TokenType;


typedef struct Token 
{
    TokenType Type;
    const U8 *Str;
    union {
        U64 Int;
        F64 Real;
        PascalStr Str;
    } Literal;
    UInt Len;
    UInt Line;
} Token;


/* initializes PascalTokenizer struct with a Pascal source file */
PascalTokenizer TokenizerInit(const U8 *Source);

/* returns the consumed token in the source file, 
 * or TOKEN_EOF if there are none left */
Token TokenizerGetToken(PascalTokenizer *Lexer);

/* return the string of a token type */
const U8 *TokenTypeToStr(TokenType Type);


#endif /* PASCAL_TOKENIZER_H */

