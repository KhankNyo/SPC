
#include <stdio.h>
#include <stdlib.h>

#include "Include/Pascal.h"
#include "Include/Memory.h"




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
        .Used = {[0] = 0},
        .Cap = {[0] = InitialCap},
        .SizeGrowFactor = SizeGrowFactor,
    };
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


void *ArenaAllocate(PascalArena *Arena, USize Bytes)
{
    UInt i = Arena->CurrentIdx;
    /* start looking elsewhere */
    if ((USize)Arena->Used[i] + Bytes > Arena->Cap[i])
    {
        for (i = 0; i < Arena->CurrentIdx; i++)
        {
            if ((USize)Arena->Used[i] + Bytes <= Arena->Cap[i])
                goto Allocate;
        }

        if (PASCAL_ARENA_COUNT - 1 == i)
        {
            USize Total = Arena->Cap[0];
            for (UInt i = 0; i < PASCAL_ARENA_COUNT; i++)
                Total += Arena->Cap[i];

            PASCAL_UNREACHABLE("Arena ran out of mem, trying to allocate %zu bytes (has %zu in total)\n",
                    Bytes, Total
            );
        }

        /* have to allocate a brand new arena */
        U32 NewCap = Arena->Cap[i] * Arena->SizeGrowFactor;
        i += 1;
        Arena->Cap[i] = NewCap;
        Arena->Used[i] = 0;
        Arena->CurrentIdx = i;
        Arena->Mem[i].Raw = MemAllocate(NewCap);
    }

    void *Ptr;
Allocate:
    Ptr = &Arena->Mem[i].Bytes[Arena->Used[i]];
    Arena->Used[i] += Bytes;
    return Ptr;
}


