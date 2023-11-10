
#include <string.h>

#include "PascalString.h"
#include "Memory.h"



bool PStrIsDyn(const PascalStr *PStr)
{
    return PStr->Loc.LenLeft >= PSTR_LOC_ISDYNBIT;
}

U8 *PStrGetPtr(PascalStr *PStr)
{
    if (PStrIsDyn(PStr))
        return PStr->Dyn.Str;
    return PStr->Loc.Str;
}

const U8 *PStrGetConstPtr(const PascalStr *PStr)
{
    if (PStrIsDyn(PStr))
        return PStr->Dyn.Str;
    return PStr->Loc.Str;
}

USize PStrGetLen(const PascalStr *PStr)
{
    if (PStrIsDyn(PStr))
        return PStr->Dyn.Len & ~PSTR_DYN_ISDYNBIT;
    return PSTR_MAX_LOCAL_LEN - PStr->Loc.LenLeft;
}

USize PStrGetCap(const PascalStr *PStr)
{
    if (PStrIsDyn(PStr))
        return PStr->Dyn.Cap;
    return PSTR_MAX_LOCAL_LEN;
}

USize PStrAddToLen(PascalStr *PStr, ISize Extra)
{
    USize Old = PStrGetLen(PStr);
    USize New = (ISize)Old + Extra;
    USize OldCap = PStrGetCap(PStr);
    if (New > OldCap)
    {
        PStrReserve(PStr, New * 2);
    }

    if (PStrIsDyn(PStr))
    {
        PStr->Dyn.Len = New | PSTR_DYN_ISDYNBIT;
        PStr->Dyn.Str[New] = '\0';
    }
    else
    {
        PStr->Loc.LenLeft = PSTR_MAX_LOCAL_LEN - New;
        PStr->Loc.Str[New] = '\0';
    }
    return Old;
}


USize PStrReserve(PascalStr *PStr, USize NewCapacity)
{
    USize OldCap = PStrGetCap(PStr);
    if (OldCap >= NewCapacity)
        return OldCap;


    if (PStrIsDyn(PStr))
    {
        PascalDynStr *Dyn = &PStr->Dyn;
        MemReallocate(Dyn->Str, NewCapacity);
        Dyn->Cap = NewCapacity;
    }
    /* transition from local to dyn */
    else if (NewCapacity > PSTR_MAX_LOCAL_LEN)
    {
        PascalStr NewStr = PStrInitReserved(0, NewCapacity);
        memcpy(NewStr.Dyn.Str, PStr->Loc.Str, PSTR_MAX_LOCAL_LEN);
        NewStr.Dyn.Len = PSTR_MAX_LOCAL_LEN - PStr->Loc.LenLeft;
        NewStr.Dyn.Len |= PSTR_DYN_ISDYNBIT;
        *PStr = NewStr;
    }
    return OldCap;
}




PascalStr PStrInitReserved(USize Len, USize Extra)
{
    PascalStr PStr;
    if (Len + Extra > PSTR_MAX_LOCAL_LEN)
    {
        PStr.Dyn = (PascalDynStr) {
            .Len = Len | PSTR_DYN_ISDYNBIT,
            .Cap = Len + Extra + 1,
            .Str = MemAllocate(Len + Extra + 1),
        };
        PASCAL_ASSERT(
                (PStr.Dyn.Len >> (sizeof(USize)*8 - 1)) == (PStr.Loc.LenLeft >> 7), 
                "Differing Dyn bit: (%zu)|%zu != (%u)|%u\n",
                PStr.Dyn.Len, PStr.Dyn.Len >> (sizeof(USize)*8 - 1),
                PStr.Loc.LenLeft, PStr.Loc.LenLeft >> 7
        );
    }
    else 
    {
        PStr.Loc = (PascalLocStr) {
            .Str = { 0 },
            .LenLeft = PSTR_MAX_LOCAL_LEN - Len,
        };
    }
    return PStr;
}





PascalStr PStrCopyReserved(const U8 *Str, USize Len, USize Extra)
{
    PascalStr PStr = PStrInitReserved(Len, Extra);
    memcpy(PStrGetPtr(&PStr), Str, Len);
    PStrGetPtr(&PStr)[Len] = '\0';
    return PStr;
}

PascalStr PStrInit(USize Len)
{
    return PStrInitReserved(Len, 0);
}

PascalStr PStrCopy(const U8 *Str, USize Len)
{
    return PStrCopyReserved(Str, Len, 0);
}

void PStrDeinit(PascalStr *PStr)
{
    if (PStrIsDyn(PStr))
        MemDeallocate(PStr->Dyn.Str);

    *PStr = (PascalStr) { {{0}} };
    PStr->Loc.LenLeft = PSTR_MAX_LOCAL_LEN;
}







U8 PStrAppendChr(PascalStr *PStr, U8 Chr)
{
    USize OldLen = PStrGetLen(PStr);
    U8 Ret = (OldLen == 0) ? '\0' : PStrGetPtr(PStr)[OldLen - 1];
    PStrAddToLen(PStr, 1);

    U8 *Ptr = PStrGetPtr(PStr);
    Ptr[OldLen] = Chr;
    Ptr[OldLen + 1] = '\0';
    return Ret;
}


USize PStrAppendStr(PascalStr *PStr, const U8 *Str, USize Len)
{
    USize OldLen = PStrGetLen(PStr);
    PStrAddToLen(PStr, Len);

    U8 *Ptr = PStrGetPtr(PStr);
    memcpy(Ptr + OldLen, Str, Len);
    Ptr[OldLen + Len] = '\0';
    return OldLen;
}





