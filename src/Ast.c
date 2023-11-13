

#include <string.h> /* strlen */
#include "Ast.h"




static void PAstPrintExpr(FILE *f, const AstExpr *Expression);
static void PAstPrintSimpleExpr(FILE *f, const AstSimpleExpr *Simple);
static void PAstPrintTerm(FILE *f, const AstTerm *Term);
static void PAstPrintFactor(FILE *f, const AstFactor *Factor);




void PAstPrint(FILE *f, const PascalAst *PAst)
{
    (void)f, (void)PAst;
    PASCAL_UNREACHABLE("TODO: PAstPrint");
}


static void PAstPrintExpr(FILE *f, const AstExpr *Expression)
{
    PAstPrintSimpleExpr(f, &Expression->Left);
    const AstOpSimpleExpr *Right = Expression->Right;
    while (NULL != Right)
    {
        fprintf(f, "%s ", TokenTypeToStr(Right->Op));
        PAstPrintSimpleExpr(f, &Right->SimpleExpr);
        Right = Right->Next;
    }
}


static void PAstPrintSimpleExpr(FILE *f, const AstSimpleExpr *Simple)
{
    PAstPrintTerm(f, &Simple->Left);
    const AstOpTerm *Term = Simple->Right;
    while (NULL != Term)
    {
        fprintf(f, "%s ", TokenTypeToStr(Term->Op));
        PAstPrintTerm(f, &Term->Term);
        Term = Term->Next;
    }
}


static void PAstPrintTerm(FILE *f, const AstTerm *Term)
{
    PAstPrintFactor(f, &Term->Left);
    const AstOpFactor *Factor = Term->Right;
    while (NULL != Factor)
    {
        fprintf(f, "%s ", TokenTypeToStr(Factor->Op));
        PAstPrintFactor(f, &Factor->Factor);
        Factor = Factor->Next;
    }
}

static void PAstPrintFactor(FILE *f, const AstFactor *Factor)
{
    switch (Factor->FactorType)
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

