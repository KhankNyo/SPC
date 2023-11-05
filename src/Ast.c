
#include "Ast.h"


static void PAstPrintExpr(FILE *f, const AstExpr *Expression);
static void PAstPrintSimpleExpr(FILE *f, const AstSimpleExpr *Simple);
static void PAstPrintTerm(FILE *f, const AstTerm *Term);
static void PAstPrintFactor(FILE *f, const AstFactor *Factor);




void PAstPrint(FILE *f, const PascalAst *PAst)
{
    PAstPrintExpr(f, &PAst->Expression);
}


static void PAstPrintExpr(FILE *f, const AstExpr *Expression)
{
    PAstPrintSimpleExpr(f, &Expression->Left);
    const AstExpr *CurrentExpr = Expression->Right;
    while (NULL != CurrentExpr)
    {
        fprintf(f, "%.*s ", CurrentExpr->InfixOp.Len, CurrentExpr->InfixOp.Str);
        PAstPrintSimpleExpr(f, &CurrentExpr->Left);
        CurrentExpr = CurrentExpr->Right;
    }
}


static void PAstPrintSimpleExpr(FILE *f, const AstSimpleExpr *Simple)
{
    PAstPrintTerm(f, &Simple->Left);
    const AstSimpleExpr *SimpleExpr = Simple->Right;
    while (NULL != SimpleExpr)
    {
        fprintf(f, "%.*s ", SimpleExpr->InfixOp.Len, SimpleExpr->InfixOp.Str);
        PAstPrintTerm(f, &SimpleExpr->Left);
        SimpleExpr = SimpleExpr->Right;
    }
}


static void PAstPrintTerm(FILE *f, const AstTerm *Term)
{
    PAstPrintFactor(f, &Term->Left);
    const AstTerm *CurrentTerm = Term->Right;
    while (NULL != CurrentTerm)
    {
        fprintf(f, "%.*s ", CurrentTerm->InfixOp.Len, CurrentTerm->InfixOp.Str);
        PAstPrintFactor(f, &CurrentTerm->Left);
        CurrentTerm = CurrentTerm->Right;
    }
}

static void PAstPrintFactor(FILE *f, const AstFactor *Factor)
{
    switch (Factor->Type)
    {
    case FACTOR_GROUP_EXPR:
    {
        fprintf(f, "( ");
        PAstPrintExpr(f, Factor->As.Expression);
        fprintf(f, ") ");
    } break;

    case FACTOR_INTEGER:
    {
        fprintf(f, "%llu ", Factor->As.Integer); 
    } break;

    case FACTOR_REAL:
    {
        fprintf(f, "%g ", Factor->As.Real);
    } break;

    case FACTOR_VARIABLE:
    {
    } break;

    case FACTOR_NOT:
    {
        fprintf(f, "Not ");
        PAstPrintFactor(f, Factor->As.NotFactor);
    } break;

    case FACTOR_CALL:
    {
    } break;

    case FACTOR_INVALID:
    {
        PASCAL_UNREACHABLE("Invalid factor\n");
    } break;

    }
}

