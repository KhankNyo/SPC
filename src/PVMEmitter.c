
#include <stdarg.h>
#include "PVM/Isa.h"
#include "PVMEmitter.h"



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
        .SavedRegisters = { 0 },
        .NumSavelist = 0,
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
        },
        .ReturnValue = {
            .LocationType = VAR_REG,
            .Type = TYPE_INVALID,
            .As.Register.ID = PVM_RETREG,
        },
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

    VarRegister Base = { .ID = Dst.RegPtr };
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
    VarRegister Base = { .ID = Src.RegPtr };
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

static bool PVMEmitIntoReg(PVMEmitter *Emitter, VarLocation *OutTarget, const VarLocation *Src)
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
    case VAR_SUBROUTINE:
    {
        PASCAL_UNREACHABLE("TODO: FnPtr in EmitIntoReg()");
    } break;
    case VAR_LIT:
    {
        *OutTarget = PVMAllocateRegister(Emitter, Tmp.Type);
        if (TYPE_POINTER == Tmp.Type || IntegralTypeIsInteger(Tmp.Type))
        {
            ChunkWriteMovImm(PVMCurrentChunk(Emitter), 
                    OutTarget->As.Register.ID, 
                    Tmp.As.Literal.Int
            );
        }
        else if (Tmp.Type == TYPE_BOOLEAN)
        {
            ChunkWriteMovImm(PVMCurrentChunk(Emitter), 
                    OutTarget->As.Register.ID, 
                    Tmp.As.Literal.Bool
            );
        }
        else if (TYPE_F64 == Tmp.Type)
        {
            VarMemory Literal = PVMEmitGlobalData(Emitter, &Tmp.As.Literal.Flt, sizeof(F64));
            DerefIntoReg(Emitter, &OutTarget->As.Register, TYPE_F64, Literal, TYPE_F64);
        }
        else if (TYPE_F32 == Tmp.Type)
        {
            F32 Flt32 = Tmp.As.Literal.Flt;
            VarMemory Literal = PVMEmitGlobalData(Emitter, &Flt32, sizeof(F32));
            DerefIntoReg(Emitter, &OutTarget->As.Register, TYPE_F32, Literal, TYPE_F32);
        }
        else if (TYPE_STRING == Tmp.Type)
        {
            VarMemory String = PVMEmitGlobalData(Emitter, 
                    &Tmp.As.Literal.Str, 
                    1 + PStrGetLen(&Tmp.As.Literal.Str)
            );
            VarLocation Location = {
                .LocationType = VAR_MEM,
                .Type = TYPE_STRING,
                .As.Memory = String,
            };
            PVMEmitLoadAddr(Emitter, OutTarget, &Location);
        }
        else 
        {
            PASCAL_UNREACHABLE("Unhandled imm type: %s in PVMEmitIntoReg()", IntegralTypeToStr(Tmp.Type));
        }
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




void PVMEmitterBeginScope(PVMEmitter *Emitter)
{
    PASCAL_NONNULL(Emitter);
    Emitter->SavedRegisters[Emitter->NumSavelist++] = Emitter->Reglist;
}

void PVMEmitSaveFrame(PVMEmitter *Emitter)
{
    PASCAL_NONNULL(Emitter);
    TransferRegister(Emitter, 
            &Emitter->Reg.FP.As.Register, TYPE_POINTER, 
            Emitter->Reg.SP.As.Register, TYPE_POINTER
    );
}

void PVMEmitterEndScope(PVMEmitter *Emitter)
{
    PASCAL_NONNULL(Emitter);
    PASCAL_ASSERT(Emitter->NumSavelist > 0, "Unreachable");
    Emitter->Reglist = Emitter->SavedRegisters[--Emitter->NumSavelist];
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
    VarLocation Target;
    bool IsOwning = PVMEmitIntoReg(Emitter, &Target, Condition);
    U32 Location = WriteOp32(Emitter, PVM_B(NZ, Target.As.Register.ID, 0), 0);
    if (IsOwning)
    {
        PVMFreeRegister(Emitter, Target.As.Register);
    }
    return Location;
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
    if (Dst->LocationType == VAR_LIT)
    {
        /* compiler should've handled this */
        PASCAL_UNREACHABLE("cannot move into a literal: %s to %s\n", 
                IntegralTypeToStr(Dst->Type), IntegralTypeToStr(Src->Type)
        );
    }
    if (Dst->LocationType == VAR_REG)
    {
        if (VAR_MEM == Src->LocationType)
        {
            DerefIntoReg(Emitter, &Dst->As.Register, Dst->Type, Src->As.Memory, Src->Type);
            return;
        }
        else if (VAR_LIT == Src->LocationType && IntegralTypeIsInteger(Dst->Type))
        {
            ChunkWriteMovImm(PVMCurrentChunk(Emitter), Dst->As.Register.ID, Src->As.Literal.Int);
            return;
        }
    }
    /* TODO: mov imm straight into reg */


    /* make sure src is in reg */
    VarLocation Tmp = *Src;
    if (VAR_LIT == Src->LocationType && IntegralTypeIsFloat(Dst->Type))
    {
        if (IntegralTypeIsInteger(Src->Type))
        {
            if (IntegralTypeIsSigned(Src->Type))
                Tmp.As.Literal.Flt = (I64)Tmp.As.Literal.Int;
            else
                Tmp.As.Literal.Flt = Tmp.As.Literal.Int;
        }
        Tmp.Type = Dst->Type;
    }
    bool IsOwning = PVMEmitIntoReg(Emitter, &Tmp, &Tmp);

    /* move src into dst */
    switch (Dst->LocationType)
    {
    case VAR_INVALID:
    case VAR_SUBROUTINE:
    case VAR_LIT:
    {
        PASCAL_UNREACHABLE("invalid Dst in %s", __func__);
    } break;


    case VAR_REG:
    {
        TransferRegister(Emitter, 
                &Dst->As.Register, Dst->Type, 
                Tmp.As.Register, Tmp.Type
        );
    } break;
    case VAR_MEM:
    {
        StoreFromReg(Emitter, 
                Dst->As.Memory, Dst->Type, 
                Tmp.As.Register, Tmp.Type
        );
    } break;
    }

    if (IsOwning)
    {
        PVMFreeRegister(Emitter, Tmp.As.Register);
    }
}


void PVMEmitLoadAddr(PVMEmitter *Emitter, VarLocation *Dst, const VarLocation *Src)
{
    PASCAL_NONNULL(Emitter);
    PASCAL_NONNULL(Dst);
    PASCAL_NONNULL(Src);
    PASCAL_ASSERT(Src->LocationType == VAR_MEM, "Src of %s must be a VAR_MEM", __func__);
    PASCAL_ASSERT(Dst->LocationType == VAR_REG, "Dst of %s must be a VAR_REG", __func__);

    UInt Base = Src->As.Memory.RegPtr;
    UInt Rd = Dst->As.Register.ID;
    U32 Offset = Src->As.Memory.Location;
    if (IN_I16(Src->As.Memory.Location))
    {
        WriteOp32(Emitter, PVM_OP(LEA, Rd, Base), Offset);
    }
    else 
    {
        WriteOp16(Emitter, PVM_OP(LEAL, Rd, Base));
        WriteOp32(Emitter, Offset, Offset >> 16);
    }
}

void PVMEmitLoadEffectiveAddr(PVMEmitter *Emitter, VarLocation *Dst, const VarLocation *Src, I32 Offset)
{
    PASCAL_NONNULL(Emitter);
    PASCAL_NONNULL(Dst);
    PASCAL_NONNULL(Src);

    PASCAL_ASSERT(Dst->LocationType == VAR_REG, "Unreachable");
    PASCAL_ASSERT(Src->LocationType == VAR_MEM, "Unreachable");

    if (IN_I16(Offset))
    {
        WriteOp32(Emitter, PVM_OP(LEA, Dst->As.Register.ID, Src->As.Memory.RegPtr), Offset);
    }
    else
    {
        WriteOp16(Emitter, PVM_OP(LEA, Dst->As.Register.ID, Src->As.Memory.RegPtr));
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

    VarMemory Mem = { .Location = 0, .RegPtr = Rd.As.Register.ID };
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

    StoreToPtr(Emitter, Rp.As.Register, 0, Ptr->PointsAt.Type, Rs.As.Register);

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

void PVMEmitAddImm(PVMEmitter *Emitter, VarLocation *Dst, I16 Imm)
{
    PASCAL_NONNULL(Emitter);
    PASCAL_NONNULL(Dst);
    if (0 == Imm)
        return;
    VarLocation Target;
    bool IsOwning = PVMEmitIntoReg(Emitter, &Target, Dst);

    if (IS_SMALL_IMM(Imm))
    {
        WriteOp16(Emitter, PVM_OP(ADDQI, Target.As.Register.ID, Imm));
    }
    else /* TODO: more ops */
    {
        WriteOp32(Emitter, PVM_OP(ADDI, Target.As.Register.ID, IMMTYPE_I16), Imm);
    }

    if (IsOwning)
    {
        PVMEmitMov(Emitter, Dst, &Target);
        PVMFreeRegister(Emitter, Target.As.Register);
    }
}





PASCAL_STATIC_ASSERT(false, "TODO; major rewrite to utilize the Persistent field in VarRegister");
#define DEFINE_INTEGER_BINARY_OP(FnName, Mnemonic)\
void FnName (PVMEmitter *Emitter, VarLocation *Dst, const VarLocation *Src) {\
    PASCAL_NONNULL(Emitter);\
    PASCAL_NONNULL(Dst);\
    PASCAL_NONNULL(Src);\
    PASCAL_ASSERT(Dst->Type == Src->Type, "Dst and Src type must be the same in %s", __func__);\
    VarLocation Rd, Rs;\
    bool OwningRd = PVMEmitIntoReg(Emitter, &Rd, Dst);\
    bool OwningRs = PVMEmitIntoReg(Emitter, &Rs, Src);\
    if (TYPE_U64 == Dst->Type || TYPE_I64 == Dst->Type) {\
        WriteOp16(Emitter, PVM_OP( Mnemonic ## 64, Rd.As.Register.ID, Rs.As.Register.ID));\
    } else {\
        WriteOp16(Emitter, PVM_OP( Mnemonic , Rd.As.Register.ID, Rs.As.Register.ID));\
    }\
    if (OwningRd) {\
        StoreFromReg(Emitter, Dst->As.Memory, Dst->Type, Rd.As.Register, Rd.Type);\
        PVMFreeRegister(Emitter, Rd.As.Register);\
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
    VarLocation Rd, Rs;\
    bool OwningRd = PVMEmitIntoReg(Emitter, &Rd, Dst);\
    bool OwningRs = PVMEmitIntoReg(Emitter, &Rs, Src);\
    if (TYPE_U64 == Dst->Type || TYPE_I64 == Dst->Type) {\
        WriteOp16(Emitter, PVM_OP( Mnemonic ## 64, Rd.As.Register.ID, Rs.As.Register.ID));\
    } else if (TYPE_F64 == Dst->Type || TYPE_F64 == Src->Type) {\
        WriteOp16(Emitter, PVM_OP( F ## Mnemonic ## 64, Rd.As.Register.ID, Rs.As.Register.ID));\
    } else if (TYPE_F32 == Dst->Type || TYPE_F32 == Dst->Type) {\
        WriteOp16(Emitter, PVM_OP( F ## Mnemonic, Rd.As.Register.ID, Rs.As.Register.ID));\
    } else {\
        WriteOp16(Emitter, PVM_OP( Mnemonic , Rd.As.Register.ID, Rs.As.Register.ID));\
    }\
    if (OwningRd) {\
        *Dst = Rd;\
    }\
    if (OwningRs) {\
        PVMFreeRegister(Emitter, Rs.As.Register);\
    }\
}\
void FnName (PVMEmitter *Emitter, VarLocation *Dst, const VarLocation *Src)


#define EMIT_POW2_OP(Emitter, SlowPathOp, FastPathOp, Bits, VarLoc_Dst, VarLoc_Src)\
do {\
    switch (Src->As.Literal.Int) {\
    case -1: WriteOp16(Emitter, PVM_OP(NEG ## Bits, Rd.As.Register.ID, Rd.As.Register.ID)); break;\
    case 1: break;\
    case 2: WriteOp16(Emitter, PVM_OP(FastPathOp ## Bits, Rd.As.Register.ID, 1)); break;\
    case 4: WriteOp16(Emitter, PVM_OP(FastPathOp ## Bits, Rd.As.Register.ID, 2)); break;\
    case 8: WriteOp16(Emitter, PVM_OP(FastPathOp ## Bits, Rd.As.Register.ID, 3)); break;\
    case 16: WriteOp16(Emitter, PVM_OP(FastPathOp ## Bits, Rd.As.Register.ID, 4)); break;\
    default: {\
        OwningRs = PVMEmitIntoReg(Emitter, &Rs, Src);\
        WriteOp16(Emitter, PVM_OP(SlowPathOp ## Bits, Rd.As.Register.ID, Rs.As.Register.ID));\
    } break;\
    }\
} while (0)





void PVMEmitAdd(PVMEmitter *Emitter, VarLocation *Dst, const VarLocation *Src) 
{
    PASCAL_NONNULL(Emitter);
    PASCAL_NONNULL(Dst);
    PASCAL_NONNULL(Src);
    PASCAL_ASSERT(Dst->Type == Src->Type, "Dst and Src type must be the same in %s", __func__);
    VarLocation Rd, Rs;
    bool OwningRd = PVMEmitIntoReg(Emitter, &Rd, Dst);
    bool OwningRs = false;

    if (IntegralTypeIsInteger(Dst->Type) && IntegralTypeIsInteger(Src->Type) && VAR_LIT == Src->LocationType)
    {
        PVMEmitAddImm(Emitter, &Rd, Src->As.Literal.Int);
    }
    else
    {
        OwningRs = PVMEmitIntoReg(Emitter, &Rs, Src);
        if (TYPE_F64 == Dst->Type || TYPE_F64 == Src->Type) 
        {
            WriteOp16(Emitter, PVM_OP(FADD64, Rd.As.Register.ID, Rs.As.Register.ID));
        } 
        else if (TYPE_F32 == Dst->Type || TYPE_F32 == Dst->Type) 
        {
            WriteOp16(Emitter, PVM_OP(FADD, Rd.As.Register.ID, Rs.As.Register.ID));
        }
        else if (TYPE_I64 == Rd.Type || TYPE_U64 == Rd.Type)
        {
            WriteOp16(Emitter, PVM_OP(ADD64, Rd.As.Register.ID, Rs.As.Register.ID));
        }
        else if (TYPE_STRING == Rd.Type)
        {
            PASCAL_ASSERT(TYPE_STRING == Rs.Type, "Unreachable in %s", __func__);
            WriteOp16(Emitter, PVM_OP(SADD, Rd.As.Register.ID, Rs.As.Register.ID));
        }
        else
        {
            WriteOp16(Emitter, PVM_OP(ADD, Rd.As.Register.ID, Rs.As.Register.ID));
        }
    }

    if (OwningRd) 
    {
        *Dst = Rd;
    }
    if (OwningRs) 
    {
        PVMFreeRegister(Emitter, Rs.As.Register);
    }
}

void PVMEmitSub(PVMEmitter *Emitter, VarLocation *Dst, const VarLocation *Src) 
{
    PASCAL_NONNULL(Emitter);
    PASCAL_NONNULL(Dst);
    PASCAL_NONNULL(Src);
    PASCAL_ASSERT(Dst->Type == Src->Type, "Dst and Src type must be the same in %s", __func__);
    VarLocation Rd, Rs;
    bool OwningRd = PVMEmitIntoReg(Emitter, &Rd, Dst);
    bool OwningRs = false;

    if (IntegralTypeIsInteger(Dst->Type) && IntegralTypeIsInteger(Src->Type) && VAR_LIT == Src->LocationType)
    {
        PVMEmitAddImm(Emitter, &Rd, -Src->As.Literal.Int);
    }
    else
    {
        OwningRs = PVMEmitIntoReg(Emitter, &Rs, Src);
        if (TYPE_F64 == Dst->Type || TYPE_F64 == Src->Type) 
        {
            WriteOp16(Emitter, PVM_OP(FSUB64, Rd.As.Register.ID, Rs.As.Register.ID));
        } 
        else if (TYPE_F32 == Dst->Type || TYPE_F32 == Dst->Type) 
        {
            WriteOp16(Emitter, PVM_OP(FSUB, Rd.As.Register.ID, Rs.As.Register.ID));
        }
        else if (TYPE_I64 == Rd.Type || TYPE_U64 == Rd.Type)
        {
            WriteOp16(Emitter, PVM_OP(SUB64, Rd.As.Register.ID, Rs.As.Register.ID));
        }
        else
        {
            WriteOp16(Emitter, PVM_OP(SUB, Rd.As.Register.ID, Rs.As.Register.ID));
        }
    }

    if (OwningRd) 
    {
        *Dst = Rd;
    }
    if (OwningRs) 
    {
        PVMFreeRegister(Emitter, Rs.As.Register);
    }
}


void PVMEmitIMul(PVMEmitter *Emitter, VarLocation *Dst, const VarLocation *Src)
{
    PASCAL_NONNULL(Emitter);
    PASCAL_NONNULL(Dst);
    PASCAL_NONNULL(Src);
    PASCAL_ASSERT(Dst->Type == Src->Type, "Dst and Src type must be the same in %s", __func__);
    VarLocation Rd, Rs;
    bool OwningRd = PVMEmitIntoReg(Emitter, &Rd, Dst);
    bool OwningRs = false;
    if (TYPE_I64 == Src->Type)
    {
        if (VAR_LIT == Src->LocationType)
        {
            EMIT_POW2_OP(Emitter, IMUL, QSHL, 64, Rd, Rs);
        }
        else
        {
            OwningRs = PVMEmitIntoReg(Emitter, &Rs, Src);
            WriteOp16(Emitter, PVM_OP(IMUL64, Rd.As.Register.ID, Rs.As.Register.ID));
        }
    }
    else if (VAR_LIT == Src->LocationType)
    {
        EMIT_POW2_OP(Emitter, IMUL, QSHL, , Rd, Rs);
    }
    else
    {
        OwningRs = PVMEmitIntoReg(Emitter, &Rs, Src);
        WriteOp16(Emitter, PVM_OP(IMUL, Rd.As.Register.ID, Rs.As.Register.ID));
    }

    if (OwningRd) 
    {
        *Dst = Rd;
    }
    if (OwningRs) 
    {
        PVMFreeRegister(Emitter, Rs.As.Register);
    }
}

void PVMEmitIDiv(PVMEmitter *Emitter, VarLocation *Dst, const VarLocation *Src)
{
    PASCAL_NONNULL(Emitter);
    PASCAL_NONNULL(Dst);
    PASCAL_NONNULL(Src);
    PASCAL_ASSERT(Dst->Type == Src->Type, "Dst and Src type must be the same in %s", __func__);
    VarLocation Rd, Rs;
    bool OwningRd = PVMEmitIntoReg(Emitter, &Rd, Dst);
    bool OwningRs = false;
    if (TYPE_I64 == Src->Type)
    {
        if (VAR_LIT == Src->LocationType)
        {
            EMIT_POW2_OP(Emitter, IDIV, QASR, 64, Rd, Rs);
        }
        else
        {
            OwningRs = PVMEmitIntoReg(Emitter, &Rs, Src);
            WriteOp16(Emitter, PVM_OP(IMUL64, Rd.As.Register.ID, Rs.As.Register.ID));
        }
    }
    else if (VAR_LIT == Src->LocationType)
    {
        EMIT_POW2_OP(Emitter, IDIV, QASR, , Rd, Rs);
    }
    else
    {
        OwningRs = PVMEmitIntoReg(Emitter, &Rs, Src);
        WriteOp16(Emitter, PVM_OP(IDIV, Rd.As.Register.ID, Rs.As.Register.ID));
    }

    if (OwningRd) 
    {
        *Dst = Rd;
    }
    if (OwningRs) 
    {
        PVMFreeRegister(Emitter, Rs.As.Register);
    }
}



void PVMEmitMul(PVMEmitter *Emitter, VarLocation *Dst, const VarLocation *Src)
{
    PASCAL_NONNULL(Emitter);
    PASCAL_NONNULL(Dst);
    PASCAL_NONNULL(Src);
    PASCAL_ASSERT(Dst->Type == Src->Type, "Dst and Src type must be the same in %s", __func__);
    if (IntegralTypeIsSigned(Dst->Type) || IntegralTypeIsSigned(Src->Type))
    {
        PVMEmitIMul(Emitter, Dst, Src);
        return;
    }
    VarLocation Rd, Rs;
    bool OwningRd = PVMEmitIntoReg(Emitter, &Rd, Dst);
    bool OwningRs = false;
    if (TYPE_U64 == Dst->Type) 
    {
        if (VAR_LIT == Src->LocationType)
        {
            EMIT_POW2_OP(Emitter, MUL, QSHL, 64, Rd, Rs);
        }
        else
        {
            OwningRs = PVMEmitIntoReg(Emitter, &Rs, Src);
            WriteOp16(Emitter, PVM_OP(MUL64, Rd.As.Register.ID, Rs.As.Register.ID));
        }
    } 
    else if (TYPE_F64 == Dst->Type || TYPE_F64 == Src->Type) 
    {
        OwningRs = PVMEmitIntoReg(Emitter, &Rs, Src);
        WriteOp16(Emitter, PVM_OP(FMUL64, Rd.As.Register.ID, Rs.As.Register.ID));
    }
    else if (TYPE_F32 == Dst->Type || TYPE_F32 == Dst->Type) 
    {
        OwningRs = PVMEmitIntoReg(Emitter, &Rs, Src);
        WriteOp16(Emitter, PVM_OP(FMUL, Rd.As.Register.ID, Rs.As.Register.ID));
    } 
    else if (VAR_LIT == Src->LocationType)
    {
        EMIT_POW2_OP(Emitter, MUL, QSHL, , Rd, Rs);
    }
    else
    {
        OwningRs = PVMEmitIntoReg(Emitter, &Rs, Src);
        WriteOp16(Emitter, PVM_OP(MUL, Rd.As.Register.ID, Rs.As.Register.ID));
    }

    if (OwningRd) 
    {
        *Dst = Rd;
    }
    if (OwningRs) 
    {
        PVMFreeRegister(Emitter, Rs.As.Register);
    }
}

void PVMEmitDiv(PVMEmitter *Emitter, VarLocation *Dst, const VarLocation *Src)
{
    PASCAL_NONNULL(Emitter);
    PASCAL_NONNULL(Dst);
    PASCAL_NONNULL(Src);
    PASCAL_ASSERT(Dst->Type == Src->Type, "Dst and Src type must be the same in %s", __func__);
    if (IntegralTypeIsSigned(Dst->Type) || IntegralTypeIsSigned(Src->Type))
    {
        PVMEmitIDiv(Emitter, Dst, Src);
        return;
    }

    VarLocation Rd, Rs;
    bool OwningRd = PVMEmitIntoReg(Emitter, &Rd, Dst);
    bool OwningRs = false;
    if (TYPE_U64 == Dst->Type || TYPE_I64 == Dst->Type) 
    {
        if (VAR_LIT == Src->LocationType)
        {
            EMIT_POW2_OP(Emitter, DIV, QSHR, 64, Rd, Rs);
        }
        else
        {
            OwningRs = PVMEmitIntoReg(Emitter, &Rs, Src);
            WriteOp16(Emitter, PVM_OP(DIV64, Rd.As.Register.ID, Rs.As.Register.ID));
        }
    } 
    else if (TYPE_F64 == Dst->Type || TYPE_F64 == Src->Type) 
    {
        OwningRs = PVMEmitIntoReg(Emitter, &Rs, Src);
        WriteOp16(Emitter, PVM_OP(FDIV64, Rd.As.Register.ID, Rs.As.Register.ID));
    }
    else if (TYPE_F32 == Dst->Type || TYPE_F32 == Dst->Type) 
    {
        OwningRs = PVMEmitIntoReg(Emitter, &Rs, Src);
        WriteOp16(Emitter, PVM_OP(FDIV, Rd.As.Register.ID, Rs.As.Register.ID));
    } 
    else if (VAR_LIT == Src->LocationType)
    {
        EMIT_POW2_OP(Emitter, DIV, QSHR, , Rd, Rs);
    }
    else
    {
        OwningRs = PVMEmitIntoReg(Emitter, &Rs, Src);
        WriteOp16(Emitter, PVM_OP(DIV, Rd.As.Register.ID, Rs.As.Register.ID));
    }

    if (OwningRd) 
    {
        *Dst = Rd;
    }
    if (OwningRs) 
    {
        PVMFreeRegister(Emitter, Rs.As.Register);
    }
}







/* kill me */
DEFINE_GENERIC_BINARY_OP(PVMEmitNeg, NEG);

DEFINE_INTEGER_BINARY_OP(PVMEmitNot, NOT);
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






VarLocation PVMEmitSetCC(PVMEmitter *Emitter, TokenType Op, const VarLocation *Dst, const VarLocation *Src) 
{
    PASCAL_NONNULL(Emitter);
    PASCAL_NONNULL(Dst);
    PASCAL_NONNULL(Src);
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

#define SSET(BitPostfix) do {\
    switch(Op) {\
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


    VarLocation Rd = PVMAllocateRegister(Emitter, Dst->Type);
    VarLocation Rs;
    bool OwningRs = PVMEmitIntoReg(Emitter, &Rs, Src);
    bool Signed = (IntegralTypeIsSigned(Dst->Type) 
                || IntegralTypeIsSigned(Src->Type));
    bool Bits64 = TYPE_U64 == Dst->Type
        || TYPE_I64 == Dst->Type
        || TYPE_U64 == Src->Type
        || TYPE_I64 == Src->Type;
    UInt RdID = Rd.As.Register.ID, RsID = Rs.As.Register.ID;
    PVMEmitMov(Emitter, &Rd, Dst);

    if (TYPE_STRING == Dst->Type)
    {
        PASCAL_ASSERT(TYPE_STRING == Src->Type, "Both dst and src must be string in %s.", __func__);
        if (TOKEN_EQUAL == Op)
        {
            WriteOp16(Emitter, PVM_OP(STREQU, RdID, RsID));
        }
        else if (UINTPTR_MAX == UINT32_MAX)
        {
            SSET( );
        }
        else SSET(64);
    }
    else if (TYPE_F64 == Src->Type || TYPE_F64 == Dst->Type)
    {
        TransferRegister(Emitter, &Rd.As.Register, TYPE_F64, Rd.As.Register, Dst->Type);
        TransferRegister(Emitter, &Rs.As.Register, TYPE_F64, Rs.As.Register, Src->Type);
        FSET(64);
        VarLocation Condition = PVMAllocateRegister(Emitter, TYPE_BOOLEAN);
        WriteOp16(Emitter, PVM_OP(GETFCC, Condition.As.Register.ID, 0));
        return Condition;
    }
    else if (TYPE_F32 == Src->Type || TYPE_F32 == Dst->Type)
    {
        TransferRegister(Emitter, &Rd.As.Register, TYPE_F32, Rd.As.Register, Dst->Type);
        TransferRegister(Emitter, &Rs.As.Register, TYPE_F32, Rs.As.Register, Src->Type);
        FSET( );
        VarLocation Condition = PVMAllocateRegister(Emitter, TYPE_BOOLEAN);
        WriteOp16(Emitter, PVM_OP(GETFCC, Condition.As.Register.ID, 0));
        return Condition;
    }
    else if (Bits64)
    {
        if (Signed)
        {
            TransferRegister(Emitter, &Rd.As.Register, TYPE_I64, Rd.As.Register, Dst->Type);
            TransferRegister(Emitter, &Rs.As.Register, TYPE_I64, Rs.As.Register, Src->Type);
            SET(I, 64);
        }
        else
        {
            TransferRegister(Emitter, &Rd.As.Register, TYPE_U64, Rd.As.Register, Dst->Type);
            TransferRegister(Emitter, &Rs.As.Register, TYPE_U64, Rs.As.Register, Src->Type);
            SET(,64);
        }
    }
    else if (Signed)
    {
        SET(I, );
    }
    else
    {
        SET(, );
    }
    

    if (OwningRs) 
    {
        PVMFreeRegister(Emitter, Rs.As.Register);
    }
    Rd.Type = TYPE_BOOLEAN;
    return Rd;
#undef SET
#undef FSET
}




/* subroutine arguments */
VarLocation PVMSetParamType(PVMEmitter *Emitter, UInt ArgNumber, IntegralType ParamType)
{
    PASCAL_NONNULL(Emitter);
    if (ArgNumber < PVM_ARGREG_COUNT)
    {
        Emitter->ArgReg[ArgNumber].Type = ParamType;
        return Emitter->ArgReg[ArgNumber];
    }

    /* TODO: pascal calling convetion */
    I32 ArgOffset = (PVM_ARGREG_COUNT - ArgNumber - 1) * sizeof(PVMGPR);
    VarLocation Location = {
        .Type = ParamType,
        .LocationType = VAR_MEM,
        .As.Memory = {
            .RegPtr = PVM_REG_FP,
            .Location = ArgOffset,
        },
    };
    return Location;
}

VarLocation PVMSetArgType(PVMEmitter *Emitter, UInt ArgNumber, IntegralType ArgType)
{
    PASCAL_NONNULL(Emitter);
    if (ArgNumber < PVM_ARGREG_COUNT)
    {
        VarLocation Arg = Emitter->ArgReg[ArgNumber];
        Arg.Type = ArgType;
        if (IntegralTypeIsFloat(ArgType))
        {
            Arg.As.Register.ID = ArgNumber + PVM_REG_COUNT;
        }
        return Arg;
    }

    /* TODO: pascal calling convention */
    I32 ArgOffset = (ArgNumber - PVM_ARGREG_COUNT) * sizeof(PVMGPR);
    ArgOffset += Emitter->StackSpace;
    VarLocation Mem = {
        .Type = ArgType,
        .LocationType = VAR_MEM,
        .As.Memory = {
            .RegPtr = PVM_REG_FP,
            .Location = ArgOffset,
        },
    };
    //PASCAL_ASSERT(ArgOffset >= 0, "allocate space before call");
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
        Emitter->StackSpace += sizeof(PVMGPR);
    }
    va_end(Args);
}





/* stack allocation */
VarMemory PVMQueueStackAllocation(PVMEmitter *Emitter, U32 Size)
{
    PASCAL_NONNULL(Emitter);
    U32 NewOffset = Emitter->StackSpace + Size;
    if (NewOffset % sizeof(PVMPTR))
        NewOffset = (NewOffset + sizeof(PVMPTR)) & ~(sizeof(PVMPTR) - 1);

    VarMemory Mem = {
        .RegPtr = PVM_REG_FP,
        .Location = Emitter->StackSpace,
    };
    Emitter->StackSpace = NewOffset;
    return Mem;
}


void PVMCommitStackAllocation(PVMEmitter *Emitter)
{
    PASCAL_NONNULL(Emitter);
	PVMAllocateStack(Emitter, Emitter->StackSpace);
}

void PVMAllocateStack(PVMEmitter *Emitter, I32 Size) 
{
    PASCAL_NONNULL(Emitter);
    PVMEmitAddImm(Emitter, &Emitter->Reg.SP, Size);
}






VarMemory PVMEmitGlobalData(PVMEmitter *Emitter, const void *Data, U32 Size)
{
    PASCAL_NONNULL(Emitter);
    VarMemory Global = {
        .RegPtr = PVM_REG_GP,
        .Location = ChunkWriteGlobalData(PVMCurrentChunk(Emitter), Data, Size),
    };
    return Global;
}

/* global instructions */
VarMemory PVMEmitGlobalSpace(PVMEmitter *Emitter, U32 Size)
{
    PASCAL_NONNULL(Emitter);
    return PVMEmitGlobalData(Emitter, NULL, Size);
}






/* subroutine */
void PVMEmitSaveCallerRegs(PVMEmitter *Emitter, UInt ReturnRegID)
{
    PASCAL_NONNULL(Emitter);
    U32 SaveReglist = Emitter->Reglist & ~(((U16)1 << ReturnRegID) | EMPTY_REGLIST);

    if (SaveReglist & 0xFF)
    {
        WriteOp16(Emitter, PVM_REGLIST(PSHL, SaveReglist & 0xFF));
    }
    else if ((SaveReglist >> 8) & 0xFF)
    {
        WriteOp16(Emitter, PVM_REGLIST(PSHH, SaveReglist >> 8));
    }
    else if ((SaveReglist >> 16) & 0xFF)
    {
        WriteOp16(Emitter, PVM_REGLIST(FPSHL, SaveReglist >> 16));
    }
    else if ((SaveReglist >> 24) & 0xFF)
    {
        WriteOp16(Emitter, PVM_REGLIST(FPSHH, SaveReglist >> 26));
    }



    if (Emitter->NumSavelist > PVM_MAX_CALL_IN_EXPR)
    {
        PASCAL_UNREACHABLE("TODO: make the limit on number of calls in expr dynamic or larger.");
    }
    Emitter->SavedRegisters[Emitter->NumSavelist++] = SaveReglist;
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


void PVMEmitUnsaveCallerRegs(PVMEmitter *Emitter, UInt ReturnRegID)
{
    PASCAL_NONNULL(Emitter);
    U32 Restorelist = Emitter->SavedRegisters[--Emitter->NumSavelist];
    if (Restorelist >> 8)
    {
        WriteOp16(Emitter, PVM_REGLIST(POPH, Restorelist >> 8));
    }
    else if (Restorelist & 0xFF)
    {
        WriteOp16(Emitter, PVM_REGLIST(POPL, Restorelist & 0xFF));
    }
    else if ((Restorelist >> 24) & 0xFF) 
    {
        WriteOp16(Emitter, PVM_REGLIST(FPOPL, Restorelist >> 24));
    }
    else if ((Restorelist >> 16) & 0xFF)
    {
        WriteOp16(Emitter, PVM_REGLIST(FPOPL, Restorelist >> 16));
    }
    Emitter->Reglist = Restorelist | EMPTY_REGLIST;
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




