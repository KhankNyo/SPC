

#include <stdarg.h>
#include  "Common.h"
#include "Parser.h"






static ParserType sCoercionRules[TYPE_COUNT][TYPE_COUNT] = {
    /*Invalid       I8            I16           I32           I64           U8            U16           U32           U64           F32           F64           Function      */
    { TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID,  },         /* Invalid */
    { TYPE_INVALID, TYPE_I32,     TYPE_I32,     TYPE_I32,     TYPE_I64,     TYPE_I32,     TYPE_I32,     TYPE_I32,     TYPE_I64,     TYPE_F32,     TYPE_F64,     TYPE_INVALID,  },         /* I8 */
    { TYPE_INVALID, TYPE_I32,     TYPE_I32,     TYPE_I32,     TYPE_I64,     TYPE_I32,     TYPE_I32,     TYPE_I32,     TYPE_I64,     TYPE_F32,     TYPE_F64,     TYPE_INVALID,  },         /* I16 */
    { TYPE_INVALID, TYPE_I32,     TYPE_I32,     TYPE_I32,     TYPE_I64,     TYPE_I32,     TYPE_I32,     TYPE_I32,     TYPE_I64,     TYPE_F32,     TYPE_F64,     TYPE_INVALID,  },         /* I32 */
    { TYPE_INVALID, TYPE_I64,     TYPE_I64,     TYPE_I64,     TYPE_I64,     TYPE_I64,     TYPE_I64,     TYPE_I64,     TYPE_I64,     TYPE_F32,     TYPE_F64,     TYPE_INVALID,  },         /* I64 */
    { TYPE_INVALID, TYPE_I32,     TYPE_I32,     TYPE_I32,     TYPE_I64,     TYPE_U32,     TYPE_U32,     TYPE_U32,     TYPE_U64,     TYPE_F32,     TYPE_F64,     TYPE_INVALID,  },         /* U8 */
    { TYPE_INVALID, TYPE_I32,     TYPE_I32,     TYPE_I32,     TYPE_I64,     TYPE_U32,     TYPE_U32,     TYPE_U32,     TYPE_U64,     TYPE_F32,     TYPE_F64,     TYPE_INVALID,  },         /* U16 */
    { TYPE_INVALID, TYPE_U32,     TYPE_U32,     TYPE_U32,     TYPE_I64,     TYPE_U32,     TYPE_U32,     TYPE_U32,     TYPE_U64,     TYPE_F32,     TYPE_F64,     TYPE_INVALID,  },         /* U32 */
    { TYPE_INVALID, TYPE_U64,     TYPE_U64,     TYPE_U64,     TYPE_U64,     TYPE_U64,     TYPE_U64,     TYPE_U64,     TYPE_U64,     TYPE_F32,     TYPE_F64,     TYPE_INVALID,  },         /* U64 */
    { TYPE_INVALID, TYPE_F32,     TYPE_F32,     TYPE_F32,     TYPE_F32,     TYPE_F32,     TYPE_F32,     TYPE_F32,     TYPE_F32,     TYPE_F32,     TYPE_F64,     TYPE_INVALID,  },         /* F32 */
    { TYPE_INVALID, TYPE_F64,     TYPE_F64,     TYPE_F64,     TYPE_F64,     TYPE_F64,     TYPE_F64,     TYPE_F64,     TYPE_F64,     TYPE_F64,     TYPE_F64,     TYPE_INVALID,  },         /* F64 */
    { TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID, TYPE_INVALID,  },         /* Function */
};


static const char *sFunction = "function";

typedef struct ArgumentList 
{
    int Count;
    AstExprList *Args;
} ArgumentList;



static void *StmtInit(PascalParser *Parser, UInt Size, AstStmtType Type);
static void StmtEndLine(const PascalParser *Parser, AstStmt *Stmt);

static void ParseType(PascalParser *Parser);
static AstStmtBlock *ParseBeginEndBlock(PascalParser *Parser);
static AstVarList *ParseVarList(PascalParser *Parser, AstVarList *List);
static AstVarBlock *ParseVar(PascalParser *Parser);
static AstFunctionBlock *ParseFunction(PascalParser *Parser, const char *Type);


static AstBeginEndStmt *ParseBeginEndStmt(PascalParser *Parser);
static AstIfStmt *ParseIfStmt(PascalParser *Parser);
static AstForStmt *ParseForStmt(PascalParser *Parser);
static AstWhileStmt *ParseWhileStmt(PascalParser *Parser);
static AstStmt *ParseIdentifierStmt(PascalParser *Parser);
static AstCallStmt *ParseCallStmt(PascalParser *Parser, U32 ID, const AstFunctionBlock *Function);
static AstAssignStmt *ParseAssignStmt(PascalParser *Parser, const PascalVar *IdenInfo);
static AstReturnStmt *ParseReturnStmt(PascalParser *Parser);


