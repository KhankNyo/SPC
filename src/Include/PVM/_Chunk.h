#ifndef PASCAL_CHUNK_H
#define PASCAL_CHUNK_H

#include "Common.h"
#include "IntegralTypes.h"

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
} PVMChunk;

PVMChunk ChunkInit(U32 InitialCap);
void ChunkDeinit(PVMChunk *Chunk);

U32 ChunkWriteCode(PVMChunk *Chunk, U16 Opcode);
U32 ChunkWriteMovImm(PVMChunk *Chunk, UInt Reg, U64 Imm, IntegralType DstType);
U32 ChunkWriteDouble(PVMChunk *Chunk, UInt Reg, F64 Double);
U32 ChunkWriteFloat(PVMChunk *Chunk, UInt Reg, F32 Float);
U32 ChunkWriteGlobalData(PVMChunk *Chunk, const void *Data, U32 Size);



#endif /* PASCAL_CHUNK_H */

