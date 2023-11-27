
#include "PVM/Isa.h"
#include "PVMEmitter.h"



/*
 * SP, GP, FP are allocated by default
 * */
#define EMPTY_REGLIST 0xE000


PVMEmitter PVMEmitterInit(PVMChunk *Chunk)
{
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
    PVMEmitExit(Emitter);
}









static PVMChunk *PVMCurrentChunk(PVMEmitter *Emitter)
{
    return Emitter->Chunk;
}


static U32 WriteOp16(PVMEmitter *Emitter, U16 Opcode)
{
    return ChunkWriteCode(PVMCurrentChunk(Emitter), Opcode);
}

static U32 WriteOp32(PVMEmitter *Emitter, U16 Opcode, U16 SecondHalf)
{
    U32 Location = WriteOp16(Emitter, Opcode);
    WriteOp16(Emitter, SecondHalf);
    return Location;
}


static bool PVMRegisterIsFree(PVMEmitter *Emitter, UInt Reg)
{
    return ((Emitter->Reglist >> Reg) & 1) == 0;
}

void PVMMarkRegisterAsAllocated(PVMEmitter *Emitter, UInt Reg)
{
    Emitter->Reglist |= (U32)1 << Reg;
}

static void PVMMarkRegisterAsFreed(PVMEmitter *Emitter, UInt Reg)
{
    Emitter->Reglist &= ~((U32)1 << Reg);
}

static void PVMEmitPush(PVMEmitter *Emitter, UInt Reg)
{
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
    if (Reg < PVM_REG_COUNT / 2)
    {
        WriteOp16(Emitter, PVM_REGLIST(POPL, 1 << Reg));
    }
    else
    {
        WriteOp16(Emitter, PVM_REGLIST(POPH, 1 << Reg));
    }
}

