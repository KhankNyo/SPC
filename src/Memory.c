
#include <stdio.h>
#include <stdlib.h>
#include <string.h> /* memset */

#include "Pascal.h"
#include "Memory.h"






static PascalGPA sAllocator = { 0 };

void MemInit(U32 InitialCap, UInt MemGrowFactor)
{
    sAllocator = GPAInit(InitialCap, MemGrowFactor);
}

void MemDeinit(void)
{
    GPADeinit(&sAllocator);
}


void *MemAllocate(USize ByteCount)
{
    return GPAAllocate(&sAllocator, ByteCount); 
}

void *MemReallocate(void *Ptr, USize NewSize)
{
    return GPAReallocate(&sAllocator, Ptr, NewSize);
}

void MemDeallocate(void *Ptr)
{
    GPADeallocate(&sAllocator, Ptr);
}





/*====================================================================
 *                          UTILS ALLOCATOR
 *====================================================================*/


static void *sMemAllocate(USize ByteCount)
{
    void *Pointer = malloc(ByteCount);
    if (NULL == Pointer)
    {
        fprintf(stderr, "Out of memory while requesting %llu bytes\n", ByteCount);
        exit(PASCAL_EXIT_FAILURE);
    }
    return Pointer;
}


static void sMemDeallocate(void *Pointer)
{
    free(Pointer);
}






/*====================================================================
 *                     GENERAL PURPOSE ALLOCATOR
 *====================================================================*/


#define GPA_HEADER(Ptr) (((GPAHeader *)(Ptr)) - 1)
#define GPA_MIN_CAPACITY (sizeof(GPAHeader) + PASCAL_MEM_ALIGNMENT)

static GPAHeader *GPAFindFreeNode(PascalGPA *GPA, U32 ByteCount);
static void GPADeallocateNode(PascalGPA *GPA, GPAHeader *Header);
static U32 GPACoalesceNode(GPAHeader *Node);

/* returns a splitted node and set it to be not freed, 
 * the other chunk is set to free */
static GPAHeader *GPASplitNode(GPAHeader *Node, U32 Size);


PascalGPA GPAInit(U32 InitialCap, UInt SizeGrowFactor)
{
    PascalGPA GPA = {
        .Mem = { [0] = { .Raw = sMemAllocate(InitialCap) } }, 
        .CurrentIdx = 0,
        .CoalesceOnFree = true,
    };

    GPA.Cap[0] = InitialCap + sizeof(GPAHeader);
    for (UInt i = 1; i < PASCAL_GPA_COUNT; i++)
    {
        GPA.Cap[i] = GPA.Cap[i - 1] * SizeGrowFactor;
    }

    GPA.Head[0] = GPA.Mem[0].Raw;

    memset(GPA.Head[0], 0, sizeof *GPA.Head[0]);
    GPA.Head[0]->Size = InitialCap;
    GPA.Head[0]->IsFree = true;
    return GPA;
}

void GPADeinit(PascalGPA *GPA)
{
    for (UInt i = 0; i < PASCAL_GPA_COUNT; i++)
    {
        sMemDeallocate(GPA->Mem[i].Raw);
    }
    *GPA = (PascalGPA){ 0 };
}


void *GPAAllocate(PascalGPA *GPA, U32 ByteCount)
{
    GPAHeader *Header = GPAFindFreeNode(GPA, ByteCount);
    return Header->Data;
}


