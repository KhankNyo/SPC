
#include <stdio.h>
#include <stdlib.h>
#include <string.h> /* memset */

#include "Pascal.h"
#include "Memory.h"




void *MemAllocate(USize ByteCount)
{
    void *Pointer = malloc(ByteCount);
    if (NULL == Pointer)
    {
        fprintf(stderr, "Out of memory while requesting %llu bytes\n", ByteCount);
        exit(PASCAL_EXIT_FAILURE);
    }
    return Pointer;
}


void *MemReallocate(void *Pointer, USize NewSize)
{
    if (0 == NewSize)
    {
        return Pointer;
    }

    void *NewPointer = realloc(Pointer, NewSize);
    if (NULL == NewPointer)
    {
        fprintf(stderr, "Out of memory while reallocating with a new size of %llu bytes\n",
                NewSize
        );
        exit(PASCAL_EXIT_FAILURE);
    }
    return NewPointer;
}


void MemDeallocate(void *Pointer)
{
    free(Pointer);
}




PascalArena ArenaInit(
        UInt InitialCap, UInt SizeGrowFactor)
{
    PascalArena Arena = {
        .Mem = {[0] = {MemAllocate(InitialCap)} },
        .Used = { 0 },
        .SizeGrowFactor = SizeGrowFactor,
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
        MemDeallocate(Arena->Mem[i].Raw);
    }
    *Arena = (PascalArena){0};
}

void ArenaReset(PascalArena *Arena)
{
    Arena->CurrentIdx = 0;
    for (UInt i = 0; i < PASCAL_ARENA_COUNT; i++)
        Arena->Used[i] = 0;
}


void *ArenaAllocate(PascalArena *Arena, USize Bytes)
{
    UInt i = Arena->CurrentIdx;
    /* start looking elsewhere */
    if ((USize)Arena->Used[i] + Bytes > Arena->Cap[i])
    {
        /* look for the arenas before */
        for (i = 0; i < Arena->CurrentIdx; i++)
        {
            if ((USize)Arena->Used[i] + Bytes <= Arena->Cap[i])
                goto Allocate;
        }

        /* if we ran out of arenas */
        if (PASCAL_ARENA_COUNT == i)
        {
            USize Total = Arena->Cap[0];
            for (UInt i = 0; i < PASCAL_ARENA_COUNT; i++)
                Total += Arena->Cap[i];

            PASCAL_UNREACHABLE("Arena ran out of mem, trying to allocate %zu bytes (has %zu in total)\n",
                    Bytes, Total
            );
        }

        /* have to allocate a brand new arena */
        i += 1;
        if (NULL == Arena->Mem[i].Raw)
        {
            Arena->CurrentIdx = i;
            Arena->Mem[i].Raw = MemAllocate(Arena->Cap[i]);
        }
    }

    void *Ptr;
    USize Size;
Allocate:
    Size = Bytes & ~(PASCAL_ARENA_ALIGNMENT - 1);
    Ptr = &Arena->Mem[i].Bytes[Arena->Used[i]];
    Arena->Used[i] += Size;
    return Ptr;
}


void *ArenaAllocateZero(PascalArena *Arena, USize Bytes)
{
    void *Ptr = ArenaAllocate(Arena, Bytes);
    memset(Ptr, 0, Bytes);
    return Ptr;
}

