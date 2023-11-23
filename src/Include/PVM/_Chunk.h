#ifndef PASCAL_CHUNK_H
#define PASCAL_CHUNK_H

#include "Common.h"

#define PVM_CHUNK_GROW_RATE 2
#define PVM_CHUNK_GLOBAL_MAX_SIZE (1 << 20)
#define PVM_CHUNK_READONLY_MAX_SIZE (1 << 20)


typedef struct PVMChunk 
{
    U16 *Code;
    U32 Count, Cap;

    struct {
        GenericPtr Data;
        U32 Count, Cap;
    } Global;
    struct {
        GenericPtr Data;
        U32 Count, Cap;
    } Readonly;
} PVMChunk;

PVMChunk ChunkInit(U32 InitialCap);
void ChunkDeinit(PVMChunk *Chunk);

U32 ChunkWriteCode(PVMChunk *Chunk, U16 Opcode);
U32 ChunkWriteMovImm(PVMChunk *Chunk, UInt Reg, U64 Imm);
U32 ChunkWriteReadOnlyData(PVMChunk *Chunk, const void *Data, U32 Size);
U32 ChunkWriteGlobalData(PVMChunk *Chunk, const void *Data, U32 Size);



#endif /* PASCAL_CHUNK_H */