static U32 ParserDefineIdentifier(PascalParser *Parser, const Token *TypeName, ParserType Type, void *Data);
static PascalVar *ParserGetIdentifierInfo(PascalParser *Parser, const Token *Identifier, const char *ErrFmt, ...);
static ParserType ParserCoerceTypes(PascalParser *Parser, ParserType Left, ParserType Right);
static void ParserBeginScope(PascalParser *Parser);
static void ParserEndScope(PascalParser *Parser);
static PascalVartab *ParserCurrentScope(PascalParser *Parser);


static AstSimpleExpr ParseSimpleExpr(PascalParser *Parser);
static AstTerm ParseTerm(PascalParser *Parser);
static AstFactor ParseFactor(PascalParser *Parser);
static AstFactor ParseVariable(PascalParser *Parser);
static ArgumentList ParseArgumentList(PascalParser *Parser);


static bool IsAtEnd(const PascalParser *Parser);
static bool NextTokenIs(const PascalParser *Parser, const TokenType Type);
static bool ConsumeIfNextIsOneOf(PascalParser *Parser, UInt Count, const TokenType Types[]);
static bool ConsumeIfNextIs(PascalParser *Parser, TokenType Type);
static void ConsumeToken(PascalParser *Parser);
static bool ConsumeOrError(PascalParser *Parser, TokenType Expected, const char *Fmt, ...);
static ParserType DetermineIntegerSize(U64 Integer);

static void Error(PascalParser *Parser, const char *Fmt, ...);
static void VaListError(PascalParser *Parser, const char *Fmt, va_list VaList);
static void VaListErrorAtToken(PascalParser *Parser, const Token *Tok, const char *Fmt, va_list VaList);
static void Unpanic(PascalParser *Parser);

static const char *ParserTypeToStr(ParserType Type);




PascalParser ParserInit(const U8 *Source, PascalVartab *PredefinedIdentifiers, PascalArena *Arena, FILE *ErrorFile)
{
    PascalParser Parser = {
        .Arena = Arena,
        .Allocator = GPAInit(
                PARSER_MAX_SCOPE 
                * PARSER_VAR_PER_SCOPE
                * sizeof(PascalVar)
        ), /* 8 nested scopes of 256 PascalVars */

        .Lexer = TokenizerInit(Source),
        .PanicMode = false,
        .Error = false,
        .ErrorFile = ErrorFile,
        .Global = PredefinedIdentifiers,
        .Scope = { {0} }, 
        .VariableID = 0,
    };
    return Parser;
}


PascalAst *ParserGenerateAst(PascalParser *Parser)
{
    Parser->Next = TokenizerGetToken(&Parser->Lexer);
    PascalAst *Ast = ArenaAllocateZero(Parser->Arena, sizeof(*Ast));
    Ast->Block = ParseBlock(Parser);

    GPADeinit(&Parser->Allocator);
    if (Parser->Error)
    {
        ParserDestroyAst(Ast);
        return NULL;
    }
    return Ast;
}

void ParserDestroyAst(PascalAst *Ast)
{
    /* The arena owns the Ast, no need to do anything here */
    (void)Ast;
}








AstBlock *ParseBlock(PascalParser *Parser)
{
    AstBlock *BlockHead = NULL;
    AstBlock **I = &BlockHead;
    do {
        switch (Parser->Next.Type)
        {
        case TOKEN_BEGIN: goto BeginEndBlock;
        case TOKEN_TYPE:
        {
            ConsumeToken(Parser);
            ParseType(Parser);
        } break;
        case TOKEN_VAR:
        {
            ConsumeToken(Parser);
            *I = (AstBlock*)ParseVar(Parser);
        } break;
        case TOKEN_CONST:
        {
            ConsumeToken(Parser);
            PASCAL_UNREACHABLE("TODO: CONST");
        } break;
        case TOKEN_LABEL:
        {
            ConsumeToken(Parser);
            PASCAL_UNREACHABLE("TODO: LABEL");
        } break;
        case TOKEN_PROCEDURE:
        {
            ConsumeToken(Parser);
            *I = (AstBlock*)ParseFunction(Parser, "procedure");
        } break;
        case TOKEN_FUNCTION: 
        {
            ConsumeToken(Parser);
            *I = (AstBlock*)ParseFunction(Parser, sFunction);
        } break;

        default:
        {
            Error(Parser, 
                    "Expected 'label', 'const', "
                    "'type', 'var', 'procedure', "
                    "'function' or 'begin' before a block."
            );
            return NULL;
        } break;
        }


        if (NULL != *I)
        {
            I = &(*I)->Next;
        }

        if (Parser->PanicMode)
        {
            Unpanic(Parser);
        }
    } while (!IsAtEnd(Parser));

BeginEndBlock:
    ConsumeOrError(Parser, TOKEN_BEGIN, "Expected 'Begin'.");
    *I = (AstBlock*)ParseBeginEndBlock(Parser);
    return BlockHead;
}


