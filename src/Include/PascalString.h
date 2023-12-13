#ifndef PASCAL_STRING_H
#define PASCAL_STRING_H 


#include "Common.h"
#include <string.h>


#define PSTR_MAX_LEN 255

union PascalStr 
{
    struct {
        U8 Buf[PSTR_MAX_LEN];
        U8 LenLeft;
    } FbString;
    U8 Data[PSTR_MAX_LEN + 1];
};


#define PSTR_LITERAL(Literal) (PascalStr) {\
    .FbString.Buf = Literal, \
    .FbString.LenLeft = sizeof(Literal) - 1\
}

static inline bool PStrIsDyn(const PascalStr *PStr) { UNUSED(PStr, PStr); return false; }

static inline U8 *PStrGetPtr(PascalStr *PStr) { return PStr->FbString.Buf; }
static inline const U8 *PStrGetConstPtr(const PascalStr *PStr) { return PStr->FbString.Buf; }

static inline USize PStrGetLen(const PascalStr *PStr) { return PSTR_MAX_LEN - PStr->FbString.LenLeft; }
static inline USize PStrGetCap(const PascalStr *PStr) { UNUSED(PStr, PStr); return PSTR_MAX_LEN; }
static inline void PStrReserve(PascalStr *PStr, USize NewCapacity) { UNUSED(PStr, NewCapacity); }
void PStrSetLen(PascalStr *PStr, USize Len);
USize PStrAddToLen(PascalStr *PStr, ISize Extra);


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
static inline USize PStrConcat(PascalStr *Dst, const PascalStr *Src) 
{ 
    return PStrAppendStr(Dst, PStrGetConstPtr(Src), PStrGetLen(Src)); 
}
static inline void PStrCopyInto(PascalStr *Dst, const PascalStr *Src)
{
    USize Len = PStrGetLen(Src);
    memcpy(PStrGetPtr(Dst), PStrGetConstPtr(Src), Len);
    PStrSetLen(Dst, Len);
}


#endif /* PASCAL_STRING_H */

