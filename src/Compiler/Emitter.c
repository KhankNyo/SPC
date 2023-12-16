
#include <stdarg.h>

#include "Compiler/Emitter.h"
#include "Compiler/Data.h"
#include "PVM/Isa.h"



/*
 * SP, GP, FP are allocated by default
 * */
#define EMPTY_REGLIST 0xE000

#if UINTPTR_MAX == UINT32_MAX
#  define CASE_PTR32(Colon) case TYPE_POINTER Colon
#  define CASE_PTR64(Colon)
#  define CASE_OBJREF32(Colon) case TYPE_STRING Colon case TYPE_RECORD Colon
#  define CASE_OBJREF64(Colon)
#else
#  define CASE_PTR32(Colon)
#  define CASE_PTR64(Colon) case TYPE_POINTER Colon
#  define CASE_OBJREF32(Colon)
#  define CASE_OBJREF64(Colon) case TYPE_STRING Colon case TYPE_RECORD Colon
#endif



PVMEmitter PVMEmitterInit(PVMChunk *Chunk)
{
    PASCAL_NONNULL(Chunk);
    PVMEmitter Emitter = {
        .Chunk = Chunk,
        .Reglist = EMPTY_REGLIST,
        .SpilledRegCount = 0,
        .Reg = {
            .SP = {
                .LocationType = VAR_REG,
                .Type = TYPE_POINTER,
                .As.Register = {
                    .ID = PVM_REG_SP,
                },
            },
            .FP = {
                .LocationType = VAR_REG,
                .Type = TYPE_POINTER,
                .As.Register = {
                    .ID = PVM_REG_FP,
                },
            },
            .GP = {
                .LocationType = VAR_REG,
                .Type = TYPE_POINTER,
                .As.Register = {
                    .ID = PVM_REG_GP,
                },
            },
            .Flag = {
                .Type = TYPE_BOOLEAN,
                .LocationType = VAR_FLAG,
            },
        },
        .ReturnValue = {
            .LocationType = VAR_REG,
            .Type = TYPE_INVALID,
            .As.Register.ID = PVM_RETREG,
        },
        .ShouldEmit = true,
    };
    for (int i = PVM_ARGREG_0; i < PVM_ARGREG_COUNT; i++)
    {
        Emitter.ArgReg[i] = (VarLocation){
            .LocationType = VAR_REG,
            .Type = TYPE_INVALID,
            .As.Register = {
                .ID = i,
            },
        };
    }
    return Emitter;
}

void PVMEmitterDeinit(PVMEmitter *Emitter)
{
    PASCAL_NONNULL(Emitter);
    PVMEmitExit(Emitter);
}