void *GPAReallocate(PascalGPA *GPA, void *Ptr, U32 NewSize)
{
    if (NULL == Ptr)
    {
        return GPAAllocate(GPA, NewSize);
    }
    PASCAL_ASSERT(0 != NewSize, "Cannot call GPAReallocate with NewSize of 0, use GPADeallocate instead");

    if (NewSize <= GPA_HEADER(Ptr)->Size)
        return Ptr;

    GPACoalesceNode(GPA_HEADER(Ptr));
    if (GPA_HEADER(Ptr)->Size == NewSize)
    {
        return Ptr;
    }
    if (GPA_HEADER(Ptr)->Size > NewSize)
    {
        GPA_HEADER(Ptr)->IsFree = true;
        return GPASplitNode(GPA_HEADER(Ptr), NewSize)->Data;
    }

    GPAHeader* NewPtr = GPAFindFreeNode(GPA, NewSize);
    memcpy(NewPtr->Data, Ptr, GPA_HEADER(Ptr)->Size);
    GPADeallocateNode(GPA, GPA_HEADER(Ptr));
    return NewPtr;
}



void GPADeallocate(PascalGPA *GPA, void *Ptr)
{
    if (NULL != Ptr)
    {
        PASCAL_ASSERT(!GPA_HEADER(Ptr)->IsFree, "Double free");
        GPADeallocateNode(GPA, GPA_HEADER(Ptr));
    }
}



static GPAHeader *GPAFindFreeNode(PascalGPA *GPA, U32 ByteCount)
{
    U32 Unaligned = ByteCount;
    if (ByteCount < PASCAL_MEM_ALIGNMENT)
        ByteCount = PASCAL_MEM_ALIGNMENT;
    else
        ByteCount &= ~(PASCAL_MEM_ALIGNMENT - 1);


    UInt ID = GPA->CurrentIdx;
    GPAHeader *Node = GPA->Head[ID];
    
    while (Node != NULL && !(Node->Size >= ByteCount && Node->IsFree))
    {
        Node = Node->Next;
    }

    
    if (NULL == Node)
    {
        for (ID = 0; ID < PASCAL_GPA_COUNT; ID++)
        {
            /* Attempt to allocate in older buffers */
            if (ID < GPA->CurrentIdx)
            {
                Node = GPA->Head[ID];
                while (Node != NULL && !(Node->Size >= ByteCount && Node->IsFree))
                {
                    Node = Node->Next;
                }

                /* found an appropriate node */
                if (NULL != Node)
                    goto Allocate;
            }

            /* brand new buffer */
            if (NULL == GPA->Head[ID])
            {
                GPA->Mem[ID].Raw = sMemAllocate((USize)GPA->Cap[ID] + sizeof(GPAHeader));
                GPA->Head[ID] = GPA->Mem[ID].Raw;
                GPA->Head[ID]->ID = ID;
                GPA->Head[ID]->Size = GPA->Cap[ID];
                GPA->Head[ID]->Prev = NULL;
                GPA->Head[ID]->Next = NULL;
                GPA->Head[ID]->IsFree = true;
                GPA->CurrentIdx++;
                Node = GPA->Head[ID];
                goto Allocate;
            }
        }

        if (ID == PASCAL_GPA_COUNT)
        {
            USize Total = 0;
            for (UInt i = 0; i < PASCAL_GPA_COUNT; i++)
            {
                Total += GPA->Cap[i];
            }
            PASCAL_UNREACHABLE(
                    "General Purpose Alloc: Out of memory trying to allocate %u bytes (%u unaligned).\n"
                    "Total: %zu bytes\n", ByteCount, Unaligned, Total
            );
        }
    }

Allocate:
    return GPASplitNode(Node, ByteCount);
}



static void GPADeallocateNode(PascalGPA *GPA, GPAHeader *Header)
{
    Header->IsFree = true;
    if (GPA->CoalesceOnFree)
    {
        GPACoalesceNode(Header);
    }
}


static U32 GPACoalesceNode(GPAHeader *Node)
{
    GPAHeader *Next = Node->Next;
    U32 Gained = 0;
    while (Next != NULL && Next->IsFree 
    && ((U8*)Node + Node->Size + sizeof(*Node) == (U8*)Node->Next))
    {
        Gained += Next->Size + sizeof(GPAHeader);
        Next->Prev = Node;
        Node->Next = Next->Next;
        Next = Next->Next;
    }
    Node->Size += Gained;
    return Gained;
}