static void MovRegister(PVMEmitter *Emitter, VarRegister Dst, IntegralType DstType, VarRegister Src, IntegralType SrcType)
{
    switch (DstType)
    {
    default: 
    {
        PASCAL_UNREACHABLE("Cannot move register of type %s into %s.", 
                IntegralTypeToStr(SrcType), IntegralTypeToStr(DstType)
        );
    } break;
    case TYPE_I64:
    {
        switch (SrcType)
        {
        case TYPE_I64:
        case TYPE_U64: WriteOp16(Emitter, PVM_OP(MOV64, Dst.ID, Src.ID)); break;
        case TYPE_I32: WriteOp16(Emitter, PVM_OP(MOVSEX64_32, Dst.ID, Src.ID)); break;
        case TYPE_U32: WriteOp16(Emitter, PVM_OP(MOVZEX64_32, Dst.ID, Src.ID)); break;
        case TYPE_I16: WriteOp16(Emitter, PVM_OP(MOVSEX64_16, Dst.ID, Src.ID)); break;
        case TYPE_U16: WriteOp16(Emitter, PVM_OP(MOVZEX64_16, Dst.ID, Src.ID)); break;
        case TYPE_I8:  WriteOp16(Emitter, PVM_OP(MOVSEX64_8, Dst.ID, Src.ID)); break;
        case TYPE_U8:  WriteOp16(Emitter, PVM_OP(MOVZEX64_8, Dst.ID, Src.ID)); break;
        default: break;
        }
    } break;
#if UINTPTR_MAX == UINT64_MAX
    case TYPE_POINTER:
#endif 
    case TYPE_U64:
    {
        switch (SrcType)
        {
        case TYPE_POINTER: 
        case TYPE_I64:
        case TYPE_U64: WriteOp16(Emitter, PVM_OP(MOV64, Dst.ID, Src.ID)); break;
        case TYPE_I32:
        case TYPE_U32: WriteOp16(Emitter, PVM_OP(MOVZEX64_32, Dst.ID, Src.ID)); break;
        case TYPE_I16:
        case TYPE_U16: WriteOp16(Emitter, PVM_OP(MOVZEX64_16, Dst.ID, Src.ID)); break;
        case TYPE_I8:
        case TYPE_U8:  WriteOp16(Emitter, PVM_OP(MOVZEX64_8, Dst.ID, Src.ID)); break;
        default: break;
        }
    } break;
    case TYPE_I32:
    case TYPE_I16:
    case TYPE_I8:
    {
        switch (SrcType)
        {
        case TYPE_I64:
        case TYPE_U64: WriteOp16(Emitter, PVM_OP(MOV64, Dst.ID, Src.ID)); break;
        case TYPE_I32:
        case TYPE_U32: WriteOp16(Emitter, PVM_OP(MOV32, Dst.ID, Src.ID)); break;
        case TYPE_I16: WriteOp16(Emitter, PVM_OP(MOVSEX32_16, Dst.ID, Src.ID)); break;
        case TYPE_U16: WriteOp16(Emitter, PVM_OP(MOVZEX32_16, Dst.ID, Src.ID)); break;
        case TYPE_I8:  WriteOp16(Emitter, PVM_OP(MOVSEX32_8, Dst.ID, Src.ID)); break;
        case TYPE_U8:  WriteOp16(Emitter, PVM_OP(MOVZEX32_8, Dst.ID, Src.ID)); break;
        default: break;
        }
    } break;
#if UINTPTR_MAX == UINT32_MAX
    case TYPE_POINTER:
#endif 
    case TYPE_U32:
    case TYPE_U16:
    case TYPE_U8:
    {
        switch (SrcType)
        {
        case TYPE_POINTER:
        case TYPE_I64:
        case TYPE_U64: WriteOp16(Emitter, PVM_OP(MOV64, Dst.ID, Src.ID)); break;
        case TYPE_I32:
        case TYPE_U32: WriteOp16(Emitter, PVM_OP(MOV32, Dst.ID, Src.ID)); break;
        case TYPE_I16:
        case TYPE_U16: WriteOp16(Emitter, PVM_OP(MOVZEX32_16, Dst.ID, Src.ID)); break;
        case TYPE_I8:
        case TYPE_U8:  WriteOp16(Emitter, PVM_OP(MOVZEX32_8, Dst.ID, Src.ID)); break;
        default: break;
        }
    } break;
    case TYPE_BOOLEAN:
    {
        WriteOp16(Emitter, PVM_OP(MOVZEX32_8, Dst.ID, Src.ID));
    } break;
    case TYPE_F32:
    {
        switch (SrcType)
        {
        case TYPE_F32: WriteOp16(Emitter, PVM_OP(FMOV, Dst.ID, Src.ID)); break;
        case TYPE_F64: WriteOp16(Emitter, PVM_OP(F64TOF32, Dst.ID, Src.ID)); break;
        }
    } break;
    case TYPE_F64:
    {
        switch (SrcType) 
        {
        case TYPE_F32: WriteOp16(Emitter, PVM_OP(F32TOF64, Dst.ID, Src.ID)); break;
        case TYPE_F64: WriteOp16(Emitter, PVM_OP(FMOV64, Dst.ID, Src.ID)); break;
        }
    } break;
    }
}

static void StoreFromReg(PVMEmitter *Emitter, VarMemory Dst, IntegralType DstType, VarRegister Src, IntegralType SrcType)
{
#define STORE(Base)\
    do {\
        if (Dst.Location < UINT16_MAX) {\
            switch (DstType) {\
            case TYPE_BOOLEAN:\
            case TYPE_I8:\
            case TYPE_U8: WriteOp32(Emitter, PVM_OP(ST8, Src.ID, Base), Dst.Location); break;\
            case TYPE_I16:\
            case TYPE_U16: WriteOp32(Emitter, PVM_OP(ST16, Src.ID, Base), Dst.Location); break;\
            case TYPE_I32:\
            case TYPE_U32: WriteOp32(Emitter, PVM_OP(ST32, Src.ID, Base), Dst.Location); break;\
            case TYPE_I64:\
            case TYPE_U64: WriteOp32(Emitter, PVM_OP(ST64, Src.ID, Base), Dst.Location); break;\
            case TYPE_F32: WriteOp32(Emitter, PVM_OP(STF32, Src.ID, Base), Dst.Location); break;\
            case TYPE_F64: WriteOp32(Emitter, PVM_OP(STF64, Src.ID, Base), Dst.Location); break;\
            default: PASCAL_UNREACHABLE("TODO: Src.Size > 8"); break;\
            }\
        } else {\
            switch (DstType) {\
            case TYPE_BOOLEAN:\
            case TYPE_I8:\
            case TYPE_U8: WriteOp16(Emitter, PVM_OP(ST8L, Src.ID, Base)); break;\
            case TYPE_I16:\
            case TYPE_U16: WriteOp16(Emitter, PVM_OP(ST16L, Src.ID, Base)); break;\
            case TYPE_I32:\
            case TYPE_U32: WriteOp16(Emitter, PVM_OP(ST32L, Src.ID, Base)); break;\
            case TYPE_I64:\
            case TYPE_U64: WriteOp16(Emitter, PVM_OP(ST64L, Src.ID, Base)); break;\
            case TYPE_F32: WriteOp32(Emitter, PVM_OP(STF32L, Src.ID, Base), Dst.Location); break;\
            case TYPE_F64: WriteOp32(Emitter, PVM_OP(STF64L, Src.ID, Base), Dst.Location); break;\
            default: PASCAL_UNREACHABLE("TODO: Dst.Size > 8"); break;\
            }\
            WriteOp32(Emitter, Dst.Location, Dst.Location >> 16);\
        }\
    } while (0)

    if (DstType != SrcType) 
    {
        MovRegister(Emitter, Src, DstType, Src, SrcType);
    }
    if (Dst.IsGlobal)
    {
        STORE(PVM_REG_GP);
    }
    else
    {
        STORE(PVM_REG_FP);
    }
#undef STORE
}

