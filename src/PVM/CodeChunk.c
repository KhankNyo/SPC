
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
            .Data = MemAllocateArray(PVMPtr, InitialCapacity),
        },
        .Debug = {
            .Count = 0,
            .Cap = InitialCapacity,
            .Info = MemAllocateArray(LineDebugInfo, InitialCapacity),
        },
    };
}

void ChunkDeinit(CodeChunk *Chunk)
{
    MemDeallocateArray(Chunk->Code);
    MemDeallocateArray(Chunk->DataSection.Data);
    MemDeallocateArray(Chunk->Debug.Info);
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


U32 ChunkReserveData(CodeChunk *Chunk, U32 Size)
{
    DataChunk *DataSection = &Chunk->DataSection;
    U32 Count = DataSection->Count;
    if (Count + Size >= DataSection->Cap)
    {
        DataSection->Cap *= CODECHUNK_GROW_RATE;
        DataSection->Data = MemReallocateArray(*DataSection->Data, 
                DataSection->Data, DataSection->Cap
        );
    }
    DataSection->Count += Size;
    return Count;
}


U32 ChunkWriteDebugInfo(CodeChunk *Chunk, UInt Len, const U8 *SrcString, U32 Line)
{
    ChunkDebugInfo *DebugInfo = &Chunk->Debug;
    U32 Count = DebugInfo->Count;
    if (Count >= DebugInfo->Cap)
    {
        DebugInfo->Cap *= CODECHUNK_GROW_RATE;
        DebugInfo->Info = MemReallocateArray(*DebugInfo->Info, 
                DebugInfo->Info, DebugInfo->Cap
        );
    }

    DebugInfo->Info[Count].InstructionOffset = Chunk->Count;
    DebugInfo->Info[Count].Len = Len;
    DebugInfo->Info[Count].Str = SrcString;
    DebugInfo->Info[Count].Line = Line;
    return DebugInfo->Count++;
}


const LineDebugInfo *ChunkGetDebugInfo(const CodeChunk *Chunk, U32 InstructionOffset)
{
    const ChunkDebugInfo *Dbg = &Chunk->Debug;
    if (Dbg->Count != 0 && InstructionOffset >= Dbg->Info[Dbg->Count - 1].InstructionOffset)
    {
        return &Dbg->Info[Dbg->Count - 1];
    }
    if (Dbg->Count == 0)
    {
        static LineDebugInfo NoInfo = {0};
        return &NoInfo;
    }

    const LineDebugInfo *Info = &Dbg->Info[0];
    for (U32 i = 0; i < Dbg->Count; i++)
    {
        if (Dbg->Info[i].InstructionOffset == InstructionOffset)
            return &Dbg->Info[i];
    }
    return Info;
}