static GPAHeader *GPASplitNode(GPAHeader *Node, U32 Size)
{
    PASCAL_ASSERT(Node->IsFree, "Cannot split non-free node");
    bool NodeIsDivisible = Node->Size >= Size + sizeof(GPAHeader) + GPA_MIN_CAPACITY;
    if (!NodeIsDivisible)
    {
        Node->IsFree = false;
        return Node;
    }


    U8 *BytePtr = (U8*)Node->Data;
    U32 Leftover = Node->Size - (Size + sizeof(GPAHeader));


    GPAHeader *New = Node;
    GPAHeader *Other = (GPAHeader*)(BytePtr + Size + sizeof(GPAHeader));

    Other->Size = Leftover - sizeof(GPAHeader);
    Other->Next = New->Next;
    Other->Prev = New;
    Other->IsFree = true;

    New->Size = Size;
    New->Next = Other;
    New->IsFree = false;

    if (Other->Next != NULL)
    {
        Other->Next->Prev = Other;
    }
    return New;
}















/*====================================================================
 *                          ARENA ALLOCATOR
 *====================================================================*/



PascalArena ArenaInit(U32 InitialCap, UInt SizeGrowFactor)
{
    PascalArena Arena = {
        .Mem = {[0] = { .Raw = sMemAllocate(InitialCap) } },
        .Used = { 0 },
    };
    Arena.Cap[0] = InitialCap;
    for (UInt i = 1; i < PASCAL_ARENA_COUNT; i++)
        Arena.Cap[i] = Arena.Cap[i - 1] * SizeGrowFactor; 

    return Arena;
}

void ArenaDeinit(PascalArena *Arena)
{
    for (UInt i = 0; i < PASCAL_ARENA_COUNT; i++)
    {
        sMemDeallocate(Arena->Mem[i].Raw);
    }
    *Arena = (PascalArena){0};
}

void ArenaReset(PascalArena *Arena)
{
    Arena->CurrentIdx = 0;
    for (UInt i = 0; i < PASCAL_ARENA_COUNT; i++)
        Arena->Used[i] = 0;
}


void *ArenaAllocate(PascalArena *Arena, U32 Bytes)
{
    UInt i = Arena->CurrentIdx;
    /* start looking elsewhere */
    if ((USize)Arena->Used[i] + Bytes > Arena->Cap[i])
    {
        /* look for the arenas before */
        for (i = 0; i < Arena->CurrentIdx; i++)
        {
            /* found one region with the appropriate size */
            if ((USize)Arena->Used[i] + Bytes <= Arena->Cap[i])
                goto Allocate;
        }

        /* if we ran out of arenas */
        if (PASCAL_ARENA_COUNT == i)
        {
            USize Total = Arena->Cap[0];
            for (UInt i = 0; i < PASCAL_ARENA_COUNT; i++)
                Total += Arena->Cap[i];

            PASCAL_UNREACHABLE("Arena ran out of mem, trying to allocate %u bytes (has %zu in total)\n",
                    Bytes, Total
            );
        }

        /* have to allocate a brand new arena */
        i += 1;
        if (NULL == Arena->Mem[i].Raw)
        {
            Arena->CurrentIdx = i;
            Arena->Mem[i].Raw = sMemAllocate(Arena->Cap[i]);
        }
    }

    void *Ptr;
    USize Size;
Allocate:
    Size = Bytes & ~(PASCAL_MEM_ALIGNMENT - 1);
    Ptr = &Arena->Mem[i].Bytes[Arena->Used[i]];
    Arena->Used[i] += Size;
    return Ptr;
}


void *ArenaAllocateZero(PascalArena *Arena, U32 Bytes)
{
    void *Ptr = ArenaAllocate(Arena, Bytes);
    memset(Ptr, 0, Bytes);
    return Ptr;
}

