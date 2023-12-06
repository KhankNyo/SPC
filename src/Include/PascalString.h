#ifndef PASCAL_STRING_H
#define PASCAL_STRING_H 


#include "Common.h"


#define PSTR_MAX_LEN 256

typedef struct PascalStr 
{
    union {
        struct {
            U8 Len;
            U8 Text[PSTR_MAX_LEN];
        };
        U8 Data[PSTR_MAX_LEN + 1];
    };
} PascalStr;

PASCAL_STATIC_ASSERT(offsetof(PascalStr, Len) == 0, "Pascal string");



static inline bool PStrIsDyn(const PascalStr *PStr) { UNUSED(PStr, PStr); return false; }

static inline U8 *PStrGetPtr(PascalStr *PStr) { return PStr->Text; }
static inline const U8 *PStrGetConstPtr(const PascalStr *PStr) { return PStr->Text; }

static inline USize PStrGetLen(const PascalStr *PStr) { return PStr->Len; }
static inline USize PStrGetCap(const PascalStr *PStr) { UNUSED(PStr, PStr); return PSTR_MAX_LEN; }
static inline void PStrReserve(PascalStr *PStr, USize NewCapacity) { UNUSED(PStr, NewCapacity); }
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


#endif /* PASCAL_STRING_H */

