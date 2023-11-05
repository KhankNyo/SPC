#ifndef PASCAL_VM_CODECHUNK_H
#define PASCAL_VM_CODECHUNK_H



#include "Common.h"

#define CODECHUNK_GROWTH_RATE 2
typedef struct CodeChunk 
{
    U32 *Data;
    U32 Count, Cap;
} CodeChunk;

CodeChunk CodeChunkInit(U32 InitialCap);
void CodeChunkDeinit(CodeChunk *Chunk);

U32 CodeChunkWrite(CodeChunk *Chunk, U32 Word);
void CodeChunkDisasm(FILE *f, const CodeChunk *Chunk);



#endif /* PASCAL_VM_CODECHUNK_H */

