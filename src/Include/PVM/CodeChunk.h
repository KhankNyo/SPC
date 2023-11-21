#ifndef PASCAL_VM_CODECHUNK_H
#define PASCAL_VM_CODECHUNK_H



#include "Common.h"
#include "PVM/Isa.h"

#define CODECHUNK_GROW_RATE 2


typedef struct LineDebugInfo 
{
    U32 InstructionOffset;
    UInt Len;
    const U8 *Str;
    U32 Line;
} LineDebugInfo;
typedef struct ChunkDebugInfo 
{
    LineDebugInfo *Info;
    U32 Cap, Count;
} ChunkDebugInfo;
typedef struct DataChunk 
{
    PVMPtr *Data;
    U32 Count, Cap;
} DataChunk;
typedef struct CodeChunk 
{
    U32 *Code;
    DataChunk DataSection;
    ChunkDebugInfo Debug;
    U32 Count, Cap;
} CodeChunk;

CodeChunk ChunkInit(U32 InitialCap);
void ChunkDeinit(CodeChunk *Chunk);
void ChunkReset(CodeChunk *Chunk);

U32 ChunkWriteCode(CodeChunk *Chunk, U32 Word);
U32 ChunkReserveData(CodeChunk *Chunk, U32 Size);
U32 ChunkWriteDebugInfo(CodeChunk *Chunk, UInt Len, const U8 *SrcString, U32 Line);
LineDebugInfo *ChunkGetDebugInfo(CodeChunk *Chunk, U32 InstructionOffset);



#endif /* PASCAL_VM_CODECHUNK_H */