static void LoadIntoReg(PVMEmitter *Emitter, VarRegister Dst, IntegralType DstType, VarMemory Src, IntegralType SrcType)
{
#define LOAD(Base)\
    do {\
        if (Src.Location < UINT16_MAX) {\
            switch (SrcType) {\
            case TYPE_BOOLEAN:\
            case TYPE_I8:\
            case TYPE_U8: WriteOp32(Emitter, PVM_OP(LD8, Dst.ID, Base), Src.Location); break;\
            case TYPE_I16:\
            case TYPE_U16: WriteOp32(Emitter, PVM_OP(LD16, Dst.ID, Base), Src.Location); break;\
            case TYPE_I32:\
            case TYPE_U32: WriteOp32(Emitter, PVM_OP(LD32, Dst.ID, Base), Src.Location); break;\
            case TYPE_I64:\
            case TYPE_U64: WriteOp32(Emitter, PVM_OP(LD64, Dst.ID, Base), Src.Location); break;\
            case TYPE_F32: WriteOp32(Emitter, PVM_OP(LDF32, Dst.ID, Base), Src.Location); break;\
            case TYPE_F64: WriteOp32(Emitter, PVM_OP(LDF64, Dst.ID, Base), Src.Location); break;\
            default: PASCAL_UNREACHABLE("TODO: Src.Size > 8"); break;\
            }\
        } else {\
            switch (SrcType) {\
            case TYPE_BOOLEAN:\
            case TYPE_I8:\
            case TYPE_U8: WriteOp16(Emitter, PVM_OP(LD8L, Dst.ID, Base)); break;\
            case TYPE_I16:\
            case TYPE_U16: WriteOp16(Emitter, PVM_OP(LD16L, Dst.ID, Base)); break;\
            case TYPE_I32:\
            case TYPE_U32: WriteOp16(Emitter, PVM_OP(LD32L, Dst.ID, Base)); break;\
            case TYPE_I64:\
            case TYPE_U64: WriteOp16(Emitter, PVM_OP(LD64L, Dst.ID, Base)); break;\
            case TYPE_F32: WriteOp32(Emitter, PVM_OP(LDF32L, Dst.ID, Base), Src.Location); break;\
            case TYPE_F64: WriteOp32(Emitter, PVM_OP(LDF64L, Dst.ID, Base), Src.Location); break;\
            default: PASCAL_UNREACHABLE("TODO: Src.Size > 8"); break;\
            }\
            WriteOp32(Emitter, Src.Location, Src.Location >> 16);\
        }\
    } while (0)

    if (Src.IsGlobal)
    {
        LOAD(PVM_REG_GP);
    }
    else
    {
        LOAD(PVM_REG_FP);
    }

    if (DstType != SrcType) 
    {
        MovRegister(Emitter, Dst, DstType, Dst, SrcType);
    }
#undef LOAD
}

