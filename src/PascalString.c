
#include <string.h>

#include "PascalString.h"
#include "Memory.h"




void PStrSetLen(PascalStr *PStr, USize Len)
{
    if (Len > PSTR_MAX_LEN)
    {
        PStr->FbString.Len = PSTR_MAX_LEN;
    }
    else
    {
        PStr->FbString.Len = Len;
    }
}

USize PStrAddToLen(PascalStr *PStr, ISize Extra)
{
    USize OldLen = PStrGetLen(PStr);
    if (OldLen + Extra > PSTR_MAX_LEN)
    {
        PStr->FbString.Len = PSTR_MAX_LEN;
    }
    else
    {
        PStr->FbString.Len += Extra;
    }
    return OldLen;
}




PascalStr PStrInitReserved(USize Len, USize Extra)
{
    UNUSED(Extra, Extra);
    if (Len > PSTR_MAX_LEN)
        Len = PSTR_MAX_LEN;
    PascalStr PStr = { .FbString = {
        .Buf = { 0 },
        .Len = Len,
    } };
    return PStr;
}





PascalStr PStrCopyReserved(const U8 *Str, USize Len, USize Extra)
{
    PascalStr PStr = PStrInitReserved(Len, Extra);
    if (Len > PSTR_MAX_LEN)
        Len = PSTR_MAX_LEN;
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
    *PStr = (PascalStr){ 0 };
}







bool PStrEqu(const PascalStr *s1, const PascalStr *s2)
{
    UInt Len = PStrGetLen(s1);
    if (Len != PStrGetLen(s2))
        return false;
    return 0 == memcmp(PStrGetConstPtr(s1), PStrGetConstPtr(s2), Len);
}

bool PStrIsLess(const PascalStr *s1, const PascalStr *s2)
{
    USize Len = PStrGetLen(s1);
    {
        USize Len2 = PStrGetLen(s2);
        if (Len > Len2)
            Len = Len2;
    }
    const U8 *PtrS1 = PStrGetConstPtr(s1);
    const U8 *PtrS2 = PStrGetConstPtr(s2);
    for (USize i = 0; i < Len; i++)
    {
        if (PtrS1[i] >= PtrS2[i])
            return false;
    }
    return true;
}









U8 PStrAppendChr(PascalStr *PStr, U8 Chr)
{
    USize OldLen = PStrGetLen(PStr);
    U8 Ret = (OldLen == 0) ? '\0' : PStrGetPtr(PStr)[OldLen - 1];
    PStrAddToLen(PStr, 1);
    USize NewLen = PStrGetLen(PStr);

    U8 *Ptr = PStrGetPtr(PStr);
    Ptr[OldLen] = Chr;
    Ptr[NewLen] = '\0';
    return Ret;
}


USize PStrAppendStr(PascalStr *PStr, const U8 *Str, USize Len)
{
    USize OldLen = PStrGetLen(PStr);
    PStrAddToLen(PStr, Len);
    USize NewLen = PStrGetLen(PStr);

    U8 *Ptr = PStrGetPtr(PStr);
    memcpy(Ptr + OldLen, Str, NewLen - OldLen);
    Ptr[NewLen] = '\0';
    return OldLen;
}