AstStmt *ParseStmt(PascalParser *Parser)
{
    AstStmt *Statement = NULL;
    switch (Parser->Next.Type)
    {
    case TOKEN_BEGIN:
    {
        ConsumeToken(Parser);
        Statement = (AstStmt*)ParseBeginEndStmt(Parser);
    } break;
    case TOKEN_EXIT:
    {
        ConsumeToken(Parser);
        Statement = (AstStmt*)ParseReturnStmt(Parser);
    } break;
    case TOKEN_WHILE:
    {
        ConsumeToken(Parser);
        Statement = (AstStmt*)ParseWhileStmt(Parser);
    } break;
    case TOKEN_FOR:
    {
        ConsumeToken(Parser);
        Statement = (AstStmt*)ParseForStmt(Parser);
    } break;
    case TOKEN_IF:
    {
        ConsumeToken(Parser);
        Statement = (AstStmt*)ParseIfStmt(Parser);
    } break;
    default: 
    {
        Statement = (AstStmt*)ParseIdentifierStmt(Parser);
    } break;
    }


    if (Parser->PanicMode)
    {
        Unpanic(Parser);
    }
    return Statement;
}


AstExpr ParseExpr(PascalParser *Parser)
{
    AstExpr Expression = {0};
    /* leftmost */
    Expression.Left = ParseSimpleExpr(Parser);
    ParserType LastType = Expression.Left.Type;
    AstOpSimpleExpr **Right = &Expression.Right;


    static const TokenType Ops[] = { 
        TOKEN_LESS, TOKEN_GREATER, 
        TOKEN_LESS_EQUAL, TOKEN_GREATER_EQUAL, 
        TOKEN_LESS_GREATER, TOKEN_EQUAL, TOKEN_IN
    };
    while (ConsumeIfNextIsOneOf(Parser, STATIC_ARRAY_SIZE(Ops), Ops))
    {
        (*Right) = ArenaAllocateZero(Parser->Arena, sizeof(**Right));
        (*Right)->Op = Parser->Curr.Type;
        (*Right)->SimpleExpr = ParseSimpleExpr(Parser);
        LastType = ParserCoerceTypes(Parser, LastType, (*Right)->SimpleExpr.Type);
        (*Right)->Type = LastType;
        Right = &(*Right)->Next;
    }
    Expression.Type = LastType;
    return Expression;
}








static void *StmtInit(PascalParser *Parser, UInt Size, AstStmtType Type)
{
    AstStmt *Stmt = ArenaAllocateZero(Parser->Arena, Size);
    Stmt->Type = Type;
    Stmt->Src = Parser->Curr.Str;
    Stmt->Line = Parser->Curr.Line;

    return Stmt;
}

static void StmtEndLine(const PascalParser *Parser, AstStmt *Stmt)
{
    Stmt->Len = Parser->Curr.Str + Parser->Curr.Len - Stmt->Src;
}



static void ParseType(PascalParser *Parser)
{
    /* TODO: parse type properly */
    if (!NextTokenIs(Parser, TOKEN_IDENTIFIER))
    {
        Error(Parser, "Expected at least 1 type definition for a 'type' block.");
        return;
    }

    do {
        ConsumeToken(Parser);
        Token Identifier = Parser->Curr;

        ConsumeOrError(Parser, TOKEN_EQUAL, "Expected '=' after '%.*s'.",
                Identifier.Len, Identifier.Str
        );
        ConsumeOrError(Parser, TOKEN_IDENTIFIER, "Expected type name after '='.");

        //ParserPushTypeName(Parser, &Identifier, &Parser->Curr);

        ConsumeOrError(Parser, TOKEN_SEMICOLON, "Expected ';' after '%.*s'.",
                Parser->Next.Len, Parser->Next.Str
        );
    } while (NextTokenIs(Parser, TOKEN_IDENTIFIER));
}




static AstStmtBlock *ParseBeginEndBlock(PascalParser *Parser)
{
    AstStmtBlock *BeginEndBlock = ArenaAllocateZero(Parser->Arena, sizeof(*BeginEndBlock));
    BeginEndBlock->Base.Type = AST_BLOCK_BEGINEND;
    BeginEndBlock->BeginEnd = ParseBeginEndStmt(Parser);
    return BeginEndBlock;
}




static AstVarList *ParseVarList(PascalParser *Parser, AstVarList *List)
{
    PASCAL_ASSERT(List != NULL, "ParseVarList does not accept NULL");
    /* var
     *      id1, id2, ...: typename;
     */
    ConsumeOrError(Parser, TOKEN_IDENTIFIER, "Expected variable name.");
    Token Variable = Parser->Curr;

    if (ConsumeIfNextIs(Parser, TOKEN_COLON))
    {
        ConsumeOrError(Parser, TOKEN_IDENTIFIER, "Expected type name.");
        PascalVar *Info = ParserGetIdentifierInfo(Parser, &Parser->Curr, "Undefined Type name.");
        if (NULL != Info)
        {
            List->Type = Info->Type;
            List->ID = ParserDefineIdentifier(Parser, &Variable, Info->Type, List);
        }
        return List;
    }
    else if (ConsumeOrError(Parser, TOKEN_COMMA, "Expected ',' or ':' after '%.*s'.", Variable.Len, Variable.Str))
    {
        List->Next = ArenaAllocateZero(Parser->Arena, sizeof(*List->Next));
        AstVarList *RetVal = ParseVarList(Parser, List->Next);
        List->Type = List->Next->Type;
        List->ID = ParserDefineIdentifier(Parser, &Variable, List->Next->Type, List);
        return RetVal;
    }

    return List;
}


