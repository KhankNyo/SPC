
#include <stdio.h>
#include <stdlib.h>
#include <string.h> /* memset */

#include "Pascal.h"
#include "Memory.h"






static PascalGPA sAllocator = { 0 };


void *MemGetAllocator(void)
{
    return &sAllocator;
}

void MemInit(U32 InitialCap)
{
    sAllocator = GPAInit(InitialCap);
}

void MemDeinit(void)
{
    GPADeinit(&sAllocator);
}


void *MemAllocate(USize ByteCount)
{
    return GPAAllocate(&sAllocator, ByteCount); 
}

void *MemAllocateZero(USize ByteCount)
{
    return GPAAllocateZero(&sAllocator, ByteCount);
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
        fprintf(stderr, "Out of memory while requesting %zu bytes\n", ByteCount);
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


#define GET_HEADER(Ptr) (((GPAHeader *)(Ptr)) - 1)
#define GPA_MIN_CAPACITY (sizeof(GPAHeader) + PASCAL_MEM_ALIGNMENT)

#define FREE_TAG ((U32)1)
#define SET_FREE_TAG(pHeader) ((pHeader)->Size |= FREE_TAG)
#define REMOVE_FREE_TAG(pHeader) ((pHeader)->Size &= ~(FREE_TAG))
#define IS_FREE(pHeader) ((pHeader)->Size & FREE_TAG)
static U32 GetSize(const GPAHeader *Header);

static GPAHeader *GPAFindFreeNode(PascalGPA *GPA, U32 ByteCount);
static void GPADeallocateNode(PascalGPA *GPA, GPAHeader *Header);
static void GPACoalesceNode(GPAHeader *Node);

/* returns a splitted node and set it to be not freed, 
 * the other chunk is set to free */
static GPAHeader *GPASplitNode(GPAHeader *Node, U32 Size);


PascalGPA GPAInit(U32 InitialCap)
{
    PascalGPA GPA = {
        .Mem.Raw = sMemAllocate(InitialCap + sizeof(GPAHeader)),
        .Cap = InitialCap,
        .CoalesceOnFree = true,
    };
    *GPA.Mem.Head = (GPAHeader){
        .Size = InitialCap,
        .Next = NULL,
        .Prev = NULL,
    };
    SET_FREE_TAG(GPA.Mem.Head);
    return GPA;
}

void GPADeinit(PascalGPA *GPA)
{
    sMemDeallocate(GPA->Mem.Raw);
    *GPA = (PascalGPA){ 0 };
}


void *GPAAllocate(PascalGPA *GPA, U32 ByteCount)
{
    GPAHeader *Header = GPAFindFreeNode(GPA, ByteCount);
    return Header->Data;
}


void *GPAReallocate(PascalGPA *GPA, void *Ptr, U32 NewSize)
{
    PASCAL_ASSERT(0 != NewSize, "Cannot call GPAReallocate with NewSize of 0, use GPADeallocate instead");
    if (NULL == Ptr)
    {
        return GPAAllocate(GPA, NewSize);
    }
    GPAHeader *PtrHeader = GET_HEADER(Ptr);
    PASCAL_ASSERT(!IS_FREE(PtrHeader), "Attempting to reallocate a freed pointer");

    if (NewSize <= PtrHeader->Size)
    {
        return Ptr;
    }

    GPACoalesceNode(PtrHeader);
    if (GetSize(PtrHeader) == NewSize)
    {
        return Ptr;
    }
    if (GetSize(PtrHeader) > NewSize)
    {
        SET_FREE_TAG(PtrHeader);
        return GPASplitNode(PtrHeader, NewSize)->Data;
    }

    GPAHeader* NewPtr = GPAFindFreeNode(GPA, NewSize);
    memcpy(NewPtr->Data, Ptr, PtrHeader->Size);
    GPADeallocateNode(GPA, PtrHeader);
    return NewPtr;
}



void GPADeallocate(PascalGPA *GPA, void *Ptr)
{
    if (NULL != Ptr)
    {
        GPAHeader *PtrHeader = GET_HEADER(Ptr);
        PASCAL_ASSERT(!IS_FREE(PtrHeader), "Double free");
        GPADeallocateNode(GPA, PtrHeader);
    }
}


static U32 GetSize(const GPAHeader *Header)
{
    return Header->Size & ~FREE_TAG;
}

static GPAHeader *GPAFindFreeNode(PascalGPA *GPA, U32 ByteCount)
{
    ByteCount = (ByteCount + PASCAL_MEM_ALIGNMENT) & ~(PASCAL_MEM_ALIGNMENT - 1);
    
    GPAHeader *Node = GPA->Mem.Head;
    while (Node != NULL 
    && (Node->Size < ByteCount || !IS_FREE(Node)))
    {
        Node = Node->Next;
    }

    if (NULL == Node)
    {
        PASCAL_UNREACHABLE("Out of memory");
    }
    return GPASplitNode(Node, ByteCount);
}



static void GPADeallocateNode(PascalGPA *GPA, GPAHeader *Header)
{
    SET_FREE_TAG(Header);
    if (GPA->CoalesceOnFree)
    {
        GPACoalesceNode(Header);
    }
}


static void GPACoalesceNode(GPAHeader *Node)
{
    GPAHeader *Next = Node->Next;
    while (Next != NULL && IS_FREE(Next))
    {
        /* consume the next node */
        /* don't need to call SetSize here, 
         * arithmetic works with the free tag */
        Node->Size += GetSize(Next) + sizeof(GPAHeader);
        Next = Next->Next;
    }
    Node->Next = Next;
    if (NULL != Next)
    {
        Next->Prev = Node;
    }
}



static GPAHeader *GPASplitNode(GPAHeader *Node, U32 Size)
{
    PASCAL_ASSERT(IS_FREE(Node), "Cannot split non-free node");
    /* the free tag does not matter in arithmetic here */
    bool NodeIsDivisible = Node->Size >= Size + sizeof(GPAHeader) + GPA_MIN_CAPACITY;
    if (!NodeIsDivisible)
    {
        REMOVE_FREE_TAG(Node);
        return Node;
    }


    U8 *BytePtr = (U8*)Node->Data;
    U32 Leftover = GetSize(Node) - Size;

    /*
     * Splitting Node into:
     *     Node      <=> Next
     * New <=> Other <=> Next
     */
    GPAHeader *New = Node;
    GPAHeader *Other = (GPAHeader*)(BytePtr + Size);
    GPAHeader *Next = Node->Next;

    /* Other is in the data section of Node.
     * That's why we can't split a non free node */
    Other->Size = (Leftover - sizeof(GPAHeader)) 
        | FREE_TAG;
    Other->Next = Next;
    if (Next != NULL)
        Next->Prev = Other;


    New->Size = Size; /* setting size clears the free flag */
    New->Next = Other;
    Other->Prev = New;
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


void *ArenaAllocate(PascalArena *Arena, U32 ByteCount)
{
    USize Size = (ByteCount + PASCAL_MEM_ALIGNMENT) & ~(PASCAL_MEM_ALIGNMENT - 1);

    UInt i = Arena->CurrentIdx;
    /* start looking elsewhere */
    if ((USize)Arena->Used[i] + Size > Arena->Cap[i])
    {
        /* look for the arenas before */
        for (i = 0; i < Arena->CurrentIdx; i++)
        {
            /* found one region with the appropriate size */
            if ((USize)Arena->Used[i] + Size <= Arena->Cap[i])
                goto Allocate;
        }

        /* if we ran out of arenas */
        if (PASCAL_ARENA_COUNT - 1 == i)
        {
            USize Total = Arena->Cap[0];
            for (UInt i = 0; i < PASCAL_ARENA_COUNT; i++)
                Total += Arena->Cap[i];

            PASCAL_UNREACHABLE("Arena ran out of mem, trying to allocate %u bytes (has %zu in total)\n",
                    ByteCount, Total
            );
        }

        /* have to allocate a brand new arena */
        i += 1;
        if (NULL == Arena->Mem[i].Raw)
        {
            Arena->CurrentIdx = i;
            Arena->Cap[i] += Size;
            Arena->Mem[i].Raw = sMemAllocate(Arena->Cap[i]);
        }
    }

    void *Ptr;
Allocate:
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

