
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
    };
    return Chunk;
}

void ChunkDeinit(PVMChunk *Chunk)
{
    MemDeallocateArray(Chunk->Code);
    MemDeallocate(Chunk->Global.Data.As.Raw);
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

U32 ChunkWriteMovImm(PVMChunk *Chunk, UInt Reg, U64 Imm, IntegralType DstType)
{
    U32 Addr = 0;
    int Count = 0;
    if (IS_SMALL_IMM(Imm))
    {
        Addr = ChunkWriteCode(Chunk, PVM_OP(MOVQI, Reg, Imm));
    }
    else switch (DstType)
    {
    case TYPE_I8:
    case TYPE_U8:
    case TYPE_I16:
    {
        Addr = ChunkWriteCode(Chunk, PVM_MOVI(Reg, I16));
        Count = 1;
    } break;
    case TYPE_U16:
    {
        Addr = ChunkWriteCode(Chunk, PVM_MOVI(Reg, U16));
        Count = 1;
    } break;
    case TYPE_I32:
    {
        Addr = ChunkWriteCode(Chunk, PVM_MOVI(Reg, I32));
        Count = 2;
    } break;
    case TYPE_U32:
    {
        Addr = ChunkWriteCode(Chunk, PVM_MOVI(Reg, U32));
        Count = 2;
    } break;

    case TYPE_I64:
    case TYPE_U64:
    {
        if (IN_I48(Imm))
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
    } break;
    case TYPE_F64:
    case TYPE_F32:
    default:
    {
        PASCAL_UNREACHABLE("ChunkWriteMovImm: Unreachable");
    } break;

    }


    for (int i = 0; i < Count; i++)
    {
        ChunkWriteCode(Chunk, Imm >> i*16);
    }
    return Addr;
}




U32 ChunkWriteGlobalData(PVMChunk *Chunk, const void *Data, U32 Size)
{
    if (Chunk->Global.Count + Size > Chunk->Global.Cap)
    {
        Chunk->Global.Cap = Chunk->Global.Cap * PVM_CHUNK_GROW_RATE + Size;
        Chunk->Global.Data.As.Raw = MemReallocateArray(*Chunk->Global.Data.As.u8,
                Chunk->Global.Data.As.u8,
                Chunk->Global.Cap
        );
    }

    U32 Old = Chunk->Global.Count;
    memcpy(&Chunk->Global.Data.As.u8[Chunk->Global.Count], Data, Size);
    Chunk->Global.Count += Size;
    return Old;
}

