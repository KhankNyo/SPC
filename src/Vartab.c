

#include <string.h> /* memset */

#include "Tokenizer.h"
#include "Common.h"
#include "Memory.h"
#include "Vartab.h"




#define IS_TOMBSTONED(pSlot) (0 == (pSlot)->Len)
#define IS_EMPTY(pSlot) (IS_TOMBSTONED(pSlot) && NULL == (pSlot)->Str)
#define SET_TOMBSTONE(pSlot) ((pSlot)->Len = 0)


static PascalVar *VartabFindValidSlot(PascalVar *Table, U32 Cap, const U8 *Key, UInt Len, U32 Hash);
static void VartabResize(PascalVartab *Vartab, U32 Newsize);
static U32 HashStr(const U8 *Key, UInt Len);



PascalVartab VartabInit(U32 InitialCap)
{
    PASCAL_ASSERT(InitialCap < (1lu << 30), 
            "vartab can only a capacity of %lu elements or less (received %lu).", 
            1ul << 30, (unsigned long)InitialCap
    );

    U32 Cap = IsolateTopBitU32(InitialCap);
    if (InitialCap > Cap)
        Cap *= VARTAB_GROW_FACTOR;


    PascalVartab Vartab = {
        .Cap = InitialCap,
        .Count = 0,
        .Table = MemAllocateArray(PascalVar, InitialCap),
    };
    memset(Vartab.Table, 0, InitialCap * sizeof(Vartab.Table[0]));
    return Vartab;
}


void VartabDeinit(PascalVartab *Vartab)
{
    MemDeallocateArray(Vartab->Table);
    memset(Vartab, 0, sizeof(*Vartab));
}





PascalVar *VartabFind(PascalVartab *Vartab, const U8 *Key, UInt Len)
{
    PascalVar *Slot = VartabFindValidSlot(Vartab->Table, Vartab->Cap, Key, Len, HashStr(Key, Len));
    if (IS_EMPTY(Slot) || IS_TOMBSTONED(Slot))
        return NULL;
    return Slot;
}



U32 *VartabGet(PascalVartab *Vartab, const U8 *Key, UInt Len)
{
    PascalVar *Slot = VartabFindValidSlot(Vartab->Table, Vartab->Cap, 
            Key, Len, HashStr(Key, Len)
    );
    if (IS_TOMBSTONED(Slot) || IS_EMPTY(Slot))
        return NULL;
    return &Slot->Value;
}


bool VartabSet(PascalVartab *Vartab, const U8 *Key, UInt Len, U32 Value)
{
    bool ExceededMaxLoad = Vartab->Count + 1 > Vartab->Cap * VARTAB_MAX_LOAD;
    if (ExceededMaxLoad)
    {
        U32 NewCap = Vartab->Cap * VARTAB_GROW_FACTOR;
        VartabResize(Vartab, NewCap);
    }

    U32 Hash = HashStr(Key, Len);
    PascalVar *Slot = VartabFindValidSlot(Vartab->Table, Vartab->Cap, 
            Key, Len, Hash
    );
    bool IsNewKey = IS_EMPTY(Slot);
    if (IsNewKey)
    {
        Vartab->Count++;
    }

    Slot->Str = Key;
    Slot->Len = Len;
    Slot->Value = Value;
    Slot->Hash = Hash;
    return IsNewKey;
}


bool VartabDelete(PascalVartab *Vartab, const U8 *Key, UInt Len)
{
    PascalVar *Slot = VartabFindValidSlot(Vartab->Table, Vartab->Cap, 
            Key, Len, HashStr(Key, Len)
    );
    if (IS_TOMBSTONED(Slot) || IS_EMPTY(Slot))
        return false;

    SET_TOMBSTONE(Slot);
    return true;
}




static PascalVar *VartabFindValidSlot(PascalVar *Table, U32 Cap, const U8 *Key, UInt Len, U32 Hash)
{
    PascalVar *Tombstoned = NULL;
    U32 Index = Hash & (Cap - 1);

    for (U32 i = 0; i < Cap; i++)
    {
        PascalVar *Slot = &Table[Index];
        if (NULL == Tombstoned && IS_TOMBSTONED(Slot))
        {
            Tombstoned = Slot;
        }
        else if (IS_EMPTY(Slot))
        {
            if (NULL == Tombstoned)
                return Slot;
            return Tombstoned;
        }
        else if (Len == Slot->Len
            && Hash == Slot->Hash
            && TokenEqualNoCase(Key, Slot->Str, Len))
        {
            return Slot;
        }

        Index = (Index + 1) & (Cap - 1);
    }
    PASCAL_UNREACHABLE("Table does not contain '%.*s'", Len, Key);
    return NULL;
}


static void VartabResize(PascalVartab *Vartab, U32 NewCap)
{
    PascalVar *NewTable = MemAllocateArray(*NewTable, NewCap);
    memset(NewTable, 0, NewCap * sizeof(*NewTable));

    /* rebuild hash table */
    Vartab->Count = 0;
    for (U32 i = 0; i < Vartab->Cap; i++)
    {
        /* skip invalid entries */
        if (IS_EMPTY(&Vartab->Table[i]) || IS_TOMBSTONED(&Vartab->Table[i]))
            continue;

        PascalVar *Slot = VartabFindValidSlot(NewTable, Vartab->Cap, 
                Vartab->Table[i].Str, Vartab->Table[i].Len, 
                Vartab->Table[i].Hash
        );
        *Slot = Vartab->Table[i];
        Vartab->Count++;
    }

    MemDeallocateArray(Vartab->Table);
    Vartab->Cap = NewCap;
    Vartab->Table = NewTable;
}


static U32 HashStr(const U8 *Key, UInt Len)
{
    U32 Hash = 2166136261u;
    for (UInt i = 0; i < Len; i++)
    {
        /* all strings will have the 5th bit off during hashing */
        Hash = (Hash ^ CHR_TO_UPPER(Key[i])) * 16777619;
    }
    return Hash;
}

