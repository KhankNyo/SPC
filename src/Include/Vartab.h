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

/* Find the entry that has the given key,
 * returns  NULL if no such entry exists,
 *          the pointer to the entry containing the key */
PascalVar *VartabFind(PascalVartab *Vartab, 
        const U8 *Key, UInt Len
);

/* Get the pointer to the value of an entry,
 * returns  NULL if no entry with the given key exists, or was deleted,
 *          poiter to the type of the entry if the key exists */
VarType *VartabGet(PascalVartab *Vartab, 
        const U8 *Key, UInt Len
);

/* Override an entry with the key and type 
 * returns  true if the entry already exist before,
 *          false if a brand new entry is created */
bool VartabSet(PascalVartab *Vartab, 
        const U8 *Key, UInt Len, 
        VarType Type
);

/* Deletes an entry from the table,
 * returns  true if the entry exists before deletion,
 *          or false otherwise */
bool VartabDelete(PascalVartab *Vartab, const U8 *Key, UInt Len);


#endif /* PASCAL_VAR_H */

