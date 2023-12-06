
#include <string.h>

#include "PascalString.h"
#include "Memory.h"



USize PStrAddToLen(PascalStr *PStr, ISize Extra)
{
    USize OldLen = PStr->Len;
    if (PStr->Len + Extra > PSTR_MAX_LEN)
    {
        PStr->Len = PSTR_MAX_LEN - 1;
    }
    else
    {
        PStr->Len += Extra;
    }
    return OldLen;
}




PascalStr PStrInitReserved(USize Len, USize Extra)
{
    UNUSED(Extra, Extra);
    PascalStr PStr = { .Len = Len };
    return PStr;
}





PascalStr PStrCopyReserved(const U8 *Str, USize Len, USize Extra)
{
    PascalStr PStr = PStrInitReserved(Len, Extra);
    if (Len > PSTR_MAX_LEN)
    {
        Len = PSTR_MAX_LEN;
    }
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
    USize Len = PStrGetLen(s1);
    return Len == PStrGetLen(s2) 
        && 0 == strncmp(
                (const char *)PStrGetConstPtr(s1), 
                (const char *)PStrGetConstPtr(s2), 
            Len);
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





