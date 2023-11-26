
#include "PVM/_Isa.h"
#include "_PVMEmitter.h"



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
                .As.Register = {
                    .ID = PVM_REG_SP,
                    .Type = TYPE_POINTER,
                },
            },
            .FP = {
                .LocationType = VAR_REG,
                .As.Register = {
                    .ID = PVM_REG_FP,
                    .Type = TYPE_POINTER,
                },
            },
            .GP = {
                .LocationType = VAR_REG,
                .As.Register = {
                    .ID = PVM_REG_GP,
                    .Type = TYPE_POINTER,
                },
            },
        },
        .ReturnValue = {
            .LocationType = VAR_REG,
            .As.Register.ID = PVM_RETREG,
            .As.Register.Type = TYPE_INVALID,
        },
    };
    for (int i = PVM_ARGREG_0; i < PVM_ARGREG_COUNT; i++)
    {
        Emitter.ArgReg[i] = (VarLocation){
            .LocationType = VAR_REG,
            .As.Register = {
                .ID = i,
                .Type = TYPE_INVALID,
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


static void LoadIntoReg(PVMEmitter *Emitter, VarRegister Dst, VarMemory Src)
{
#define LOAD(Base)\
    do {\
        if (Src.Location < UINT16_MAX) {\
            switch (Src.Type) {\
            case TYPE_BOOLEAN:\
            case TYPE_I8:\
            case TYPE_U8: WriteOp32(Emitter, PVM_OP(LD8, Dst.ID, Base), Src.Location); break;\
            case TYPE_I16:\
            case TYPE_U16: WriteOp32(Emitter, PVM_OP(LD16, Dst.ID, Base), Src.Location); break;\
            case TYPE_I32:\
            case TYPE_U32: WriteOp32(Emitter, PVM_OP(LD32, Dst.ID, Base), Src.Location); break;\
            case TYPE_I64:\
            case TYPE_U64: WriteOp32(Emitter, PVM_OP(LD64, Dst.ID, Base), Src.Location); break;\
            default: PASCAL_UNREACHABLE("TODO: Src.Size > 8"); break;\
            }\
        } else {\
            switch (Src.Type) {\
            case TYPE_BOOLEAN:\
            case TYPE_I8:\
            case TYPE_U8: WriteOp16(Emitter, PVM_OP(LD8L, Dst.ID, Base)); break;\
            case TYPE_I16:\
            case TYPE_U16: WriteOp16(Emitter, PVM_OP(LD16L, Dst.ID, Base)); break;\
            case TYPE_I32:\
            case TYPE_U32: WriteOp16(Emitter, PVM_OP(LD32L, Dst.ID, Base)); break;\
            case TYPE_I64:\
            case TYPE_U64: WriteOp16(Emitter, PVM_OP(LD64L, Dst.ID, Base)); break;\
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
    /* TODO: sign extend */
#undef LOAD
}


static void LoadFromReg(PVMEmitter *Emitter, VarMemory Dst, VarRegister Src)
{
#define STORE(Base)\
    do {\
        if (Dst.Location < UINT16_MAX) {\
            switch (Dst.Type) {\
            case TYPE_BOOLEAN:\
            case TYPE_I8:\
            case TYPE_U8: WriteOp32(Emitter, PVM_OP(ST8, Src.ID, Base), Dst.Location); break;\
            case TYPE_I16:\
            case TYPE_U16: WriteOp32(Emitter, PVM_OP(ST16, Src.ID, Base), Dst.Location); break;\
            case TYPE_I32:\
            case TYPE_U32: WriteOp32(Emitter, PVM_OP(ST32, Src.ID, Base), Dst.Location); break;\
            case TYPE_I64:\
            case TYPE_U64: WriteOp32(Emitter, PVM_OP(ST64, Src.ID, Base), Dst.Location); break;\
            default: PASCAL_UNREACHABLE("TODO: Src.Size > 8"); break;\
            }\
        } else {\
            switch (Dst.Type) {\
            case TYPE_BOOLEAN:\
            case TYPE_I8:\
            case TYPE_U8: WriteOp16(Emitter, PVM_OP(ST8L, Src.ID, Base)); break;\
            case TYPE_I16:\
            case TYPE_U16: WriteOp16(Emitter, PVM_OP(ST16L, Src.ID, Base)); break;\
            case TYPE_I32:\
            case TYPE_U32: WriteOp16(Emitter, PVM_OP(ST32L, Src.ID, Base)); break;\
            case TYPE_I64:\
            case TYPE_U64: WriteOp16(Emitter, PVM_OP(ST64L, Src.ID, Base)); break;\
            default: PASCAL_UNREACHABLE("TODO: Dst.Size > 8"); break;\
            }\
            WriteOp32(Emitter, Dst.Location, Dst.Location >> 16);\
        }\
    } while (0)

    if (Dst.IsGlobal)
    {
        STORE(PVM_REG_GP);
    }
    else
    {
        STORE(PVM_REG_FP);
    }
#undef LOAD
}


static bool PVMEmitIntoReg(PVMEmitter *Emitter, VarLocation *OutTarget, const VarLocation *Src)
{
    switch (Src->LocationType)
    {
    case VAR_REG: break;
    case VAR_MEM:
    {
        *OutTarget = PVMAllocateRegister(Emitter, Src->As.Memory.Type);
        LoadIntoReg(Emitter, OutTarget->As.Register, Src->As.Memory);
        return true;
    } break;
    case VAR_SUBROUTINE:
    {
        PASCAL_UNREACHABLE("TODO: FnPtr in EmitIntoReg()");
    } break;
    case VAR_INVALID:
    {
        PASCAL_UNREACHABLE("VAR_INVALID encountered");
    } break;
    }

    *OutTarget = *Src;
    return false;
}



static bool PVMRegisterIsFree(PVMEmitter *Emitter, UInt Reg)
{
    return ((Emitter->Reglist >> Reg) & 1) == 0;
}

void PVMMarkRegisterAsAllocated(PVMEmitter *Emitter, UInt Reg)
{
    Emitter->Reglist |= (UInt)1 << Reg;
}

static void PVMMarkRegisterAsFreed(PVMEmitter *Emitter, UInt Reg)
{
    Emitter->Reglist &= ~((UInt)1 << Reg);
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

static void MovRegister(PVMEmitter *Emitter, VarRegister Dst, VarRegister Src)
{
    switch (Dst.Type)
    {
    default: 
Unreachable:
    {
        PASCAL_UNREACHABLE("Cannot move register of type %s into %s.", 
                IntegralTypeToStr(Src.Type), IntegralTypeToStr(Dst.Type)
        );
    } break;
    case TYPE_I64:
    {
        switch (Src.Type)
        {
        case TYPE_I64: WriteOp16(Emitter, PVM_OP(MOV64, Dst.ID, Src.ID)); break;
        case TYPE_U64: WriteOp16(Emitter, PVM_OP(MOV64, Dst.ID, Src.ID)); break;
        case TYPE_I32: WriteOp16(Emitter, PVM_OP(MOVSEX64_32, Dst.ID, Src.ID)); break;
        case TYPE_U32: WriteOp16(Emitter, PVM_OP(MOVZEX64_32, Dst.ID, Src.ID)); break;
        case TYPE_I16: WriteOp16(Emitter, PVM_OP(MOVSEX64_16, Dst.ID, Src.ID)); break;
        case TYPE_U16: WriteOp16(Emitter, PVM_OP(MOVZEX64_16, Dst.ID, Src.ID)); break;
        case TYPE_I8:  WriteOp16(Emitter, PVM_OP(MOVSEX64_8, Dst.ID, Src.ID)); break;
        case TYPE_U8:  WriteOp16(Emitter, PVM_OP(MOVZEX64_8, Dst.ID, Src.ID)); break;
        default: goto Unreachable;
        }
    } break;
#if UINTPTR_MAX == UINT64_MAX
    case TYPE_POINTER:
#endif 
    case TYPE_U64:
    {
        switch (Src.Type)
        {
        case TYPE_POINTER: 
        case TYPE_I64: WriteOp16(Emitter, PVM_OP(MOV64, Dst.ID, Src.ID)); break;
        case TYPE_U64: WriteOp16(Emitter, PVM_OP(MOV64, Dst.ID, Src.ID)); break;
        case TYPE_I32:
        case TYPE_U32: WriteOp16(Emitter, PVM_OP(MOVZEX64_32, Dst.ID, Src.ID)); break;
        case TYPE_I16:
        case TYPE_U16: WriteOp16(Emitter, PVM_OP(MOVZEX64_16, Dst.ID, Src.ID)); break;
        case TYPE_I8:
        case TYPE_U8:  WriteOp16(Emitter, PVM_OP(MOVZEX64_8, Dst.ID, Src.ID)); break;
        default: goto Unreachable;
        }
    } break;
    case TYPE_I32:
    {
        switch (Src.Type)
        {
        case TYPE_I64:
        case TYPE_U64:
        case TYPE_I32:
        case TYPE_U32: WriteOp16(Emitter, PVM_OP(MOV32, Dst.ID, Src.ID)); break;
        case TYPE_I16: WriteOp16(Emitter, PVM_OP(MOVSEX64_16, Dst.ID, Src.ID)); break;
        case TYPE_U16: WriteOp16(Emitter, PVM_OP(MOVZEX32_16, Dst.ID, Src.ID)); break;
        case TYPE_I8:  WriteOp16(Emitter, PVM_OP(MOVSEX32_8, Dst.ID, Src.ID)); break;
        case TYPE_U8:  WriteOp16(Emitter, PVM_OP(MOVZEX32_8, Dst.ID, Src.ID)); break;
        default: goto Unreachable;
        }
    } break;
#if UINTPTR_MAX == UINT32_MAX
    case TYPE_POINTER:
#endif 
    case TYPE_U32:
    {
        switch (Src.Type)
        {
        case TYPE_POINTER:
        case TYPE_I64:
        case TYPE_U64:
        case TYPE_I32:
        case TYPE_U32: WriteOp16(Emitter, PVM_OP(MOV32, Dst.ID, Src.ID)); break;
        case TYPE_I16:
        case TYPE_U16: WriteOp16(Emitter, PVM_OP(MOVZEX32_16, Dst.ID, Src.ID)); break;
        case TYPE_I8:
        case TYPE_U8:  WriteOp16(Emitter, PVM_OP(MOVZEX32_8, Dst.ID, Src.ID)); break;
        default: goto Unreachable;
        }
    } break;
    case TYPE_I16:
    {
        switch (Src.Type)
        {
        case TYPE_I64:
        case TYPE_U64:
        case TYPE_I32:
        case TYPE_U32:
        case TYPE_I16:
        case TYPE_U16: WriteOp16(Emitter, PVM_OP(MOV16, Dst.ID, Src.ID)); break;
        case TYPE_I8:  WriteOp16(Emitter, PVM_OP(MOVSEX32_16, Dst.ID, Src.ID)); break;
        case TYPE_U8:  WriteOp16(Emitter, PVM_OP(MOVZEX32_8, Dst.ID, Src.ID)); break;
        default: goto Unreachable;
        }
    } break;
    case TYPE_U16:
    {
        switch (Src.Type)
        {
        case TYPE_I64:
        case TYPE_U64:
        case TYPE_I32:
        case TYPE_U32:
        case TYPE_I16:
        case TYPE_U16: WriteOp16(Emitter, PVM_OP(MOV16, Dst.ID, Src.ID)); break;
        case TYPE_I8:
        case TYPE_U8:  WriteOp16(Emitter, PVM_OP(MOVZEX32_8, Dst.ID, Src.ID)); break;
        default: goto Unreachable;
        }
    } break;
    case TYPE_I8:
    case TYPE_U8:
    case TYPE_BOOLEAN:
    {
        WriteOp16(Emitter, PVM_OP(MOV8, Dst.ID, Src.ID));
    } break;
    }
}





void PVMEmitterBeginScope(PVMEmitter *Emitter)
{
    Emitter->SavedRegisters[Emitter->NumSavelist++] = Emitter->Reglist;
    MovRegister(Emitter, Emitter->Reg.FP.As.Register, Emitter->Reg.SP.As.Register);
}

void PVMEmitterEndScope(PVMEmitter *Emitter)
{
    PASCAL_ASSERT(Emitter->NumSavelist > 0, "Unreachable");
    Emitter->Reglist = Emitter->SavedRegisters[--Emitter->NumSavelist];
    /* TODO: stack space */
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
    for (UInt i = 0; i < PVM_REG_COUNT; i++)
    {
        /* found free reg */
        if (PVMRegisterIsFree(Emitter, i))
        {
            /* mark reg as allocated */
            PVMMarkRegisterAsAllocated(Emitter, i);
            return (VarLocation) {
                .LocationType = VAR_REG,
                .As.Register.ID = i,
                .As.Register.Type = Type,
            };
        }
    }


    /* spill register */
    UInt Reg = Emitter->SpilledRegCount % PVM_REG_COUNT;
    Emitter->SpilledRegCount++;
    PVMEmitPush(Emitter, Reg);

    return (VarLocation) {
        .LocationType = VAR_REG,
        .As.Register.ID = Reg,
        .As.Register.Type = Type,
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

U32 PVMEmitBranchIfTrue(PVMEmitter *Emitter, const VarLocation *Condition)
{
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
    VarLocation Tmp;
    bool IsOwning = PVMEmitIntoReg(Emitter, &Tmp, Src);
    switch (Dst->LocationType)
    {
    case VAR_INVALID:
    case VAR_SUBROUTINE:
    {
        PASCAL_UNREACHABLE("PVMEmitMov: invalid Dst");
    } break;

    case VAR_REG:
    {
        if (Tmp.As.Register.ID == Dst->As.Register.ID)
            break;

        MovRegister(Emitter, Dst->As.Register, Tmp.As.Register);
    } break;
    case VAR_MEM:
    {
        LoadFromReg(Emitter, Dst->As.Memory, Tmp.As.Register);
    } break;
    }

    if (IsOwning)
    {
        PVMFreeRegister(Emitter, Tmp.As.Register);
    }
}


void PVMEmitExtend(PVMEmitter *Emitter, IntegralType Type, VarRegister Rd, VarRegister Rs)
{
    if (Type == Rs.Type && Rd.ID == Rs.ID)
    {
        return;
    }

    MovRegister(Emitter, Rd, Rs);
}

void PVMEmitLoadImm(PVMEmitter *Emitter, VarRegister Register, U64 Integer)
{
    ChunkWriteMovImm(PVMCurrentChunk(Emitter), Register.ID, Integer, Register.Type);
}


void PVMEmitAddImm(PVMEmitter *Emitter, const VarLocation *Dst, I16 Imm)
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
void FnName (PVMEmitter *Emitter, const VarLocation *Dst, const VarLocation *Src) {\
    VarLocation Rd, Rs;\
    bool OwningRd = PVMEmitIntoReg(Emitter, &Rd, Dst);\
    bool OwningRs = PVMEmitIntoReg(Emitter, &Rs, Src);\
    if (Rd.As.Register.Type == TYPE_U64\
    || Rd.As.Register.Type == TYPE_I64) {\
        WriteOp16(Emitter, PVM_OP( Mnemonic ## 64, Rd.As.Register.ID, Rs.As.Register.ID));\
    } else  {\
        WriteOp16(Emitter, PVM_OP( Mnemonic , Rd.As.Register.ID, Rs.As.Register.ID));\
    }\
    if (OwningRd) {\
        LoadFromReg(Emitter, Dst->As.Memory, Rd.As.Register);\
        PVMFreeRegister(Emitter, Rd.As.Register);\
    }\
    if (OwningRs) {\
        PVMFreeRegister(Emitter, Rs.As.Register);\
    }\
}\
void FnName (PVMEmitter *Emitter, const VarLocation *Dst, const VarLocation *Src)


/* kill me */
DEFINE_INTEGER_BINARY_OP(PVMEmitAdd, ADD);
DEFINE_INTEGER_BINARY_OP(PVMEmitSub, SUB);
DEFINE_INTEGER_BINARY_OP(PVMEmitNeg, NEG);
DEFINE_INTEGER_BINARY_OP(PVMEmitMul, MUL);
DEFINE_INTEGER_BINARY_OP(PVMEmitIMul, IMUL);
DEFINE_INTEGER_BINARY_OP(PVMEmitDiv, DIV);
DEFINE_INTEGER_BINARY_OP(PVMEmitIDiv, IDIV);
DEFINE_INTEGER_BINARY_OP(PVMEmitMod, MOD);


#undef DEFINE_INTEGER_BINARY_OP






bool PVMEmitSetCC(PVMEmitter *Emitter, TokenType Op, const VarLocation *Dst, const VarLocation *Src) 
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

    VarLocation Rd, Rs;
    bool OwningRd = PVMEmitIntoReg(Emitter, &Rd, Dst);
    bool OwningRs = PVMEmitIntoReg(Emitter, &Rs, Src);
    bool Signed = (IntegralTypeIsSigned(Rd.As.Register.Type) 
                || IntegralTypeIsSigned(Rs.As.Register.Type));
    bool Bits64 = TYPE_U64 == Rd.As.Register.Type
        || TYPE_I64 == Rd.As.Register.Type
        || TYPE_U64 == Rs.As.Register.Type
        || TYPE_I64 == Rs.As.Register.Type;
    UInt RdID = Rd.As.Register.ID, RsID = Rs.As.Register.ID;

    if (Bits64)
    {
        if (Signed)
        {
            PVMEmitExtend(Emitter, TYPE_I64, Rd.As.Register, Rd.As.Register);
            PVMEmitExtend(Emitter, TYPE_I64, Rs.As.Register, Rs.As.Register);
            SET(I, 64);
        }
        else
        {
            PVMEmitExtend(Emitter, TYPE_U64, Rd.As.Register, Rd.As.Register);
            PVMEmitExtend(Emitter, TYPE_U64, Rs.As.Register, Rs.As.Register);
            SET(,64);
        }
    }
    else if (Signed)
    {
        PVMEmitExtend(Emitter, TYPE_I32, Rd.As.Register, Rd.As.Register);
        PVMEmitExtend(Emitter, TYPE_I32, Rs.As.Register, Rs.As.Register);
        SET(I, );
    }
    else
    {
        PVMEmitExtend(Emitter, TYPE_U32, Rd.As.Register, Rd.As.Register);
        PVMEmitExtend(Emitter, TYPE_U32, Rs.As.Register, Rs.As.Register);
        SET(, );
    }
    

    if (OwningRd) 
    {
        LoadFromReg(Emitter, Dst->As.Memory, Rd.As.Register);
        PVMFreeRegister(Emitter, Rd.As.Register);
    }
    if (OwningRs) 
    {
        PVMFreeRegister(Emitter, Rs.As.Register);
    }
    return true;
#undef SET
}




/* stack allocation */
VarMemory PVMQueueStackAllocation(PVMEmitter *Emitter, U32 Size, IntegralType Type)
{
    U32 NewOffset = Emitter->StackSpace + Size;
    if (NewOffset % sizeof(PVMPTR))
        NewOffset = (NewOffset + sizeof(PVMPTR)) & ~(sizeof(PVMPTR) - 1);

    VarMemory Mem = {
        .IsGlobal = false,
        .Type = Type,
        .Location = Emitter->StackSpace,
    };
    Emitter->StackSpace = NewOffset;
    return Mem;
}

void PVMCommitStackAllocation(PVMEmitter *Emitter)
{
    PVMEmitAddImm(Emitter, &Emitter->Reg.SP, Emitter->StackSpace);
}



/* global instructions */
VarMemory PVMEmitGlobalSpace(PVMEmitter *Emitter, U32 Size, IntegralType Type)
{
    VarMemory Global = {
        .IsGlobal = true,
        .Type = Type,
        .Location = 
            ChunkWriteGlobalData(PVMCurrentChunk(Emitter), NULL, Size),
    };
    return Global;
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






