
#include "Common.h"

#include "Memory.h"
#include "PVM/Chunk.h"
#include "PVM/Isa.h"



PVMChunk ChunkInit(U32 InitialCap)
{
    PVMChunk Chunk = {
        .Cap = InitialCap,
        .Count = 0,
        .Code = MemAllocateArray(*Chunk.Code, InitialCap),

        .Global.Data.As.Raw = MemAllocateZero(1024),
        .Global.Cap = 1024,

        .Debug = {
            .Cap = 64,
            .Count = 0,
            .Info = MemAllocateArray(*Chunk.Debug.Info, 64),
        },
        .EntryPoint = 0,
    };
    return Chunk;
}

void ChunkDeinit(PVMChunk *Chunk)
{
    MemDeallocateArray(Chunk->Code);
    MemDeallocate(Chunk->Global.Data.As.Raw);
    *Chunk = (PVMChunk){ 0 };
}

void ChunkReset(PVMChunk *Chunk, bool PreserveFunctions)
{
    if (PreserveFunctions)
    {
        Chunk->Count = Chunk->EntryPoint;
        /* TODO: debug info */
        U32 DebugInfoCount = Chunk->Debug.Count;
        for (U32 i = 0; i < Chunk->Debug.Count; i++)
        {
            if (Chunk->Debug.Info[i].StreamOffset >= Chunk->EntryPoint)
            {
                DebugInfoCount--;
                Chunk->Debug.Info[i].Count = 0;
            }
        }
        Chunk->Debug.Count = DebugInfoCount;
    }
    else
    {
        Chunk->Count = 0;
        Chunk->Global.Count = 0;
        Chunk->Debug.Count = 0;
    }
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
    int Count = 0;
    if (IS_SMALL_IMM(Imm))
    {
        Addr = ChunkWriteCode(Chunk, PVM_OP(MOVQI, Reg, Imm));
    }
    else if (IN_I16(Imm))
    {
        Addr = ChunkWriteCode(Chunk, PVM_MOVI(Reg, I16));
        Count = 1;
    }
    else if (IN_U16(Imm))
    {
        Addr = ChunkWriteCode(Chunk, PVM_MOVI(Reg, U16));
        Count = 1;
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
    if (NULL != Data)
    {
        memcpy(&Chunk->Global.Data.As.u8[Chunk->Global.Count], Data, Size);
    }

    /* round to word boundary */
    if (Size % sizeof(U32))
    {
        Size = (Size + sizeof(U32)) & ~(sizeof(U32) - 1);
    }
    Chunk->Global.Count += Size;
    return Old;
}




void ChunkWriteDebugInfo(PVMChunk *Chunk, const U8 *Src, U32 SrcLen, U32 Line)
{    
    /* diff src mapped to the same instruction */
    if (Chunk->Debug.Count > 0 && Chunk->Debug.Info[Chunk->Debug.Count - 1].StreamOffset == Chunk->Count)
    {
        LineDebugInfo *CurrentInfo = &Chunk->Debug.Info[Chunk->Debug.Count - 1];

        U32 SrcCount = CurrentInfo->Count;
        if (SrcCount < 8)
        {
            CurrentInfo->Count++;
        }
        else
        {
            SrcCount = 7;
        }

        CurrentInfo->Src[SrcCount] = Src;
        CurrentInfo->SrcLen[SrcCount] = SrcLen;
        CurrentInfo->Line[SrcCount] = Line;
        return;
    }


    /* resize */
    if (Chunk->Debug.Count >= Chunk->Debug.Cap)
    {
        Chunk->Debug.Cap *= PVM_CHUNK_GROW_RATE;
        Chunk->Debug.Info = MemReallocateArray(*Chunk->Debug.Info, 
                Chunk->Debug.Info, Chunk->Debug.Cap
        );
    }

    /* brand new line maps to a branch new instruction */
    LineDebugInfo *CurrentInfo = &Chunk->Debug.Info[Chunk->Debug.Count++];
    CurrentInfo->Count = 1;

    CurrentInfo->Src[0] = Src;
    CurrentInfo->SrcLen[0] = SrcLen;
    CurrentInfo->Line[0] = Line;
    CurrentInfo->StreamOffset = Chunk->Count;
}


LineDebugInfo *ChunkGetDebugInfo(PVMChunk *Chunk, U32 StreamOffset)
{
    U32 InfoCount = Chunk->Debug.Count;
    if (InfoCount == 0)
    {
        return NULL;
    }

    /* chk upper bound */
    if (InfoCount > 0 
    && Chunk->Debug.Info[InfoCount - 1].StreamOffset <= StreamOffset)
    {
        return &Chunk->Debug.Info[InfoCount - 1];
    }

    LineDebugInfo *Ret = &Chunk->Debug.Info[InfoCount / 2];
    LineDebugInfo *Upper = &Chunk->Debug.Info[InfoCount - 1];
    LineDebugInfo *Lower = &Chunk->Debug.Info[0];

    /* bin search */
    while (Upper - Lower > 10)
    {
        if (StreamOffset > Ret->StreamOffset)
        {
            Lower = Ret;
            Ret = Lower + (Upper - Ret) / 2;
        }
        else if (StreamOffset < Ret->StreamOffset)
        {
            Upper = Ret;
            Ret = Lower + (Ret - Lower) / 2;
        }
        else return Ret;
    }

    /* linear search */
    Ret = Lower;
    while (Ret < Upper)
    {
        if (Ret->StreamOffset == StreamOffset
        || (Ret + 1)->StreamOffset > StreamOffset)
            break;
        Ret++;
    }
    return Ret;
}

const LineDebugInfo *ChunkGetConstDebugInfo(const PVMChunk *Chunk, U32 StreamOffset)
{
    return ChunkGetDebugInfo((PVMChunk *)Chunk, StreamOffset);
}