static AstVarBlock *ParseVar(PascalParser *Parser)
{
    AstVarBlock *BlockDeclaration = ArenaAllocateZero(Parser->Arena, sizeof(*BlockDeclaration));

    BlockDeclaration->Line = Parser->Next.Line;
    BlockDeclaration->Src = Parser->Next.Str;
    BlockDeclaration->Base.Type = AST_BLOCK_VAR;
    AstVarList *Decl = &BlockDeclaration->Decl;

    Decl = ParseVarList(Parser, Decl);
    ConsumeOrError(Parser, TOKEN_SEMICOLON, "Expected ';' after type name.");

    while (NextTokenIs(Parser, TOKEN_IDENTIFIER))
    {
        Decl->Next = ArenaAllocateZero(Parser->Arena, sizeof(*Decl));
        Decl = ParseVarList(Parser, Decl->Next);

        ConsumeOrError(Parser, TOKEN_SEMICOLON, "Expected ';' after type name.");
    }
    BlockDeclaration->Len = Parser->Curr.Str - BlockDeclaration->Src;

    return BlockDeclaration;
}




static AstFunctionBlock *ParseFunction(PascalParser *Parser, const char *Type)
{
    /*
     * function Iden(param1, param2: typename; param3...: typename): typename;
     * procedure Iden(param, param2: typename; param3...: typename);
     */


    /* TODO: forward decl, nested functions */
    AstFunctionBlock *Function = ArenaAllocateZero(Parser->Arena, sizeof(*Function));
    Function->Base.Type = AST_BLOCK_FUNCTION;

    /* Name */
    ConsumeOrError(Parser, TOKEN_IDENTIFIER, "Expected %s name.", Type);
    Token FunctionName = Parser->Curr;
    Function->ID = ParserDefineIdentifier(Parser, &FunctionName, TYPE_FUNCTION, Function);


    /* args */
    ParserBeginScope(Parser);
    if (ConsumeIfNextIs(Parser, TOKEN_LEFT_PAREN)
    && !ConsumeIfNextIs(Parser, TOKEN_RIGHT_PAREN))
    {
        Function->Params = ArenaAllocateZero(Parser->Arena, sizeof(*Function->Params));
        AstVarList *ArgList = Function->Params;
        do {
            ArgList = ParseVarList(Parser, ArgList);
            if (ConsumeIfNextIs(Parser, TOKEN_RIGHT_PAREN)
            || !ConsumeOrError(Parser, TOKEN_SEMICOLON, "Expected ';' after type name."))
            {
                break;
            }
        } while (!IsAtEnd(Parser));


        /* TODO: array instead of linked list to avoid counting */
        ArgList = Function->Params;
        while (NULL != ArgList)
        {
            Function->ArgCount++;
            ArgList = ArgList->Next;
        }
    }
    Token BeforeColon = Parser->Curr;


    /* return type */
    Token BeforeSemicolon;
    if (Type == sFunction)
    {
        Function->HasReturnType = true;
        ConsumeOrError(Parser, TOKEN_COLON, "Expected ':' after '%.*s'.", BeforeColon.Len, BeforeColon.Str);
        ConsumeOrError(Parser, TOKEN_IDENTIFIER, "Expected function return type.");
        BeforeSemicolon = Parser->Curr;

        PascalVar *ReturnTypeInfo = ParserGetIdentifierInfo(Parser, &Parser->Curr, 
                "Return type of function '%.*s' is not defined.", FunctionName.Len, FunctionName.Str
        );
        if (NULL != ReturnTypeInfo)
        {
            Function->ReturnType = ReturnTypeInfo->Type;
        }
    }
    else 
    {
        Function->HasReturnType = false;
        BeforeSemicolon = Parser->Curr;
    }
    ConsumeOrError(Parser, TOKEN_SEMICOLON, "Expected ';' after '%.*s'", 
            BeforeSemicolon.Len, BeforeSemicolon.Str
    );


    /* TODO: optional Body by detecting 'forward' keyword */
    Function->Block = ParseBlock(Parser);
    ParserEndScope(Parser);
    ConsumeOrError(Parser, TOKEN_SEMICOLON, "Expected ';' after '%.*s'", 
            Parser->Curr.Len, Parser->Curr.Str
    );
    return Function;
}










static AstBeginEndStmt *ParseBeginEndStmt(PascalParser *Parser)
{
    AstBeginEndStmt *BeginEnd = StmtInit(Parser, sizeof(*BeginEnd), AST_STMT_BEGINEND);
    StmtEndLine(Parser, &BeginEnd->Base);

    AstStmtList **Stmts = &BeginEnd->Statements;
    while (!IsAtEnd(Parser) && !NextTokenIs(Parser, TOKEN_END))
    {
        (*Stmts) = ArenaAllocateZero(Parser->Arena, sizeof(**Stmts));
        (*Stmts)->Statement = ParseStmt(Parser);
        ConsumeOrError(Parser, TOKEN_SEMICOLON, "Expected ';' after statement.");
        Stmts = &(*Stmts)->Next;
    }

    ConsumeOrError(Parser, TOKEN_END, "Expected 'end' after statement.");
    BeginEnd->EndLine = Parser->Curr.Line;
    BeginEnd->EndStr = Parser->Curr.Str;
    BeginEnd->EndLen = Parser->Curr.Len;
    return BeginEnd;
}


