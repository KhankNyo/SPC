
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

#define OP32_OR_OP64(pEmitter, Mnemonic, bOperandIs64, pDstLocation, Src) do{\
    if (bOperandIs64) {\
        WriteOp16(Emitter, PVM_OP(Mnemonic ## 64, (pDstLocation)->As.Register.ID, Src));\
    } else {\
        WriteOp16(Emitter, PVM_OP(Mnemonic, (pDstLocation)->As.Register.ID, Src));\
    }\
} while (0)




PVMEmitter PVMEmitterInit(PVMChunk *Chunk)
{
    PASCAL_NONNULL(Chunk);

    PVMEmitter Emitter = {
        .Chunk = Chunk,
        .Reglist = EMPTY_REGLIST,
        .SpilledRegCount = 0,
        .Reg = {
            .SP = {
                .Type = VarTypeInit(TYPE_POINTER, sizeof(void*)),
                .LocationType = VAR_REG,
                .As.Register = {
                    .ID = PVM_REG_SP,
                    .Persistent = true,
                },
            },
            .FP = {
                .Type = VarTypeInit(TYPE_POINTER, sizeof(void*)),
                .LocationType = VAR_REG,
                .As.Register = {
                    .ID = PVM_REG_FP,
                    .Persistent = true,
                },
            },
            .GP = {
                .Type = VarTypeInit(TYPE_POINTER, sizeof(void*)),
                .LocationType = VAR_REG,
                .As.Register = {
                    .ID = PVM_REG_GP,
                    .Persistent = true,
                },
            },
            .Flag = {
                .LocationType = VAR_FLAG,
                .Type = VarTypeInit(TYPE_BOOLEAN, 0),
            },
        },
        .ReturnValue = {
            .Type = VarTypeInit(TYPE_INVALID, 0),
            .LocationType = VAR_REG,
            .As.Register = {
                .ID = PVM_RETREG,
                .Persistent = false,
            }
        },
        .ShouldEmit = true,
    };
    return Emitter;
}

void PVMEmitterDeinit(PVMEmitter *Emitter)
{
    PASCAL_NONNULL(Emitter);
    PVMEmitExit(Emitter);
    PVMEmitterReset(Emitter, false);
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
    Emitter->SpilledRegCount = 0;
}








static U32 WriteOp16(PVMEmitter *Emitter, U16 Opcode)
{
    PASCAL_NONNULL(Emitter);
    if (!Emitter->ShouldEmit) return PVMCurrentChunk(Emitter)->Count;
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

static void PVMMovImm(PVMEmitter *Emitter, VarRegister Reg, I64 Imm)
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

void PVMMarkRegisterAsAllocated(PVMEmitter *Emitter, UInt Reg)
{
    PASCAL_NONNULL(Emitter);
    if (PVM_REG_COUNT + PVM_FREG_COUNT >= Reg)
    {
        Emitter->Reglist |= (U32)1 << Reg;
    }
}

void PVMMarkRegisterAsFreed(PVMEmitter *Emitter, UInt Reg)
{
    PASCAL_NONNULL(Emitter);
    Emitter->Reglist &= ~((U32)1 << Reg);
}


static U32 PVMCreateRegListLocation(PVMEmitter *Emitter, 
        SaveRegInfo Info, U32 Location[PVM_REG_COUNT + PVM_FREG_COUNT])
{
    U32 Size = 0;
    for (UInt i = 0; i < STATIC_ARRAY_SIZE(Info.RegLocation); i++)
    {
        if ((Info.Regs >> i) & 0x1)
        {
            Location[i] = Emitter->StackSpace;
            /* free registers that are not in EMPTY_REGLIST */
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
    if ((Reg & 0xFF))
    {
        WriteOp16(Emitter, PVM_REGLIST(PSHL, 1 << Reg));
    }
    else if ((Reg >> 8) & 0xFF)
    {
        WriteOp16(Emitter, PVM_REGLIST(PSHH, 1 << Reg));
    }
    /* floating point reg */
    else if ((Reg >> 16) & 0xFF)
    {
        WriteOp16(Emitter, PVM_REGLIST(FPSHL, 1 << (Reg - PVM_REG_COUNT)));
    }
    else 
    {
        WriteOp16(Emitter, PVM_REGLIST(FPSHH, 1 << (Reg - PVM_REG_COUNT)));
    }
    Emitter->StackSpace += sizeof(PVMGPR);
}

static void PVMEmitPop(PVMEmitter *Emitter, UInt Reg)
{
    PASCAL_NONNULL(Emitter);
    if ((Reg & 0xFF))
    {
        WriteOp16(Emitter, PVM_REGLIST(POPL, 1 << Reg));
    }
    else if ((Reg >> 8) & 0xFF)
    {
        WriteOp16(Emitter, PVM_REGLIST(POPH, 1 << Reg));
    }
    /* floating point reg */
    else if ((Reg >> 16) & 0xFF)
    {
        WriteOp16(Emitter, PVM_REGLIST(FPOPL, 1 << (Reg - PVM_REG_COUNT)));
    }
    else 
    {
        WriteOp16(Emitter, PVM_REGLIST(FPOPH, 1 << (Reg - PVM_REG_COUNT)));
    }
    Emitter->StackSpace -= sizeof(PVMGPR);
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
Unreachable:
        PASCAL_UNREACHABLE("Move %s %s is invalid in function %s", __func__,
                VarTypeToStr(DstType), VarTypeToStr(SrcType)
        );
    } break;
    }
#undef OP
}

static void MoveMemToReg(PVMEmitter *Emitter,
        VarRegister Dst, VarMemory Src, VarType SrcType)
{
#define OP(LoadOp) do {\
    if (IN_I16(Src.Location)) {\
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
        PVMMovImm(Emitter, Reg, Literal->Int);
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

    case TYPE_RECORD:
    case TYPE_INVALID:
    case TYPE_COUNT:
    case TYPE_STATIC_ARRAY:
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
    if (IN_I16(Dst.Location)) {\
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

bool PVMEmitIntoReg(PVMEmitter *Emitter, VarRegister *OutTarget, bool ReadOnly, const VarLocation *Src)
{
    PASCAL_NONNULL(Emitter);
    PASCAL_NONNULL(OutTarget);
    PASCAL_NONNULL(Src);

    IntegralType SrcType = Src->Type.Integral;
    switch (Src->LocationType)
    {
    case VAR_REG: 
    {
        if (Src->As.Register.Persistent && !ReadOnly)
        {
            *OutTarget = PVMAllocateRegister(Emitter, Src->Type.Integral);
            MoveRegToReg(Emitter, 
                    *OutTarget, Src->Type,
                    Src->As.Register, Src->Type
            );
            return true;
        }
    } break;
    case VAR_MEM:
    {
        *OutTarget = PVMAllocateRegister(Emitter, SrcType);
        MoveMemToReg(Emitter, *OutTarget, Src->As.Memory, Src->Type);
        return true;
    } break;
    case VAR_BUILTIN:
    case VAR_SUBROUTINE:
    {
        PASCAL_UNREACHABLE("TODO: emitting function pointer");
    } break;
    case VAR_FLAG:
    {
        PASCAL_ASSERT(SrcType == TYPE_BOOLEAN, "Unreachable");
        *OutTarget = PVMAllocateRegister(Emitter, SrcType);
        WriteOp16(Emitter, PVM_OP(GETFLAG, OutTarget->ID, 0));
        return true;
    } break;
    case VAR_LIT:
    {
        *OutTarget = PVMAllocateRegister(Emitter, SrcType);
        MoveLiteralToReg(Emitter, 
                *OutTarget, Src->Type,
                &Src->As.Literal
        );
        return true;
    } break;
    case VAR_INVALID: break;
    }

    *OutTarget = Src->As.Register;
    return false;
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

void PVMEmitSaveFrame(PVMEmitter *Emitter)
{
    PASCAL_NONNULL(Emitter);
    PVMEmitMove(Emitter, &Emitter->Reg.FP, &Emitter->Reg.SP);
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

VarLocation PVMAllocateRegisterLocation(PVMEmitter *Emitter, VarType Type)
{
    return (VarLocation) {
        .Type = Type,
        .LocationType = VAR_REG,
        .As.Register = PVMAllocateRegister(Emitter, Type.Integral),
    };
}

VarRegister PVMAllocateRegister(PVMEmitter *Emitter, IntegralType Type)
{
    PASCAL_NONNULL(Emitter);
    PASCAL_ASSERT(Type != TYPE_INVALID, "invalid type");
    VarRegister Register = { 0 };

    if (IntegralTypeIsFloat(Type))
    {
        for (UInt i = PVM_REG_COUNT; i < PVM_REG_COUNT*2; i++)
        {
            if (PVMRegisterIsFree(Emitter, i))
            {
                PVMMarkRegisterAsAllocated(Emitter, i);
                Register.ID = i;
                return Register;
            }
        }

        PASCAL_UNREACHABLE("TODO: spilling floating point register");
    }
    for (UInt i = 0; i < PVM_REG_COUNT; i++)
    {
        /* found free reg */
        if (PVMRegisterIsFree(Emitter, i))
        {
            /* mark reg as allocated */
            PVMMarkRegisterAsAllocated(Emitter, i);
            Register.ID = i;
            return Register;
        }
    }


    /* spill register */
    UInt Reg = Emitter->SpilledRegCount % PVM_REG_COUNT;
    Emitter->SpilledRegCount++;
    PVMEmitPushReg(Emitter, Reg);

    Register.ID = Reg;
    return Register;
}

void PVMFreeRegister(PVMEmitter *Emitter, VarRegister Reg)
{
    PASCAL_NONNULL(Emitter);
    if (Reg.Persistent || Reg.ID == PVM_REG_FP
    || Reg.ID == PVM_REG_GP || Reg.ID == PVM_REG_SP)
        return;

    PASCAL_ASSERT(!PVMRegisterIsFree(Emitter, Reg.ID), "double free register: %d", Reg.ID);

    UInt SpilledReg = (Emitter->SpilledRegCount - 1) % PVM_REG_COUNT;
    if (Emitter->SpilledRegCount > 0 && SpilledReg == Reg.ID)
    {
        Emitter->SpilledRegCount--;
        PVMEmitPop(Emitter, SpilledReg);
    }
    else
    {
        PVMMarkRegisterAsFreed(Emitter, Reg.ID);
    }
}





/* branching instructions */

U32 PVMEmitBranchIfFalse(PVMEmitter *Emitter, const VarLocation *Condition)
{
    PASCAL_NONNULL(Emitter);
    PASCAL_NONNULL(Condition);
    if (VAR_FLAG == Condition->LocationType)
    {
        return PVMEmitBranchOnFalseFlag(Emitter);
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
        return PVMEmitBranchOnTrueFlag(Emitter);
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

U32 PVMEmitBranchOnFalseFlag(PVMEmitter *Emitter)
{
    PASCAL_NONNULL(Emitter);
    return WriteOp32(Emitter, PVM_BR_COND(F, 0), 0); 
}

U32 PVMEmitBranchOnTrueFlag(PVMEmitter *Emitter)
{
    PASCAL_NONNULL(Emitter);
    return WriteOp32(Emitter, PVM_BR_COND(T, 0), 0); 
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
    PASCAL_ASSERT(VarTypeEqual(&Dst->Type, &Src->Type), "TODO: make type equal before Move");

    switch (Dst->LocationType)
    {
    case VAR_LIT:
    case VAR_INVALID:
    case VAR_BUILTIN:
    case VAR_SUBROUTINE:
    {
        PASCAL_UNREACHABLE("Are you crazy???");
    } break;
    case VAR_REG:
    {
        MoveLocationToReg(Emitter, Dst->As.Register, Dst->Type, Src);
    } break;
    case VAR_FLAG:
    {
        if (Src->LocationType == VAR_FLAG)
            return;
        PASCAL_ASSERT(Src->Type.Integral == TYPE_BOOLEAN, "Src must be boolean");

        VarRegister Rs;
        bool Owning = PVMEmitIntoReg(Emitter, &Rs, false, Src);
        WriteOp16(Emitter, PVM_OP(SETFLAG, Rs.ID, 0));
        if (Owning)
        {
            PVMFreeRegister(Emitter, Rs);
        }
    } break;
    case VAR_MEM:
    {
        VarRegister Tmp;
        bool Owning = PVMEmitIntoReg(Emitter, &Tmp, true, Src);
        MoveRegToMem(Emitter, 
                Dst->As.Memory, Dst->Type, 
                Tmp, Src->Type
        );
        if (Owning)
        {
            PVMFreeRegister(Emitter, Tmp);
        }
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
    PASCAL_ASSERT(!VarTypeIsTriviallyCopiable(Dst->Type), "Unhandled case: %s", 
            VarTypeToStr(Dst->Type)
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
    VarLocation ScaledIndex;
    PVMEmitIntoRegLocation(Emitter, &ScaledIndex, false, Index);

    /* scale it */
    PVMEmitIMulConst(Emitter, &ScaledIndex, ElementType->Size);

    /* Add the base register from array */
    OP32_OR_OP64(Emitter, ADD, 
        UINTPTR_MAX == UINT64_MAX, 
        &ScaledIndex, Array->As.Memory.RegPtr.ID
    );

    /* calculate effective addr of the element, treat it as a memory location */
    VarLocation Element = {
        .Type = *ElementType,
        .LocationType = VAR_MEM,
        .As.Memory = {
            .RegPtr = ScaledIndex.As.Register,
            .Location = 
                Array->As.Memory.Location - Array->Type.As.StaticArray.Range.Low*ElementType->Size,
        },
    };
    return Element;
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

void PVMEmitAddImm(PVMEmitter *Emitter, const VarLocation *Dst, I64 Imm)
{
    PASCAL_NONNULL(Emitter);
    PASCAL_NONNULL(Dst);
    if (0 == Imm)
        return;
    VarRegister Target;
    /* TODO: the readonly param does not make sense here, 
     * because in this case we want dst to be modified 
     * without having extra instructions 
     * moving it into a tmp register */
    bool IsOwning = PVMEmitIntoReg(Emitter, &Target, true, Dst);

    if (Dst->Type.Integral == TYPE_U64 || Dst->Type.Integral == TYPE_I64)
    {
        if (IS_SMALL_IMM(Imm))
            WriteOp16(Emitter, PVM_OP(ADDQI64, Target.ID, Imm));
        else if (IN_I16(Imm))
            WriteOp32(Emitter, PVM_OP(ADDI64, Target.ID, IMMTYPE_I16), Imm);
        else if (IN_U16(Imm))
            WriteOp32(Emitter, PVM_OP(ADDI64, Target.ID, IMMTYPE_U16), Imm);
        else if (IN_I32(Imm))
        {
            WriteOp16(Emitter, PVM_OP(ADDI64, Target.ID, IMMTYPE_I32));
            Write32(Emitter, Imm);
        }
        else if (IN_U32(Imm))
        {
            WriteOp16(Emitter, PVM_OP(ADDI64, Target.ID, IMMTYPE_U32));
            Write32(Emitter, Imm);
        }
        else if (IN_I48(Imm))
        {
            WriteOp32(Emitter, PVM_OP(ADDI64, Target.ID, IMMTYPE_I48), Imm);
            Write32(Emitter, Imm >> 16);
        }
        else if (IN_U48(Imm))
        {
            WriteOp32(Emitter, PVM_OP(ADDI64, Target.ID, IMMTYPE_U48), Imm);
            Write32(Emitter, Imm >> 16);
        }
        else
        {
            WriteOp16(Emitter, PVM_OP(ADDI64, Target.ID, IMMTYPE_U64));
            Write32(Emitter, Imm);
            Write32(Emitter, Imm >> 32);
        }
    }
    else if (IS_SMALL_IMM(Imm))
        WriteOp16(Emitter, PVM_OP(ADDQI, Target.ID, Imm));
    else if (IN_I16(Imm))
        WriteOp32(Emitter, PVM_OP(ADDI, Target.ID, IMMTYPE_I16), Imm);
    else if (IN_U16(Imm))
        WriteOp32(Emitter, PVM_OP(ADDI, Target.ID, IMMTYPE_U16), Imm);
    else if (IN_I32(Imm))
    {
        WriteOp16(Emitter, PVM_OP(ADDI, Target.ID, IMMTYPE_I32));
        Write32(Emitter, Imm);
    }
    else if (IN_U32(Imm))
    {
        WriteOp16(Emitter, PVM_OP(ADDI, Target.ID, IMMTYPE_U32));
        Write32(Emitter, Imm);
    }
    else if (IN_I48(Imm))
    {
        WriteOp32(Emitter, PVM_OP(ADDI, Target.ID, IMMTYPE_I48), Imm);
        Write32(Emitter, Imm >> 16);
    }
    else if (IN_U48(Imm))
    {
        WriteOp32(Emitter, PVM_OP(ADDI, Target.ID, IMMTYPE_U48), Imm);
        Write32(Emitter, Imm >> 16);
    }
    else
    {
        WriteOp16(Emitter, PVM_OP(ADDI, Target.ID, IMMTYPE_U64));
        Write32(Emitter, Imm);
        Write32(Emitter, Imm >> 32);
    }    

    if (IsOwning)
    {
        PVMEmitMove(Emitter, Dst, 
                &(VarLocation) {.Type = Dst->Type, .LocationType = VAR_REG, .As.Register = Target}
        );
        PVMFreeRegister(Emitter, Target);
    }
}





#define DEFINE_INTEGER_BINARY_OP(FnName, Mnemonic)\
void FnName (PVMEmitter *Emitter, const VarLocation *Dst, const VarLocation *Src) {\
    PASCAL_NONNULL(Emitter);\
    PASCAL_NONNULL(Dst);\
    PASCAL_NONNULL(Src);\
    PASCAL_ASSERT(Dst->Type.Integral == Src->Type.Integral, "Dst and Src type must be the same");\
    PASCAL_ASSERT(Dst->LocationType == VAR_REG, "Dst must be register");\
    VarRegister Rs, Rd = Dst->As.Register;\
    bool OwningRs = PVMEmitIntoReg(Emitter, &Rs, true, Src);\
    if (TYPE_U64 == Dst->Type.Integral || TYPE_I64 == Dst->Type.Integral) {\
        WriteOp16(Emitter, PVM_OP( Mnemonic ## 64, Rd.ID, Rs.ID));\
    } else {\
        WriteOp16(Emitter, PVM_OP( Mnemonic , Rd.ID, Rs.ID));\
    }\
    if (OwningRs) {\
        PVMFreeRegister(Emitter, Rs);\
    }\
}\
void FnName (PVMEmitter *Emitter, const VarLocation *Dst, const VarLocation *Src)

#define DEFINE_GENERIC_BINARY_OP(FnName, Mnemonic)\
void FnName (PVMEmitter *Emitter, const VarLocation *Dst, const VarLocation *Src) {\
    PASCAL_NONNULL(Emitter);\
    PASCAL_NONNULL(Dst);\
    PASCAL_NONNULL(Src);\
    PASCAL_ASSERT(Dst->LocationType == VAR_REG, "Dst must be register");\
    PASCAL_ASSERT(Dst->Type.Integral == Src->Type.Integral, \
            "Dst and Src type must be the same in %s", __func__);\
    VarRegister Rs, Rd = Dst->As.Register;\
    bool OwningRs = PVMEmitIntoReg(Emitter, &Rs, true, Src);\
    if (TYPE_U64 == Dst->Type.Integral || TYPE_I64 == Dst->Type.Integral) {\
        WriteOp16(Emitter, PVM_OP( Mnemonic ## 64, Rd.ID, Rs.ID));\
    } else if (TYPE_F64 == Dst->Type.Integral || TYPE_F64 == Src->Type.Integral) {\
        WriteOp16(Emitter, PVM_OP( F ## Mnemonic ## 64, Rd.ID, Rs.ID));\
    } else if (TYPE_F32 == Dst->Type.Integral || TYPE_F32 == Dst->Type.Integral) {\
        WriteOp16(Emitter, PVM_OP( F ## Mnemonic, Rd.ID, Rs.ID));\
    } else {\
        WriteOp16(Emitter, PVM_OP( Mnemonic , Rd.ID, Rs.ID));\
    }\
    if (OwningRs) {\
        PVMFreeRegister(Emitter, Rs);\
    }\
}\
void FnName (PVMEmitter *Emitter, const VarLocation *Dst, const VarLocation *Src)




void PVMEmitAdd(PVMEmitter *Emitter, const VarLocation *Dst, const VarLocation *Src) 
{
    PASCAL_NONNULL(Emitter);
    PASCAL_NONNULL(Dst);
    PASCAL_NONNULL(Src);

    PASCAL_ASSERT(Dst->Type.Integral == Src->Type.Integral, "Dst and Src type must be the same");
    PASCAL_ASSERT(Dst->LocationType == VAR_REG, "Dst must be a register");

    if (IntegralTypeIsInteger(Dst->Type.Integral) 
    && IntegralTypeIsInteger(Src->Type.Integral) 
    && VAR_LIT == Src->LocationType)
    {
        PVMEmitAddImm(Emitter, Dst, Src->As.Literal.Int);
        return;
    }

    VarRegister Rs, Rd = Dst->As.Register;
    bool OwningRs = PVMEmitIntoReg(Emitter, &Rs, true, Src);
    if (TYPE_F64 == Dst->Type.Integral || TYPE_F64 == Src->Type.Integral) 
    {
        WriteOp16(Emitter, PVM_OP(FADD64, Rd.ID, Rs.ID));
    } 
    else if (TYPE_F32 == Dst->Type.Integral || TYPE_F32 == Dst->Type.Integral) 
    {
        WriteOp16(Emitter, PVM_OP(FADD, Rd.ID, Rs.ID));
    }
    else if (TYPE_I64 == Dst->Type.Integral || TYPE_U64 == Dst->Type.Integral)
    {
        WriteOp16(Emitter, PVM_OP(ADD64, Rd.ID, Rs.ID));
    }
    else if (TYPE_STRING == Dst->Type.Integral)
    {
        PASCAL_ASSERT(TYPE_STRING == Src->Type.Integral, "Unreachable in %s", __func__);
        WriteOp16(Emitter, PVM_OP(SADD, Rd.ID, Rs.ID));
    }
    else
    {
        WriteOp16(Emitter, PVM_OP(ADD, Rd.ID, Rs.ID));
    }

    if (OwningRs) 
        PVMFreeRegister(Emitter, Rs);
}

void PVMEmitSub(PVMEmitter *Emitter, const VarLocation *Dst, const VarLocation *Src) 
{
    PASCAL_NONNULL(Emitter);
    PASCAL_NONNULL(Dst);
    PASCAL_NONNULL(Src);

    PASCAL_ASSERT(Dst->Type.Integral == Src->Type.Integral, "Dst and Src type must be the same in %s", __func__);
    PASCAL_ASSERT(Dst->LocationType == VAR_REG, "Dst must be a register");

    if (IntegralTypeIsInteger(Dst->Type.Integral) 
    && IntegralTypeIsInteger(Src->Type.Integral) 
    && VAR_LIT == Src->LocationType)
    {
        PVMEmitAddImm(Emitter, Dst, 0 - Src->As.Literal.Int);
        return;
    }

    VarRegister Rs, Rd = Dst->As.Register;
    bool OwningRs = PVMEmitIntoReg(Emitter, &Rs, true, Src);
    if (TYPE_F64 == Dst->Type.Integral || TYPE_F64 == Src->Type.Integral) 
    {
        WriteOp16(Emitter, PVM_OP(FSUB64, Rd.ID, Rs.ID));
    } 
    else if (TYPE_F32 == Dst->Type.Integral || TYPE_F32 == Dst->Type.Integral) 
    {
        WriteOp16(Emitter, PVM_OP(FSUB, Rd.ID, Rs.ID));
    }
    else if (TYPE_I64 == Dst->Type.Integral || TYPE_U64 == Dst->Type.Integral)
    {
        WriteOp16(Emitter, PVM_OP(SUB64, Rd.ID, Rs.ID));
    }
    else
    {
        WriteOp16(Emitter, PVM_OP(SUB, Rd.ID, Rs.ID));
    }
    if (OwningRs) 
        PVMFreeRegister(Emitter, Rs);
}


#define EMIT_REGISTER_OP16(pEmitter, Mnemonic, bOperandIs64, pDstLocation, pSrcLocation) do {\
    VarRegister Tmp;\
    bool OwningTmp = PVMEmitIntoReg(pEmitter, &Tmp, true, pSrcLocation);\
    if (bOperandIs64) {\
        WriteOp16(pEmitter, PVM_OP(Mnemonic ## 64, (pDstLocation)->As.Register.ID, Tmp.ID));\
    } else {\
        WriteOp16(pEmitter, PVM_OP(Mnemonic, (pDstLocation)->As.Register.ID, Tmp.ID));\
    }\
    if (OwningTmp) {\
        PVMFreeRegister(pEmitter, Tmp);\
    }\
} while (0)


void PVMEmitIMulConst(PVMEmitter *Emitter, const VarLocation *Dst, I64 Const)
{
    bool OperandIs64 = 
        Dst->Type.Integral == TYPE_I64 
        || Dst->Type.Integral == TYPE_U64;

    if (Const == -1)
    {
        OP32_OR_OP64(Emitter, NEG, OperandIs64, Dst, Dst->As.Register.ID);
    }
    else if (Const == 0)
    {
        PVMMovImm(Emitter, Dst->As.Register, 0);
    }
    else if (Const > 0 && IS_POW2(Const))
    {
        unsigned ShiftAmount = BitCount(Const - 1);
        if (ShiftAmount > 16) /* poor design of the PVM */
        {
            VarRegister Tmp = PVMAllocateRegister(Emitter, TYPE_U8);
            OP32_OR_OP64(Emitter, VSHL, OperandIs64, Dst, Tmp.ID);
            PVMFreeRegister(Emitter, Tmp);
        }
        else
        {
            OP32_OR_OP64(Emiter, QSHL, OperandIs64, Dst, ShiftAmount);
        }
    }
    else 
    {
        VarRegister Tmp = PVMAllocateRegister(Emitter, Dst->Type.Integral);
        if (IntegralTypeIsSigned(Dst->Type.Integral))
        {
            OP32_OR_OP64(Emitter, IMUL, OperandIs64, Dst, Tmp.ID);
        }
        else
        {
            OP32_OR_OP64(Emitter, MUL, OperandIs64, Dst, Tmp.ID);
        }
        PVMFreeRegister(Emitter, Tmp);
    }
}

void PVMEmitMul(PVMEmitter *Emitter, const VarLocation *Dst, const VarLocation *Src)
{
    PASCAL_NONNULL(Emitter);
    PASCAL_NONNULL(Dst);
    PASCAL_NONNULL(Src);
    PASCAL_ASSERT(Dst->Type.Integral == Src->Type.Integral, "srctype != dsttype");
    PASCAL_ASSERT(Dst->LocationType == VAR_REG, "dst must be reg");
    if (IntegralTypeIsInteger(Dst->Type.Integral))
    {
        if (VAR_LIT == Src->LocationType && IntegralTypeIsOrdinal(Src->Type.Integral))
        {
            PVMEmitIMulConst(Emitter, Dst, OrdinalLiteralToI64(Src->As.Literal, Src->Type.Integral));
        }
        else if (IntegralTypeIsSigned(Dst->Type.Integral))
        {
            EMIT_REGISTER_OP16(Emitter, IMUL, Dst->Type.Integral == TYPE_I64, Dst, Src);
        }
        else
        {
            EMIT_REGISTER_OP16(Emitter, MUL, Dst->Type.Integral == TYPE_U64, Dst, Src);
        }
    }
    if (IntegralTypeIsFloat(Dst->Type.Integral))
    {
        EMIT_REGISTER_OP16(Emitter, 
            FMUL, Dst->Type.Integral == TYPE_F64,
            Dst, Src
        );
    }
}

void PVMEmitDiv(PVMEmitter *Emitter, const VarLocation *Dst, const VarLocation *Src)
{
    PASCAL_NONNULL(Emitter);
    PASCAL_NONNULL(Dst);
    PASCAL_NONNULL(Src);
    PASCAL_ASSERT(Dst->Type.Integral == Src->Type.Integral, "srctype != dsttype");
    PASCAL_ASSERT(Dst->LocationType == VAR_REG, "dst must be reg");

    if (IntegralTypeIsInteger(Dst->Type.Integral))
    {
        bool OperandIs64 = 
            Dst->Type.Integral == TYPE_I64
            || Dst->Type.Integral == TYPE_U64;
        /* signed division */
        if (IntegralTypeIsSigned(Dst->Type.Integral))
        {
            /* division by shifting */
            if (Src->LocationType == VAR_LIT 
            && Src->As.Literal.Int > 0 
            && IS_POW2(Src->As.Literal.Int))
            {
                if (Src->As.Literal.Int == 1)
                {
                    return;
                }

                unsigned ShiftAmount = BitCount(Src->As.Literal.Int - 1);
                if (ShiftAmount > 16) /* very poor design of the PVM */
                {
                    /* load the constant into a reg and shift dst by it */
                    VarRegister Tmp = PVMAllocateRegister(Emitter, Dst->Type.Integral);
                    PVMMovImm(Emitter, Tmp, ShiftAmount);
                    OP32_OR_OP64(Emitter, VASR, OperandIs64, Dst, Tmp.ID);
                    PVMFreeRegister(Emitter, Tmp);
                }
                else 
                {
                    OP32_OR_OP64(Emitter, QASR, OperandIs64, Dst, ShiftAmount);
                }
            }
            else if (Src->LocationType == VAR_LIT && -1 == (I64)Src->As.Literal.Int)
            {
                OP32_OR_OP64(Emitter, NEG, OperandIs64, Dst, Dst->As.Register.ID);
            }
            else
            {
                EMIT_REGISTER_OP16(Emitter, 
                    IDIV, OperandIs64,
                    Dst, Src
                );
            }
        }
        else /* unsigned division */
        {
            /* division by shifting */
            if (Src->LocationType == VAR_LIT 
            && Src->As.Literal.Int > 0 
            && IS_POW2(Src->As.Literal.Int))
            {
                unsigned ShiftAmount = BitCount(Src->As.Literal.Int - 1);
                if (ShiftAmount > 16)
                {
                    /* load the constant into a reg and shift dst by it */
                    VarRegister Tmp = PVMAllocateRegister(Emitter, Dst->Type.Integral);
                    PVMMovImm(Emitter, Tmp, ShiftAmount);
                    OP32_OR_OP64(Emitter, VSHR, OperandIs64, Dst, Tmp.ID);
                    PVMFreeRegister(Emitter, Tmp);
                }
                else /* poor design of PVM */
                {
                    OP32_OR_OP64(Emitter, QSHR, OperandIs64, Dst, ShiftAmount);
                }
            }
            else /* normal division */
            {
                EMIT_REGISTER_OP16(Emitter,
                    DIV, OperandIs64,
                    Dst, Src
                );
            }
        }
    }
    else if (IntegralTypeIsFloat(Dst->Type.Integral))
    {
        EMIT_REGISTER_OP16(Emitter, 
            FDIV, Dst->Type.Integral == TYPE_F64,
            Dst, Src
        );
    }
}


void PVMEmitNot(PVMEmitter *Emitter, const VarLocation *Dst, const VarLocation *Src)
{
    PASCAL_NONNULL(Emitter);
    PASCAL_NONNULL(Dst);
    PASCAL_NONNULL(Src);
    PASCAL_ASSERT(Dst->Type.Integral == Src->Type.Integral, "Dst and Src type must be the same");
    if (Dst->Type.Integral == TYPE_BOOLEAN)
    {
        if (VAR_FLAG == Dst->LocationType && VAR_FLAG == Src->LocationType)
        {
            WriteOp16(Emitter, PVM_OP(NEGFLAG, 0, 0));
            return;
        }
        VarRegister Rs, Rd = Dst->As.Register;
        bool Owning = PVMEmitIntoReg(Emitter, &Rs, true, Src);
        if (VAR_REG == Dst->LocationType)
        {
            WriteOp16(Emitter, PVM_OP(SETEZ, Rd.ID, Rs.ID));
        }
        else if (VAR_FLAG == Dst->LocationType)
        {
            WriteOp16(Emitter, PVM_OP(SETNFLAG, Rs.ID, 0));
        }
        if (Owning)
        {
            PVMFreeRegister(Emitter, Rs);
        }
    }
    else if (IntegralTypeIsInteger(Dst->Type.Integral))
    {
        PASCAL_ASSERT(Dst->LocationType == VAR_REG, "Dst can only be a register");
        VarRegister Rs, Rd = Dst->As.Register;
        bool Owning = PVMEmitIntoReg(Emitter, &Rs, true, Src);
        if (TYPE_U64 == Dst->Type.Integral || TYPE_I64 == Dst->Type.Integral)
        {
            WriteOp16(Emitter, PVM_OP(NOT64, Rd.ID, Rs.ID));
        }
        else
        {
            WriteOp16(Emitter, PVM_OP(NOT, Rd.ID, Rs.ID));
        }
        if (Owning)
            PVMFreeRegister(Emitter, Rs);
    }
    else
    {
        PASCAL_UNREACHABLE("Invalid type");
    }
}






/* kill me */
DEFINE_GENERIC_BINARY_OP(PVMEmitNeg, NEG);

DEFINE_INTEGER_BINARY_OP(PVMEmitAnd, AND);
DEFINE_INTEGER_BINARY_OP(PVMEmitOr, OR);
DEFINE_INTEGER_BINARY_OP(PVMEmitXor, XOR);
DEFINE_INTEGER_BINARY_OP(PVMEmitMod, MOD);
DEFINE_INTEGER_BINARY_OP(PVMEmitShl, VSHL);
DEFINE_INTEGER_BINARY_OP(PVMEmitShr, VSHR);
DEFINE_INTEGER_BINARY_OP(PVMEmitAsr, VASR);


#undef DEFINE_CONST_OP
#undef OP32_OR_OP64
#undef DEFINE_INTEGER_BINARY_OP
#undef DEFINE_GENERIC_BINARY_OP





VarLocation PVMEmitSetFlag(PVMEmitter *Emitter, TokenType Op, 
        const VarLocation *Left, const VarLocation *Right)
{
#define SET(SignPrefix, BitPostfix)\
    do {\
        switch (Op) {\
        case TOKEN_EQUAL:           WriteOp16(Emitter, PVM_OP(SEQ ## BitPostfix, RdID, RsID)); break;\
        case TOKEN_LESS_GREATER:    WriteOp16(Emitter, PVM_OP(SNE ## BitPostfix, RdID, RsID)); break;\
        case TOKEN_LESS:            WriteOp16(Emitter, PVM_OP(SignPrefix ## SLT ## BitPostfix, RdID, RsID)); break;\
        case TOKEN_GREATER:         WriteOp16(Emitter, PVM_OP(SignPrefix ## SGT ## BitPostfix, RdID, RsID)); break;\
        case TOKEN_GREATER_EQUAL:   WriteOp16(Emitter, PVM_OP(SignPrefix ## SGE ## BitPostfix, RdID, RsID)); break;\
        case TOKEN_LESS_EQUAL:      WriteOp16(Emitter, PVM_OP(SignPrefix ## SLE ## BitPostfix, RdID, RsID)); break;\
        default: PASCAL_UNREACHABLE("Invalid operator: %s in SetCC\n", TokenTypeToStr(Op)); break;\
        }\
    } while (0)

#define FSET(BitPostfix)\
    do {\
        switch (Op) {\
        case TOKEN_EQUAL:           WriteOp16(Emitter, PVM_OP(FSEQ ## BitPostfix, RdID, RsID)); break;\
        case TOKEN_LESS_GREATER:    WriteOp16(Emitter, PVM_OP(FSNE ## BitPostfix, RdID, RsID)); break;\
        case TOKEN_LESS:            WriteOp16(Emitter, PVM_OP(FSLT ## BitPostfix, RdID, RsID)); break;\
        case TOKEN_GREATER:         WriteOp16(Emitter, PVM_OP(FSGT ## BitPostfix, RdID, RsID)); break;\
        case TOKEN_GREATER_EQUAL:   WriteOp16(Emitter, PVM_OP(FSGE ## BitPostfix, RdID, RsID)); break;\
        case TOKEN_LESS_EQUAL:      WriteOp16(Emitter, PVM_OP(FSLE ## BitPostfix, RdID, RsID)); break;\
        default: PASCAL_UNREACHABLE("Invalid operator: %s in FSetCC\n", TokenTypeToStr(Op)); break;\
        }\
    } while (0)

#define SSET(BitPostfix) \
    do {\
        switch(Op) {\
        case TOKEN_EQUAL: WriteOp16(Emitter, PVM_OP(STREQU, RdID, RsID)); break;\
        case TOKEN_LESS: WriteOp16(Emitter, PVM_OP(STRLT, RdID, RsID)); break;\
        case TOKEN_GREATER: WriteOp16(Emitter, PVM_OP(STRGT, RdID, RsID)); break;\
        case TOKEN_LESS_GREATER: { \
            WriteOp16(Emitter, PVM_OP(STRLT, RdID, RsID));\
            WriteOp16(Emitter, PVM_OP(SETEZ ## BitPostfix, RdID, RdID));\
        } break;\
        case TOKEN_LESS_EQUAL: { \
            WriteOp16(Emitter, PVM_OP(STRGT, RdID, RsID));\
            WriteOp16(Emitter, PVM_OP(SETEZ ## BitPostfix, RdID, RdID));\
        } break;\
        default: PASCAL_UNREACHABLE("Invalid string compare operation."); break;\
        }\
    } while (0)

    PASCAL_NONNULL(Emitter);
    PASCAL_NONNULL(Left);
    PASCAL_NONNULL(Right);
    PASCAL_ASSERT(Left->Type.Integral == Right->Type.Integral, "Left and Right must have the same type");
    if (!Emitter->ShouldEmit) 
    {
        return VAR_LOCATION_LIT(.Bool = false, TYPE_BOOLEAN);
    }

    VarRegister Rd, Rs;
    bool OwningRd = PVMEmitIntoReg(Emitter, &Rd, true, Left);
    bool OwningRs = PVMEmitIntoReg(Emitter, &Rs, true, Right);
    UInt RdID = Rd.ID;
    UInt RsID = Rs.ID;

    if (TYPE_STRING == Left->Type.Integral)
    {
        if (UINTPTR_MAX == UINT32_MAX)
            SSET();
        else 
            SSET();
    }
    else if (IntegralTypeIsFloat(Left->Type.Integral))
    {
        if (TYPE_F64 == Left->Type.Integral)
            FSET(64);
        else if (TYPE_F32 == Left->Type.Integral)
            FSET();
    }
    else if (IntegralTypeIsInteger(Left->Type.Integral))
    {
        if (TYPE_I64 == Left->Type.Integral) 
            SET(I, 64);
        else if (TYPE_U64 == Left->Type.Integral)
            SET(, 64); 
        else if (IntegralTypeIsSigned(Left->Type.Integral))
            SET(I, );
        else 
            SET(,);
    }
    else if (TYPE_POINTER == Left->Type.Integral)
    {
        if (UINTPTR_MAX == UINT32_MAX)
            SET(,);
        else 
            SET(,64);
    }
    else 
    {
        PASCAL_UNREACHABLE("EmitSetCC: %s %s %s\n", 
                VarTypeToStr(Left->Type),
                TokenTypeToStr(Op),
                VarTypeToStr(Right->Type)
        );
    }

    if (OwningRd)
    {
        PVMFreeRegister(Emitter, Rd);
    }
    if (OwningRs) 
    {
        PVMFreeRegister(Emitter, Rs);
    }
    return Emitter->Reg.Flag;
#undef SET
#undef FSET
#undef SSET
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



void PVMQueuePushMultiple(PVMEmitter *Emitter, SaveRegInfo *RegList, const VarLocation *Location)
{
    PASCAL_NONNULL(Emitter);
    PASCAL_NONNULL(RegList);
    PASCAL_NONNULL(Location);

    VarRegister Reg;
    PVMEmitIntoReg(Emitter, &Reg, true, Location);
    RegList->Regs |= 1 << Reg.ID;
}


void PVMQueueAndCommitOnFull(
        PVMEmitter *Emitter, SaveRegInfo *RegList, const VarLocation *Location)
{
    PASCAL_NONNULL(Emitter);
    PASCAL_NONNULL(RegList);
    PASCAL_NONNULL(Location);

    if (PVMQueueIsFull(RegList))
    {
        PVMQueueCommit(Emitter, RegList);
        PVMQueueRefresh(Emitter, RegList);
    }
    PVMQueuePushMultiple(Emitter, RegList, Location);
}

void PVMQueueCommit(PVMEmitter *Emitter, SaveRegInfo *RegList)
{
    PASCAL_NONNULL(Emitter);
    PASCAL_NONNULL(RegList);
    if (0 == RegList->Regs)
        return;

    *RegList = PVMEmitPushRegList(Emitter, RegList->Regs);
}

void PVMQueueRefresh(PVMEmitter *Emitter, SaveRegInfo *RegList)
{
    PASCAL_NONNULL(RegList);

    UInt RegCount = BitCount(RegList->Regs);
    UInt Last = RegList->Regs;

    for (UInt i = 0; i < RegCount; i++)
    {
        RegList->Regs &= RegList->Regs - 1;         /* toggle the first bit off */
        UInt RegIndex = (Last ^ (Last - 1)) - 1;    /* get the index of the first set bit */
        Last = RegList->Regs;                       /* update */

        PVMMarkRegisterAsFreed(Emitter, RegIndex);
    }
    *RegList = (SaveRegInfo){ 0 };
}

bool PVMQueueIsFull(const SaveRegInfo *RegList)
{
    PASCAL_NONNULL(RegList);
    return ((RegList->Regs & (U16)~EMPTY_REGLIST) == (U16)~(EMPTY_REGLIST)  /* int reglist */
        || ((RegList->Regs >> 16) & 0xFFFF) == 0xFFFF) /* float reglist */;
}




/* stack allocation */

void PVMEmitStackAllocation(PVMEmitter *Emitter, I32 Size) 
{
    PASCAL_NONNULL(Emitter);
    I64 Aligned = iRoundUpToMultipleOfPow2(Size, sizeof(PVMGPR));
    PVMEmitAddImm(Emitter, &Emitter->Reg.SP, Aligned);
    Emitter->StackSpace += Aligned;
}


VarLocation PVMCreateStackLocation(PVMEmitter *Emitter, VarType Type, int FpOffset)
{
    VarLocation Location = {
        .LocationType = VAR_MEM,
        .Type = Type,
        .As.Memory = {
            .Location = FpOffset,
            .RegPtr = Emitter->Reg.FP.As.Register,
        },
    };
    if (STACK_TOP == FpOffset)
        Location.As.Memory.Location = Emitter->StackSpace;
    return Location;
}






/* global instructions */
U32 PVMGetGlobalOffset(PVMEmitter *Emitter)
{
    PASCAL_NONNULL(Emitter);
    return PVMCurrentChunk(Emitter)->Global.Count;
}

void PVMInitializeGlobal(PVMEmitter *Emitter, 
        const VarLocation *Global, const VarLiteral *Data, USize Size, IntegralType Type)
{
    PASCAL_NONNULL(Emitter);
    PASCAL_NONNULL(Global);
    PASCAL_NONNULL(Data);

    U32 Location = Global->As.Memory.Location;
    PVMChunk *Chunk = PVMCurrentChunk(Emitter);
    PASCAL_ASSERT(Global->Type.Integral == Type, "Types must be equal");
    PASCAL_ASSERT(Global->LocationType == VAR_MEM, "Invalid location");
    PASCAL_ASSERT(Location < Chunk->Global.Count, "Invalid location");
    PASCAL_ASSERT(Location + Size <= Chunk->Global.Count, "Invalid size");

    switch (Type)
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
        PASCAL_STATIC_ASSERT(offsetof(PascalStr, FbString.Len) == 0, "??");
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
    if (Restorelist >> 8)
    {
        WriteOp16(Emitter, PVM_REGLIST(POPH, Restorelist >> 8));
    }
    if (Restorelist & 0xFF)
    {
        WriteOp16(Emitter, PVM_REGLIST(POPL, Restorelist & 0xFF));
    }
    if ((Restorelist >> 24) & 0xFF) 
    {
        WriteOp16(Emitter, PVM_REGLIST(FPOPL, Restorelist >> 24));
    }
    if ((Restorelist >> 16) & 0xFF)
    {
        WriteOp16(Emitter, PVM_REGLIST(FPOPL, Restorelist >> 16));
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




