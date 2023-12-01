#ifndef PASCAL_STRING_H
#define PASCAL_STRING_H 


#include "Common.h"


typedef struct PascalDynStr 
{
    U8 *Str;
    USize Cap;
    USize Len;
} PascalDynStr;

#define PSTR_MAX_LOCAL_LEN (sizeof(PascalDynStr) - 1)
#define PSTR_DYN_ISDYNBIT ((USize)1 << (sizeof(USize)*8 - 1))
#define PSTR_LOC_ISDYNBIT (0x80)
typedef struct PascalLocStr 
{
    U8 Str[PSTR_MAX_LOCAL_LEN];
    U8 LenLeft;
} PascalLocStr;


typedef struct PascalStr 
{
    union {
        PascalDynStr Dyn;
        PascalLocStr Loc;
    };
} PascalStr;

PASCAL_STATIC_ASSERT(sizeof(PascalDynStr) == sizeof(PascalLocStr), "Incompatible sizes");



bool PStrIsDyn(const PascalStr *PStr);

U8 *PStrGetPtr(PascalStr *PStr);
const U8 *PStrGetConstPtr(const PascalStr *PStr);

USize PStrGetLen(const PascalStr *PStr);
USize PStrGetCap(const PascalStr *PStr);
USize PStrAddToLen(PascalStr *PStr, ISize Extra);
USize PStrReserve(PascalStr *PStr, USize NewCapacity);


PascalStr PStrInitReserved(USize Len, USize Extra);
PascalStr PStrCopyReserved(const U8 *Str, USize Len, USize Extra);
PascalStr PStrInit(USize Len);
PascalStr PStrCopy(const U8 *Str, USize Len);
void PStrDeinit(PascalStr *PStr);


/* returns true if s1 == s2, else returns false */
bool PStrEqu(const PascalStr *s1, const PascalStr *s2);
/* returns true is s1 is lexicographically less than s2, else returns false */
bool PStrIsLess(const PascalStr *s1, const PascalStr *s2);


U8 PStrAppendChr(PascalStr *PStr, U8 Chr);
USize PStrAppendStr(PascalStr *PStr, const U8 *Str, USize Len);


#endif /* PASCAL_STRING_H */