static PVMChunk *PVMCurrentChunk(PVMEmitter *Emitter)
{
    PASCAL_NONNULL(Emitter);
    return Emitter->Chunk;
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


static bool PVMRegisterIsFree(PVMEmitter *Emitter, UInt Reg)
{
    PASCAL_NONNULL(Emitter);
    return ((Emitter->Reglist >> Reg) & 1) == 0;
}

void PVMMarkRegisterAsAllocated(PVMEmitter *Emitter, UInt Reg)
{
    PASCAL_NONNULL(Emitter);
    Emitter->Reglist |= (U32)1 << Reg;
}

static void PVMMarkRegisterAsFreed(PVMEmitter *Emitter, UInt Reg)
{
    PASCAL_NONNULL(Emitter);
    Emitter->Reglist &= ~((U32)1 << Reg);
}

static void PVMEmitPushReg(PVMEmitter *Emitter, UInt Reg)
{
    PASCAL_NONNULL(Emitter);
    if (Reg < PVM_REG_COUNT / 2)
    {
        WriteOp16(Emitter, PVM_REGLIST(PSHL, 1 << Reg));
    }
    else
    {
        WriteOp16(Emitter, PVM_REGLIST(PSHH, 1 << Reg));
    }
    Emitter->StackSpace += sizeof(PVMGPR);
}

static void PVMEmitPop(PVMEmitter *Emitter, UInt Reg)
{
    PASCAL_NONNULL(Emitter);
    if (Reg < PVM_REG_COUNT / 2)
    {
        WriteOp16(Emitter, PVM_REGLIST(POPL, 1 << Reg));
    }
    else
    {
        WriteOp16(Emitter, PVM_REGLIST(POPH, 1 << Reg));
    }
    Emitter->StackSpace -= sizeof(PVMGPR);
}



/* 
 * if Dst ID and Src ID conflicts, then it will allocate a floating point reg and free Dst
 * only call this function when SrcType is integer and DstType is some kind of float
 * TODO: not do this, this is horrible 
 */
static void MakeDstTypeCompatible(PVMEmitter *Emitter, VarRegister *Dst, IntegralType DstType, VarRegister Src)
{
    PASCAL_NONNULL(Emitter);
    PASCAL_NONNULL(Dst);
    if (Dst->ID == Src.ID)
    {
        VarLocation NewLocation = PVMAllocateRegister(Emitter, DstType);
        PVMFreeRegister(Emitter, *Dst);
        *Dst = NewLocation.As.Register;
    }
}

static void TransferRegister(PVMEmitter *Emitter, VarRegister *Dst, IntegralType DstType, VarRegister Src, IntegralType SrcType)
{
    PASCAL_NONNULL(Emitter);
    PASCAL_NONNULL(Dst);
    if (DstType == SrcType && Dst->ID == Src.ID)
        return;

    switch (DstType) 
    {
    default:
Unreachable:
    {
        break;
        PASCAL_UNREACHABLE("Cannot move register of type %s into %s", IntegralTypeToStr(DstType), IntegralTypeToStr(SrcType));
    } break;
    case TYPE_I64:
    {
        switch (SrcType)
        {
        case TYPE_I64:
        case TYPE_U64: if (Dst->ID != Src.ID) WriteOp16(Emitter, PVM_OP(MOV64, Dst->ID, Src.ID)); break;
        case TYPE_BOOLEAN:
        case TYPE_I8:
        case TYPE_U8:
        case TYPE_I16:
        case TYPE_U16:
        case TYPE_I32: WriteOp16(Emitter, PVM_OP(MOVSEX64_32, Dst->ID, Src.ID)); break;
        case TYPE_U32: WriteOp16(Emitter, PVM_OP(MOVZEX64_32, Dst->ID, Src.ID)); break;
        default: break;
        }
    } break;
    CASE_PTR64(:)
    CASE_OBJREF64(:)
    case TYPE_U64:
    {
        switch (SrcType) 
        {
        CASE_PTR64(:)
        CASE_OBJREF64(:)
        case TYPE_U64:
        case TYPE_I64: if (Dst->ID != Src.ID) WriteOp16(Emitter, PVM_OP(MOV64, Dst->ID, Src.ID)); break;
        case TYPE_I32:
        case TYPE_U32: WriteOp16(Emitter, PVM_OP(MOVZEX64_32, Dst->ID, Src.ID)); break;
        case TYPE_U16:
        case TYPE_I16: WriteOp16(Emitter, PVM_OP(MOVZEX64_16, Dst->ID, Src.ID)); break;
        case TYPE_U8:
        case TYPE_I8: WriteOp16(Emitter, PVM_OP(MOVZEX64_8, Dst->ID, Src.ID)); break;
        default: goto Unreachable;
        }
    } break;
    CASE_PTR32(:)
    CASE_OBJREF32(:)
    case TYPE_BOOLEAN:
    case TYPE_U8:
    case TYPE_U16: 
    case TYPE_U32:
    {
        switch (SrcType) 
        {
        case TYPE_I8:
        case TYPE_U8:   WriteOp16(Emitter, PVM_OP(MOVZEX32_8, Dst->ID, Src.ID)); break;
        case TYPE_I16:
        case TYPE_U16:  WriteOp16(Emitter, PVM_OP(MOVZEX32_16, Dst->ID, Src.ID)); break;
        default: if (Dst->ID != Src.ID) WriteOp16(Emitter, PVM_OP(MOV32, Dst->ID, Src.ID));
        }
    } break;
    case TYPE_I8:
    case TYPE_I16:
    case TYPE_I32:
    {
        switch (SrcType) 
        {
        case TYPE_U8:   WriteOp16(Emitter, PVM_OP(MOVZEX32_8, Dst->ID, Src.ID)); break;
        case TYPE_U16:  WriteOp16(Emitter, PVM_OP(MOVZEX32_16, Dst->ID, Src.ID)); break;
        default: if (Dst->ID != Src.ID) WriteOp16(Emitter, PVM_OP(MOV32, Dst->ID, Src.ID));
        }
    } break;

    case TYPE_F32: 
    {
        switch (SrcType) 
        {
        case TYPE_F32: if (Dst->ID != Src.ID) WriteOp16(Emitter, PVM_OP(FMOV, Dst->ID, Src.ID)); break;
        case TYPE_F64: WriteOp16(Emitter, PVM_OP(F64TOF32, Dst->ID, Src.ID)); break;
        case TYPE_U8:
        case TYPE_U16:
        case TYPE_U32:
        {
            MakeDstTypeCompatible(Emitter, Dst, DstType, Src);
            WriteOp16(Emitter, PVM_OP(U32TOF32, Dst->ID, Src.ID)); break;
        } break;
        case TYPE_U64: 
        {
            MakeDstTypeCompatible(Emitter, Dst, DstType, Src);
            WriteOp16(Emitter, PVM_OP(U64TOF32, Dst->ID, Src.ID)); break;
        } break;
        case TYPE_I8:
        case TYPE_I16:
        case TYPE_I32:
        {
            MakeDstTypeCompatible(Emitter, Dst, TYPE_F32, Src);
            WriteOp16(Emitter, PVM_OP(I32TOF32, Dst->ID, Src.ID));
        } break;
        case TYPE_I64: 
        {
            MakeDstTypeCompatible(Emitter, Dst, DstType, Src);
            WriteOp16(Emitter, PVM_OP(I64TOF32, Dst->ID, Src.ID));
        } break;
        default: goto Unreachable;
        }
    } break;
    case TYPE_F64:
    {
        switch (SrcType) 
        {
        case TYPE_F32: WriteOp16(Emitter, PVM_OP(F32TOF64, Dst->ID, Src.ID)); break;
        case TYPE_F64: if (Dst->ID != Src.ID) WriteOp16(Emitter, PVM_OP(FMOV64, Dst->ID, Src.ID)); break;
        case TYPE_U8:
        case TYPE_U16:
        case TYPE_U32:
        {
            MakeDstTypeCompatible(Emitter, Dst, DstType, Src);
            WriteOp16(Emitter, PVM_OP(U32TOF64, Dst->ID, Src.ID));
        } break;
        case TYPE_U64: 
        {
            MakeDstTypeCompatible(Emitter, Dst, DstType, Src);
            WriteOp16(Emitter, PVM_OP(U64TOF64, Dst->ID, Src.ID));
        } break;
        case TYPE_I8:
        case TYPE_I16:
        case TYPE_I32:
        {
            MakeDstTypeCompatible(Emitter, Dst, DstType, Src);
            WriteOp16(Emitter, PVM_OP(I32TOF64, Dst->ID, Src.ID));
        } break;
        case TYPE_I64: 
        {
            MakeDstTypeCompatible(Emitter, Dst, DstType, Src);
            WriteOp16(Emitter, PVM_OP(I64TOF64, Dst->ID, Src.ID));
        } break;
        default: goto Unreachable;
        }
    } break;
    }
}


static void StoreToPtr(PVMEmitter *Emitter, 
        VarRegister Base, I32 Offset, IntegralType DstType, 
        VarRegister Src)
{
    if (IN_I16(Offset)) 
    {
        switch (DstType) 
        {
        case TYPE_BOOLEAN:
        case TYPE_I8:
        case TYPE_U8: WriteOp32(Emitter, PVM_OP(ST8, Src.ID, Base.ID), Offset); break;
        case TYPE_I16:
        case TYPE_U16: WriteOp32(Emitter, PVM_OP(ST16, Src.ID, Base.ID), Offset); break;
        CASE_PTR32(:)
        case TYPE_I32:
        case TYPE_U32: WriteOp32(Emitter, PVM_OP(ST32, Src.ID, Base.ID), Offset); break;
        case TYPE_I64:
        CASE_PTR64(:)
        case TYPE_U64: WriteOp32(Emitter, PVM_OP(ST64, Src.ID, Base.ID), Offset); break;
        case TYPE_F32: WriteOp32(Emitter, PVM_OP(STF32, Src.ID, Base.ID), Offset); break;
        case TYPE_F64: WriteOp32(Emitter, PVM_OP(STF64, Src.ID, Base.ID), Offset); break;
        case TYPE_STRING:
        {
            VarLocation Rd = PVMAllocateRegister(Emitter, TYPE_STRING);
            WriteOp32(Emitter, PVM_OP(LEA, Rd.As.Register.ID, Base.ID), Offset);
            WriteOp16(Emitter, PVM_OP(SCPY, Rd.As.Register.ID, Src.ID));
            PVMFreeRegister(Emitter, Rd.As.Register);
        } break;
        default: PASCAL_UNREACHABLE("TODO: Src.Size > 8"); break;
        }
    } 
    else 
    {
        switch (DstType) 
        {
        case TYPE_BOOLEAN:
        case TYPE_I8:
        case TYPE_U8: WriteOp16(Emitter, PVM_OP(ST8L, Src.ID, Base.ID)); break;
        case TYPE_I16:
        case TYPE_U16: WriteOp16(Emitter, PVM_OP(ST16L, Src.ID, Base.ID)); break;
        CASE_PTR32(:)
        case TYPE_I32:
        case TYPE_U32: WriteOp16(Emitter, PVM_OP(ST32L, Src.ID, Base.ID)); break;
        CASE_PTR64(:)
        case TYPE_I64:
        case TYPE_U64: WriteOp16(Emitter, PVM_OP(ST64L, Src.ID, Base.ID)); break;
        case TYPE_F32: WriteOp16(Emitter, PVM_OP(STF32L, Src.ID, Base.ID)); break;
        case TYPE_F64: WriteOp16(Emitter, PVM_OP(STF64L, Src.ID, Base.ID)); break;
        case TYPE_STRING:
        {
            VarLocation Rd = PVMAllocateRegister(Emitter, TYPE_STRING);
            WriteOp32(Emitter, PVM_OP(LEAL, Rd.As.Register.ID, Base.ID), Offset);
            WriteOp16(Emitter, PVM_OP(SCPY, Rd.As.Register.ID, Src.ID));
            PVMFreeRegister(Emitter, Rd.As.Register);
        } break;
        default: PASCAL_UNREACHABLE("TODO: Dst.Size > 8"); break;
        }
        WriteOp32(Emitter, Offset, (U32)Offset >> 16);
    }
}

static void StoreFromReg(PVMEmitter *Emitter, VarMemory Dst, IntegralType DstType, VarRegister Src, IntegralType SrcType)
{    
    PASCAL_NONNULL(Emitter);
    if (DstType != SrcType) 
    {
        TransferRegister(Emitter, &Src, DstType, Src, SrcType);
    }

    VarRegister Base = Dst.RegPtr;
    StoreToPtr(Emitter,
            Base, Dst.Location, DstType,
            Src
    );
}

static void DerefIntoFloatReg(PVMEmitter *Emitter, 
        VarRegister *Dst, IntegralType DstType, 
        VarRegister Base, I32 Offset, IntegralType SrcType)
{
    PASCAL_NONNULL(Emitter);
#define LOAD(FloatType)\
do {\
    if (IN_I16(Offset)) {\
        WriteOp16(Emitter, PVM_OP(LD ## FloatType, Dst->ID, Base.ID));\
        WriteOp16(Emitter, Offset);\
    } else {\
        WriteOp16(Emitter, PVM_OP(LD ## FloatType ## L, Dst->ID, Base.ID));\
        WriteOp32(Emitter, Offset, (U32)Offset >> 16);\
    }\
} while (0)


    switch (SrcType) {
    case TYPE_F32: LOAD(F32); break;
    case TYPE_F64: LOAD(F64); break;
    case TYPE_I8:  LOAD(SEX32_8); break;
    case TYPE_I16: LOAD(SEX32_16); break;
    case TYPE_U8:  LOAD(ZEX32_8); break;
    case TYPE_U16: LOAD(ZEX32_16); break;
    case TYPE_U32:
    case TYPE_I32: LOAD(32); break;
    case TYPE_I64:
    case TYPE_U64: LOAD(64); break;
    default: PASCAL_UNREACHABLE("Cannot convert %s to float.", IntegralTypeToStr(SrcType));
    }
    TransferRegister(Emitter, Dst, DstType, *Dst, SrcType);

#undef LOAD
}


static void DerefIntoIntReg(PVMEmitter *Emitter, 
        VarRegister *Dst, IntegralType DstType, 
        VarRegister Base, I32 Offset, IntegralType SrcType)
{
    PASCAL_NONNULL(Emitter);
    PASCAL_NONNULL(Dst);
#define LOAD_OP(LongMode, ExtendType, DstSize, SrcSize, Base)\
    WriteOp16(Emitter, PVM_OP(LD ## ExtendType ## DstSize ## SrcSize ## LongMode, Dst->ID, Base))
#define LOAD_INTO(LongMode, ExtendType, DstSize, Base) do {\
    switch (SrcType) {\
    CASE_PTR64(:)\
    case TYPE_I64:\
    case TYPE_U64: LOAD_OP(LongMode, , DstSize,, Base); break;\
    case TYPE_I32:\
    CASE_PTR32(:)\
    case TYPE_U32: LOAD_OP(LongMode, ,DstSize,, Base); break;\
    case TYPE_I16:\
    case TYPE_U16: LOAD_OP(LongMode, ExtendType, DstSize, _16, Base); break;\
    case TYPE_I8:\
    case TYPE_BOOLEAN:\
    case TYPE_U8: LOAD_OP(LongMode, ExtendType, DstSize, _8, Base); break;\
    default: PASCAL_UNREACHABLE("Unhandled case for src in %s: %s", __func__, IntegralTypeToStr(SrcType)); break;\
    }\
} while (0)
#define LOAD(Base, LongMode) do {\
    switch (DstType) {\
    case TYPE_I64: LOAD_INTO(LongMode, SEX, 64, Base); break;\
    CASE_PTR64(:)\
    case TYPE_U64: LOAD_INTO(LongMode, ZEX, 64, Base); break;\
    case TYPE_I8:\
    case TYPE_I16:\
    case TYPE_I32: LOAD_INTO(LongMode, SEX, 32, Base); break;\
    CASE_PTR32(:)\
    case TYPE_BOOLEAN:\
    case TYPE_U8:\
    case TYPE_U16:\
    case TYPE_U32: LOAD_INTO(LongMode, ZEX, 32, Base); break;\
    CASE_OBJREF32(:)\
    CASE_OBJREF64(:) {\
        WriteOp16(Emitter, PVM_OP(LEA ## LongMode, Dst->ID, Base));\
    } break;\
    default: PASCAL_UNREACHABLE("Unhandled case in %s: %s", __func__, IntegralTypeToStr(DstType)); break;\
    }\
} while (0)

    if (IN_I16(Offset))
    {
        LOAD(Base.ID, );
        WriteOp16(Emitter, Offset);
    }
    else
    {
        LOAD(Base.ID, L);
        WriteOp32(Emitter, Offset, (U32)Offset >> 16);
    }

#undef LOAD
#undef LOAD_INTO
#undef LOAD_OP
}



static void DerefIntoReg(PVMEmitter *Emitter, 
        VarRegister *Dst, IntegralType DstType, VarMemory Src, IntegralType SrcType)
{
    PASCAL_NONNULL(Emitter);
    PASCAL_NONNULL(Dst);
    VarRegister Base = Src.RegPtr;
    if (IntegralTypeIsFloat(DstType))
    {
        DerefIntoFloatReg(Emitter,
                Dst, DstType,
                Base, Src.Location, SrcType
        );
    }
    else
    {
        DerefIntoIntReg(Emitter,
                Dst, DstType,
                Base, Src.Location, SrcType
        );
    }
}


static void MoveLiteralIntoReg(PVMEmitter *Emitter, 
        VarRegister *Rd, IntegralType DstType, const VarLiteral *Literal)
{
    if (!Emitter->ShouldEmit)
        return;
    if (TYPE_POINTER == DstType || IntegralTypeIsInteger(DstType))
    {
        ChunkWriteMovImm(PVMCurrentChunk(Emitter), 
                Rd->ID, Literal->Int
        );
    }
    else if (DstType == TYPE_BOOLEAN)
    {
        ChunkWriteMovImm(PVMCurrentChunk(Emitter), 
                Rd->ID, Literal->Bool
        );
    }
    else if (TYPE_F64 == DstType)
    {
        VarMemory Mem = PVMEmitGlobalData(Emitter, &Literal->Flt, sizeof(F64));
        DerefIntoReg(Emitter, Rd, TYPE_F64, Mem, TYPE_F64);
    }
    else if (TYPE_F32 == DstType)
    {
        F32 Flt32 = Literal->Flt;
        VarMemory Mem = PVMEmitGlobalData(Emitter, &Flt32, sizeof(F32));
        DerefIntoReg(Emitter, Rd, DstType, Mem, TYPE_F32);
    }
    else if (TYPE_STRING == DstType)
    {
        VarMemory String = PVMEmitGlobalData(Emitter, 
                &Literal->Str, 1 + PStrGetLen(&Literal->Str)
        );
        VarLocation Location = {
            .LocationType = VAR_MEM,
            .Type = TYPE_STRING,
            .As.Memory = String,
        };
        VarLocation Tmp = {
            .LocationType = VAR_REG,
            .Type = TYPE_STRING,
            .As.Register = *Rd,
        };
        PVMEmitLoadAddr(Emitter, &Tmp, &Location);
    }
    else 
    {
        PASCAL_UNREACHABLE("Unhandled imm type: %s", IntegralTypeToStr(DstType));
    }
}



bool PVMEmitIntoReg(PVMEmitter *Emitter, VarLocation *OutTarget, const VarLocation *Src)
{
    PASCAL_NONNULL(Emitter);
    PASCAL_NONNULL(OutTarget);
    PASCAL_NONNULL(Src);

    VarLocation Tmp = *Src;
    switch (Tmp.LocationType)
    {
    case VAR_REG: break;
    case VAR_MEM:
    {
        *OutTarget = PVMAllocateRegister(Emitter, Tmp.Type);
        DerefIntoReg(Emitter, &OutTarget->As.Register, Tmp.Type, Tmp.As.Memory, Tmp.Type);
        return true;
    } break;
    case VAR_BUILTIN:
    case VAR_SUBROUTINE:
    {
        PASCAL_UNREACHABLE("TODO: emitting function pointer");
    } break;
    case VAR_FLAG:
    {
        PASCAL_ASSERT(Tmp.Type == TYPE_BOOLEAN, "Unreachable");
        *OutTarget = PVMAllocateRegister(Emitter, Tmp.Type);
        WriteOp16(Emitter, PVM_OP(GETFLAG, OutTarget->As.Register.ID, 0));
    } break;
    case VAR_LIT:
    {
        *OutTarget = PVMAllocateRegister(Emitter, Tmp.Type);
        MoveLiteralIntoReg(Emitter, &OutTarget->As.Register, Tmp.Type, &Tmp.As.Literal);
        return true;
    } break;
    case VAR_INVALID: break;
    }

    *OutTarget = Tmp;
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
    PVMEmitMov(Emitter, &Emitter->Reg.FP, &Emitter->Reg.SP);
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

VarLocation PVMAllocateRegister(PVMEmitter *Emitter, IntegralType Type)
{
    PASCAL_NONNULL(Emitter);
    PASCAL_ASSERT(Type != TYPE_INVALID, "PVMAllocateRegister: received invalid type");
    if (IntegralTypeIsFloat(Type))
    {
        for (UInt i = PVM_REG_COUNT; i < PVM_REG_COUNT*2; i++)
        {
            if (PVMRegisterIsFree(Emitter, i))
            {
                PVMMarkRegisterAsAllocated(Emitter, i);
                return (VarLocation) {
                    .LocationType = VAR_REG,
                    .Type = Type,
                    .As.Register.ID = i,
                    .As.Register.Persistent = false,
                };
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
            return (VarLocation) {
                .LocationType = VAR_REG,
                .Type = Type,
                .As.Register.ID = i,
                .As.Register.Persistent = false,
            };
        }
    }


    /* spill register */
    UInt Reg = Emitter->SpilledRegCount % PVM_REG_COUNT;
    Emitter->SpilledRegCount++;
    PVMEmitPushReg(Emitter, Reg);

    return (VarLocation) {
        .LocationType = VAR_REG,
        .Type = Type,
        .As.Register.ID = Reg,
        .As.Register.Persistent = false,
    };
}

void PVMFreeRegister(PVMEmitter *Emitter, VarRegister Reg)
{
    PASCAL_NONNULL(Emitter);
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

    VarLocation Target;
    bool IsOwning = PVMEmitIntoReg(Emitter, &Target, Condition);
    U32 Location = WriteOp32(Emitter, PVM_B(EZ, Target.As.Register.ID, 0), 0);
    if (IsOwning)
    {
        PVMFreeRegister(Emitter, Target.As.Register);
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

    VarLocation Target;
    bool IsOwning = PVMEmitIntoReg(Emitter, &Target, Condition);
    U32 Location = WriteOp32(Emitter, PVM_B(NZ, Target.As.Register.ID, 0), 0);
    if (IsOwning)
    {
        PVMFreeRegister(Emitter, Target.As.Register);
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
    return WriteOp32(Emitter, PVM_BR(Offset >> 16), Offset & 0xFFFF);
}

void PVMPatchBranch(PVMEmitter *Emitter, U32 From, U32 To, PVMBranchType Type)
{
    PASCAL_NONNULL(Emitter);
    if (!Emitter->ShouldEmit) 
        return;

    /* size of the branch instruction is 2 16 opcode word */
    U32 Offset = To - From - PVM_BRANCH_INS_SIZE;
    PVMCurrentChunk(Emitter)->Code[From] = 
        (PVMCurrentChunk(Emitter)->Code[From] & ~(U32)Type)
        | ((U32)Type & (Offset >> 16));
    PVMCurrentChunk(Emitter)->Code[From + 1] = (U16)Offset;
}

void PVMPatchBranchToCurrent(PVMEmitter *Emitter, U32 From, PVMBranchType Type)
{
    PASCAL_NONNULL(Emitter);
    PVMPatchBranch(Emitter, From, PVMCurrentChunk(Emitter)->Count, Type);
}



/* move and load */
/* TODO: mov is bloated just like x86, make it not? */
void PVMEmitMov(PVMEmitter *Emitter, VarLocation *Dst, const VarLocation *Src)
{
    PASCAL_NONNULL(Emitter);
    PASCAL_NONNULL(Dst);
    PASCAL_NONNULL(Src);
    if (Src->LocationType != VAR_MEM)
    {
        PASCAL_ASSERT(Dst->Type == Src->Type, "Dst must equal Src type");
    }

    switch (Dst->LocationType)
    {
    case VAR_LIT:
    case VAR_INVALID:
    case VAR_BUILTIN:
    case VAR_SUBROUTINE:
    {
        PASCAL_UNREACHABLE("Are you crazy???");
    } break;

    case VAR_FLAG:
    {
        if (Src->LocationType == VAR_FLAG)
            return;

        VarLocation Rs;
        bool Owning = PVMEmitIntoReg(Emitter, &Rs, Src);
        PASCAL_ASSERT(Src->Type == TYPE_BOOLEAN, "Src must be boolean");
        WriteOp16(Emitter, PVM_OP(SETFLAG, Src->As.Register.ID, 0));
        if (Owning)
            PVMFreeRegister(Emitter, Rs.As.Register);
    } break;
    case VAR_REG:
    {
        VarRegister *Rd = &Dst->As.Register;
        IntegralType RdType = Dst->Type;
        switch (Src->LocationType)
        {
        case VAR_INVALID:
        case VAR_SUBROUTINE:
        case VAR_BUILTIN:
        {
            PASCAL_UNREACHABLE("No");
        } break;

        case VAR_LIT: MoveLiteralIntoReg(Emitter, Rd, RdType, &Src->As.Literal); break;
        case VAR_FLAG: WriteOp16(Emitter, PVM_OP(GETFLAG, Rd->ID, 0)); break;
        case VAR_REG: TransferRegister(Emitter, Rd, RdType, Src->As.Register, Src->Type); break;
        case VAR_MEM: DerefIntoReg(Emitter, Rd, RdType, Src->As.Memory, Src->Type); break;
        }
    } break;
    case VAR_MEM:
    {
        VarLocation Tmp;
        bool Owning = PVMEmitIntoReg(Emitter, &Tmp, Src);
        StoreFromReg(Emitter, 
                Dst->As.Memory, Dst->Type, 
                Tmp.As.Register, Tmp.Type
        );
        if (Owning)
            PVMFreeRegister(Emitter, Tmp.As.Register);
    } break;
    }
}


void PVMEmitLoadAddr(PVMEmitter *Emitter, const VarLocation *Dst, const VarLocation *Src)
{
    PASCAL_NONNULL(Emitter);
    PASCAL_NONNULL(Dst);
    PASCAL_NONNULL(Src);
    PASCAL_ASSERT(Src->LocationType == VAR_MEM, "Src of %s must be a VAR_MEM", __func__);
    PASCAL_ASSERT(Dst->LocationType == VAR_REG, "Dst of %s must be a VAR_REG", __func__);

    UInt Base = Src->As.Memory.RegPtr.ID;
    UInt Rd = Dst->As.Register.ID;
    U32 Offset = Src->As.Memory.Location;
    if (0 == Src->As.Memory.Location)
    {
        VarRegister Rd = Dst->As.Register;
        TransferRegister(Emitter, 
                &Rd, Dst->Type, 
                Src->As.Memory.RegPtr, Src->Type
        );
    }
    else if (IN_I16(Src->As.Memory.Location))
    {
        WriteOp32(Emitter, PVM_OP(LEA, Rd, Base), Offset);
    }
    else 
    {
        WriteOp16(Emitter, PVM_OP(LEAL, Rd, Base));
        WriteOp32(Emitter, Offset, Offset >> 16);
    }
}

void PVMEmitLoadEffectiveAddr(PVMEmitter *Emitter, const VarLocation *Dst, const VarLocation *Src, I32 Offset)
{
    PASCAL_NONNULL(Emitter);
    PASCAL_NONNULL(Dst);
    PASCAL_NONNULL(Src);

    PASCAL_ASSERT(Dst->LocationType == VAR_REG, "Unreachable");
    PASCAL_ASSERT(Src->LocationType == VAR_MEM, "Unreachable");

    if (0 == Offset)
    {
        VarRegister Rd = Dst->As.Register;
        TransferRegister(Emitter, &Rd, Dst->Type, Src->As.Memory.RegPtr, Src->Type);
    }
    else if (IN_I16(Offset))
    {
        WriteOp32(Emitter, PVM_OP(LEA, Dst->As.Register.ID, Src->As.Memory.RegPtr.ID), Offset);
    }
    else
    {
        WriteOp16(Emitter, PVM_OP(LEA, Dst->As.Register.ID, Src->As.Memory.RegPtr.ID));
        WriteOp32(Emitter, Offset, (U32)Offset >> 16);
    }
}

void PVMEmitDerefPtr(PVMEmitter *Emitter, VarLocation *Dst, const VarLocation *Ptr)
{
    PASCAL_NONNULL(Emitter);
    PASCAL_NONNULL(Dst);
    PASCAL_NONNULL(Ptr);
    /* move value of Ptr into Dst */
    /* treat Dst as VarMem */
    VarLocation Rd;
    bool IsOwning = PVMEmitIntoReg(Emitter, &Rd, Dst);

    Rd.Type = TYPE_POINTER;
    PVMEmitMov(Emitter, &Rd, Ptr);
    Rd.Type = Dst->Type;

    VarMemory Mem = { .Location = 0, .RegPtr = Rd.As.Register };
    Dst->As.Memory = Mem;
    Dst->LocationType = VAR_MEM;

    if (IsOwning)
    {
        /* ownership is transfered */
    }
}

void PVMEmitStoreToPtr(PVMEmitter *Emitter, const VarLocation *Ptr, const VarLocation *Src)
{
    PASCAL_NONNULL(Emitter);
    PASCAL_NONNULL(Ptr);
    PASCAL_NONNULL(Src);
    PASCAL_ASSERT(TYPE_POINTER == Ptr->Type, "must be pointer in %s", __func__);
    VarLocation Rp, Rs;
    bool OwningRp = PVMEmitIntoReg(Emitter, &Rp, Ptr);
    bool OwningRs = PVMEmitIntoReg(Emitter, &Rs, Src);

    StoreToPtr(Emitter, Rp.As.Register, 0, Ptr->PointerTo.Var.Type, Rs.As.Register);

    if (OwningRp)
    {
        PVMFreeRegister(Emitter, Rp.As.Register);
    }
    if (OwningRs)
    {
        PVMFreeRegister(Emitter, Rs.As.Register);
    }
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

void PVMEmitAddImm(PVMEmitter *Emitter, VarLocation *Dst, I64 Imm)
{
    PASCAL_NONNULL(Emitter);
    PASCAL_NONNULL(Dst);
    if (0 == Imm)
        return;
    VarLocation Target;
    bool IsOwning = PVMEmitIntoReg(Emitter, &Target, Dst);

    if (Dst->Type == TYPE_U64 || Dst->Type == TYPE_I64)
    {
        if (IS_SMALL_IMM(Imm))
            WriteOp16(Emitter, PVM_OP(ADDQI64, Target.As.Register.ID, Imm));
        else if (IN_I16(Imm))
            WriteOp32(Emitter, PVM_OP(ADDI64, Target.As.Register.ID, IMMTYPE_I16), Imm);
        else if (IN_U16(Imm))
            WriteOp32(Emitter, PVM_OP(ADDI64, Target.As.Register.ID, IMMTYPE_U16), Imm);
        else if (IN_I32(Imm))
            WriteOp32(Emitter, PVM_OP(ADDI64, Target.As.Register.ID, IMMTYPE_I32), Imm);
        else if (IN_U32(Imm))
            WriteOp32(Emitter, PVM_OP(ADDI64, Target.As.Register.ID, IMMTYPE_U32), Imm);
        else if (IN_I48(Imm))
            WriteOp32(Emitter, PVM_OP(ADDI64, Target.As.Register.ID, IMMTYPE_I48), Imm);
        else if (IN_U48(Imm))
            WriteOp32(Emitter, PVM_OP(ADDI64, Target.As.Register.ID, IMMTYPE_U48), Imm);
        else
            WriteOp32(Emitter, PVM_OP(ADDI64, Target.As.Register.ID, IMMTYPE_U64), Imm);
    }
    else if (IS_SMALL_IMM(Imm))
        WriteOp16(Emitter, PVM_OP(ADDQI, Target.As.Register.ID, Imm));
    else if (IN_I16(Imm))
        WriteOp32(Emitter, PVM_OP(ADDI, Target.As.Register.ID, IMMTYPE_I16), Imm);
    else if (IN_U16(Imm))
        WriteOp32(Emitter, PVM_OP(ADDI, Target.As.Register.ID, IMMTYPE_U16), Imm);
    else if (IN_I32(Imm))
        WriteOp32(Emitter, PVM_OP(ADDI, Target.As.Register.ID, IMMTYPE_I32), Imm);
    else if (IN_U32(Imm))
        WriteOp32(Emitter, PVM_OP(ADDI, Target.As.Register.ID, IMMTYPE_U32), Imm);
    else if (IN_I48(Imm))
        WriteOp32(Emitter, PVM_OP(ADDI, Target.As.Register.ID, IMMTYPE_I48), Imm);
    else if (IN_U48(Imm))
        WriteOp32(Emitter, PVM_OP(ADDI, Target.As.Register.ID, IMMTYPE_U48), Imm);
    else
        WriteOp32(Emitter, PVM_OP(ADDI, Target.As.Register.ID, IMMTYPE_U64), Imm);

    if (IsOwning)
    {
        PVMEmitMov(Emitter, Dst, &Target);
        PVMFreeRegister(Emitter, Target.As.Register);
    }
}





#define DEFINE_INTEGER_BINARY_OP(FnName, Mnemonic)\
void FnName (PVMEmitter *Emitter, VarLocation *Dst, const VarLocation *Src) {\
    PASCAL_NONNULL(Emitter);\
    PASCAL_NONNULL(Dst);\
    PASCAL_NONNULL(Src);\
    PASCAL_ASSERT(Dst->Type == Src->Type, "Dst and Src type must be the same");\
    PASCAL_ASSERT(Dst->LocationType == VAR_REG, "Dst must be register");\
    VarLocation Rs;\
    bool OwningRs = PVMEmitIntoReg(Emitter, &Rs, Src);\
    if (TYPE_U64 == Dst->Type || TYPE_I64 == Dst->Type) {\
        WriteOp16(Emitter, PVM_OP( Mnemonic ## 64, Dst->As.Register.ID, Rs.As.Register.ID));\
    } else {\
        WriteOp16(Emitter, PVM_OP( Mnemonic , Dst->As.Register.ID, Rs.As.Register.ID));\
    }\
    if (OwningRs) {\
        PVMFreeRegister(Emitter, Rs.As.Register);\
    }\
}\
void FnName (PVMEmitter *Emitter, VarLocation *Dst, const VarLocation *Src)

#define DEFINE_GENERIC_BINARY_OP(FnName, Mnemonic)\
void FnName (PVMEmitter *Emitter, VarLocation *Dst, const VarLocation *Src) {\
    PASCAL_NONNULL(Emitter);\
    PASCAL_NONNULL(Dst);\
    PASCAL_NONNULL(Src);\
    PASCAL_ASSERT(Dst->Type == Src->Type, "Dst and Src type must be the same in %s", __func__);\
    PASCAL_ASSERT(Dst->LocationType == VAR_REG, "Dst must be register");\
    VarLocation Rs;\
    bool OwningRs = PVMEmitIntoReg(Emitter, &Rs, Src);\
    if (TYPE_U64 == Dst->Type || TYPE_I64 == Dst->Type) {\
        WriteOp16(Emitter, PVM_OP( Mnemonic ## 64, Dst->As.Register.ID, Rs.As.Register.ID));\
    } else if (TYPE_F64 == Dst->Type || TYPE_F64 == Src->Type) {\
        WriteOp16(Emitter, PVM_OP( F ## Mnemonic ## 64, Dst->As.Register.ID, Rs.As.Register.ID));\
    } else if (TYPE_F32 == Dst->Type || TYPE_F32 == Dst->Type) {\
        WriteOp16(Emitter, PVM_OP( F ## Mnemonic, Dst->As.Register.ID, Rs.As.Register.ID));\
    } else {\
        WriteOp16(Emitter, PVM_OP( Mnemonic , Dst->As.Register.ID, Rs.As.Register.ID));\
    }\
    if (OwningRs) {\
        PVMFreeRegister(Emitter, Rs.As.Register);\
    }\
}\
void FnName (PVMEmitter *Emitter, VarLocation *Dst, const VarLocation *Src)




void PVMEmitAdd(PVMEmitter *Emitter, VarLocation *Dst, const VarLocation *Src) 
{
    PASCAL_NONNULL(Emitter);
    PASCAL_NONNULL(Dst);
    PASCAL_NONNULL(Src);

    PASCAL_ASSERT(Dst->Type == Src->Type, "Dst and Src type must be the same");
    PASCAL_ASSERT(Dst->LocationType == VAR_REG, "Dst must be a register");

    if (IntegralTypeIsInteger(Dst->Type) 
    && IntegralTypeIsInteger(Src->Type) 
    && VAR_LIT == Src->LocationType)
    {
        PVMEmitAddImm(Emitter, Dst, Src->As.Literal.Int);
        return;
    }

    VarLocation Rs;
    bool OwningRs = PVMEmitIntoReg(Emitter, &Rs, Src);
    if (TYPE_F64 == Dst->Type || TYPE_F64 == Src->Type) 
    {
        WriteOp16(Emitter, PVM_OP(FADD64, Dst->As.Register.ID, Rs.As.Register.ID));
    } 
    else if (TYPE_F32 == Dst->Type || TYPE_F32 == Dst->Type) 
    {
        WriteOp16(Emitter, PVM_OP(FADD, Dst->As.Register.ID, Rs.As.Register.ID));
    }
    else if (TYPE_I64 == Dst->Type || TYPE_U64 == Dst->Type)
    {
        WriteOp16(Emitter, PVM_OP(ADD64, Dst->As.Register.ID, Rs.As.Register.ID));
    }
    else if (TYPE_STRING == Dst->Type)
    {
        PASCAL_ASSERT(TYPE_STRING == Rs.Type, "Unreachable in %s", __func__);
        WriteOp16(Emitter, PVM_OP(SADD, Dst->As.Register.ID, Rs.As.Register.ID));
    }
    else
    {
        WriteOp16(Emitter, PVM_OP(ADD, Dst->As.Register.ID, Rs.As.Register.ID));
    }

    if (OwningRs) 
        PVMFreeRegister(Emitter, Rs.As.Register);
}

void PVMEmitSub(PVMEmitter *Emitter, VarLocation *Dst, const VarLocation *Src) 
{
    PASCAL_NONNULL(Emitter);
    PASCAL_NONNULL(Dst);
    PASCAL_NONNULL(Src);

    PASCAL_ASSERT(Dst->Type == Src->Type, "Dst and Src type must be the same in %s", __func__);
    PASCAL_ASSERT(Dst->LocationType == VAR_REG, "Dst must be a register");

    if (IntegralTypeIsInteger(Dst->Type) 
    && IntegralTypeIsInteger(Src->Type) 
    && VAR_LIT == Src->LocationType)
    {
        PVMEmitAddImm(Emitter, Dst, 0 - Src->As.Literal.Int);
        return;
    }

    VarLocation Rs;
    bool OwningRs = PVMEmitIntoReg(Emitter, &Rs, Src);
    if (TYPE_F64 == Dst->Type || TYPE_F64 == Src->Type) 
    {
        WriteOp16(Emitter, PVM_OP(FSUB64, Dst->As.Register.ID, Rs.As.Register.ID));
    } 
    else if (TYPE_F32 == Dst->Type || TYPE_F32 == Dst->Type) 
    {
        WriteOp16(Emitter, PVM_OP(FSUB, Dst->As.Register.ID, Rs.As.Register.ID));
    }
    else if (TYPE_I64 == Dst->Type || TYPE_U64 == Dst->Type)
    {
        WriteOp16(Emitter, PVM_OP(SUB64, Dst->As.Register.ID, Rs.As.Register.ID));
    }
    else
    {
        WriteOp16(Emitter, PVM_OP(SUB, Dst->As.Register.ID, Rs.As.Register.ID));
    }
    if (OwningRs) 
        PVMFreeRegister(Emitter, Rs.As.Register);
}



#define EMIT_POW2_OP(Emitter, SlowPathOp, FastPathOp, Bits, VarLoc_Dst, VarLoc_Src)\
do {\
    switch (Src->As.Literal.Int) {\
    case -1: WriteOp16(Emitter, PVM_OP(NEG ## Bits, (VarLoc_Dst).As.Register.ID, (VarLoc_Dst).As.Register.ID)); break;\
    case 1: break;\
    case 2: WriteOp16(Emitter, PVM_OP(FastPathOp ## Bits, (VarLoc_Dst).As.Register.ID, 1)); break;\
    case 4: WriteOp16(Emitter, PVM_OP(FastPathOp ## Bits, (VarLoc_Dst).As.Register.ID, 2)); break;\
    case 8: WriteOp16(Emitter, PVM_OP(FastPathOp ## Bits, (VarLoc_Dst).As.Register.ID, 3)); break;\
    case 16: WriteOp16(Emitter, PVM_OP(FastPathOp ## Bits, (VarLoc_Dst).As.Register.ID, 4)); break;\
    default: {\
        VarLocation Rs;\
        bool OwningRs = PVMEmitIntoReg(Emitter, &Rs, &(VarLoc_Src));\
        WriteOp16(Emitter, PVM_OP(SlowPathOp ## Bits, (VarLoc_Dst).As.Register.ID, Rs.As.Register.ID));\
        if (OwningRs) {\
            PVMFreeRegister(Emitter, Rs.As.Register);\
        }\
    } break;\
    }\
} while (0)




void PVMEmitIMul(PVMEmitter *Emitter, VarLocation *Dst, const VarLocation *Src)
{
    PASCAL_NONNULL(Emitter);
    PASCAL_NONNULL(Dst);
    PASCAL_NONNULL(Src);

    PASCAL_ASSERT(Dst->Type == Src->Type, "Dst and Src type must be the same");
    PASCAL_ASSERT(Dst->LocationType == VAR_REG, "Dst must be a register");

    if (VAR_LIT == Src->LocationType)
    {
        if (TYPE_I64 == Src->Type)
        {
            EMIT_POW2_OP(Emitter, IMUL, QSHL, 64, *Dst, *Src);
        }
        else
        {
            EMIT_POW2_OP(Emitter, IMUL, QSHL, , *Dst, *Src);
        }
    }
    else
    {
        VarLocation Rs;
        bool OwningRs = PVMEmitIntoReg(Emitter, &Rs, Src);
        if (TYPE_I64 == Dst->Type)
        {
            WriteOp16(Emitter, PVM_OP(IMUL64, Dst->As.Register.ID, Rs.As.Register.ID));
        }
        else
        {
            WriteOp16(Emitter, PVM_OP(IMUL, Dst->As.Register.ID, Rs.As.Register.ID));
        }
        if (OwningRs) 
        {
            PVMFreeRegister(Emitter, Rs.As.Register);
        }
    }
}

void PVMEmitIDiv(PVMEmitter *Emitter, VarLocation *Dst, const VarLocation *Src)
{
    PASCAL_NONNULL(Emitter);
    PASCAL_NONNULL(Dst);
    PASCAL_NONNULL(Src);

    PASCAL_ASSERT(Dst->Type == Src->Type, "Dst and Src type must be the same");
    PASCAL_ASSERT(Dst->LocationType == VAR_REG, "Dst must be a register");

    if (VAR_LIT == Src->LocationType)
    {
        if (TYPE_I64 == Src->Type)
        {
            EMIT_POW2_OP(Emitter, IDIV, QASR, 64, *Dst, *Src);
        }
        else
        {
            EMIT_POW2_OP(Emitter, IDIV, QASR, , *Dst, *Src);
        }
    }
    else
    {
        VarLocation Rs;
        bool OwningRs = PVMEmitIntoReg(Emitter, &Rs, Src);
        if (TYPE_I64 == Dst->Type)
        {
            WriteOp16(Emitter, PVM_OP(IDIV64, Dst->As.Register.ID, Rs.As.Register.ID));
        }
        else
        {
            WriteOp16(Emitter, PVM_OP(IDIV, Dst->As.Register.ID, Rs.As.Register.ID));
        }
        if (OwningRs) 
        {
            PVMFreeRegister(Emitter, Rs.As.Register);
        }
    }
}



void PVMEmitMul(PVMEmitter *Emitter, VarLocation *Dst, const VarLocation *Src)
{
    PASCAL_NONNULL(Emitter);
    PASCAL_NONNULL(Dst);
    PASCAL_NONNULL(Src);
    PASCAL_ASSERT(Dst->Type == Src->Type, "Dst and Src type must be the same");
    PASCAL_ASSERT(Dst->LocationType == VAR_REG, "Dst must be in a register");

    if (IntegralTypeIsFloat(Dst->Type))
    {
        VarLocation Rs;
        bool OwningRs = PVMEmitIntoReg(Emitter, &Rs, Src);
        if (TYPE_F64 == Dst->Type)
        {
            WriteOp16(Emitter, PVM_OP(FMUL64, Dst->As.Register.ID, Rs.As.Register.ID));
        }
        else
        {
            WriteOp16(Emitter, PVM_OP(FMUL, Dst->As.Register.ID, Rs.As.Register.ID));
        }

        if (OwningRs) 
        {
            PVMFreeRegister(Emitter, Rs.As.Register);
        }
    }
    else
    {
        if (IntegralTypeIsSigned(Dst->Type))
        {
            PVMEmitIMul(Emitter, Dst, Src);
        }
        else if (TYPE_U64 == Dst->Type)
        {
            EMIT_POW2_OP(Emitter, MUL, QSHL, 64, *Dst, *Src);
        }
        else
        {
            EMIT_POW2_OP(Emitter, MUL, QSHL, , *Dst, *Src);
        }
    }
}

void PVMEmitDiv(PVMEmitter *Emitter, VarLocation *Dst, const VarLocation *Src)
{
    PASCAL_NONNULL(Emitter);
    PASCAL_NONNULL(Dst);
    PASCAL_NONNULL(Src);
    PASCAL_ASSERT(Dst->Type == Src->Type, "Dst and Src type must be the same");
    PASCAL_ASSERT(Dst->LocationType == VAR_REG, "Dst must be in a register");

    if (IntegralTypeIsFloat(Dst->Type))
    {
        VarLocation Rs;
        bool OwningRs = PVMEmitIntoReg(Emitter, &Rs, Src);
        if (TYPE_F64 == Dst->Type)
        {
            WriteOp16(Emitter, PVM_OP(FDIV64, Dst->As.Register.ID, Rs.As.Register.ID));
        }
        else
        {
            WriteOp16(Emitter, PVM_OP(FDIV, Dst->As.Register.ID, Rs.As.Register.ID));
        }

        if (OwningRs) 
        {
            PVMFreeRegister(Emitter, Rs.As.Register);
        }
    }
    else
    {
        if (IntegralTypeIsSigned(Dst->Type))
        {
            PVMEmitIDiv(Emitter, Dst, Src);
        }
        else if (TYPE_U64 == Dst->Type)
        {
            EMIT_POW2_OP(Emitter, DIV, QSHR, 64, *Dst, *Src);
        }
        else
        {
            EMIT_POW2_OP(Emitter, DIV, QSHR, , *Dst, *Src);
        }
    }
}


void PVMEmitNot(PVMEmitter *Emitter, VarLocation *Dst, const VarLocation *Src)
{
    PASCAL_NONNULL(Emitter);
    PASCAL_NONNULL(Dst);
    PASCAL_NONNULL(Src);
    PASCAL_ASSERT(Dst->Type == Src->Type, "Dst and Src type must be the same");
    if (Dst->Type == TYPE_BOOLEAN)
    {
        if (VAR_FLAG == Dst->LocationType && VAR_FLAG == Src->LocationType)
        {
            WriteOp16(Emitter, PVM_OP(NEGFLAG, 0, 0));
            return;
        }
        VarLocation Rs;
        bool Owning = PVMEmitIntoReg(Emitter, &Rs, Src);
        if (VAR_REG == Dst->LocationType)
        {
            WriteOp16(Emitter, PVM_OP(SETEZ, Dst->As.Register.ID, Rs.As.Register.ID));
        }
        else if (VAR_FLAG == Dst->LocationType)
        {
            WriteOp16(Emitter, PVM_OP(SETNFLAG, 0, Rs.As.Register.ID));
        }
        if (Owning)
            PVMFreeRegister(Emitter, Rs.As.Register);
    }
    else if (IntegralTypeIsInteger(Dst->Type))
    {
        PASCAL_ASSERT(Dst->LocationType == VAR_REG, "Dst can only be a register");
        VarLocation Rs;
        bool Owning = PVMEmitIntoReg(Emitter, &Rs, Src);
        if (TYPE_U64 == Dst->Type || TYPE_I64 == Dst->Type)
        {
            WriteOp16(Emitter, PVM_OP(NOT64, Dst->As.Register.ID, Rs.As.Register.ID));
        }
        else
        {
            WriteOp16(Emitter, PVM_OP(NOT, Dst->As.Register.ID, Rs.As.Register.ID));
        }
        if (Owning)
            PVMFreeRegister(Emitter, Rs.As.Register);
    }
    else
    {
        PASCAL_UNREACHABLE("Invalid type");
    }
}






/* kill me */
DEFINE_GENERIC_BINARY_OP(PVMEmitNeg, NEG);

//DEFINE_INTEGER_BINARY_OP(PVMEmitNot, NOT);
DEFINE_INTEGER_BINARY_OP(PVMEmitAnd, AND);
DEFINE_INTEGER_BINARY_OP(PVMEmitOr, OR);
DEFINE_INTEGER_BINARY_OP(PVMEmitXor, XOR);
DEFINE_INTEGER_BINARY_OP(PVMEmitMod, MOD);
DEFINE_INTEGER_BINARY_OP(PVMEmitShl, VSHL);
DEFINE_INTEGER_BINARY_OP(PVMEmitShr, VSHR);
DEFINE_INTEGER_BINARY_OP(PVMEmitAsr, VASR);


#undef EMIT_POW2_OP
#undef DEFINE_INTEGER_BINARY_OP
#undef DEFINE_GENERIC_BINARY_OP





VarLocation PVMEmitSetFlag(PVMEmitter *Emitter, TokenType Op, const VarLocation *Left, const VarLocation *Right)
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
    PASCAL_ASSERT(Left->Type == Right->Type, "Left and Right must have the same type");
    if (!Emitter->ShouldEmit) 
        return (VarLocation) {.Type = TYPE_BOOLEAN, .LocationType = VAR_LIT, .As.Literal.Bool = false};

    VarLocation Rd, Rs;
    bool OwningRd = PVMEmitIntoReg(Emitter, &Rd, Left);
    bool OwningRs = PVMEmitIntoReg(Emitter, &Rs, Right);
    UInt RdID = Rd.As.Register.ID;
    UInt RsID = Rs.As.Register.ID;

    if (TYPE_STRING == Left->Type)
    {
        if (UINTPTR_MAX == UINT32_MAX)
            SSET();
        else 
            SSET();
    }
    else if (IntegralTypeIsFloat(Left->Type))
    {
        if (TYPE_F64 == Left->Type)
            FSET(64);
        else if (TYPE_F32 == Left->Type)
            FSET();
    }
    else if (IntegralTypeIsInteger(Left->Type))
    {
        if (TYPE_I64 == Left->Type) 
            SET(I, 64);
        else if (TYPE_U64 == Left->Type)
            SET(, 64); 
        else if (IntegralTypeIsSigned(Left->Type))
            SET(I, );
        else 
            SET(,);
    }
    else if (TYPE_POINTER == Left->Type)
    {
        if (UINTPTR_MAX == UINT32_MAX)
            SET(,);
        else 
            SET(,64);
    }
    else 
    {
        PASCAL_UNREACHABLE("EmitSetCC: %s %s %s\n", 
                IntegralTypeToStr(Left->Type), 
                TokenTypeToStr(Op),
                IntegralTypeToStr(Right->Type)
        );
    }

    if (OwningRd)
    {
        PVMFreeRegister(Emitter, Rd.As.Register);
    }
    if (OwningRs) 
    {
        PVMFreeRegister(Emitter, Rs.As.Register);
    }
    return Emitter->Reg.Flag;
#undef SET
#undef FSET
#undef SSET
}









/* subroutine arguments */
VarLocation PVMSetParam(PVMEmitter *Emitter, UInt ArgNumber, IntegralType ParamType, U32 ParamSize, I32 *Base)
{
    PASCAL_NONNULL(Emitter);
    PASCAL_NONNULL(Base);
    if (ArgNumber < PVM_ARGREG_COUNT)
    {
        VarLocation RegParam = Emitter->ArgReg[ArgNumber];
        RegParam.Type = ParamType;
        if (IntegralTypeIsFloat(ParamType))
            RegParam.As.Register.ID = ArgNumber + PVM_REG_COUNT;
        return RegParam;
    }

    I32 ParamOffset = *Base - ParamSize;
    VarLocation Location = {
        .Type = ParamType,
        .LocationType = VAR_MEM,
        .Size = ParamSize,
        .As.Memory = {
            .RegPtr = Emitter->Reg.FP.As.Register,
            .Location = ParamOffset,
        },
    };
    *Base = ParamOffset;
    return Location;
}

I32 PVMStartArg(PVMEmitter *Emitter, U32 ArgSize)
{
    PVMEmitStackAllocation(Emitter, ArgSize);
    return Emitter->StackSpace;
}


VarLocation PVMSetArg(PVMEmitter *Emitter, UInt ArgNumber, IntegralType ArgType, U32 ArgSize, I32 *Base)
{
    PASCAL_NONNULL(Emitter);
    PASCAL_NONNULL(Base);

    if (ArgNumber < PVM_ARGREG_COUNT)
    {
        VarLocation Arg = Emitter->ArgReg[ArgNumber];
        Arg.Type = ArgType;
        if (IntegralTypeIsFloat(ArgType))
            Arg.As.Register.ID = ArgNumber + PVM_REG_COUNT;
        return Arg;
    }

    *Base -= ArgSize;
    I32 ArgOffset = *Base;
    VarLocation Mem = {
        .Type = ArgType,
        .LocationType = VAR_MEM,
        .Size = ArgSize,
        .As.Memory = {
            .RegPtr = Emitter->Reg.FP.As.Register,
            .Location = ArgOffset,
        },
    };
    return Mem;
}


void PVMMarkArgAsOccupied(PVMEmitter *Emitter, VarLocation *Arg)
{
    PASCAL_NONNULL(Emitter);
    if (VAR_REG == Arg->LocationType)
    {
        PVMMarkRegisterAsAllocated(Emitter, Arg->As.Register.ID);
    }
}


VarLocation PVMSetReturnType(PVMEmitter *Emitter, IntegralType ReturnType)
{
    PASCAL_NONNULL(Emitter);
    VarLocation ReturnValue = Emitter->ReturnValue;
    PASCAL_ASSERT(ReturnValue.LocationType == VAR_REG, "??");

    if (IntegralTypeIsFloat(ReturnType))
    {
        ReturnValue.As.Register.ID = Emitter->ReturnValue.As.Register.ID + PVM_REG_COUNT;
    }
    PVMMarkRegisterAsAllocated(Emitter, ReturnValue.As.Register.ID);
    ReturnValue.Type = ReturnType;
    return ReturnValue;
}


void PVMEmitPushMultiple(PVMEmitter *Emitter, int Count, ...)
{
    PASCAL_NONNULL(Emitter);
    va_list Args;
    va_start(Args, Count);
    for (int i = 0; i < Count; i++)
    {
        VarLocation Reg;
        VarLocation *Location = va_arg(Args, VarLocation *);
        PASCAL_NONNULL(Location);

        bool Owning = PVMEmitIntoReg(Emitter, &Reg, Location);

        PVMEmitPushReg(Emitter, Reg.As.Register.ID);

        if (Owning)
        {
            PVMFreeRegister(Emitter, Reg.As.Register);
        }
    }
    va_end(Args);
}





/* stack allocation */
U32 PVMGetStackOffset(PVMEmitter *Emitter)
{
    return Emitter->StackSpace;
}

void PVMEmitStackAllocation(PVMEmitter *Emitter, I32 Size) 
{
    PASCAL_NONNULL(Emitter);
    U64 Aligned = (I64)Size;
    U64 Extra = sizeof(PVMGPR);
    if (Size < 0)
        Extra = 0;

    if (Aligned % sizeof(PVMGPR))
        Aligned = (Aligned + Extra) & ~(sizeof(PVMGPR) - 1);
    PVMEmitAddImm(Emitter, &Emitter->Reg.SP, Aligned);
    Emitter->StackSpace += Aligned;
}






/* global instructions */
U32 PVMGetGlobalOffset(PVMEmitter *Emitter)
{
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
    PASCAL_ASSERT(Global->Type == Type, "Types must be equal");
    PASCAL_ASSERT(Global->LocationType == VAR_MEM, "Invalid location");
    PASCAL_ASSERT(Location < Chunk->Global.Count, "Invalid location");
    PASCAL_ASSERT(Location + Size <= Chunk->Global.Count, "Invalid size");

    switch (Type)
    {
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
    SaveRegInfo Info = {
        .Regs = Emitter->Reglist & ~(((U16)1 << ReturnRegID) | EMPTY_REGLIST),
    };
    Info.Size = BitCount(Info.Regs) * sizeof(PVMGPR);

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
        WriteOp16(Emitter, PVM_REGLIST(FPSHH, Info.Regs >> 26));
    }
    Emitter->StackSpace += BitCount(Info.Regs) * sizeof(PVMGPR);
    return Info;
}


U32 PVMEmitCall(PVMEmitter *Emitter, const VarSubroutine *Callee)
{
    PASCAL_NONNULL(Emitter);
    PASCAL_NONNULL(Callee);

    U32 CurrentLocation = PVMCurrentChunk(Emitter)->Count;
    U32 Location = Callee->Location - CurrentLocation - PVM_BRANCH_INS_SIZE;
    WriteOp32(Emitter, PVM_BSR(Location >> 16), Location & 0xFFFF);
    return CurrentLocation;
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
    PVMMarkRegisterAsAllocated(Emitter, ReturnRegID);
}




/* exit/return */
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




