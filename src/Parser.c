

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



static void ParseType(PascalParser *Parser);
static AstStmtBlock *ParseBeginEndBlock(PascalParser *Parser);
static AstVarBlock *ParseVar(PascalParser *Parser);
static AstFunctionBlock *ParseFunction(PascalParser *Parser, const char *Type);


static AstBeginEndStmt *ParseBeginEndStmt(PascalParser *Parser);
static AstIfStmt *ParseIfStmt(PascalParser *Parser);
static AstForStmt *ParseForStmt(PascalParser *Parser);
static AstWhileStmt *ParseWhileStmt(PascalParser *Parser);
static AstAssignStmt *ParseAssignStmt(PascalParser *Parser);
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
static AstExprList *ParseArgumentList(PascalParser *Parser);


static bool IsAtEnd(const PascalParser *Parser);
static bool NextTokenIs(const PascalParser *Parser, const TokenType Type);
static bool ConsumeIfNextIsOneOf(PascalParser *Parser, UInt Count, const TokenType Types[]);
static bool ConsumeIfNextIs(PascalParser *Parser, TokenType Type);
static void ConsumeToken(PascalParser *Parser);
static bool ConsumeOrError(PascalParser *Parser, TokenType Expected, const char *Fmt, ...);
static ParserType DetermineIntegerSize(U64 Integer);

