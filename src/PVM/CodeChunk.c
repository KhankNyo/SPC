
#include "Memory.h"
#include "PVM/CodeChunk.h"




CodeChunk ChunkInit(U32 InitialCapacity)
{
    return (CodeChunk) {
        .Code = MemAllocateArray(U32, InitialCapacity),
        .Cap = InitialCapacity, 
        .Count = 0,
        .DataSection = {
            .Count = 0,
            .Cap = InitialCapacity,
            .Data = MemAllocateArray(F64, InitialCapacity),
        },
    };
}

void ChunkDeinit(CodeChunk *Chunk)
{
    MemDeallocateArray(Chunk->Code);
    MemDeallocateArray(Chunk->DataSection.Data);
    *Chunk = (CodeChunk) { 0 };
}

U32 ChunkWriteCode(CodeChunk *Chunk, U32 Word)
{
    if (Chunk->Count >= Chunk->Cap)
    {
        Chunk->Cap *= CODECHUNK_GROW_RATE;
        Chunk->Code = MemReallocateArray(U32, Chunk->Code, Chunk->Cap);
    }

    Chunk->Code[Chunk->Count] = Word;
    return Chunk->Count++;
}

U32 ChunkWriteData(CodeChunk *Chunk, F64 Data)
{
    DataChunk *DataSection = &Chunk->DataSection;
    if (DataSection->Count <= DataSection->Cap)
    {
        DataSection->Cap *= CODECHUNK_GROW_RATE;
        DataSection->Data = MemReallocateArray(F64, DataSection->Data, DataSection->Cap);
    }

    DataSection->Data[DataSection->Count] = Data;
    return DataSection->Count++;
}




