#ifndef PASCAL_VM_CODECHUNK_H
#define PASCAL_VM_CODECHUNK_H



#include "Common.h"

#define CODECHUNK_GROW_RATE 2

typedef struct DataChunk 
{
    F64 *Data;
    U32 Count, Cap;
} DataChunk;
typedef struct CodeChunk 
{
    U32 *Code;
    DataChunk DataSection;
    U32 Count, Cap;
} CodeChunk;

CodeChunk ChunkInit(U32 InitialCap);
void ChunkDeinit(CodeChunk *Chunk);

U32 ChunkWriteCode(CodeChunk *Chunk, U32 Word);
U32 ChunkWriteData(CodeChunk *Chunk, F64 Real);



#endif /* PASCAL_VM_CODECHUNK_H */

