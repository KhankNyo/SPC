
#include "Common.h"

#include "Memory.h"
#include "PVM/_Chunk.h"
#include "PVM/_Isa.h"



PVMChunk ChunkInit(U32 InitialCap)
{
    PVMChunk Chunk = {
        .Cap = InitialCap,
        .Count = 0,
        .Code = MemAllocateArray(*Chunk.Code, InitialCap),

        .Global.Data.As.Raw = MemAllocateZero(PVM_CHUNK_GLOBAL_MAX_SIZE),
        .Global.Cap = PVM_CHUNK_GLOBAL_MAX_SIZE,
        .Readonly.Data.As.Raw = MemAllocateZero(PVM_CHUNK_READONLY_MAX_SIZE),
        .Readonly.Cap = PVM_CHUNK_READONLY_MAX_SIZE,
    };
    return Chunk;
}

void ChunkDeinit(PVMChunk *Chunk)
{
    MemDeallocateArray(Chunk->Code);
    MemDeallocate(Chunk->Global.Data.As.Raw);
    MemDeallocate(Chunk->Readonly.Data.As.Raw);
    *Chunk = (PVMChunk){ 0 };
}

U32 ChunkWriteCode(PVMChunk *Chunk, U16 Opcode)
{
    if (Chunk->Count + 1 > Chunk->Cap)
    {
        Chunk->Cap *= PVM_CHUNK_GROW_RATE;
        Chunk->Code = MemReallocateArray(*Chunk->Code,
                Chunk->Code, Chunk->Cap
        );
    }

    Chunk->Code[Chunk->Count] = Opcode;
    return Chunk->Count++;
}

U32 ChunkWriteMovImm(PVMChunk *Chunk, UInt Reg, U64 Imm)
{
    U32 Addr = 0;
    int Count = 1;
    if (IN_I8(Imm) || IN_U8(Imm) || IN_I16(Imm))
    {
        Addr = ChunkWriteCode(Chunk, PVM_MOVI(Reg, I16));
    }
    else if (IN_U16(Imm))
    {
        Addr = ChunkWriteCode(Chunk, PVM_MOVI(Reg, U16));
    }
    else if (IN_I32(Imm))
    {
        Addr = ChunkWriteCode(Chunk, PVM_MOVI(Reg, I32));
        Count = 2;
    }
    else if (IN_U32(Imm))
    {
        Addr = ChunkWriteCode(Chunk, PVM_MOVI(Reg, U32));
        Count = 2;
    }
    else if (IN_I48(Imm))
    {
        Addr = ChunkWriteCode(Chunk, PVM_MOVI(Reg, I48));
        Count = 3;
    }
    else if (IN_U48(Imm))
    {
        Addr = ChunkWriteCode(Chunk, PVM_MOVI(Reg, U48));
        Count = 3;
    }
    else 
    {
        Addr = ChunkWriteCode(Chunk, PVM_MOVI(Reg, U64));
        Count = 4;
    }

    for (int i = 0; i < Count; i++)
    {
        ChunkWriteCode(Chunk, Imm >> i*16);
    }
    return Addr;
}




U32 ChunkWriteReadOnlyData(PVMChunk *Chunk, const void *Data, U32 Size)
{
    if (Chunk->Readonly.Count + Size > Chunk->Readonly.Cap)
    {
        PASCAL_UNREACHABLE("TODO: more graceful way of handling this.");
    }

    U32 Old = Chunk->Readonly.Count;
    memcpy(&Chunk->Readonly.Data.As.u8[Chunk->Readonly.Count], Data, Size);
    Chunk->Readonly.Count += Size;
    return Old;
}

U32 ChunkWriteGlobalData(PVMChunk *Chunk, const void *Data, U32 Size)
{
    if (Chunk->Global.Count + Size > Chunk->Global.Cap)
    {
        PASCAL_UNREACHABLE("TODO: more graceful way of handling this.");
    }

    U32 Old = Chunk->Global.Count;
    memcpy(&Chunk->Global.Data.As.u8[Chunk->Global.Count], Data, Size);
    Chunk->Global.Count += Size;
    return Old;
}

