
#include <stdarg.h>

#include "Compiler/Emitter.h"
#include "Compiler/Data.h"
#include "PVM/Isa.h"



/*
 * SP, GP, FP are allocated by default
 * */
#define EMPTY_REGLIST 0xE000
PASCAL_STATIC_ASSERT(IS_POW2(sizeof(PVMGPR)), "Unreachable");

#if UINTPTR_MAX == UINT32_MAX
#  define CASE_PTR32(Colon) case TYPE_POINTER Colon case TYPE_FUNCTION Colon
#  define CASE_PTR64(Colon)
#  define CASE_OBJREF32(Colon) case TYPE_STRING Colon case TYPE_RECORD Colon case TYPE_STATIC_ARRAY Colon
#  define CASE_OBJREF64(Colon)
#else
#  define CASE_PTR32(Colon)
#  define CASE_PTR64(Colon) case TYPE_POINTER Colon case TYPE_FUNCTION Colon
#  define CASE_OBJREF32(Colon)
#  define CASE_OBJREF64(Colon) case TYPE_STRING Colon case TYPE_RECORD Colon case TYPE_STATIC_ARRAY Colon
#endif

#define OP32_OR_OP64(pEmitter, Mnemonic, bOperandIs64, Dst, Src) do{\
    if (bOperandIs64) {\
        WriteOp16(Emitter, PVM_OP(Mnemonic ## 64, Dst, Src));\
    } else {\
        WriteOp16(Emitter, PVM_OP(Mnemonic, Dst, Src));\
    }\
} while (0)

static void PVMEmitIMulConst(PVMEmitter *Emitter, VarRegister Dst, IntegralType RegisterType, I64 Const);



PVMEmitter PVMEmitterInit(PVMChunk *Chunk)
{
    PASCAL_NONNULL(Chunk);

    PVMEmitter Emitter = {
        .Chunk = Chunk,
        .Reglist = EMPTY_REGLIST,
        .SpilledIntRegs = 0,
        .SpilledFltRegs = 0,
        .Reg = {
            .SP = VAR_LOCATION_REG(
                PVM_REG_SP, true, 
                VarTypePtr(NULL)
            ),
            .FP = VAR_LOCATION_REG(
                PVM_REG_FP, true,
                VarTypePtr(NULL)
            ),
            .GP = VAR_LOCATION_REG(
                PVM_REG_GP, true, 
                VarTypePtr(NULL)
            ),
            .Flag = {
                .LocationType = VAR_FLAG,
                .Type = VarTypeInit(TYPE_BOOLEAN, 0),
                .As.FlagValueAsIs = true,
            },
        },
        .ReturnValue = VAR_LOCATION_REG(
            PVM_RETREG, false,
            VarTypeInit(TYPE_INVALID, 0)
        ),
        .ShouldEmit = true,
    };
    return Emitter;
}

void PVMEmitterDeinit(PVMEmitter *Emitter)
{
    PASCAL_NONNULL(Emitter);
    PVMEmitExit(Emitter);
    bool ShouldPreserveFunctions = false;
    PVMEmitterReset(Emitter, ShouldPreserveFunctions);
}

static PVMChunk *PVMCurrentChunk(PVMEmitter *Emitter)
{
    PASCAL_NONNULL(Emitter);
    return Emitter->Chunk;
}

void PVMEmitterReset(PVMEmitter *Emitter, bool PreserveFunctions)
{
    ChunkReset(PVMCurrentChunk(Emitter), PreserveFunctions);
    Emitter->Reglist = EMPTY_REGLIST;
    Emitter->StackSpace = 0;
    Emitter->SpilledIntRegs = 0;
    Emitter->SpilledFltRegs = 0;
    Emitter->SpilledRegSpace = 0;
}






static U32 WriteOp16(PVMEmitter *Emitter, U16 Opcode)
{
    PASCAL_NONNULL(Emitter);
    if (!Emitter->ShouldEmit) 
        return PVMCurrentChunk(Emitter)->Count;
    return ChunkWriteCode(PVMCurrentChunk(Emitter), Opcode);
}

static U32 WriteOp32(PVMEmitter *Emitter, U16 Opcode, U16 SecondHalf)
{
    PASCAL_NONNULL(Emitter);
    U32 Location = WriteOp16(Emitter, Opcode);
    WriteOp16(Emitter, SecondHalf);
    return Location;
}

static U32 Write32(PVMEmitter *Emitter, U32 DWord)
{
    return WriteOp32(Emitter, DWord, DWord >> 16);
}

void PVMEmitMoveImm(PVMEmitter *Emitter, VarRegister Reg, I64 Imm)
{
    if (Emitter->ShouldEmit)
    {
        ChunkWriteMovImm(Emitter->Chunk, Reg.ID, Imm);
    }
}


bool PVMRegisterIsFree(PVMEmitter *Emitter, UInt Reg)
{
    PASCAL_NONNULL(Emitter);
    return ((Emitter->Reglist >> Reg) & 1) == 0;
}

static void PVMMarkRegisterAsAllocated(PVMEmitter *Emitter, UInt Reg)
{
    PASCAL_NONNULL(Emitter);
    if (PVM_REG_COUNT + PVM_FREG_COUNT >= Reg)
    {
        Emitter->Reglist |= (U32)1 << Reg;
    }
}

static void PVMMarkRegisterAsFreed(PVMEmitter *Emitter, UInt Reg)
{
    PASCAL_NONNULL(Emitter);
    Emitter->Reglist &= ~((U32)1 << Reg);
}


static U32 PVMCreateRegListLocation(PVMEmitter *Emitter, 
        SaveRegInfo Info, U32 Location[PVM_REG_COUNT + PVM_FREG_COUNT])
{
    U32 Size = 0;
    for (int i = STATIC_ARRAY_SIZE(Info.RegLocation) - 1; i >= 0; i--)
    {
        if ((Info.Regs >> i) & 0x1)
        {
            /* free registers that are not in EMPTY_REGLIST */
            /* freeing bc those regs now live on the stack */
            Location[i] = Emitter->StackSpace;
            VarRegister Register = {
                .ID = i,
                .Persistent = EMPTY_REGLIST & ((U32)1 << i),
            };
            PVMFreeRegister(Emitter, Register);
            Emitter->StackSpace += sizeof(PVMGPR);
            Size += sizeof(PVMGPR);
        }
    }
    return Size;
}


static SaveRegInfo PVMEmitPushRegList(PVMEmitter *Emitter, U32 RegList)
{
    SaveRegInfo Info = {
        .Regs = RegList,
        .Size = sizeof(PVMGPR) * BitCount(RegList),
        .RegLocation = { 0 },
    };
    if (Info.Regs & 0xFF)
    {
        WriteOp16(Emitter, PVM_REGLIST(PSHL, Info.Regs & 0xFF));
    }
    if ((Info.Regs >> 8) & 0xFF)
    {
        WriteOp16(Emitter, PVM_REGLIST(PSHH, Info.Regs >> 8));
    }
    if ((Info.Regs >> 16) & 0xFF)
    {
        WriteOp16(Emitter, PVM_REGLIST(FPSHL, Info.Regs >> 16));
    }
    if ((Info.Regs >> 24) & 0xFF)
    {
        WriteOp16(Emitter, PVM_REGLIST(FPSHH, Info.Regs >> 24));
    }

    PVMCreateRegListLocation(Emitter, Info, Info.RegLocation);
    return Info;
}

static void PVMEmitPushReg(PVMEmitter *Emitter, UInt Reg)
{
    PASCAL_NONNULL(Emitter);
    U32 RegIndex = 1 << Reg;
    if ((RegIndex & 0xFF))
    {
        WriteOp16(Emitter, PVM_REGLIST(PSHL, RegIndex));
    }
    else if ((RegIndex >> 8) & 0xFF)
    {
        WriteOp16(Emitter, PVM_REGLIST(PSHH, RegIndex >> 8));
    }
    /* floating point reg */
    else if ((RegIndex >> 16) & 0xFF)
    {
        WriteOp16(Emitter, PVM_REGLIST(FPSHL, RegIndex >> 16));
    }
    else 
    {
        WriteOp16(Emitter, PVM_REGLIST(FPSHH, RegIndex >> 24));
    }
    Emitter->StackSpace += sizeof(PVMGPR);
}

static void PVMEmitPopReg(PVMEmitter *Emitter, UInt Reg)
{
    PASCAL_NONNULL(Emitter);
    U32 RegIndex = 1 << Reg;
    if (RegIndex & 0xFF)
    {
        WriteOp16(Emitter, PVM_REGLIST(POPL, RegIndex));
    }
    else if ((RegIndex >> 8) & 0xFF)
    {
        WriteOp16(Emitter, PVM_REGLIST(POPH, RegIndex >> 8));
    }
    /* floating point reg */
    else if ((RegIndex >> 16) & 0xFF)
    {
        WriteOp16(Emitter, PVM_REGLIST(FPOPL, RegIndex >> 16));
    }
    else 
    {
        WriteOp16(Emitter, PVM_REGLIST(FPOPH, RegIndex >> 24));
    }
    Emitter->StackSpace -= sizeof(PVMGPR);
}



static bool OperandIs64(IntegralType Type)
{
    return Type == TYPE_I64
        || Type == TYPE_U64 
        || Type == TYPE_F64 
        || (UINT64_MAX == UINTPTR_MAX && Type == TYPE_POINTER);
}



static void MoveRegToReg(PVMEmitter *Emitter, 
        VarRegister Dst, VarType DstType, VarRegister Src, VarType SrcType)
{
#define OP(Op) WriteOp16(Emitter, PVM_OP(Op, Dst.ID, Src.ID))

    switch (DstType.Integral)
    {
    case TYPE_I8:
    case TYPE_I16:
    case TYPE_I32:
    {
        switch (SrcType.Integral)
        {
        case TYPE_U8:
        case TYPE_CHAR:
        case TYPE_BOOLEAN:  OP(MOVZEX32_8); break;
        case TYPE_U16:      OP(MOVZEX32_16); break;
        default:            if (Dst.ID != Src.ID) OP(MOV32); break;
        }
    } break;

    case TYPE_U8:
    case TYPE_BOOLEAN:
    case TYPE_CHAR:
    case TYPE_U16:
    case TYPE_U32:
    CASE_PTR32(:)
    CASE_OBJREF32(:)
    {
        switch (SrcType.Integral)
        {
        case TYPE_I8:   OP(MOVZEX32_8); break;
        case TYPE_I16:  OP(MOVZEX32_16); break;
        default:        if (Dst.ID != Src.ID) OP(MOV32); break;
        }
    } break;

    case TYPE_U64:
    CASE_PTR64(:)
    CASE_OBJREF64(:)
    {
        switch (SrcType.Integral)
        {
        default:        if (Dst.ID != Src.ID) OP(MOV64); break;
        case TYPE_I32:
        case TYPE_U32:  OP(MOVZEX64_32); break;
        case TYPE_I16:
        case TYPE_U16:  OP(MOVZEX64_16); break;
        case TYPE_BOOLEAN:
        case TYPE_CHAR:
        case TYPE_I8:
        case TYPE_U8:   OP(MOVZEX64_8); break;
        }
    } break;

    case TYPE_I64:
    {
        switch (SrcType.Integral)
        {
        default:        if (Dst.ID != Src.ID) OP(MOV64); break;
        case TYPE_I8:
        case TYPE_I16:
        case TYPE_I32:  OP(MOVSEX64_32); break;
        case TYPE_BOOLEAN:
        case TYPE_CHAR:
        case TYPE_U8:
        case TYPE_U16:
        case TYPE_U32:  OP(MOVZEX64_32); break;
        }
    } break;

    case TYPE_F32:
    {
        switch (DstType.Integral)
        {
        case TYPE_F32: if (Dst.ID != Src.ID) OP(FMOV); break;
        case TYPE_F64: OP(F32TOF64); break;
        default: goto Unreachable;
        }
    } break;
    case TYPE_F64:
    {
        switch (DstType.Integral)
        {
        case TYPE_F32: OP(F64TOF32); break;
        case TYPE_F64: if (Dst.ID != Src.ID) OP(FMOV64); break;
        default: goto Unreachable;
        }
    } break;

    case TYPE_INVALID:
    case TYPE_COUNT:
    {
        StringView DstStr, SrcStr;
Unreachable:
        DstStr = VarTypeToStringView(DstType);
        SrcStr = VarTypeToStringView(SrcType);
        PASCAL_UNREACHABLE("Move "STRVIEW_FMT", "STRVIEW_FMT" is invalid in function %s",
            STRVIEW_FMT_ARG(DstStr), STRVIEW_FMT_ARG(SrcStr),
            __func__
        );
    } break;
    }
#undef OP
}

static void MoveMemToReg(PVMEmitter *Emitter,
        VarRegister Dst, VarMemory Src, VarType SrcType)
{
#define OP(LoadOp) do {\
    if (IN_I16((I32)Src.Location)) {\
        WriteOp32(Emitter, PVM_OP(LoadOp, Dst.ID, Src.RegPtr.ID), Src.Location);\
    } else {\
        WriteOp16(Emitter, PVM_OP(LoadOp ## L, Dst.ID, Src.RegPtr.ID));\
        Write32(Emitter, Src.Location);\
    }\
} while (0)
    switch (SrcType.Integral)
    {
    case TYPE_I8: OP(LDSEX32_8); break;
    case TYPE_I16: OP(LDSEX32_16); break;
    case TYPE_U32:
    CASE_PTR32(:)
    case TYPE_I32: OP(LD32); break;
    case TYPE_U64:
    CASE_PTR64(:)
    case TYPE_I64: OP(LD64); break;

    case TYPE_BOOLEAN:
    case TYPE_CHAR:
    case TYPE_U8: OP(LDZEX32_8); break;
    case TYPE_U16: OP(LDZEX32_8); break;
    case TYPE_F32: OP(LDF32); break;
    case TYPE_F64: OP(LDF64); break;

    CASE_OBJREF32(:)
    CASE_OBJREF64(:)
    {
        PVMEmitLoadAddr(Emitter, Dst, Src);
    } break;

    case TYPE_INVALID:
    case TYPE_COUNT:
    {
        PASCAL_UNREACHABLE("Invalid type in %s", __func__);
    } break;
    }
#undef OP
}


static void MoveLiteralToReg(PVMEmitter *Emitter, 
        VarRegister Reg, VarType Type, const VarLiteral *Literal
)
{
    if (!Emitter->ShouldEmit)
        return;
    PVMChunk *Chunk = PVMCurrentChunk(Emitter);
    switch (Type.Integral)
    {
    case TYPE_BOOLEAN:
    case TYPE_CHAR:
    case TYPE_I8:
    case TYPE_I16:
    case TYPE_I32:
    case TYPE_I64:
    case TYPE_U8:
    case TYPE_U16:
    case TYPE_U32:
    case TYPE_U64:
    CASE_PTR32(:)
    CASE_PTR64(:)
    {
        PVMEmitMoveImm(Emitter, Reg, Literal->Int);
    } break;
    case TYPE_F32:
    {
        F32 Float = Literal->Flt;
        U32 Location = ChunkWriteGlobalData(Chunk, &Float, sizeof Float);
        VarMemory Mem = {
            .Location = Location,
            .RegPtr = Emitter->Reg.GP.As.Register,
        };
        MoveMemToReg(Emitter, Reg, Mem, Type);
    } break;
    case TYPE_F64:
    {
        F64 Float = Literal->Flt;
        U32 Location = ChunkWriteGlobalData(Chunk, &Float, sizeof Float);
        VarMemory Mem = {
            .Location = Location,
            .RegPtr = Emitter->Reg.GP.As.Register,
        };
        MoveMemToReg(Emitter, Reg, Mem, Type);
    } break;
    case TYPE_STRING:
    {
        /* TODO: other types of string */
        U32 Location = ChunkWriteGlobalData(Chunk, &Literal->Str, PStrGetLen(&Literal->Str) + 1); 
        VarMemory Mem = {
            .Location = Location,
            .RegPtr = Emitter->Reg.GP.As.Register,
        };
        MoveMemToReg(Emitter, Reg, Mem, Type);
    } break;

    case TYPE_STATIC_ARRAY:
    case TYPE_RECORD:
    /* TODO: static array and record? */
    case TYPE_INVALID:
    case TYPE_COUNT:
    {
        PASCAL_UNREACHABLE("Invalid type in %s", __func__);
    } break;
    }
}


static void MoveLocationToReg(PVMEmitter *Emitter, 
        VarRegister Dst, VarType Type, const VarLocation *Src)
{
    switch (Src->LocationType)
    {
    case VAR_INVALID:
    case VAR_BUILTIN:
    case VAR_SUBROUTINE:
    {
        PASCAL_UNREACHABLE("");
    } break;

    case VAR_FLAG:
    {
        WriteOp16(Emitter, PVM_OP(GETFLAG, Dst.ID, 0));
    } break;
    case VAR_LIT:
    {
        MoveLiteralToReg(Emitter, Dst, Type, &Src->As.Literal);
    } break;
    case VAR_REG:
    {
        MoveRegToReg(Emitter, Dst, Type, Src->As.Register, Src->Type);
    } break;
    case VAR_MEM:
    {
        MoveMemToReg(Emitter, Dst, Src->As.Memory, Src->Type);
    } break;
    }
}

static void MoveRegToMem(PVMEmitter *Emitter, 
        VarMemory Dst, VarType DstType, VarRegister Src, VarType SrcType)
{
#define OP(StoreOp) do {\
    if (IN_I16((I32)Dst.Location)) {\
        WriteOp32(Emitter, PVM_OP(StoreOp, Src.ID, Dst.RegPtr.ID), Dst.Location);\
    } else {\
        WriteOp16(Emitter, PVM_OP(StoreOp ## L, Src.ID, Dst.RegPtr.ID));\
        Write32(Emitter, Dst.Location);\
    }\
} while (0)
    PASCAL_NONNULL(Emitter);
    if (!VarTypeEqual(&DstType, &SrcType)) 
    {
        MoveRegToReg(Emitter, Src, DstType, Src, SrcType);
    }

    switch (DstType.Integral)
    {
    case TYPE_CHAR:
    case TYPE_BOOLEAN:
    case TYPE_I8:
    case TYPE_U8: OP(ST8); break;
    case TYPE_I16:
    case TYPE_U16: OP(ST16); break;
    case TYPE_I32:
    CASE_PTR32(:)
    CASE_OBJREF32(:)
    case TYPE_U32: OP(ST32); break;
    case TYPE_I64:
    CASE_PTR64(:)
    CASE_OBJREF64(:)
    case TYPE_U64: OP(ST64); break;

    case TYPE_F32: OP(STF32); break;
    case TYPE_F64: OP(STF64); break;

    case TYPE_INVALID:
    case TYPE_COUNT:
    {
        PASCAL_UNREACHABLE("Invalid type in %s", __func__);
    } break;
    }
#undef OP
}


void PVMEmitLoadAddr(PVMEmitter *Emitter, VarRegister Dst, VarMemory Src)
{
    PASCAL_NONNULL(Emitter);

    UInt Base = Src.RegPtr.ID;
    UInt Rd = Dst.ID;
    I32 Offset = Src.Location;
    if (0 == Offset)
    {
        if (Rd == Base)
            return;

        if (UINTPTR_MAX == UINT32_MAX)
            WriteOp16(Emitter, PVM_OP(MOV32, Rd, Base));
        else 
            WriteOp16(Emitter, PVM_OP(MOV64, Rd, Base));
    }
    else if (IN_I16(Offset))
    {
        WriteOp32(Emitter, PVM_OP(LEA, Rd, Base), Offset);
    }
    else 
    {
        WriteOp16(Emitter, PVM_OP(LEAL, Rd, Base));
        Write32(Emitter, Offset);
    }
}



bool PVMEmitIntoRegLocation(PVMEmitter *Emitter, VarLocation *OutTarget, bool ReadOnly, const VarLocation *Src)
{
    PASCAL_NONNULL(Emitter);
    PASCAL_NONNULL(OutTarget);
    PASCAL_NONNULL(Src);
    PASCAL_ASSERT(OutTarget != Src, "no");

    OutTarget->Type = Src->Type;
    OutTarget->LocationType = VAR_REG;
    return PVMEmitIntoReg(Emitter, &OutTarget->As.Register, ReadOnly, Src);
}


bool PVMEmitIntoReg(PVMEmitter *Emitter, VarRegister *Reg, bool ReadOnly, const VarLocation *Src)
{
    PASCAL_NONNULL(Emitter);
    PASCAL_NONNULL(Src);
    PASCAL_NONNULL(Reg);
    if (Src->LocationType == VAR_REG 
    && (!Src->As.Register.Persistent || ReadOnly))
    {
        *Reg = Src->As.Register;
        return false;
    }
    VarRegister Tmp = PVMAllocateRegister(Emitter, Src->Type.Integral);
    MoveLocationToReg(Emitter, Tmp, Src->Type, Src);
    *Reg = Tmp;
    return true;
}











void PVMSetEntryPoint(PVMEmitter *Emitter, U32 EntryPoint)
{
    PASCAL_NONNULL(Emitter);
    PVMCurrentChunk(Emitter)->EntryPoint = EntryPoint;
}


SaveRegInfo PVMEmitterBeginScope(PVMEmitter *Emitter)
{
    PASCAL_NONNULL(Emitter);
    Emitter->StackSpace = 0;
    SaveRegInfo PrevScope = {
        .Regs = Emitter->Reglist,
    };
    return PrevScope;
}

void PVMEmitterEndScope(PVMEmitter *Emitter, SaveRegInfo PrevScope)
{
    PASCAL_NONNULL(Emitter);
    Emitter->Reglist = PrevScope.Regs;
    Emitter->StackSpace = 0;
}





void PVMEmitDebugInfo(PVMEmitter *Emitter, const U8 *Src, U32 SrcLen, U32 LineNum)
{
    PASCAL_NONNULL(Emitter);
    ChunkWriteDebugInfo(PVMCurrentChunk(Emitter), Src, SrcLen, LineNum);
}

void PVMUpdateDebugInfo(PVMEmitter *Emitter, U32 LineLen, bool IsSubroutine)
{
    PASCAL_NONNULL(Emitter);
    LineDebugInfo *Info = ChunkGetDebugInfo(PVMCurrentChunk(Emitter), UINT32_MAX);
    PASCAL_ASSERT(NULL != Info, "PVMUpdateDebugInfo: Info is NULL");
    Info->SrcLen[Info->Count - 1] = LineLen;
    Info->IsSubroutine = IsSubroutine;
}

U32 PVMGetCurrentLocation(PVMEmitter *Emitter)
{
    PASCAL_NONNULL(Emitter);
    return PVMCurrentChunk(Emitter)->Count;
}






static UInt AllocateRegister(PVMEmitter *Emitter, UInt Base, I32 *SpilledCount, 
    I32 *SpilledLocation, USize SpilledLocationSize)
{
    for (UInt i = Base; i < Base + PVM_REG_COUNT; i++)
    {
        if (PVMRegisterIsFree(Emitter, i))
        {
            PVMMarkRegisterAsAllocated(Emitter, i);
            printf("Alloc  : %d, spill: %d, list: %04x\n", i, *SpilledCount, Emitter->Reglist);
            return i;
        }
    }

    I32 Offset = sizeof(PVMGPR)*(Emitter->SpilledIntRegs + Emitter->SpilledFltRegs + 1);
    U32 Reg = *SpilledCount % PVM_REG_COUNT;

    PASCAL_ASSERT(*SpilledCount < (int)SpilledLocationSize, "dynamic register spilling table?");

    /* offset directly from SP, negative */
    SpilledLocation[*SpilledCount] = -Offset;
    (*SpilledCount) += 1;

    /* update space for spilled registers */
    Emitter->SpilledRegSpace = uMax(Offset, Emitter->SpilledRegSpace);

    /* 'push' the content of the register being spilled onto the stack */
    VarType Type = VarTypeInit(TYPE_U64, 8);
    MoveRegToMem(Emitter, 
        (VarMemory) { 
            .RegPtr = Emitter->Reg.SP.As.Register, 
            .Location = -Offset
        }, Type,
        (VarRegister) { 
            .ID = Reg 
        }, Type
    );
    printf("Alloc  : %d, spill: %d (spilling), list: %04x\n", Reg, *SpilledCount, Emitter->Reglist);
    return Reg;
}

static void FreeRegister(PVMEmitter *Emitter, I32 *SpilledCount, I32 *SpilledLocation, int Reg)
{
    /* NOTE: registers are allocated linearly, so it's fine to check the topmost register */
    if (*SpilledCount > 0 && Reg == ((*SpilledCount - 1) % PVM_REG_COUNT)) 
    {
        printf("dealloc: %d, spill: %d (unspilling), list: %04x\n", Reg, *SpilledCount, Emitter->Reglist);
        /* freeing a spilled register, get its location first */
        int Index = --(*SpilledCount);
        PASCAL_ASSERT(Index > -1, "Unreachable");

        /* load the old content of the freed register */
        MoveMemToReg(Emitter, 
            (VarRegister) { 
                .ID = Reg 
            }, (VarMemory) {   
                .RegPtr = Emitter->Reg.SP.As.Register, 
                .Location = SpilledLocation[Index]
            }, VarTypeInit(TYPE_U64, 8)
        );
        /* TODO: what if the caller intended to use the old content? */
        /* NOTE: the register is still marked as allocated because it now contains 
         * whatever content it has before being spilled */
    }

    if (Reg - PVM_REG_COUNT < *SpilledCount)
    {
        printf("dealloc: %d, spill: %d, list: %04x\n", Reg, *SpilledCount, Emitter->Reglist);
        PVMMarkRegisterAsFreed(Emitter, Reg);
    }
}

VarRegister PVMAllocateIntReg(PVMEmitter *Emitter)
{
    PASCAL_NONNULL(Emitter);
    VarRegister Reg = {
        .ID = AllocateRegister(Emitter, 0, &Emitter->SpilledIntRegs, 
            Emitter->SpilledIntRegLocation, STATIC_ARRAY_SIZE(Emitter->SpilledIntRegLocation)
        ),
        .Persistent = false,
    };
    return Reg;
}

VarRegister PVMAllocateFltReg(PVMEmitter *Emitter)
{
    PASCAL_NONNULL(Emitter);
    VarRegister Reg = { 
        .ID = AllocateRegister(Emitter, PVM_REG_COUNT, &Emitter->SpilledFltRegs, 
            Emitter->SpilledFltRegLocation, STATIC_ARRAY_SIZE(Emitter->SpilledFltRegLocation)
        ),
        .Persistent = false,
    };
    return Reg;
}


VarLocation PVMAllocateRegisterLocation(PVMEmitter *Emitter, VarType Type)
{
    PASCAL_NONNULL(Emitter);
    VarRegister Reg = PVMAllocateRegister(Emitter, Type.Integral);
    return VAR_LOCATION_REG(
        Reg.ID, Reg.Persistent,
        Type
    );
}

void PVMFreeRegister(PVMEmitter *Emitter, VarRegister Reg)
{
    PASCAL_NONNULL(Emitter);
    if (Reg.ID < PVM_REG_COUNT) /* int reg */
    {
        FreeRegister(Emitter, &Emitter->SpilledIntRegs, Emitter->SpilledIntRegLocation, Reg.ID);
    }
    else /* float reg */
    {
        FreeRegister(Emitter, &Emitter->SpilledFltRegs, Emitter->SpilledFltRegLocation, Reg.ID - PVM_REG_COUNT);
    }
}








/* branching instructions */
static U32 PVMEmitBranchOnFalseFlag(PVMEmitter *Emitter)
{
    PASCAL_NONNULL(Emitter);
    return WriteOp32(Emitter, PVM_BR_COND(F, 0), 0); 
}

static U32 PVMEmitBranchOnTrueFlag(PVMEmitter *Emitter)
{
    PASCAL_NONNULL(Emitter);
    return WriteOp32(Emitter, PVM_BR_COND(T, 0), 0); 
}


U32 PVMEmitBranchIfFalse(PVMEmitter *Emitter, const VarLocation *Condition)
{
    PASCAL_NONNULL(Emitter);
    PASCAL_NONNULL(Condition);
    if (VAR_FLAG == Condition->LocationType)
    {
        return Condition->As.FlagValueAsIs? 
            PVMEmitBranchOnFalseFlag(Emitter) 
            : PVMEmitBranchOnTrueFlag(Emitter);
    }

    VarRegister Test;
    bool IsOwning = PVMEmitIntoReg(Emitter, &Test, true, Condition);
    U32 Location = WriteOp32(Emitter, PVM_B(EZ, Test.ID, 0), 0);
    if (IsOwning)
    {
        PVMFreeRegister(Emitter, Test);
    }
    return Location;
}

U32 PVMEmitBranchIfTrue(PVMEmitter *Emitter, const VarLocation *Condition)
{
    PASCAL_NONNULL(Emitter);
    PASCAL_NONNULL(Condition);
    if (VAR_FLAG == Condition->LocationType)
    {
        return Condition->As.FlagValueAsIs? 
            PVMEmitBranchOnTrueFlag(Emitter)
            : PVMEmitBranchOnFalseFlag(Emitter);
    }

    VarRegister Test;
    bool IsOwning = PVMEmitIntoReg(Emitter, &Test, true, Condition);
    U32 Location = WriteOp32(Emitter, PVM_B(NZ, Test.ID, 0), 0);
    if (IsOwning)
    {
        PVMFreeRegister(Emitter, Test);
    }
    return Location;
}


U32 PVMEmitBranchAndInc(PVMEmitter *Emitter, VarRegister Reg, I8 Imm, U32 To)
{
    PASCAL_NONNULL(Emitter);
    I32 Offset = To - PVMCurrentChunk(Emitter)->Count - PVM_BRANCH_INS_SIZE;
    return WriteOp32(Emitter, PVM_OP(BRI, Reg.ID, Imm), Offset);
}


U32 PVMEmitBranch(PVMEmitter *Emitter, U32 To)
{
    PASCAL_NONNULL(Emitter);
    /* size of the branch instruction is 2 16 opcode word */
    U32 Offset = To - PVMCurrentChunk(Emitter)->Count - PVM_BRANCH_INS_SIZE;
    return WriteOp32(Emitter, PVM_BR(Offset & 0xFF), Offset >> 8);
}

void PVMPatchBranch(PVMEmitter *Emitter, U32 From, U32 To)
{
    PASCAL_NONNULL(Emitter);
    if (!Emitter->ShouldEmit) 
        return;

    U16 *Code = &PVMCurrentChunk(Emitter)->Code[From];
    I32 Offset = To - From;

    /*
     *  C: opcode
     *  R: register
     *  I: immediate offset
     */
    switch (PVM_GET_OP(*Code)) 
    {
    case OP_BRI:
    {
        /* i: increment immediate */
        /* 0xCCRi 0xIIII */
        Offset -= 1;
        Code[1] = Offset;
    } break;
    
    case OP_BNZ:
    case OP_BEZ:
    {
        /* 0xCCRI 0xIIII */
        Offset -= 2;
        Code[0] = (Code[0] & 0xFFF0) | (((U32)Offset) & 0xF);
        Code[1] = Offset >> 4;
    } break;

    case OP_BCF:
    case OP_BCT:
    case OP_BR:
    case OP_CALL:
    {
        /* 0xCCII 0xIIII */
        Offset -= 2;
        Code[0] = (Code[0] & 0xFF00) | (((U32)Offset) & 0xFF);
        Code[1] = Offset >> 8;
    } break;

    case OP_LDRIP:
    {
        /* 0xCCCC 0xIIII 0xIIII */
        Offset -= 3;
        Code[1] = Offset;
        Code[2] = Offset >> 16;
    } break;

    default: PASCAL_UNREACHABLE("Not a branch or IP-relative instruction."); break;
    }
}

void PVMPatchBranchToCurrent(PVMEmitter *Emitter, U32 From)
{
    PVMPatchBranch(Emitter, From, PVMCurrentChunk(Emitter)->Count);
}



/* move and load */
void PVMEmitMove(PVMEmitter *Emitter, const VarLocation *Dst, const VarLocation *Src)
{
    PASCAL_NONNULL(Emitter);
    PASCAL_NONNULL(Dst);
    PASCAL_NONNULL(Src);
    PASCAL_ASSERT(VarTypeEqual(&Dst->Type, &Src->Type), "Types must be equal for Move");
    switch (Dst->LocationType)
    {
    case VAR_REG:
    {
        MoveLocationToReg(Emitter, Dst->As.Register, Src->Type, Src);
    } break;
    case VAR_MEM:
    {
        VarRegister Tmp;
        bool Owning = PVMEmitIntoReg(Emitter, &Tmp, true, Src);
        MoveRegToMem(Emitter, Dst->As.Memory, Dst->Type, Tmp, Src->Type);
        if (Owning) 
            PVMFreeRegister(Emitter, Tmp);
    } break;
    case VAR_FLAG:
    {
        if (Src->LocationType == Dst->LocationType)
        {
            return;
        }

        PASCAL_ASSERT(Src->Type.Integral == TYPE_BOOLEAN, "Src must be boolean");
        VarRegister Tmp;
        bool Owning = PVMEmitIntoReg(Emitter, &Tmp, false, Src);
        WriteOp16(Emitter, PVM_OP(SETFLAG, Tmp.ID, 0));
        if (Owning)
            PVMFreeRegister(Emitter, Tmp);
    } break;
    case VAR_LIT:
    case VAR_INVALID:
    case VAR_BUILTIN:
    case VAR_SUBROUTINE:
    {
    } break;
    }
}

void PVMEmitCopy(PVMEmitter *Emitter, const VarLocation *Dst, const VarLocation *Src)
{
    PASCAL_NONNULL(Emitter);
    PASCAL_NONNULL(Dst);
    PASCAL_NONNULL(Src);

    PASCAL_ASSERT(Dst->Type.Integral == Src->Type.Integral, "Unreachable");
    PASCAL_ASSERT(Dst->Type.Size == Src->Type.Size, "Unreachable");
    PASCAL_ASSERT(!VarTypeIsTriviallyCopiable(Dst->Type), "Unhandled case: "STRVIEW_FMT, 
            STRVIEW_FMT_ARG(VarTypeToStringView(Dst->Type))
    );
    PASCAL_ASSERT(Dst->Type.Size <= UINT32_MAX, "record too big");

    VarRegister DstPtr, SrcPtr;
    bool OwningDstPtr = PVMEmitIntoReg(Emitter, &DstPtr, true, Dst); /* the addr itself is readonly */
    bool OwningSrcPtr = PVMEmitIntoReg(Emitter, &SrcPtr, true, Src);

    if (DstPtr.ID != SrcPtr.ID)
    {
        WriteOp16(Emitter, PVM_OP(MEMCPY, DstPtr.ID, SrcPtr.ID));
        Write32(Emitter, Dst->Type.Size);
    }

    if (OwningDstPtr)
    {
        PVMFreeRegister(Emitter, DstPtr);
    }
    if (OwningSrcPtr)
    {
        PVMFreeRegister(Emitter, SrcPtr);
    }
}



U32 PVMEmitLoadSubroutineAddr(PVMEmitter *Emitter, VarRegister Dst, U32 SubroutineAddr)
{
    PASCAL_NONNULL(Emitter);
    const PVMImmType ImmType = IMMTYPE_I32;
    I32 Offset = SubroutineAddr - (PVMGetCurrentLocation(Emitter) + 3);
    U32 Location = WriteOp16(Emitter, PVM_IMM_OP(LDRIP, Dst.ID, ImmType));
    Write32(Emitter, Offset);
    return Location;
}

void PVMEmitLoadEffectiveAddr(PVMEmitter *Emitter, VarRegister Dst, VarMemory Src, I32 Offset)
{
    PASCAL_NONNULL(Emitter);

    Src.Location += Offset;
    PVMEmitLoadAddr(Emitter, Dst, Src);
}

VarLocation PVMEmitLoadArrayElement(PVMEmitter *Emitter, const VarLocation *Array, const VarLocation *Index)
{
    PASCAL_NONNULL(Emitter);
    PASCAL_NONNULL(Array);
    PASCAL_NONNULL(Index);
    const VarType *ElementType = Array->Type.As.StaticArray.ElementType;
    PASCAL_NONNULL(ElementType);
    PASCAL_ASSERT(Array->LocationType == VAR_MEM, "Array must be in memory");

    /* copy the unscaled index */
    VarRegister ScaledIndex;
    PVMEmitIntoReg(Emitter, &ScaledIndex, false, Index);

    /* scale it */
    PVMEmitIMulConst(Emitter, ScaledIndex, TYPE_I32, ElementType->Size);

    /* Add the base register from array */
    UInt ArrayBasePtr = Array->As.Memory.RegPtr.ID;
    U16 AddOpcode = (UINTPTR_MAX == UINT16_MAX)
        ? PVM_OP(ADD64, ScaledIndex.ID, ArrayBasePtr)
        : PVM_OP(ADD, ScaledIndex.ID, ArrayBasePtr);
    WriteOp16(Emitter, AddOpcode);

    /* calculate effective addr of the element, treat it as a memory location */
    return VAR_LOCATION_MEM(
        .RegPtr = ScaledIndex,
        Array->As.Memory.Location - Array->Type.As.StaticArray.Range.Low*ElementType->Size,
        *ElementType
    );
}


/* type conversion */

void PVMEmitIntegerTypeConversion(PVMEmitter *Emitter, 
        VarRegister Dst, IntegralType DstType, 
        VarRegister Src, IntegralType SrcType
)
{
    PASCAL_NONNULL(Emitter);
    if (Dst.ID == Src.ID && DstType == SrcType)
        return;

    switch (DstType)
    {
    case TYPE_U8:
    case TYPE_U16:
    case TYPE_U32:
    {
        switch (SrcType)
        {
        case TYPE_I8:
        case TYPE_U8: WriteOp16(Emitter, PVM_OP(MOVZEX32_8, Dst.ID, Src.ID)); break;
        case TYPE_I16:
        case TYPE_U16: WriteOp16(Emitter, PVM_OP(MOVZEX32_16, Dst.ID, Src.ID)); break;
        case TYPE_I64:
        case TYPE_U64:
        case TYPE_I32:
        case TYPE_U32: if (Dst.ID != Src.ID) WriteOp16(Emitter, PVM_OP(MOV32, Dst.ID, Src.ID)); break;
        default: 
        {
            PASCAL_UNREACHABLE("Invalid src type in %s: %s", __func__, IntegralTypeToStr(SrcType));
        } break;
        }
    } break;
    case TYPE_U64:
    {
        switch (SrcType)
        {
        case TYPE_I8:
        case TYPE_U8: WriteOp16(Emitter, PVM_OP(MOVZEX64_8, Dst.ID, Src.ID)); break;
        case TYPE_I16:
        case TYPE_U16: WriteOp16(Emitter, PVM_OP(MOVZEX64_16, Dst.ID, Src.ID)); break;
        case TYPE_I32:
        case TYPE_U32: WriteOp16(Emitter, PVM_OP(MOVZEX64_32, Dst.ID, Src.ID)); break;
        case TYPE_I64:
        case TYPE_U64: if (Dst.ID != Src.ID) WriteOp16(Emitter, PVM_OP(MOV64, Dst.ID, Src.ID)); break;
        default: 
        {
            PASCAL_UNREACHABLE("Invalid src type in %s: %s", __func__, IntegralTypeToStr(SrcType));
        } break;
        }
    } break;
    case TYPE_I8:
    case TYPE_I16:
    case TYPE_I32:
    {
        switch (SrcType)
        {
        case TYPE_U8:  WriteOp16(Emitter, PVM_OP(MOVZEX32_8, Dst.ID, Src.ID)); break;
        case TYPE_U16: WriteOp16(Emitter, PVM_OP(MOVZEX32_16, Dst.ID, Src.ID)); break;
        case TYPE_U32:
        case TYPE_I8:
        case TYPE_I16:
        case TYPE_I32:
        case TYPE_I64:
        case TYPE_U64: if (Dst.ID != Src.ID) WriteOp16(Emitter, PVM_OP(MOV32, Dst.ID, Src.ID)); break;
        default: 
        {
            PASCAL_UNREACHABLE("Invalid src type in %s: %s", __func__, IntegralTypeToStr(SrcType));
        } break;
        }
    } break;
    case TYPE_I64:
    {
        switch (SrcType)
        {
        case TYPE_I8:
        case TYPE_I16:
        case TYPE_I32: WriteOp16(Emitter, PVM_OP(MOVSEX64_32, Dst.ID, Src.ID)); break;
        case TYPE_U8:  WriteOp16(Emitter, PVM_OP(MOVZEX64_8, Dst.ID, Src.ID)); break;
        case TYPE_U16: WriteOp16(Emitter, PVM_OP(MOVZEX64_16, Dst.ID, Src.ID)); break;
        case TYPE_U32: WriteOp16(Emitter, PVM_OP(MOVZEX64_32, Dst.ID, Src.ID)); break;
        case TYPE_I64:
        case TYPE_U64: if (Dst.ID != Src.ID) WriteOp16(Emitter, PVM_OP(MOV64, Dst.ID, Src.ID)); break;
        default: 
        {
            PASCAL_UNREACHABLE("Invalid src type in %s: %s", __func__, IntegralTypeToStr(SrcType));
        } break;
        }
    } break;
    default: 
    {
        PASCAL_UNREACHABLE("invalid dst type in %s: %s", __func__, IntegralTypeToStr(DstType));
    } break;
    }
}

void PVMEmitFloatTypeConversion(PVMEmitter *Emitter,
        VarRegister Dst, IntegralType DstType, 
        VarRegister Src, IntegralType SrcType
)
{
    PASCAL_NONNULL(Emitter);
    if (Dst.ID == Src.ID && SrcType == DstType)
        return;

    if (TYPE_F32 == DstType)
    {
        if (TYPE_F64 == SrcType)
        {
            WriteOp16(Emitter, PVM_OP(F64TOF32, Dst.ID, Src.ID));
        }
        else 
        {
            WriteOp16(Emitter, PVM_OP(FMOV, Dst.ID, Src.ID));
        }
        return;
    }

    if (TYPE_F64 == DstType)
    {
        if (TYPE_F64 == SrcType)
        {
            WriteOp16(Emitter, PVM_OP(FMOV64, Dst.ID, Src.ID));
        }
        else 
        {
            WriteOp16(Emitter, PVM_OP(F32TOF64, Dst.ID, Src.ID));
        }
        return;
    }

    PASCAL_UNREACHABLE("Invalid type in %s: dst=%s and src=%s", 
            __func__, IntegralTypeToStr(DstType), IntegralTypeToStr(SrcType)
    );
}

void PVMEmitIntToFltTypeConversion(PVMEmitter *Emitter,
        VarRegister Dst, IntegralType DstType, 
        VarRegister Src, IntegralType SrcType
)
{
    PASCAL_NONNULL(Emitter);
    if (TYPE_F64 == DstType)
    {
        switch (SrcType)
        {
        case TYPE_I8:
        case TYPE_I16:
        case TYPE_I32:
        {
            WriteOp16(Emitter, PVM_OP(I32TOF64, Dst.ID, Src.ID));
        } break;
        case TYPE_I64:
        {
            WriteOp16(Emitter, PVM_OP(I64TOF64, Dst.ID, Src.ID));
        } break;
        case TYPE_U8:
        case TYPE_U16:
        case TYPE_U32:
        {
            WriteOp16(Emitter, PVM_OP(U32TOF64, Dst.ID, Src.ID));
        } break;
        case TYPE_U64:
        {
            WriteOp16(Emitter, PVM_OP(U64TOF64, Dst.ID, Src.ID));
        } break;
        default: goto Unreachable;
        }
        return;
    }
    if (TYPE_F32 == DstType)
    {
        switch (SrcType)
        {
        case TYPE_I8:
        case TYPE_I16:
        case TYPE_I32:
        {
            WriteOp16(Emitter, PVM_OP(I32TOF32, Dst.ID, Src.ID));
        } break;
        case TYPE_I64:
        {
            WriteOp16(Emitter, PVM_OP(I64TOF32, Dst.ID, Src.ID));
        } break;
        case TYPE_U8:
        case TYPE_U16:
        case TYPE_U32:
        {
            WriteOp16(Emitter, PVM_OP(U32TOF32, Dst.ID, Src.ID));
        } break;
        case TYPE_U64:
        {
            WriteOp16(Emitter, PVM_OP(U64TOF32, Dst.ID, Src.ID));
        } break;
        default: goto Unreachable;
        }
        return;
    }
Unreachable:
    PASCAL_UNREACHABLE("Invalid type in %s: dst=%s and src=%s", 
            __func__, IntegralTypeToStr(DstType), IntegralTypeToStr(SrcType)
    );
}






/* arith instructions */
static void PVMEmitIMulConst(PVMEmitter *Emitter, VarRegister Dst, IntegralType RegisterType, I64 Const)
{
    bool OperandIs64 = 
        RegisterType == TYPE_I64 
        || RegisterType == TYPE_U64;

    UInt DstReg = Dst.ID;
    if (Const == -1)
    {
        OP32_OR_OP64(Emitter, NEG, OperandIs64, DstReg, DstReg);
    }
    else if (Const == 0)
    {
        PVMEmitMoveImm(Emitter, Dst, 0);
    }
    else if (Const == 1)
    {
        return;
    }
    else if (Const > 1 && IS_POW2(Const))
    {
        unsigned ShiftAmount = BitCount(Const - 1);
        if (ShiftAmount > 16) /* poor design of the PVM */
        {
            VarRegister Tmp = PVMAllocateRegister(Emitter, TYPE_U8);
            OP32_OR_OP64(Emitter, VSHL, OperandIs64, DstReg, Tmp.ID);
            PVMFreeRegister(Emitter, Tmp);
        }
        else
        {
            OP32_OR_OP64(Emiter, QSHL, OperandIs64, DstReg, ShiftAmount);
        }
    }
    else 
    {
        VarRegister Tmp = PVMAllocateRegister(Emitter, RegisterType);
        if (IntegralTypeIsSigned(RegisterType))
        {
            OP32_OR_OP64(Emitter, IMUL, OperandIs64, DstReg, Tmp.ID);
        }
        else
        {
            OP32_OR_OP64(Emitter, MUL, OperandIs64, DstReg, Tmp.ID);
        }
        PVMFreeRegister(Emitter, Tmp);
    }
}



void PVMEmitAddImm(PVMEmitter *Emitter, VarRegister Dst, IntegralType DstType, I64 Imm)
{
    PASCAL_NONNULL(Emitter);
    if (0 == Imm)
        return;

    bool Oper64 = DstType == TYPE_U64 || DstType == TYPE_I64;
    bool Signed = IntegralTypeIsSigned(DstType);
    if (Signed)
    {
        if (IS_SMALL_IMM(Imm))
        {
            OP32_OR_OP64(Emitter, ADDI, Oper64, Dst.ID, Imm);
        }
        else if (IN_I16(Imm))
        {
            OP32_OR_OP64(Emitter, ADDI, Oper64, Dst.ID, IMMTYPE_I16);
            WriteOp16(Emitter, (U16)Imm);
        }
        else if (IN_I32(Imm))
        {
            OP32_OR_OP64(Emitter, ADDI, Oper64, Dst.ID, IMMTYPE_I32);
            Write32(Emitter, (U32)Imm);
        }
        else if (IN_I48(Imm))
        {
            OP32_OR_OP64(Emitter, ADDI, Oper64, Dst.ID, IMMTYPE_I48);
            Write32(Emitter, (U32)Imm);
            WriteOp16(Emitter, (U64)Imm >> 32);
        }
        else 
        {
            OP32_OR_OP64(Emitter, ADDI, Oper64, Dst.ID, IMMTYPE_U64);
            Write32(Emitter, (U32)Imm);
            Write32(Emitter, (U64)Imm >> 32);
        }
    }
    else
    {
        if (IN_U16(Imm))
        {
            OP32_OR_OP64(Emitter, ADDI, Oper64, Dst.ID, IMMTYPE_U16);
            WriteOp16(Emitter, (U16)Imm);
        }
        else if (IN_U32(Imm))
        {
            OP32_OR_OP64(Emitter, ADDI, Oper64, Dst.ID, IMMTYPE_U32);
            Write32(Emitter, (U32)Imm);
        }
        else if (IN_U48(Imm))
        {
            OP32_OR_OP64(Emitter, ADDI, Oper64, Dst.ID, IMMTYPE_U48);
            Write32(Emitter, (U32)Imm);
            WriteOp16(Emitter, (U64)Imm >> 32);
        }
        else 
        {
            OP32_OR_OP64(Emitter, ADDI, Oper64, Dst.ID, IMMTYPE_U64);
            Write32(Emitter, (U32)Imm);
            Write32(Emitter, (U64)Imm >> 32);
        }
    }
}





#define DEFINE_INTEGER_BINARY_OP(FnName, Mnemonic)\
void FnName (PVMEmitter *Emitter, VarRegister Dst, const VarLocation *Src) {\
    PASCAL_NONNULL(Emitter);\
    PASCAL_NONNULL(Src);\
    IntegralType Type = Src->Type.Integral;\
    VarRegister Rs;\
    bool OwningRs = PVMEmitIntoReg(Emitter, &Rs, true, Src);\
    OP32_OR_OP64(Emitter, Mnemonic, TYPE_I64 == Type || TYPE_U64 == Type, Dst.ID, Rs.ID);\
    if (OwningRs) {\
        PVMFreeRegister(Emitter, Rs);\
    }\
}\
void FnName (PVMEmitter *Emitter, VarRegister Dst, const VarLocation *Src)


void PVMEmitAdd(PVMEmitter *Emitter, VarRegister Dst, const VarLocation *Src)
{
    PASCAL_NONNULL(Emitter);
    PASCAL_NONNULL(Src);
    IntegralType SrcType = Src->Type.Integral;
    if (VAR_LIT == Src->LocationType && IntegralTypeIsOrdinal(SrcType))
    {
        PVMEmitAddImm(Emitter, Dst, SrcType,
            -OrdinalLiteralToI64(Src->As.Literal, SrcType)
        );
        return;
    }

    VarRegister SrcReg;
    bool OwningSrc = PVMEmitIntoReg(Emitter, &SrcReg, true, Src);
    bool Oper64 = OperandIs64(SrcType);
    if (IntegralTypeIsOrdinal(SrcType))
    {
        OP32_OR_OP64(Emitter, ADD, Oper64, Dst.ID, SrcReg.ID);
    }
    else if (IntegralTypeIsFloat(SrcType))
    {
        OP32_OR_OP64(Emitter, FADD, Oper64, Dst.ID, SrcReg.ID);
    }
    else if (TYPE_STRING == SrcType)
    {
        WriteOp16(Emitter, PVM_OP(SADD, Dst.ID, SrcReg.ID));
    }
    else
    {
        PASCAL_UNREACHABLE("Invalid type");
    }

    if (OwningSrc)
    {
        PVMFreeRegister(Emitter, SrcReg);
    }
}

void PVMEmitSub(PVMEmitter *Emitter, VarRegister Dst, const VarLocation *Src)
{
    PASCAL_NONNULL(Emitter);
    PASCAL_NONNULL(Src);
    IntegralType SrcType = Src->Type.Integral;
    if (VAR_LIT == Src->LocationType && IntegralTypeIsOrdinal(SrcType))
    {
        PVMEmitAddImm(Emitter, Dst, SrcType,
            -OrdinalLiteralToI64(Src->As.Literal, SrcType)
        );
        return;
    }

    VarRegister SrcReg;
    bool OwningSrc = PVMEmitIntoReg(Emitter, &SrcReg, true, Src);
    bool Oper64 = OperandIs64(SrcType);
    if (IntegralTypeIsOrdinal(SrcType))
    {
        OP32_OR_OP64(Emitter, SUB, Oper64, Dst.ID, SrcReg.ID);
    }
    else if (IntegralTypeIsFloat(SrcType))
    {
        OP32_OR_OP64(Emitter, FSUB, Oper64, Dst.ID, SrcReg.ID);
    }
    else
    {
        PASCAL_UNREACHABLE("Invalid type");
    }

    if (OwningSrc)
    {
        PVMFreeRegister(Emitter, SrcReg);
    }
}


void PVMEmitMul(PVMEmitter *Emitter, VarRegister Dst, const VarLocation *Src)
{
    PASCAL_NONNULL(Emitter);
    PASCAL_NONNULL(Src);

    IntegralType Type = Src->Type.Integral;
    if (IntegralTypeIsOrdinal(Type) && VAR_LIT == Src->LocationType)
    {
        PVMEmitIMulConst(Emitter, Dst, Type, 
            OrdinalLiteralToI64(Src->As.Literal, Src->Type.Integral)
        );
        return;
    }

    VarRegister Tmp;
    bool OwningTmp = PVMEmitIntoReg(Emitter, &Tmp, true, Src);
    if (IntegralTypeIsOrdinal(Type))
    {
        if (IntegralTypeIsSigned(Type))
            OP32_OR_OP64(Emitter, IMUL, Type == TYPE_I64, Dst.ID, Tmp.ID);
        else OP32_OR_OP64(Emitter, MUL, Type == TYPE_U64, Dst.ID, Tmp.ID);
    }
    if (IntegralTypeIsFloat(Type))
    {
        OP32_OR_OP64(Emitter, FMUL, Type == TYPE_F64, Dst.ID, Tmp.ID);
    }

    if (OwningTmp)
        PVMFreeRegister(Emitter, Tmp);
}

void PVMEmitDiv(PVMEmitter *Emitter, VarRegister Dst, const VarLocation *Src)
{
    PASCAL_NONNULL(Emitter);
    PASCAL_NONNULL(Src);
    IntegralType Type = Src->Type.Integral;
    bool Signed = IntegralTypeIsSigned(Type);
    bool Oper64 = OperandIs64(Type);
    if (IntegralTypeIsOrdinal(Type) && VAR_LIT == Src->LocationType)
    {
        I64 Imm = OrdinalLiteralToI64(Src->As.Literal, Type);
        if (1 == Imm)
            return;

        unsigned ShiftAmount = BitCount(Imm - 1);
        if (Signed)
        {
            if (-1 == Imm)
            {
                MoveLocationToReg(Emitter, Dst, Src->Type, Src);
                PVMEmitNeg(Emitter, Dst, Dst, Type);
                return;
            }
            if (Imm > 0 && IS_POW2(Imm) && ShiftAmount < 16) /* shift */
            {
                OP32_OR_OP64(Emitter, QASR, Oper64, Dst.ID, ShiftAmount);
                return;
            }
            /* not worth it to move a literal into a reg and shift */
            /* fall back to regular div */
        }
        else
        {
            if (IS_POW2(Imm) && ShiftAmount < 16)
            {
                OP32_OR_OP64(Emitter, QSHR, Oper64, Dst.ID, ShiftAmount);
                return;
            }
            /* fall back to regular div */
        }
    }

    VarRegister Tmp;
    bool OwningTmp = PVMEmitIntoReg(Emitter, &Tmp, true, Src);

    if (IntegralTypeIsOrdinal(Type))
    {
        if (Signed)
            OP32_OR_OP64(Emitter, IDIV, Oper64, Dst.ID, Tmp.ID);
        else OP32_OR_OP64(Emitter, DIV, Oper64, Dst.ID, Tmp.ID);
    }
    else if (IntegralTypeIsFloat(Type))
    {
        OP32_OR_OP64(Emitter, FDIV, Oper64, Dst.ID, Tmp.ID);
    }
    else
    {
        PASCAL_UNREACHABLE("Invalid type");
    }

    if (OwningTmp)
        PVMFreeRegister(Emitter, Tmp);
}

void PVMEmitNot(PVMEmitter *Emitter, VarRegister Dst, VarRegister Src, IntegralType Type)
{
    PASCAL_NONNULL(Emitter);

    bool Oper64 = OperandIs64(Type);
    if (IntegralTypeIsInteger(Type))
        OP32_OR_OP64(Emitter, NOT, Oper64, Dst.ID, Src.ID);
    else if (TYPE_BOOLEAN == Type)
        WriteOp16(Emitter, PVM_OP(SETEZ, Dst.ID, Src.ID));
    else PASCAL_UNREACHABLE("Invalid type");
}

void PVMEmitNeg(PVMEmitter *Emitter, VarRegister Dst, VarRegister Src, IntegralType Type)
{
    PASCAL_NONNULL(Emitter);
    bool Oper64 = OperandIs64(Type);
    if (IntegralTypeIsFloat(Type))
        OP32_OR_OP64(Emitter, FNEG, Oper64, Dst.ID, Src.ID);
    else OP32_OR_OP64(Emitter, NEG, Oper64, Dst.ID, Src.ID);
}

void PVMEmitShl(PVMEmitter *Emitter, VarRegister Dst, const VarLocation *Src)
{
    IntegralType Type = Src->Type.Integral;
    bool Oper64 = OperandIs64(Type);
    PASCAL_NONNULL(Emitter);
    PASCAL_NONNULL(Src);
    PASCAL_ASSERT(IntegralTypeIsOrdinal(Type), "Invalid type");

    if (VAR_LIT == Src->LocationType)
    {
        unsigned ShiftAmount = OrdinalLiteralToI64(Src->As.Literal, Type);
        if (ShiftAmount < 16)
        {
            OP32_OR_OP64(Emitter, QSHL, Oper64, Dst.ID, ShiftAmount);
            return;
        }
        /* fallback to emitting to reg */
    }

    VarRegister Tmp;
    bool Owning = PVMEmitIntoReg(Emitter, &Tmp, true, Src);
    OP32_OR_OP64(Emitter, VSHL, Oper64, Dst.ID, Tmp.ID);
    if (Owning)
        PVMFreeRegister(Emitter, Tmp);
}

void PVMEmitAsr(PVMEmitter *Emitter, VarRegister Dst, const VarLocation *Src)
{
    IntegralType Type = Src->Type.Integral;
    bool Oper64 = OperandIs64(Type);
    PASCAL_NONNULL(Emitter);
    PASCAL_NONNULL(Src);
    PASCAL_ASSERT(IntegralTypeIsOrdinal(Type), "Invalid type");

    if (VAR_LIT == Src->LocationType)
    {
        unsigned ShiftAmount = OrdinalLiteralToI64(Src->As.Literal, Type);
        if (ShiftAmount < 16)
        {
            OP32_OR_OP64(Emitter, QASR, Oper64, Dst.ID, ShiftAmount);
            return;
        }
        /* fallback to emitting to reg */
    }

    VarRegister Tmp;
    bool Owning = PVMEmitIntoReg(Emitter, &Tmp, true, Src);
    OP32_OR_OP64(Emitter, VASR, Oper64, Dst.ID, Tmp.ID);
    if (Owning)
        PVMFreeRegister(Emitter, Tmp);
}

void PVMEmitShr(PVMEmitter *Emitter, VarRegister Dst, const VarLocation *Src)
{
    IntegralType Type = Src->Type.Integral;
    bool Oper64 = OperandIs64(Type);
    PASCAL_NONNULL(Emitter);
    PASCAL_NONNULL(Src);
    PASCAL_ASSERT(IntegralTypeIsOrdinal(Type), "Invalid type");

    if (VAR_LIT == Src->LocationType)
    {
        unsigned ShiftAmount = OrdinalLiteralToI64(Src->As.Literal, Type);
        if (ShiftAmount < 16)
        {
            OP32_OR_OP64(Emitter, QSHR, Oper64, Dst.ID, ShiftAmount);
            return;
        }
        /* fallback to emitting to reg */
    }

    VarRegister Tmp;
    bool Owning = PVMEmitIntoReg(Emitter, &Tmp, true, Src);
    OP32_OR_OP64(Emitter, VSHR, Oper64, Dst.ID, Tmp.ID);
    if (Owning)
        PVMFreeRegister(Emitter, Tmp);
}




DEFINE_INTEGER_BINARY_OP(PVMEmitAnd, AND);
DEFINE_INTEGER_BINARY_OP(PVMEmitOr, OR);
DEFINE_INTEGER_BINARY_OP(PVMEmitXor, XOR);
DEFINE_INTEGER_BINARY_OP(PVMEmitMod, MOD);


#undef DEFINE_CONST_OP
#undef DEFINE_INTEGER_BINARY_OP



VarLocation PVMEmitSetIfLessOrEqual(PVMEmitter *Emitter, 
        VarRegister A, VarRegister B, IntegralType CommonType
)
{
    PASCAL_NONNULL(Emitter);
    bool Oper64 = OperandIs64(CommonType);
    U16 Opcode = 0;
    VarLocation Flag = Emitter->Reg.Flag;
    if (IntegralTypeIsFloat(CommonType))
    {
        Opcode = PVM_OP_ALT(FSLE, Oper64, A.ID, B.ID);
    }
    else if (IntegralTypeIsOrdinal(CommonType))
    {
        Opcode = IntegralTypeIsSigned(CommonType)
            ? PVM_OP_ALT(SLT, Oper64, B.ID, A.ID) 
            : PVM_OP_ALT(ISLT, Oper64, B.ID, A.ID);
        Flag.As.FlagValueAsIs = false;
    }
    else if (TYPE_STRING == CommonType)
    {
        Opcode = PVM_OP(STRLT, B.ID, A.ID);
        Flag.As.FlagValueAsIs = false;
    }
    WriteOp16(Emitter, Opcode);
    return Flag;
}

VarLocation PVMEmitSetIfGreaterOrEqual(PVMEmitter *Emitter, 
        VarRegister A, VarRegister B, IntegralType CommonType
)
{
    PASCAL_NONNULL(Emitter);
    bool Oper64 = OperandIs64(CommonType);
    U16 Opcode = 0;
    VarLocation Flag = Emitter->Reg.Flag;
    if (IntegralTypeIsFloat(CommonType))
    {
        Opcode = PVM_OP_ALT(FSGE, Oper64, A.ID, B.ID);
    }
    else if (IntegralTypeIsOrdinal(CommonType))
    {
        Opcode = IntegralTypeIsSigned(CommonType)
            ? PVM_OP_ALT(SLT, Oper64, A.ID, B.ID) 
            : PVM_OP_ALT(ISLT, Oper64, A.ID, B.ID);
        Flag.As.FlagValueAsIs = false;
    }
    else if (TYPE_STRING == CommonType)
    {
        Opcode = PVM_OP(STRLT, A.ID, B.ID);
        Flag.As.FlagValueAsIs = false;
    }
    WriteOp16(Emitter, Opcode);
    return Flag;
}

VarLocation PVMEmitSetIfLess(PVMEmitter *Emitter, 
        VarRegister A, VarRegister B, IntegralType CommonType
)
{
    PASCAL_NONNULL(Emitter);
    bool Oper64 = OperandIs64(CommonType);
    U16 Opcode = 0;
    VarLocation Flag = Emitter->Reg.Flag;
    if (IntegralTypeIsFloat(CommonType))
    {
        Opcode = PVM_OP_ALT(FSLT, Oper64, A.ID, B.ID);
    }
    else if (IntegralTypeIsOrdinal(CommonType))
    {
        Opcode = IntegralTypeIsSigned(CommonType)
            ? PVM_OP_ALT(SLT, Oper64, A.ID, B.ID) 
            : PVM_OP_ALT(ISLT, Oper64, A.ID, B.ID);
    }
    else if (TYPE_STRING == CommonType)
    {
        Opcode = PVM_OP(STRLT, A.ID, B.ID);
    }
    WriteOp16(Emitter, Opcode);
    return Flag;
}

VarLocation PVMEmitSetIfGreater(PVMEmitter *Emitter, 
        VarRegister A, VarRegister B, IntegralType CommonType
)
{
    PASCAL_NONNULL(Emitter);
    bool Oper64 = OperandIs64(CommonType);
    U16 Opcode = 0;
    VarLocation Flag = Emitter->Reg.Flag;
    if (IntegralTypeIsFloat(CommonType))
    {
        Opcode = PVM_OP_ALT(FSLT, Oper64, A.ID, B.ID);
    }
    else if (IntegralTypeIsOrdinal(CommonType))
    {
        Opcode = IntegralTypeIsSigned(CommonType)
            ? PVM_OP_ALT(SLT, Oper64, B.ID, A.ID) 
            : PVM_OP_ALT(ISLT, Oper64, B.ID, A.ID);
    }
    else if (TYPE_STRING == CommonType)
    {
        Opcode = PVM_OP(STRLT, B.ID, A.ID);
    }
    WriteOp16(Emitter, Opcode);
    return Flag;
}

VarLocation PVMEmitSetIfEqual(PVMEmitter *Emitter, 
        VarRegister A, VarRegister B, IntegralType CommonType
)
{
    PASCAL_NONNULL(Emitter);
    bool Oper64 = OperandIs64(CommonType);
    U16 Opcode = 0;
    VarLocation Flag = Emitter->Reg.Flag;
    if (IntegralTypeIsFloat(CommonType))
    {
        Opcode = PVM_OP_ALT(FSEQ, Oper64, A.ID, B.ID);
    }
    else if (IntegralTypeIsOrdinal(CommonType))
    {
        Opcode = PVM_OP_ALT(SEQ, Oper64, A.ID, B.ID);
    }
    else if (TYPE_STRING == CommonType)
    {
        Opcode = PVM_OP(STREQ, A.ID, B.ID);
    }
    WriteOp16(Emitter, Opcode);
    return Flag;
}

VarLocation PVMEmitSetIfNotEqual(PVMEmitter *Emitter, 
        VarRegister A, VarRegister B, IntegralType CommonType
)
{
    PASCAL_NONNULL(Emitter);
    bool Oper64 = OperandIs64(CommonType);
    U16 Opcode = 0;
    VarLocation Flag = Emitter->Reg.Flag;
    if (IntegralTypeIsFloat(CommonType))
    {
        Opcode = PVM_OP_ALT(FSNE, Oper64, A.ID, B.ID);
    }
    else if (IntegralTypeIsOrdinal(CommonType))
    {
        Opcode = PVM_OP_ALT(SEQ, Oper64, A.ID, B.ID);
        Flag.As.FlagValueAsIs = false;
    }
    else if (TYPE_STRING == CommonType)
    {
        Opcode = PVM_OP(STREQ, A.ID, B.ID);
        Flag.As.FlagValueAsIs = false;
    }
    WriteOp16(Emitter, Opcode);
    return Flag;
}


VarLocation PVMEmitMemcmp(PVMEmitter *Emitter, VarRegister PtrA, VarRegister PtrB, VarRegister Size)
{
    PASCAL_NONNULL(Emitter);
    WriteOp32(Emitter, PVM_OP(VMEMEQU, PtrA.ID, PtrB.ID), Size.ID << 4);
    return Emitter->Reg.Flag;
}









I32 PVMStartArg(PVMEmitter *Emitter, U32 ArgSize)
{
    PASCAL_NONNULL(Emitter);
    PVMEmitStackAllocation(Emitter, ArgSize);
    return PVM_STACK_ALIGNMENT;
}


VarLocation PVMSetArg(PVMEmitter *Emitter, UInt ArgNumber, VarType ArgType, I32 *Base)
{
    PASCAL_NONNULL(Emitter);
    PASCAL_NONNULL(Base);

    /* arguments in register */
    if (ArgNumber < PVM_ARGREG_COUNT)
    {
        VarLocation ArgReg = VAR_LOCATION_REG(
                ArgNumber, false, ArgType
        );
        if (IntegralTypeIsFloat(ArgType.Integral))
        {
            ArgReg.As.Register.ID += PVM_ARGREG_F0;
        }
        return ArgReg;
    }

    /* arguments on stack */
    *Base -= ArgType.Size;
    VarLocation Memory = VAR_LOCATION_MEM(
            .RegPtr = Emitter->Reg.SP.As.Register, 
            *Base, ArgType
    );
    return Memory;
}


void PVMMarkArgAsOccupied(PVMEmitter *Emitter, const VarLocation *Arg)
{
    PASCAL_NONNULL(Emitter);
    PASCAL_NONNULL(Arg);
    if (VAR_REG == Arg->LocationType)
    {
        PVMMarkRegisterAsAllocated(Emitter, Arg->As.Register.ID);
    }
    else if (VAR_MEM == Arg->LocationType)
    {
        PVMMarkRegisterAsAllocated(Emitter, Arg->As.Memory.RegPtr.ID);
    }
}


VarLocation PVMSetReturnType(PVMEmitter *Emitter, VarType Type)
{
    PASCAL_NONNULL(Emitter);
    VarLocation ReturnRegister = VAR_LOCATION_REG(
            PVM_RETREG, false, Type
    );
    if (!VarTypeIsTriviallyCopiable(Type))
    {
        ReturnRegister = VAR_LOCATION_MEM(
                /* C is horrible */
                .RegPtr = ((VarRegister){ PVM_RETREG, false }), 
                0, Type
        );
    }
    else if (IntegralTypeIsFloat(Type.Integral))
    {
        ReturnRegister.As.Register.ID = PVM_ARGREG_F0;
    }

    return ReturnRegister;
}





/* stack allocation */
void PVMEmitStackAllocation(PVMEmitter *Emitter, I32 Size) 
{
    PASCAL_NONNULL(Emitter);
    I64 Aligned = iRoundUpToMultipleOfPow2(Size, sizeof(PVMGPR));
    PVMEmitAddImm(Emitter, Emitter->Reg.SP.As.Register, TYPE_POINTER, Aligned);
    Emitter->StackSpace += Aligned;
}

void PVMEmitPush(PVMEmitter *Emitter, const VarLocation *Src)
{
    VarRegister Reg;
    bool Owning = PVMEmitIntoReg(Emitter, &Reg, true, Src);
    PVMEmitPushReg(Emitter, Reg.ID);
    if (Owning)
        PVMFreeRegister(Emitter, Reg);
}

void PVMEmitPop(PVMEmitter *Emitter, const VarLocation *Dst)
{
    if (Dst->LocationType == VAR_REG)
    {
        PVMEmitPopReg(Emitter, Dst->As.Register.ID);
    }
    else
    {
        VarLocation TmpReg;
        PVMEmitIntoRegLocation(Emitter, &TmpReg, true, Dst); 
        PVMEmitPopReg(Emitter, TmpReg.As.Register.ID);
        PVMEmitMove(Emitter, Dst, &TmpReg);
        PVMFreeRegister(Emitter, TmpReg.As.Register);
    }
}


VarLocation PVMCreateStackLocation(PVMEmitter *Emitter, VarType Type, int FpOffset)
{
    VarLocation Location = VAR_LOCATION_MEM(
        Emitter->Reg.FP.As.Register, 
        (STACK_TOP == FpOffset)?
            (int)Emitter->StackSpace : FpOffset,
        Type
    );
    return Location;
}






/* global instructions */
U32 PVMGetGlobalOffset(PVMEmitter *Emitter)
{
    PASCAL_NONNULL(Emitter);
    return PVMCurrentChunk(Emitter)->Global.Count;
}

void PVMInitializeGlobal(PVMEmitter *Emitter, VarMemory Global, const VarLiteral *Data, VarType Type)
{
    PASCAL_NONNULL(Emitter);
    PASCAL_NONNULL(Data);

    U32 Location = Global.Location;
    PVMChunk *Chunk = PVMCurrentChunk(Emitter);
    PASCAL_ASSERT(Location < Chunk->Global.Count, "Invalid location");
    PASCAL_ASSERT(Location + Type.Size <= Chunk->Global.Count, "Invalid size");

    switch (Type.Integral)
    {
    case TYPE_CHAR:
    case TYPE_BOOLEAN:
    case TYPE_U8:
    case TYPE_I8:
    {
        U8 Byte = Data->Int;
        ChunkWriteGlobalDataAt(Chunk, Location, &Byte, sizeof Byte);
    } break;
    case TYPE_U16:
    case TYPE_I16:
    {
        U16 u16 = Data->Int;
        ChunkWriteGlobalDataAt(Chunk, Location, &u16, u16);
    } break;
    case TYPE_POINTER:
    {
        void *Ptr = Data->Ptr.As.Raw;
        ChunkWriteGlobalDataAt(Chunk, Location, &Ptr, sizeof Ptr);
    } break;
    case TYPE_U32:
    case TYPE_I32:
    {
        U32 u32 = Data->Int;
        ChunkWriteGlobalDataAt(Chunk, Location, &u32, sizeof u32);
    } break;
    case TYPE_U64:
    case TYPE_I64:
    {
        U64 u64 = Data->Int;
        ChunkWriteGlobalDataAt(Chunk, Location, &u64, sizeof u64);
    } break;

    case TYPE_F32:
    {
        F32 f32 = Data->Flt;
        ChunkWriteGlobalDataAt(Chunk, Location, &f32, sizeof f32);
    } break;
    case TYPE_F64:
    {
        F64 f64 = Data->Flt;
        ChunkWriteGlobalDataAt(Chunk, Location, &f64, sizeof f64);
    } break;
    case TYPE_STRING:
    {
        ChunkWriteGlobalDataAt(Chunk, Location, Data->Str.Data, PStrGetLen(&Data->Str) + 1);
    } break;
    case TYPE_STATIC_ARRAY:
    {
        PASCAL_UNREACHABLE("TODO: array initialization");
    } break;

    case TYPE_FUNCTION:
    case TYPE_INVALID:
    case TYPE_RECORD:
    case TYPE_COUNT:
    {
        PASCAL_UNREACHABLE("Invalid type");
    } break;
    }
}

VarMemory PVMEmitGlobalData(PVMEmitter *Emitter, const void *Data, U32 Size)
{
    PASCAL_NONNULL(Emitter);
    VarMemory Global = {
        .RegPtr = Emitter->Reg.GP.As.Register,
        .Location = ChunkWriteGlobalData(PVMCurrentChunk(Emitter), Data, Size),
    };
    return Global;
}

VarMemory PVMEmitGlobalSpace(PVMEmitter *Emitter, U32 Size)
{
    PASCAL_NONNULL(Emitter);
    return PVMEmitGlobalData(Emitter, NULL, Size);
}






/* subroutine */
SaveRegInfo PVMEmitSaveCallerRegs(PVMEmitter *Emitter, UInt ReturnRegID)
{
    PASCAL_NONNULL(Emitter);
    if (NO_RETURN_REG == ReturnRegID)
    {
        return PVMEmitPushRegList(Emitter, Emitter->Reglist & ~EMPTY_REGLIST);
    }
    return PVMEmitPushRegList(Emitter, Emitter->Reglist & ~(((U32)1 << ReturnRegID) | EMPTY_REGLIST));
}

bool PVMRegIsSaved(SaveRegInfo Saved, UInt RegID)
{
    return (Saved.Regs & (1 << RegID)) != 0;
}

VarLocation PVMRetreiveSavedCallerReg(PVMEmitter *Emitter, SaveRegInfo Saved, UInt RegID, VarType Type)
{
    PASCAL_NONNULL(Emitter);
    PASCAL_ASSERT(RegID < STATIC_ARRAY_SIZE(Saved.RegLocation), "Invalid index");

    VarLocation RegLocation = {
        .LocationType = VAR_MEM,
        .Type = Type,
        .As.Memory = {
            .Location = Saved.RegLocation[RegID],
            .RegPtr = Emitter->Reg.FP.As.Register,
        },
    };
    return RegLocation;
}


U32 PVMEmitCall(PVMEmitter *Emitter, U32 Location)
{
    PASCAL_NONNULL(Emitter);

    U32 CurrentLocation = PVMCurrentChunk(Emitter)->Count;
    U32 Offset = Location - CurrentLocation - PVM_BRANCH_INS_SIZE;
    WriteOp32(Emitter, PVM_CALL(Offset & 0xFF), Offset >> 8);
    return CurrentLocation;
}

void PVMEmitCallPtr(PVMEmitter *Emitter, const VarLocation *Ptr)
{
    PASCAL_NONNULL(Emitter);
    PASCAL_NONNULL(Ptr);
    VarRegister Callee;
    bool Owning = PVMEmitIntoReg(Emitter, &Callee, true, Ptr);
    WriteOp16(Emitter, PVM_OP(CALLPTR, Callee.ID, 0));
    if (Owning)
    {
        PVMFreeRegister(Emitter, Callee);
    }
}


void PVMEmitUnsaveCallerRegs(PVMEmitter *Emitter, UInt ReturnRegID, SaveRegInfo Save)
{
    PASCAL_NONNULL(Emitter);
    U32 Restorelist = Save.Regs;
    if (Restorelist & 0xFF)
    {
        WriteOp16(Emitter, PVM_REGLIST(POPL, Restorelist & 0xFF));
    }
    if ((Restorelist >> 8) & 0xFF)
    {
        WriteOp16(Emitter, PVM_REGLIST(POPH, Restorelist >> 8));
    }
    if ((Restorelist >> 16) & 0xFF) 
    {
        WriteOp16(Emitter, PVM_REGLIST(FPOPL, Restorelist >> 16));
    }
    if ((Restorelist >> 24) & 0xFF)
    {
        WriteOp16(Emitter, PVM_REGLIST(FPOPH, Restorelist >> 24));
    }
    Emitter->Reglist = Restorelist | EMPTY_REGLIST;
    Emitter->StackSpace -= Save.Size;
    if (NO_RETURN_REG != ReturnRegID)
    {
        PVMMarkRegisterAsAllocated(Emitter, ReturnRegID);
    }
}




/* enter and exit/return */
U32 PVMEmitEnter(PVMEmitter *Emitter)
{
    PASCAL_NONNULL(Emitter);
    U32 InstructionLocation = WriteOp16(Emitter, PVM_SYS(ENTER));
    Write32(Emitter, 0);
    return InstructionLocation;
}

void PVMPatchEnter(PVMEmitter *Emitter, U32 Location, U32 StackSize)
{
    PASCAL_NONNULL(Emitter);

    PVMChunk *Chunk = PVMCurrentChunk(Emitter);
    StackSize = uRoundUpToMultipleOfPow2(StackSize, PVM_STACK_ALIGNMENT);

    /* cleanup reg space since we're going out of scope */
    /* TODO: the compiler might need to take care of this instead because of nested functions */
    StackSize += Emitter->SpilledRegSpace;
    Emitter->SpilledRegSpace = 0;

    Chunk->Code[Location + 1] = StackSize;
    Chunk->Code[Location + 2] = StackSize >> 16;
}

void PVMEmitExit(PVMEmitter *Emitter)
{
    PASCAL_NONNULL(Emitter);
    WriteOp16(Emitter, PVM_SYS(EXIT));
}



/* system calls */
void PVMEmitWrite(PVMEmitter *Emitter)
{
    PASCAL_NONNULL(Emitter);
    WriteOp16(Emitter, PVM_SYS(WRITE));
}




