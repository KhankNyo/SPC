

#include <string.h> /* memset */

#include "Tokenizer.h"
#include "Common.h"
#include "Vartab.h"
#include "Variable.h"




#define IS_TOMBSTONED(pSlot) (0 == (pSlot)->Str.Len)
#define IS_EMPTY(pSlot) (IS_TOMBSTONED(pSlot) && NULL == (pSlot)->Str.Str)
#define SET_TOMBSTONE(pSlot) ((pSlot)->Str.Len = 0)


static PascalVar *VartabFindValidSlot(PascalVar *Table, ISize Cap, const U8 *Key, UInt Len, U32 Hash);
static void VartabResize(PascalVartab *Vartab, U32 Newsize);



PascalVartab VartabInit(PascalGPA *Allocator, ISize InitialCap)
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
        .Allocator = Allocator,
    };
    Vartab.Table = GPAAllocate(Allocator, sizeof(Vartab.Table[0])*InitialCap),
    memset(Vartab.Table, 0, InitialCap * sizeof(Vartab.Table[0]));
    return Vartab;
}


PascalVartab VartabPredefinedIdentifiers(PascalGPA *Allocator, ISize InitialCap)
{
    PascalVartab Identifiers = VartabInit(Allocator, InitialCap);
    VartabSet(&Identifiers, (const U8*)"INTEGER", 7, 0, VarTypeInit(TYPE_I16, 2), NULL);
    VartabSet(&Identifiers, (const U8*)"POINTER", 7, 0, VarTypeInit(TYPE_POINTER, sizeof(void*)), NULL);
    VartabSet(&Identifiers, (const U8*)"CHAR", 4, 0, VarTypeInit(TYPE_CHAR, sizeof(char)), NULL);

    VartabSet(&Identifiers, (const U8*)"REAL", 4, 0, VarTypeInit(TYPE_F32, 4), NULL);
    VartabSet(&Identifiers, (const U8*)"REAL32", 6, 0, VarTypeInit(TYPE_F32, 4), NULL);
    VartabSet(&Identifiers, (const U8*)"REAL64", 6, 0, VarTypeInit(TYPE_F64, 8), NULL);
    VartabSet(&Identifiers, (const U8*)"BOOLEAN", 7, 0, VarTypeInit(TYPE_BOOLEAN, 1), NULL);

    VartabSet(&Identifiers, (const U8*)"STRING", 6, 0, VarTypeInit(TYPE_STRING, sizeof(PascalStr)), NULL);
    VartabSet(&Identifiers, (const U8*)"ShortString", 11, 0, VarTypeInit(TYPE_STRING, sizeof(PascalStr)), NULL);

    VartabSet(&Identifiers, (const U8*)"int8", 4, 0, VarTypeInit(TYPE_I8, 1), NULL);
    VartabSet(&Identifiers, (const U8*)"int16", 5, 0, VarTypeInit(TYPE_I16, 2), NULL);
    VartabSet(&Identifiers, (const U8*)"int32", 5, 0, VarTypeInit(TYPE_I32, 4), NULL);
    VartabSet(&Identifiers, (const U8*)"int64", 5, 0, VarTypeInit(TYPE_I64, 8), NULL);

    VartabSet(&Identifiers, (const U8*)"uint8", 5, 0, VarTypeInit(TYPE_U8, 1), NULL);
    VartabSet(&Identifiers, (const U8*)"uint16", 6, 0, VarTypeInit(TYPE_U16, 2), NULL);
    VartabSet(&Identifiers, (const U8*)"uint32", 6, 0, VarTypeInit(TYPE_U32, 4), NULL);
    VartabSet(&Identifiers, (const U8*)"uint64", 6, 0, VarTypeInit(TYPE_U64, 8), NULL);
    return Identifiers;
}


PascalVartab VartabClone(PascalGPA *Allocator, const PascalVartab *Vartab)
{
    if (NULL == Allocator)
    {
        Allocator = Vartab->Allocator;
    }
    PascalVartab NewTable = VartabInit(Allocator, Vartab->Cap);
    for (ISize i = 0; i < NewTable.Cap; i++)
    {
        PascalVar *Slot = &Vartab->Table[i];
        if (!IS_EMPTY(Slot))
        {
            VartabSet(&NewTable, 
                    Slot->Str.Str, Slot->Str.Len, 
                    Slot->Line, Slot->Type, Slot->Location
            );
        }
    }
    return NewTable;
}



void VartabDeinit(PascalVartab *Vartab)
{
    GPADeallocate(Vartab->Allocator, Vartab->Table);
    memset(Vartab, 0, sizeof(*Vartab));
}


