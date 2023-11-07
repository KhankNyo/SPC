
#include "Memory.h"
#include "PVM/CodeChunk.h"




CodeChunk CodeChunkInit(U32 InitialCapacity)
{
    return (CodeChunk) {
        .Data = MemAllocateArray(U32, InitialCapacity),
        .Cap = InitialCapacity, 
        .Count = 0,
    };
}

void CodeChunkDeinit(CodeChunk *Chunk)
{
    MemDeallocateArray(Chunk->Data);
    *Chunk = (CodeChunk) { 0 };
}

U32 CodeChunkWrite(CodeChunk *Chunk, U32 Word)
{
    if (Chunk->Count >= Chunk->Cap)
    {
        Chunk->Cap *= CODECHUNK_GROWTH_RATE;
        Chunk->Data = MemReallocateArray(U32, Chunk->Data, Chunk->Cap);
    }

    Chunk->Data[Chunk->Count] = Word;
    return Chunk->Count++;
}