static AstIfStmt *ParseIfStmt(PascalParser *Parser)
{
    AstIfStmt *IfStmt = StmtInit(Parser, sizeof(*IfStmt), AST_STMT_IF);

    IfStmt->Condition = ParseExpr(Parser);
    ConsumeOrError(Parser, TOKEN_THEN, "Expected 'then' after expression.");
    StmtEndLine(Parser, &IfStmt->Base);

    IfStmt->IfCase = ParseStmt(Parser);
    if (ConsumeIfNextIs(Parser, TOKEN_ELSE))
    {
        IfStmt->ElseCase = ParseStmt(Parser);
    }
    return IfStmt;
}


static AstForStmt *ParseForStmt(PascalParser *Parser)
{
    AstForStmt *ForStmt = StmtInit(Parser, sizeof *ForStmt, AST_STMT_FOR);

    /* loop variable */
    ConsumeOrError(Parser, TOKEN_IDENTIFIER, "Expected identifier after 'for'.");
    PascalVar *LoopVar = ParserGetIdentifierInfo(Parser, &Parser->Curr, "Undefined variable.");
    if (NULL != LoopVar)
    {
        ForStmt->VarType = LoopVar->Type;
        ForStmt->VarID = LoopVar->ID;
    }
    ConsumeOrError(Parser, TOKEN_COLON_EQUAL, "Expected ':=' after variable name.");
    ForStmt->InitExpr = ParseExpr(Parser);


    /* downto/to */
    ForStmt->Comparison = TOKEN_GREATER;
    ForStmt->Imm = -1;
    if (!ConsumeIfNextIs(Parser, TOKEN_DOWNTO))
    {
        ConsumeOrError(Parser, TOKEN_TO, "Expected 'downto' or 'to' after expression.");
        ForStmt->Comparison = TOKEN_LESS;
        ForStmt->Imm = 1;
    }

    /* performs typecheck */
    ForStmt->StopExpr = ParseExpr(Parser);
    (void)ParserCoerceTypes(Parser, 
            ForStmt->VarType,
            ForStmt->StopExpr.Type
    );

    /* do */
    ConsumeOrError(Parser, TOKEN_DO, "Expected 'do' after expression.");
    StmtEndLine(Parser, &ForStmt->Base);


    /* body */
    ForStmt->Stmt = ParseStmt(Parser);
    return ForStmt;
}


static AstWhileStmt *ParseWhileStmt(PascalParser *Parser)
{
    AstWhileStmt *WhileStmt = StmtInit(Parser, sizeof *WhileStmt, AST_STMT_WHILE);

    WhileStmt->Expr = ParseExpr(Parser);
    ConsumeOrError(Parser, TOKEN_DO, "Expected 'do' after expression.");
    StmtEndLine(Parser, &WhileStmt->Base);

    WhileStmt->Stmt = ParseStmt(Parser);
    return WhileStmt;
}


static AstStmt *ParseIdentifierStmt(PascalParser *Parser)
{
    ConsumeOrError(Parser, TOKEN_IDENTIFIER, "Expected identifier.");

    PascalVar *IdenInfo = ParserGetIdentifierInfo(Parser, &Parser->Curr, "Undefined variable.");
    if (NULL != IdenInfo)
    {
        if (TYPE_FUNCTION == IdenInfo->Type)
        {
            return (AstStmt*)ParseCallStmt(Parser, IdenInfo->ID, IdenInfo->Data);
        }
    }
    return (AstStmt*)ParseAssignStmt(Parser, IdenInfo);
}


static AstCallStmt *ParseCallStmt(PascalParser *Parser, U32 ID, const AstFunctionBlock *Function)
{
    AstCallStmt *CallStmt = StmtInit(Parser, sizeof(*CallStmt), AST_STMT_CALL);
    CallStmt->ProcedureID = ID;
    Token FunctionName = Parser->Curr;

    if (ConsumeIfNextIs(Parser, TOKEN_LEFT_PAREN))
    {
        ArgumentList ArgList = ParseArgumentList(Parser);
        
        CallStmt->ArgList = ArgList.Args;
        if (ArgList.Count != Function->ArgCount)
        {
            Error(Parser, "Expected %d arguments but call to function '%.*s' has %d.", 
                    Function->ArgCount, FunctionName.Len, FunctionName.Str, ArgList.Count
            );
        }
        ConsumeOrError(Parser, TOKEN_RIGHT_PAREN, "Expected ')' after expression.");
    }
    /* TODO: check argument count */

    StmtEndLine(Parser, &CallStmt->Base);
    return CallStmt;
}

