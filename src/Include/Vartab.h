#ifndef PASCAL_VAR_H
#define PASCAL_VAR_H



#include "Common.h"
#include "PascalString.h"



#define VARTAB_MAX_LOAD 3/4
#define VARTAB_GROW_FACTOR 2 /* should be powers of 2 */


typedef enum VarType 
{
    VARTYPE_DEAD = 0,
    VARTYPE_SHORT_INT,
    VARTYPE_F32,
} VarType;

typedef struct PascalVar
{
    const U8 *Str;
    UInt Len;
    U32 Hash;
    VarType Type;
} PascalVar;

typedef struct PascalVartab 
{
    PascalVar *Table;
    U32 Cap, Count;
} PascalVartab;


PascalVartab VartabInit(U32 InitialCap);
void VartabDeinit(PascalVartab *Vartab);

PascalVar *VartabFind(PascalVartab *Vartab, const U8 *Key, UInt Len);
VarType *VartabGet(PascalVartab *Vartab, const U8 *Key, UInt Len);
bool VartabSet(PascalVartab *Vartab, const U8 *Key, UInt Len, VarType Type);
bool VartabDelete(PascalVartab *Vartab, const U8 *Key, UInt Len);


#endif /* PASCAL_VAR_H */

