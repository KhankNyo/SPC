
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