static bool PVMEmitIntoReg(PVMEmitter *Emitter, VarLocation *OutTarget, const VarLocation *Src)
{
    VarLocation Tmp = *Src;
    switch (Tmp.LocationType)
    {
    case VAR_REG: break;
    case VAR_MEM:
    {
        *OutTarget = PVMAllocateRegister(Emitter, Tmp.Type);
        LoadIntoReg(Emitter, OutTarget->As.Register, Tmp.Type, Tmp.As.Memory, Tmp.Type);
        return true;
    } break;
    case VAR_SUBROUTINE:
    {
        PASCAL_UNREACHABLE("TODO: FnPtr in EmitIntoReg()");
    } break;
    case VAR_INVALID:
    {
        PASCAL_UNREACHABLE("VAR_INVALID encountered in EmitIntoReg()");
    } break;
    case VAR_LIT:
    {
        *OutTarget = PVMAllocateRegister(Emitter, Tmp.Type);
        if (TYPE_POINTER == Tmp.Type || IntegralTypeIsInteger(Tmp.Type))
        {
            ChunkWriteMovImm(PVMCurrentChunk(Emitter), 
                    OutTarget->As.Register.ID, 
                    Tmp.As.Literal.Int, Tmp.Type
            );
        }
        else if (Tmp.Type == TYPE_BOOLEAN)
        {
            ChunkWriteMovImm(PVMCurrentChunk(Emitter), 
                    OutTarget->As.Register.ID, 
                    Tmp.As.Literal.Bool, TYPE_U8
            );
        }
        else if (TYPE_F64 == Tmp.Type)
        {
            VarMemory Literal = PVMEmitGlobalData(Emitter, &Tmp.As.Literal.F64, sizeof(F64));
            LoadIntoReg(Emitter, OutTarget->As.Register, TYPE_F64, Literal, TYPE_F64);
        }
        else if (TYPE_F32 == Tmp.Type)
        {
            VarMemory Literal = PVMEmitGlobalData(Emitter, &Tmp.As.Literal.F32, sizeof(F32));
            LoadIntoReg(Emitter, OutTarget->As.Register, TYPE_F32, Literal, TYPE_F32);
        }
        else 
        {
            PASCAL_UNREACHABLE("Unhandled imm type: %s in PVMEmitIntoReg()", IntegralTypeToStr(Tmp.Type));
        }
        return true;
    } break;
    }

    *OutTarget = Tmp;
    return false;
}











void PVMSetEntryPoint(PVMEmitter *Emitter, U32 EntryPoint)
{
    PVMCurrentChunk(Emitter)->EntryPoint = EntryPoint;
}




void PVMEmitterBeginScope(PVMEmitter *Emitter)
{
    Emitter->SavedRegisters[Emitter->NumSavelist++] = Emitter->Reglist;
    MovRegister(Emitter, 
            Emitter->Reg.FP.As.Register, TYPE_POINTER, 
            Emitter->Reg.SP.As.Register, TYPE_POINTER
    );
}

void PVMEmitterEndScope(PVMEmitter *Emitter)
{
    PASCAL_ASSERT(Emitter->NumSavelist > 0, "Unreachable");
    Emitter->Reglist = Emitter->SavedRegisters[--Emitter->NumSavelist];
}





void PVMEmitDebugInfo(PVMEmitter *Emitter, const U8 *Src, U32 SrcLen, U32 LineNum)
{
    ChunkWriteDebugInfo(PVMCurrentChunk(Emitter), Src, SrcLen, LineNum);
}

void PVMUpdateDebugInfo(PVMEmitter *Emitter, U32 LineLen, bool IsSubroutine)
{
    LineDebugInfo *Info = ChunkGetDebugInfo(PVMCurrentChunk(Emitter), UINT32_MAX);
    PASCAL_ASSERT(NULL != Info, "PVMUpdateDebugInfo: Info is NULL");
    Info->SrcLen[Info->Count - 1] = LineLen;
    Info->IsSubroutine = IsSubroutine;
}










U32 PVMGetCurrentLocation(PVMEmitter *Emitter)
{
    return PVMCurrentChunk(Emitter)->Count;
}

VarLocation PVMAllocateRegister(PVMEmitter *Emitter, IntegralType Type)
{
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
    PVMEmitPush(Emitter, Reg);

    return (VarLocation) {
        .LocationType = VAR_REG,
        .Type = Type,
        .As.Register.ID = Reg,
    };
}

