#ifndef PASCAL_VAR_H
#define PASCAL_VAR_H



#include "Common.h"
#include "Memory.h"
#include "IntegralTypes.h"
typedef struct PascalVar PascalVar;
typedef struct VarLocation VarLocation;



#define VARTAB_MAX_LOAD 3/4
#define VARTAB_GROW_FACTOR 2 /* should be powers of 2 */

typedef struct PascalVartab 
{
    PascalVar *Table;
    ISize Cap, Count;
    PascalGPA *Allocator;
} PascalVartab;


PascalVartab VartabInit(PascalGPA *Allocator, ISize InitialCap);
PascalVartab VartabPredefinedIdentifiers(PascalGPA *Allocator, ISize InitialCap);
void VartabDeinit(PascalVartab *Vartab);
void VartabReset(PascalVartab *Vartab);

/* Find the entry that has the given key and hash,
 * returns  NULL if no such entry exists,
 *          the pointer to the entry containing the key */
PascalVar *VartabFindWithHash(PascalVartab *Vartab, 
        const U8 *Key, UInt Len, U32 Hash
);

/* Override an entry with the key and type 
 * returns the entry, never null
 */
PascalVar *VartabSet(PascalVartab *Vartab, 
        const U8 *Key, UInt Len, U32 Line,
        IntegralType Type, VarLocation *Location
);


/* Deletes an entry from the table,
 * returns the entry deleted, or NULL if the key did not exist
 */
PascalVar *VartabDelete(PascalVartab *Vartab, const U8 *Key, UInt Len);


/* hashes a string */
U32 VartabHashStr(const U8 *Str, UInt Len);


#endif /* PASCAL_VAR_H */