void VartabReset(PascalVartab *Vartab)
{
    memset(Vartab->Table, 0, sizeof(Vartab->Table[0]) * Vartab->Cap);
    Vartab->Count = 0;
}





PascalVar *VartabFindWithHash(PascalVartab *Vartab, const U8 *Key, UInt Len, U32 Hash)
{
    PascalVar *Slot = VartabFindValidSlot(Vartab->Table, Vartab->Cap, 
            Key, Len, Hash
    );
    if (IS_EMPTY(Slot) || IS_TOMBSTONED(Slot))
        return NULL;
    return Slot;
}



PascalVar *VartabSet(PascalVartab *Vartab, 
        const U8 *Key, UInt Len, U32 Line, VarType Type, VarLocation *Location)
{
    PASCAL_ASSERT(NULL != Vartab->Allocator, "Attempting to call VartabSet on a const Vartab");

    bool ExceededMaxLoad = Vartab->Count + 1 > Vartab->Cap * VARTAB_MAX_LOAD;
    if (ExceededMaxLoad)
    {
        ISize NewCap = Vartab->Cap * VARTAB_GROW_FACTOR;
        VartabResize(Vartab, NewCap);
    }

    U32 Hash = VartabHashStr(Key, Len);
    PascalVar *Slot = VartabFindValidSlot(Vartab->Table, Vartab->Cap, 
            Key, Len, Hash
    );
    bool IsNewKey = IS_EMPTY(Slot);
    if (IsNewKey)
    {
        Vartab->Count++;
    }


    Slot->Str = STRVIEW_INIT(Key, Len);
    Slot->Line = Line;
    Slot->Hash = Hash;

    Slot->Type = Type;
    Slot->Location = Location;
    return Slot;
}


PascalVar *VartabDelete(PascalVartab *Vartab, const U8 *Key, UInt Len)
{
    PASCAL_ASSERT(NULL != Vartab->Allocator, "Attempting to call VartabDelete on a const Vartab");

    PascalVar *Slot = VartabFindValidSlot(Vartab->Table, Vartab->Cap, 
            Key, Len, VartabHashStr(Key, Len)
    );
    if (IS_TOMBSTONED(Slot) || IS_EMPTY(Slot))
        return NULL;

    SET_TOMBSTONE(Slot);
    return Slot;
}


U32 VartabHashStr(const U8 *Key, UInt Len)
{
    U32 Hash = 2166136261u;
    for (UInt i = 0; i < Len; i++)
    {
        /* all strings will have the 5th bit off during hashing */
        Hash = (Hash ^ CHR_TO_UPPER(Key[i])) * 16777619;
    }
    return Hash;
}







static PascalVar *VartabFindValidSlot(PascalVar *Table, ISize Cap, const U8 *Key, UInt Len, U32 Hash)
{
    PascalVar *Tombstoned = NULL;
    USize Index = Hash & (Cap - 1);

    for (ISize i = 0; i < 2*Cap; i++)
    {
        PascalVar *Slot = &Table[Index];
        if (Len == Slot->Str.Len
            && Hash == Slot->Hash
            && TokenEqualNoCase(Key, Slot->Str.Str, Len))
        {
            return Slot;
        }
        if (IS_EMPTY(Slot))
        {
            if (NULL == Tombstoned)
                return Slot;
            return Tombstoned;
        }
        if (NULL == Tombstoned && IS_TOMBSTONED(Slot))
        {
            Tombstoned = Slot;
        }

        Index = (Index + 1) & ((USize)Cap - 1);
    }
    PASCAL_UNREACHABLE("Table does not contain '%.*s'", Len, Key);
    return NULL;
}


static void VartabResize(PascalVartab *Vartab, U32 NewCap)
{
    PascalVar *NewTable = GPAAllocate(Vartab->Allocator, sizeof(*NewTable)*NewCap);
    memset(NewTable, 0, NewCap * sizeof(*NewTable));

    /* rebuild hash table */
    Vartab->Count = 0;
    for (ISize i = 0; i < Vartab->Cap; i++)
    {
        /* skip invalid entries */
        if (IS_EMPTY(&Vartab->Table[i]) || IS_TOMBSTONED(&Vartab->Table[i]))
            continue;

        PascalVar *Slot = VartabFindValidSlot(NewTable, Vartab->Cap, 
                Vartab->Table[i].Str.Str, Vartab->Table[i].Str.Len, Vartab->Table[i].Hash
        );
        *Slot = Vartab->Table[i];
        Vartab->Count++;
    }

    GPADeallocate(Vartab->Allocator, Vartab->Table);
    Vartab->Table = NewTable;
    Vartab->Cap = NewCap;
}


