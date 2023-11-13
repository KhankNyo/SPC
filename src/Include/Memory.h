#ifndef PASCAL_MEMORY_H
#define PASCAL_MEMORY_H

#include "Common.h"

#define PASCAL_MEM_ALIGNMENT (sizeof(LargeType))



void *MemGetAllocator(void);
void MemInit(U32 InitialCap);
void MemDeinit(void);

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






typedef struct GPAHeader
{
    struct GPAHeader *Prev, *Next;
    USize Size;
    LargeType Data[];
} GPAHeader;


typedef struct PascalGPA 
{
    union {
        void *Raw;
        GPAHeader *Head;
        U8 *Bytes;
    } Mem;
    USize Cap;
    bool CoalesceOnFree;
} PascalGPA;


PascalGPA GPAInit(U32 InitialCap);
void GPADeinit(PascalGPA *GPA);

void *GPAAllocate(PascalGPA *GPA, U32 ByteCount);
void *GPAReallocate(PascalGPA *GPA, void *Ptr, U32 NewSize);
void GPADeallocate(PascalGPA *GPA, void *Ptr);





#define PASCAL_ARENA_COUNT 4

typedef struct PascalArena
{
    union {
        void *Raw;
        U8 *Bytes;
    } Mem[PASCAL_ARENA_COUNT];
    U32 Used[PASCAL_ARENA_COUNT];
    U32 Cap[PASCAL_ARENA_COUNT];
    UInt CurrentIdx;
} PascalArena;

/* 
 * InitialCap:  the capacity of the first arena,
 *              consecutive arenas will have their size multiplied by SizeGrowFactor 
 * SizeGrowFactor:  the factor to multiply with the current arena's capacity 
 *                  the result of which will be the capacity of the next arena
 */
PascalArena ArenaInit(U32 InitialCap, UInt SizeGrowFactor);

/* free all arenas and set every field in the arena to 0 */
void ArenaDeinit(PascalArena *Arena);

/* reset all Used and CurrentIdx to 0, but does not free the arena */
void ArenaReset(PascalArena *Arena);



/* Allocate memory from the arena, never returns NULL, crash and die instead */
void *ArenaAllocate(PascalArena *Arena, U32 Bytes);

/* Allocate memory inited to all 0's from the arena */
void *ArenaAllocateZero(PascalArena *Arena, U32 Bytes);


#endif /* PASCAL_MEMORY_H */