static AstAssignStmt *ParseAssignStmt(PascalParser *Parser, const PascalVar *IdenInfo)
{
    /* TODO: assignment to function */
    AstAssignStmt *Assignment = StmtInit(Parser, sizeof(*Assignment), AST_STMT_ASSIGNMENT);

    ParserType LhsType = TYPE_INVALID;
    if (NULL != IdenInfo)
    {
        Assignment->VariableID = IdenInfo->ID;
        LhsType = IdenInfo->Type;
    }

    ConsumeOrError(Parser, TOKEN_COLON_EQUAL, "TODO: other assignment operators");

    Assignment->TypeOfAssignment = Parser->Curr.Type;
    Assignment->Expr = ParseExpr(Parser);

    /* typecheck */
    (void)ParserCoerceTypes(Parser, LhsType, Assignment->Expr.Type);
    Assignment->LhsType = LhsType;

    StmtEndLine(Parser, &Assignment->Base);
    return Assignment;
}


static AstReturnStmt *ParseReturnStmt(PascalParser *Parser)
{
    AstReturnStmt *RetStmt = StmtInit(Parser, sizeof(*RetStmt), AST_STMT_RETURN);

    if (ConsumeIfNextIs(Parser, TOKEN_LEFT_PAREN))
    {
        RetStmt->Expr = ArenaAllocateZero(Parser->Arena, sizeof *RetStmt->Expr);
        *RetStmt->Expr = ParseExpr(Parser);
        ConsumeOrError(Parser, TOKEN_RIGHT_PAREN, "Expected ')' after expression.");
    }

    StmtEndLine(Parser, &RetStmt->Base);
    return RetStmt;
}



















static U32 ParserDefineIdentifier(PascalParser *Parser, const Token *TypeName, ParserType Type, void *Data)
{
    U32 ID = Parser->VariableID++;
    VartabSet(ParserCurrentScope(Parser), TypeName->Str, TypeName->Len, Type, ID, Data);
    return ID;
}


static PascalVar *ParserGetIdentifierInfo(PascalParser *Parser, const Token *Identifier, const char *ErrFmt, ...)
{
    U32 Hash = VartabHashStr(Identifier->Str, Identifier->Len);
    PascalVar *Info = VartabFindWithHash(ParserCurrentScope(Parser), 
            Identifier->Str, Identifier->Len, Hash
    );
    if (NULL == Info)
    {
        Info = VartabFindWithHash(Parser->Global, 
                Identifier->Str, Identifier->Len, Hash
        );
    }

    if (NULL == Info)
    {
        for (int i = Parser->ScopeCount - 2; i >= 0; i--)
        {
            Info = VartabFindWithHash(&Parser->Scope[i], 
                    Identifier->Str, Identifier->Len, Hash
            );
            if (NULL != Info) 
                return Info;
        }

        va_list ArgList;
        va_start(ArgList, ErrFmt);
        VaListErrorAtToken(Parser, Identifier, ErrFmt, ArgList);
        va_end(ArgList);
        return NULL;
    }
    return Info;
}

static ParserType ParserCoerceTypes(PascalParser *Parser, ParserType Left, ParserType Right)
{
    PASCAL_ASSERT(Left >= TYPE_INVALID && Right >= TYPE_INVALID, "Unreachable");
    if (Left > TYPE_COUNT || Right > TYPE_COUNT)
    {
        Error(Parser, "Invalid combination of type %d and %d", 
                ParserTypeToStr(Left), ParserTypeToStr(Right)
        );
        return TYPE_INVALID;
    }
    return sCoercionRules[Left][Right];
}


static void ParserBeginScope(PascalParser *Parser)
{
    PascalVartab *NewScope = &Parser->Scope[Parser->ScopeCount++];
    PASCAL_ASSERT(Parser->ScopeCount <= PARSER_MAX_SCOPE, "Too many scopes");

    if (0 == NewScope->Cap)
    {
        *NewScope = VartabInit(&Parser->Allocator, PARSER_VAR_PER_SCOPE);
    }
    else
    {
        VartabReset(NewScope);
    }
}

static void ParserEndScope(PascalParser *Parser)
{
    Parser->ScopeCount--;
}

static PascalVartab *ParserCurrentScope(PascalParser *Parser)
{
    if (0 == Parser->ScopeCount)
    {
        return Parser->Global;
    }
    else 
    {
        return &Parser->Scope[Parser->ScopeCount - 1];
    }
}










static AstSimpleExpr ParseSimpleExpr(PascalParser *Parser)
{
    AstSimpleExpr SimpleExpression = {0};

    /* prefixes */
    static const TokenType PrefixOps[] = { TOKEN_PLUS, TOKEN_MINUS };
    UInt NegateCount = 0;
    while (ConsumeIfNextIsOneOf(Parser, STATIC_ARRAY_SIZE(PrefixOps), PrefixOps))
    {
        if (TOKEN_MINUS == Parser->Curr.Type)
            NegateCount++;
    }
    SimpleExpression.Negated = NegateCount % 2;


    /* left */
    SimpleExpression.Left = ParseTerm(Parser);
    ParserType LastType = SimpleExpression.Left.Type;
    AstOpTerm **Right = &SimpleExpression.Right;

    static const TokenType Ops[] = { TOKEN_MINUS, TOKEN_PLUS, TOKEN_OR };
    while (ConsumeIfNextIsOneOf(Parser, STATIC_ARRAY_SIZE(Ops), Ops))
    {
        (*Right) = ArenaAllocateZero(Parser->Arena, sizeof(**Right));
        (*Right)->Op = Parser->Curr.Type;
        (*Right)->Term = ParseTerm(Parser);
        LastType = ParserCoerceTypes(Parser, LastType, (*Right)->Term.Type);
        (*Right)->Type = LastType;
        Right = &(*Right)->Next;
    }
    SimpleExpression.Type = LastType;
    return SimpleExpression;
}