void PVMFreeRegister(PVMEmitter *Emitter, VarRegister Reg)
{
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
    VarLocation Target;
    bool IsOwning = PVMEmitIntoReg(Emitter, &Target, Condition);
    U32 Location = WriteOp32(Emitter, PVM_B(EZ, Target.As.Register.ID, 0), 0);
    if (IsOwning)
    {
        PVMFreeRegister(Emitter, Target.As.Register);
    }
    return Location;
}

U32 PVMEmitBranch(PVMEmitter *Emitter, U32 To)
{
    /* size of the branch instruction is 2 16 opcode word */
    U32 Offset = To - PVMCurrentChunk(Emitter)->Count - PVM_BRANCH_INS_SIZE;
    return WriteOp32(Emitter, PVM_BR(Offset >> 16), Offset & 0xFFFF);
}

void PVMPatchBranch(PVMEmitter *Emitter, U32 From, U32 To, PVMBranchType Type)
{
    /* size of the branch instruction is 2 16 opcode word */
    U32 Offset = To - From - PVM_BRANCH_INS_SIZE;
    PVMCurrentChunk(Emitter)->Code[From] = 
        (PVMCurrentChunk(Emitter)->Code[From] & ~(U32)Type)
        | ((U32)Type & (Offset >> 16));
    PVMCurrentChunk(Emitter)->Code[From + 1] = (U16)Offset;
}

void PVMPatchBranchToCurrent(PVMEmitter *Emitter, U32 From, PVMBranchType Type)
{
    PVMPatchBranch(Emitter, From, PVMCurrentChunk(Emitter)->Count, Type);
}



