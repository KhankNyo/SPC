#ifndef PASCAL_MEMORY_H
#define PASCAL_MEMORY_H

#include "Common.h"

/* 
 * returns a pointer to a block of memory with size of ByteCount
 * never return NULL 
 */
void *MemAllocate(USize ByteCount);

/* 
 * Allocates an array,
 * never return NULL 
 */
#define MemAllocateArray(TypeName_T, USize_ElemCount)\
    MemAllocate((USize_ElemCount) * sizeof(TypeName_T))

/* 
 * NewSize of 0 is a nop,
 * otherwise, this function acts like realloc without ever returning NULL
 */
void *MemReallocate(void *Pointer, USize NewSize);

/* wrapper around MemReallocate for arrays */
#define MemReallocateArray(TypeName_T, Pointer, USize_NewElemCount)\
    MemReallocate(Pointer, (USize_NewElemCount) * sizeof(TypeName_T))

/* 
 * Deallocate a block of memory, 
 * currently a wrapper around free
 */
void MemDeallocate(void *Pointer);

/* Wrapper around MemDeallocate */
#define MemDeallocateArray(Pointer) MemDeallocate(Pointer)




#define PASCAL_ARENA_COUNT 4
#define PASCAL_ARENA_ALIGNMENT (sizeof(void*))

typedef struct PascalArena
{
    union {
        void *Raw;
        U8 *Bytes;
    } Mem[PASCAL_ARENA_COUNT];
    U32 Used[PASCAL_ARENA_COUNT];
    U32 Cap[PASCAL_ARENA_COUNT];
    U32 CurrentIdx;
    U32 SizeGrowFactor;
} PascalArena;

/* 
 * InitialCap:  the capacity of the first arena,
 *              consecutive arenas will have their size multiplied by SizeGrowFactor 
 * SizeGrowFactor:  the factor to multiply with the current arena's capacity 
 *                  the result of which will be the capacity of the next arena
 */
PascalArena ArenaInit(
        UInt InitialCap, UInt SizeGrowFactor
);
/* free all arenas and set every field in the arena to 0 */
void ArenaDeinit(PascalArena *Arena);

/* reset all Used and CurrentIdx to 0, but does not free the arena */
void ArenaReset(PascalArena *Arena);


void *ArenaAllocate(PascalArena *Arena, USize Bytes);
void *ArenaAllocateZero(PascalArena *Arena, USize Bytes);


#endif /* PASCAL_MEMORY_H */