static AstTerm ParseTerm(PascalParser *Parser)
{
    AstTerm CurrentTerm = {0};
    /* left oper */
    CurrentTerm.Left = ParseFactor(Parser);
    ParserType LastType = CurrentTerm.Left.Type;
    AstOpFactor **Right = &CurrentTerm.Right;

    static const TokenType InfixOps[] = { TOKEN_STAR, TOKEN_SLASH, TOKEN_DIV, TOKEN_MOD, TOKEN_AND };
    while (ConsumeIfNextIsOneOf(Parser, STATIC_ARRAY_SIZE(InfixOps), InfixOps))
    {
        (*Right) = ArenaAllocateZero(Parser->Arena, sizeof(**Right));
        (*Right)->Op = Parser->Curr.Type;
        (*Right)->Factor = ParseFactor(Parser);
        LastType = ParserCoerceTypes(Parser, LastType, (*Right)->Factor.Type);
        (*Right)->Type = LastType;
        Right = &(*Right)->Next;
    }
    CurrentTerm.Type = LastType;
    return CurrentTerm;
}

static AstFactor ParseFactor(PascalParser *Parser)
{
    AstFactor Factor = {0};
    switch (Parser->Next.Type)
    {
    case TOKEN_INTEGER_LITERAL:
    {
        ConsumeToken(Parser);
        Factor.FactorType = FACTOR_INTEGER;
        Factor.Type = DetermineIntegerSize(Parser->Curr.Literal.Int);
        Factor.As.Integer = Parser->Curr.Literal.Int;
    } break;
    case TOKEN_NUMBER_LITERAL:
    {
        ConsumeToken(Parser);
        Factor.FactorType = FACTOR_REAL;
        Factor.Type = TYPE_F32;
        Factor.As.Real = Parser->Curr.Literal.Real;
    } break;
    case TOKEN_LEFT_PAREN:
    {
        ConsumeToken(Parser);
        AstExpr Expression = ParseExpr(Parser);
        if (ConsumeOrError(Parser, TOKEN_RIGHT_PAREN, "Expected ')' after expression."))
        {
            Factor.FactorType = FACTOR_GROUP_EXPR;
            Factor.Type = Expression.Type;
            Factor.As.Expression = ArenaAllocateZero(Parser->Arena, sizeof(*Factor.As.Expression));
            *Factor.As.Expression = Expression;
        }
    } break;
    case TOKEN_NOT:
    {
        ConsumeToken(Parser);
        Factor.FactorType = FACTOR_NOT;
        Factor.As.NotFactor = ArenaAllocateZero(Parser->Arena, sizeof(*Factor.As.NotFactor));
        *Factor.As.NotFactor = ParseFactor(Parser);
        Factor.Type = Factor.As.NotFactor->Type;
    } break;
    case TOKEN_IDENTIFIER:
    {
        Factor = ParseVariable(Parser);
    } break;

    default: 
    {
        Error(Parser, "Expected expression");
    } break;

    }
    return Factor;
}



static AstFactor ParseVariable(PascalParser *Parser)
{
    AstFactor Factor = { 0 };
    ConsumeToken(Parser);
    Token Identifier = Parser->Curr;

    PascalVar *Info = ParserGetIdentifierInfo(Parser, 
            &Identifier, "Undefined identifier."
    );
    if (NULL != Info)
    {
        Factor.Type = Info->Type;
        if (TYPE_FUNCTION == Info->Type)
        {
            Factor.FactorType = FACTOR_CALL;
            if (ConsumeIfNextIs(Parser, TOKEN_LEFT_PAREN))
            {
                const AstFunctionBlock *Function = Info->Data;
                if (Function->HasReturnType)
                {
                    Factor.Type = Function->ReturnType;
                }
                Factor.As.Function.ID = Info->ID;
                ArgumentList ArgList = ParseArgumentList(Parser);
                Factor.As.Function.ArgList = ArgList.Args;

                if (ArgList.Count != Function->ArgCount)
                {
                    Error(Parser, "Expected %d arguments but call to '%.*s' has %d.", 
                            Function->ArgCount, Info->Len, Info->Str, ArgList.Count
                    );
                }
            }
        }
        else
        {
            Factor.As.VarID = Info->ID;
            Factor.FactorType = FACTOR_VARIABLE;
        }
    }
    return Factor;
}