/* move and load */
void PVMEmitMov(PVMEmitter *Emitter, const VarLocation *Dst, const VarLocation *Src)
{
    if (Dst->LocationType == VAR_LIT)
    {
        /* compiler should've handled this */
        PASCAL_UNREACHABLE("cannot move into a literal: %s to %s\n", 
                IntegralTypeToStr(Dst->Type), IntegralTypeToStr(Src->Type)
        );
    }


    VarLocation Tmp = *Src;
    if (TYPE_F32 == Dst->Type)
    {
        if (TYPE_F64 == Src->Type)
        {
            Tmp.As.Literal.F32 = Tmp.As.Literal.F64;
        }
        else if (IntegralTypeIsInteger(Src->Type))
        {
            Tmp.As.Literal.F32 = Tmp.As.Literal.Int;
        }
        Tmp.Type = TYPE_F32;
    }
    else if (TYPE_F64 == Dst->Type)
    {
        if (TYPE_F32 == Src->Type)
        {
            Tmp.As.Literal.F64 = Tmp.As.Literal.F32;
        }
        else if (IntegralTypeIsInteger(Src->Type))
        {
            Tmp.As.Literal.F64 = Tmp.As.Literal.Int;
        }
        Tmp.Type = TYPE_F64;
    }

    bool IsOwning = PVMEmitIntoReg(Emitter, &Tmp, &Tmp);

    switch (Dst->LocationType)
    {
    case VAR_INVALID:
    case VAR_SUBROUTINE:
    case VAR_LIT:
    {
        PASCAL_UNREACHABLE("PVMEmitMov: invalid Dst");
    } break;


    case VAR_REG:
    {
        if (Tmp.As.Register.ID == Dst->As.Register.ID)
            break;

        MovRegister(Emitter, 
                Dst->As.Register, Dst->Type, 
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


void PVMEmitExtend(PVMEmitter *Emitter, VarRegister Rd, IntegralType To, VarRegister Rs, IntegralType From)
{
    if (To == From && Rd.ID == Rs.ID)
    {
        return;
    }

    MovRegister(Emitter, Rd, To, Rs, From);
}

void PVMEmitAddImm(PVMEmitter *Emitter, VarLocation *Dst, I16 Imm)
{
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





#define DEFINE_INTEGER_BINARY_OP(FnName, Mnemonic)\
void FnName (PVMEmitter *Emitter, VarLocation *Dst, const VarLocation *Src) {\
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
    VarLocation Rd, Rs;\
    bool OwningRd = PVMEmitIntoReg(Emitter, &Rd, Dst);\
    bool OwningRs = PVMEmitIntoReg(Emitter, &Rs, Src);\
    if (TYPE_U64 == Dst->Type || TYPE_I64 == Dst->Type) {\
        WriteOp16(Emitter, PVM_OP( Mnemonic ## 64, Rd.As.Register.ID, Rs.As.Register.ID));\
    } else if (TYPE_F64 == Dst->Type) {\
        WriteOp16(Emitter, PVM_OP( F ## Mnemonic ## 64, Rd.As.Register.ID, Rs.As.Register.ID));\
    } else if (TYPE_F32 == Dst->Type) {\
        WriteOp16(Emitter, PVM_OP( F ## Mnemonic, Rd.As.Register.ID, Rs.As.Register.ID));\
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

/* kill me */
DEFINE_GENERIC_BINARY_OP(PVMEmitAdd, ADD);
DEFINE_GENERIC_BINARY_OP(PVMEmitSub, SUB);
DEFINE_GENERIC_BINARY_OP(PVMEmitNeg, NEG);
DEFINE_GENERIC_BINARY_OP(PVMEmitMul, MUL);
DEFINE_GENERIC_BINARY_OP(PVMEmitDiv, DIV);

DEFINE_INTEGER_BINARY_OP(PVMEmitNot, NOT);
DEFINE_INTEGER_BINARY_OP(PVMEmitAnd, AND);
DEFINE_INTEGER_BINARY_OP(PVMEmitOr, OR);
DEFINE_INTEGER_BINARY_OP(PVMEmitXor, XOR);
DEFINE_INTEGER_BINARY_OP(PVMEmitIMul, IMUL);
DEFINE_INTEGER_BINARY_OP(PVMEmitIDiv, IDIV);
DEFINE_INTEGER_BINARY_OP(PVMEmitMod, MOD);
DEFINE_INTEGER_BINARY_OP(PVMEmitShl, VSHL);
DEFINE_INTEGER_BINARY_OP(PVMEmitShr, VSHR);
DEFINE_INTEGER_BINARY_OP(PVMEmitAsr, VASR);


#undef DEFINE_INTEGER_BINARY_OP
#undef DEFINE_GENERIC_BINARY_OP






VarLocation PVMEmitSetCC(PVMEmitter *Emitter, TokenType Op, const VarLocation *Dst, const VarLocation *Src) 
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

    VarLocation Rd, Rs;
    bool OwningRd = PVMEmitIntoReg(Emitter, &Rd, Dst);
    bool OwningRs = PVMEmitIntoReg(Emitter, &Rs, Src);
    bool Signed = (IntegralTypeIsSigned(Dst->Type) 
                || IntegralTypeIsSigned(Src->Type));
    bool Bits64 = TYPE_U64 == Dst->Type
        || TYPE_I64 == Dst->Type
        || TYPE_U64 == Src->Type
        || TYPE_I64 == Src->Type;
    UInt RdID = Rd.As.Register.ID, RsID = Rs.As.Register.ID;

    if (TYPE_F64 == Src->Type || TYPE_F64 == Dst->Type)
    {
        PVMEmitExtend(Emitter, Rd.As.Register, TYPE_F64, Rd.As.Register, Dst->Type);
        PVMEmitExtend(Emitter, Rs.As.Register, TYPE_F64, Rs.As.Register, Src->Type);
        FSET(64);
        VarLocation Condition = PVMAllocateRegister(Emitter, TYPE_BOOLEAN);
        WriteOp16(Emitter, PVM_OP(GETFCC, Condition.As.Register.ID, 0));
        return Condition;
    }
    else if (TYPE_F32 == Src->Type || TYPE_F32 == Dst->Type)
    {
        PVMEmitExtend(Emitter, Rd.As.Register, TYPE_F64, Rd.As.Register, Dst->Type);
        PVMEmitExtend(Emitter, Rs.As.Register, TYPE_F64, Rs.As.Register, Src->Type);
        FSET( );
        VarLocation Condition = PVMAllocateRegister(Emitter, TYPE_BOOLEAN);
        WriteOp16(Emitter, PVM_OP(GETFCC, Condition.As.Register.ID, 0));
        return Condition;
    }
    else if (Bits64)
    {
        if (Signed)
        {
            PVMEmitExtend(Emitter, Rd.As.Register, TYPE_I64, Rd.As.Register, Dst->Type);
            PVMEmitExtend(Emitter, Rs.As.Register, TYPE_I64, Rs.As.Register, Src->Type);
            SET(I, 64);
        }
        else
        {
            PVMEmitExtend(Emitter, Rd.As.Register, TYPE_U64, Rd.As.Register, Dst->Type);
            PVMEmitExtend(Emitter, Rs.As.Register, TYPE_U64, Rs.As.Register, Src->Type);
            SET(,64);
        }
    }
    else if (Signed)
    {
        PVMEmitExtend(Emitter, Rd.As.Register, TYPE_I32, Rd.As.Register, Dst->Type);
        PVMEmitExtend(Emitter, Rs.As.Register, TYPE_I32, Rs.As.Register, Src->Type);
        SET(I, );
    }
    else
    {
        PVMEmitExtend(Emitter, Rd.As.Register, TYPE_U32, Rd.As.Register, Dst->Type);
        PVMEmitExtend(Emitter, Rs.As.Register, TYPE_U32, Rs.As.Register, Src->Type);
        SET(, );
    }
    

    if (OwningRd) 
    {
        StoreFromReg(Emitter, Dst->As.Memory, Dst->Type, Rd.As.Register, TYPE_BOOLEAN);
        PVMFreeRegister(Emitter, Rd.As.Register);
    }
    if (OwningRs) 
    {
        PVMFreeRegister(Emitter, Rs.As.Register);
    }
    return *Dst;
#undef SET
#undef FSET
}




/* stack allocation */
VarMemory PVMQueueStackAllocation(PVMEmitter *Emitter, U32 Size)
{
    U32 NewOffset = Emitter->StackSpace + Size;
    if (NewOffset % sizeof(PVMPTR))
        NewOffset = (NewOffset + sizeof(PVMPTR)) & ~(sizeof(PVMPTR) - 1);

    VarMemory Mem = {
        .IsGlobal = false,
        .Location = Emitter->StackSpace,
    };
    Emitter->StackSpace = NewOffset;
    return Mem;
}

void PVMCommitStackAllocation(PVMEmitter *Emitter)
{
    PVMEmitAddImm(Emitter, &Emitter->Reg.SP, Emitter->StackSpace);
}



VarMemory PVMEmitGlobalData(PVMEmitter *Emitter, const void *Data, U32 Size)
{
    VarMemory Global = {
        .IsGlobal = true,
        .Location = ChunkWriteGlobalData(PVMCurrentChunk(Emitter), Data, Size),
    };
    return Global;
}

/* global instructions */
VarMemory PVMEmitGlobalSpace(PVMEmitter *Emitter, U32 Size)
{
    return PVMEmitGlobalData(Emitter, NULL, Size);
}






/* subroutine */
void PVMEmitSaveCallerRegs(PVMEmitter *Emitter, UInt ReturnRegID)
{
    U16 SaveReglist = Emitter->Reglist & ~(((U16)1 << ReturnRegID) | EMPTY_REGLIST);
    if (SaveReglist & 0xFF)
    {
        WriteOp16(Emitter, PVM_REGLIST(PSHL, SaveReglist & 0xFF));
    }
    if (SaveReglist >> 8)
    {
        WriteOp16(Emitter, PVM_REGLIST(PSHH, SaveReglist >> 8));
    }

    if (Emitter->NumSavelist > PVM_MAX_CALL_IN_EXPR)
    {
        PASCAL_UNREACHABLE("TODO: make the limit on number of calls in expr dynamic or larger.");
    }
    Emitter->SavedRegisters[Emitter->NumSavelist++] = SaveReglist;
}


U32 PVMEmitCall(PVMEmitter *Emitter, VarSubroutine *Callee)
{
    U32 CurrentLocation = PVMCurrentChunk(Emitter)->Count;
    U32 Location = Callee->Location - CurrentLocation - PVM_BRANCH_INS_SIZE;
    WriteOp32(Emitter, PVM_BSR(Location >> 16), Location & 0xFFFF);
    return CurrentLocation;
}


void PVMEmitUnsaveCallerRegs(PVMEmitter *Emitter)
{
    U16 Restorelist = Emitter->SavedRegisters[--Emitter->NumSavelist];
    if (Restorelist >> 8)
    {
        WriteOp16(Emitter, PVM_REGLIST(POPH, Restorelist >> 8));
    }
    if (Restorelist & 0xFF)
    {
        WriteOp16(Emitter, PVM_REGLIST(POPL, Restorelist & 0xFF));
    }
}



/* exit/return */
void PVMEmitExit(PVMEmitter *Emitter)
{
    WriteOp16(Emitter, PVM_SYS(EXIT));
}






