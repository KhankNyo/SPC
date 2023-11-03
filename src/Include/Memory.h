#ifndef PASCAL_MEMORY_H
#define PASCAL_MEMORY_H

#include "Common.h"

/* 
 * returns a pointer to a block of memory with size of ByteCount
 * never return NULL 
 */
void *MemAllocate(USize ByteCount);

/* 
 * NewSize of 0 is a nop,
 * otherwise, this function acts like realloc without ever returning NULL
 */
void *MemReallocate(void *Pointer, USize NewSize);

/* 
 * Deallocate a block of memory, 
 * currently a wrapper around free
 */
void MemDeallocate(void *Pointer);

#endif /* PASCAL_MEMORY_H */