static ArgumentList ParseArgumentList(PascalParser *Parser)
{
    ArgumentList ArgList = { 0 };
    AstExprList **Current = &ArgList.Args;
    while (!IsAtEnd(Parser))
    {
        *Current = ArenaAllocate(Parser->Arena, sizeof **Current);
        (*Current)->Expr = ParseExpr(Parser);
        ArgList.Count++;
        if (ConsumeIfNextIs(Parser, TOKEN_RIGHT_PAREN))
        {
            break;
        }
        ConsumeOrError(Parser, TOKEN_COMMA, "Expected ',' or ')' after expression.");
        Current = &(*Current)->Next;
    }
    return ArgList;
}







static bool IsAtEnd(const PascalParser *Parser)
{
    return Parser->Next.Type == TOKEN_EOF;
}

static bool NextTokenIs(const PascalParser *Parser, const TokenType Type)
{
    return Parser->Next.Type == Type;
}

static bool ConsumeIfNextIsOneOf(PascalParser *Parser, UInt Count, const TokenType Types[])
{
    for (UInt i = 0; i < Count; i++)
    {
        if (Parser->Next.Type == Types[i])
        {
            ConsumeToken(Parser);
            return true;
        }
    }
    return false;
}

static bool ConsumeIfNextIs(PascalParser *Parser, TokenType Type)
{
    if (Parser->Next.Type == Type)
    {
        ConsumeToken(Parser);
        return true;
    }
    return false;
}

static void ConsumeToken(PascalParser *Parser)
{
    Parser->Curr = Parser->Next;
    Parser->Next = TokenizerGetToken(&Parser->Lexer);
}


static bool ConsumeOrError(PascalParser *Parser, TokenType Expected, const char *Fmt, ...)
{
    if (!ConsumeIfNextIs(Parser, Expected))
    {
        va_list VaList;
        va_start(VaList, Fmt);
        VaListError(Parser, Fmt, VaList);
        va_end(VaList);
        return false;
    }
    return true;
}



static ParserType DetermineIntegerSize(U64 Integer)
{
    if (IN_I8(Integer))
        return TYPE_I8;
    if (IN_U8(Integer))
        return TYPE_U8;
    if (IN_I16(Integer))
        return TYPE_I16;
    if (IN_U16(Integer))
        return TYPE_U16;
    if (IN_I32(Integer))
        return TYPE_I32;
    if (IN_U32(Integer))
        return TYPE_U32;
    return TYPE_U64;
}






static void Error(PascalParser *Parser, const char *Fmt, ...)
{
    va_list VaList;
    va_start(VaList, Fmt);
    VaListError(Parser, Fmt, VaList);
    va_end(VaList);
}

static void VaListError(PascalParser *Parser, const char *Fmt, va_list VaList)
{
    Parser->Error = true;
    if (!Parser->PanicMode)
    {
        Parser->PanicMode = true;
        fprintf(Parser->ErrorFile, "Parser [line %d]: '%.*s'\n    ", 
                Parser->Next.Line, Parser->Next.Len, Parser->Next.Str);
        vfprintf(Parser->ErrorFile, Fmt, VaList);
        fputc('\n', Parser->ErrorFile);
    }
}

static void VaListErrorAtToken(PascalParser *Parser, const Token *Tok, const char *Fmt, va_list VaList)
{
    Parser->Error = true;
    if (!Parser->PanicMode)
    {
        Parser->PanicMode = true;
        fprintf(Parser->ErrorFile, "Parser [line %d]: '%.*s'\n    ", 
                Tok->Line, Tok->Len, Tok->Str
        );
        vfprintf(Parser->ErrorFile, Fmt, VaList);
        fputc('\n', Parser->ErrorFile);
    }
}


static void Unpanic(PascalParser *Parser)
{
    Parser->PanicMode = false;
    while (!IsAtEnd(Parser))
    {
        if (Parser->Next.Type == TOKEN_SEMICOLON)
            return;

        if (Parser->Curr.Type == TOKEN_SEMICOLON)
        {
            switch (Parser->Next.Type)
            {
            case TOKEN_LABEL:
            case TOKEN_CONST:
            case TOKEN_TYPE:
            case TOKEN_VAR:
            case TOKEN_PROCEDURE:
            case TOKEN_FUNCTION:
            case TOKEN_BEGIN:
            case TOKEN_END:

            case TOKEN_IF:
            case TOKEN_FOR:
            case TOKEN_WHILE:
                return;

            default: break;
            }
        }
        ConsumeToken(Parser);
    }
}


static const char *ParserTypeToStr(ParserType Type)
{
    static const char *StrLut[] = {
        [TYPE_INVALID] = "invalid",
        [TYPE_I8] = "int8",
        [TYPE_I16] = "int16",
        [TYPE_I32] = "int32",
        [TYPE_I64] = "int64",
        [TYPE_U8] = "uint8",
        [TYPE_U16] = "uint16",
        [TYPE_U32] = "uint32",
        [TYPE_U64] = "uint64",
        [TYPE_F32] = "float",
        [TYPE_F64] = "double",
        [TYPE_FUNCTION] = "function",
    };
    PASCAL_STATIC_ASSERT(TYPE_COUNT == STATIC_ARRAY_SIZE(StrLut), "");
    if (Type < TYPE_COUNT)
        return StrLut[Type];
    return "invalid";
}