static void Error(PascalParser *Parser, const char *Fmt, ...);
static void VaListError(PascalParser *Parser, const char *Fmt, va_list VaList);
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

        if (Parser->PanicMode)
        {
            Unpanic(Parser);
        }

        if (NULL != *I)
        {
            I = &(*I)->Next;
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
        Statement = (AstStmt*)ParseAssignStmt(Parser);
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











static void ParseType(PascalParser *Parser)
{
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
    AstStmtBlock *Statements = ArenaAllocateZero(Parser->Arena, sizeof(*Statements));
    Statements->Base.Type = AST_BLOCK_STATEMENTS;
    AstStmtList **CurrStmt = &Statements->Statements;

    while (!IsAtEnd(Parser) && !ConsumeIfNextIs(Parser, TOKEN_END))
    {
        *CurrStmt = ArenaAllocateZero(Parser->Arena, sizeof(**CurrStmt));
        (*CurrStmt)->Statement = ParseStmt(Parser);
        ConsumeOrError(Parser, TOKEN_SEMICOLON, "Expected ';' after statement.");
        CurrStmt = &(*CurrStmt)->Next;
    }
    return Statements;
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
    else 
    {
        ConsumeOrError(Parser, TOKEN_COMMA, "Expected ',' after variable name.");

        List->Next = ArenaAllocateZero(Parser->Arena, sizeof(*List->Next));
        AstVarList *RetVal = ParseVarList(Parser, List->Next);
        List->Type = List->Next->Type;
        List->ID = ParserDefineIdentifier(Parser, &Variable, List->Next->Type, List);
        return RetVal;
    }
}


static AstVarBlock *ParseVar(PascalParser *Parser)
{
    AstVarBlock *BlockDeclaration = ArenaAllocateZero(Parser->Arena, sizeof(*BlockDeclaration));
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
    Function->ID = ParserDefineIdentifier(Parser, &Parser->Curr, TYPE_FUNCTION, Function);



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
                "Return type of '%.*s' is not defined.", FunctionName.Len, FunctionName.Str
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


    /* Body (Opt) */
    Function->Block = ParseBlock(Parser);
    ParserEndScope(Parser);
    ConsumeOrError(Parser, TOKEN_SEMICOLON, "Expected ';' after 'end'");
    return Function;
}










static AstBeginEndStmt *ParseBeginEndStmt(PascalParser *Parser)
{
    AstBeginEndStmt *BeginEnd = ArenaAllocateZero(Parser->Arena, sizeof(*BeginEnd));
    BeginEnd->Base.Type = AST_STMT_BEGINEND;
    AstStmtList **Stmts = &BeginEnd->Statements;

    while (!IsAtEnd(Parser) && !NextTokenIs(Parser, TOKEN_END))
    {
        (*Stmts) = ArenaAllocateZero(Parser->Arena, sizeof(**Stmts));
        (*Stmts)->Statement = ParseStmt(Parser);
        ConsumeOrError(Parser, TOKEN_SEMICOLON, "Expected ';' after statement.");
        Stmts = &(*Stmts)->Next;
    }

    ConsumeOrError(Parser, TOKEN_END, "Expected 'end' after statement.");
    return BeginEnd;
}


static AstIfStmt *ParseIfStmt(PascalParser *Parser)
{
    AstIfStmt *IfStmt = ArenaAllocateZero(Parser->Arena, sizeof(*IfStmt));
    IfStmt->Base.Type = AST_STMT_IF;

    IfStmt->Condition = ParseExpr(Parser);
    ConsumeOrError(Parser, TOKEN_THEN, "Expected 'then' after expression.");

    IfStmt->IfCase = ParseStmt(Parser);
    if (ConsumeIfNextIs(Parser, TOKEN_ELSE))
    {
        IfStmt->ElseCase = ParseStmt(Parser);
    }
    return IfStmt;
}


static AstForStmt *ParseForStmt(PascalParser *Parser)
{
    AstForStmt *ForStmt = ArenaAllocateZero(Parser->Arena, sizeof *ForStmt);
    ForStmt->Base.Type = AST_STMT_FOR;

    ForStmt->InitStmt = ParseAssignStmt(Parser);
    ForStmt->Comparison = TOKEN_GREATER;
    ForStmt->Imm = -1;
    if (!ConsumeIfNextIs(Parser, TOKEN_DOWNTO))
    {
        ConsumeOrError(Parser, TOKEN_TO, "Expected 'downto' or 'to' after expression.");
        ForStmt->Comparison = TOKEN_LESS;
        ForStmt->Imm = 1;
    }

    ForStmt->StopExpr = ParseExpr(Parser);
    ConsumeOrError(Parser, TOKEN_DO, "Expected 'do' after expression.");
    ForStmt->StopExpr.Type = ParserCoerceTypes(Parser, 
            ForStmt->InitStmt->LhsType, 
            ForStmt->StopExpr.Type
    );

    ForStmt->Stmt = ParseStmt(Parser);
    return ForStmt;
}


static AstWhileStmt *ParseWhileStmt(PascalParser *Parser)
{
    AstWhileStmt *WhileStmt = ArenaAllocateZero(Parser->Arena, sizeof *WhileStmt);
    WhileStmt->Base.Type = AST_STMT_WHILE;

    WhileStmt->Expr = ParseExpr(Parser);
    ConsumeOrError(Parser, TOKEN_DO, "Expected 'do' after expression.");

    WhileStmt->Stmt = ParseStmt(Parser);
    return WhileStmt;
}


static AstAssignStmt *ParseAssignStmt(PascalParser *Parser)
{
    AstAssignStmt *Assignment = ArenaAllocateZero(Parser->Arena, sizeof(*Assignment));
    Assignment->Base.Type = AST_STMT_ASSIGNMENT;

    /* TODO: assignment to function */
    ConsumeOrError(Parser, TOKEN_IDENTIFIER, "Expected identifier.");
    PascalVar *Lhs = ParserGetIdentifierInfo(Parser, &Parser->Curr, "Assignment target is undefined.");
    Assignment->VariableID = VAR_ID_INVALID;
    ParserType LhsType = TYPE_INVALID;
    if (NULL != Lhs)
    {
        Assignment->VariableID = Lhs->ID;
        LhsType = Lhs->Type;
    }

    ConsumeOrError(Parser, TOKEN_COLON_EQUAL, "TODO: other assignment operators");

    Assignment->TypeOfAssignment = Parser->Curr.Type;
    Assignment->Expr = ParseExpr(Parser);

    /* typecheck */
    (void)ParserCoerceTypes(Parser, LhsType, Assignment->Expr.Type);
    Assignment->LhsType = LhsType;
    return Assignment;
}


static AstReturnStmt *ParseReturnStmt(PascalParser *Parser)
{
    AstReturnStmt *RetStmt = ArenaAllocateZero(Parser->Arena, sizeof(*RetStmt));
    RetStmt->Base.Type = AST_STMT_RETURN;
    if (ConsumeIfNextIs(Parser, TOKEN_LEFT_PAREN))
    {
        RetStmt->Expr = ArenaAllocateZero(Parser->Arena, sizeof *RetStmt->Expr);
        *RetStmt->Expr = ParseExpr(Parser);
        ConsumeOrError(Parser, TOKEN_RIGHT_PAREN, "Expected ')' after expression.");
    }
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
        VaListError(Parser, ErrFmt, ArgList);
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
        Factor.As.VarID = Info->ID;
        if (TYPE_FUNCTION == Info->Type)
        {
            Factor.FactorType = FACTOR_CALL;
            Factor.As.CallArgList = ParseArgumentList(Parser);
        }
        else
        {
            Factor.FactorType = FACTOR_VARIABLE;
        }
    }
    return Factor;
}



static AstExprList *ParseArgumentList(PascalParser *Parser)
{
    AstExprList *Args = NULL;
    AstExprList **Current = &Args;
    while (!ConsumeIfNextIs(Parser, TOKEN_RIGHT_PAREN))
    {
        *Current = ArenaAllocate(Parser->Arena, sizeof **Current);
        (*Current)->Expr = ParseExpr(Parser);
        Current = &(*Current)->Next;
    }
    return Args;
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


static void Unpanic(PascalParser *Parser)
{
    Parser->PanicMode = false;
    while (!IsAtEnd(Parser))
    {
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
        [TYPE_F32] = "Ffoat",
        [TYPE_F64] = "Double",
        [TYPE_FUNCTION] = "function",
    };
    PASCAL_STATIC_ASSERT(TYPE_COUNT == STATIC_ARRAY_SIZE(StrLut), "");
    if (Type < TYPE_COUNT)
        return StrLut[Type];
    return "invalid";
}


