#ifndef PASCAL_CHUNK_H
#define PASCAL_CHUNK_H

#include "Common.h"
#include "IntegralTypes.h"

#define PVM_CHUNK_GROW_RATE 2
#define PVM_CHUNK_GLOBAL_MAX_SIZE (1 << 20)



typedef struct LineDebugInfo 
{
    const U8 *Src[8];
    U32 SrcLen[8];
    U32 Line[8];
    bool IsSubroutine;

    U32 Count;
    U32 StreamOffset;
} LineDebugInfo;
typedef struct PVMChunk 
{
    U16 *Code;
    U32 Count, Cap;
    U32 EntryPoint;

    struct {
        GenericPtr Data;
        U32 Count, Cap;
    } Global;

    struct {
        LineDebugInfo *Info;
        U32 Count, Cap;
    } Debug;
} PVMChunk;

PVMChunk ChunkInit(U32 InitialCap);
void ChunkDeinit(PVMChunk *Chunk);

U32 ChunkWriteCode(PVMChunk *Chunk, U16 Opcode);
U32 ChunkWriteMovImm(PVMChunk *Chunk, UInt Reg, U64 Imm);
U32 ChunkWriteDouble(PVMChunk *Chunk, UInt Reg, F64 Double);
U32 ChunkWriteFloat(PVMChunk *Chunk, UInt Reg, F32 Float);
U32 ChunkWriteGlobalData(PVMChunk *Chunk, const void *Data, U32 Size);
void ChunkReset(PVMChunk *Chunk, bool PreserveFunctions);

void ChunkWriteDebugInfo(PVMChunk *Chunk, const U8 *Src, U32 SrcLen, U32 Line);
LineDebugInfo *ChunkGetDebugInfo(PVMChunk *Chunk, U32 StreamOffset);
const LineDebugInfo *ChunkGetConstDebugInfo(const PVMChunk *Chunk, U32 StreamOffset);




#endif /* PASCAL_CHUNK_H */

