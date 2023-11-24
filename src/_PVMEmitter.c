
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
        .CurrentScopeDepth = 0,
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
    };
    return Emitter;
}

void PVMEmitterDeinit(PVMEmitter *Emitter)
{
    PVMEmitExit(Emitter);
}


void PVMEmitterBeginScope(PVMEmitter *Emitter)
{
    Emitter->SavedRegisters[Emitter->CurrentScopeDepth++] = Emitter->Reglist;
}

void PVMEmitterEndScope(PVMEmitter *Emitter)
{
    PASCAL_ASSERT(Emitter->CurrentScopeDepth > 0, "Unreachable");
    Emitter->Reglist = Emitter->SavedRegisters[--Emitter->CurrentScopeDepth];
    /* TODO: stack space */
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
    case VAR_REG:
    {
        *OutTarget = *Src;
    } break;
    case VAR_MEM:
    {
        *OutTarget = PVMAllocRegister(Emitter, Src->As.Memory.Type);
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

    return false;
}



static bool PVMRegisterIsFree(PVMEmitter *Emitter, UInt Reg)
{
    return (Emitter->Reglist >> Reg) & 1;
}

static void PVMMarkRegisterAsAllocated(PVMEmitter *Emitter, UInt Reg)
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








U32 PVMGetCurrentLocation(PVMEmitter *Emitter)
{
    return PVMCurrentChunk(Emitter)->Count;
}

VarLocation PVMAllocRegister(PVMEmitter *Emitter, IntegralType Type)
{
    PASCAL_ASSERT(Type != TYPE_INVALID, "PVMAllocRegister: received invalid type");
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

U32 PVMEmitBranch(PVMEmitter *Emitter, U32 To)
{
    U32 Offset = To - PVMCurrentChunk(Emitter)->Count - 1;
    return WriteOp32(Emitter, PVM_BR(Offset >> 16), Offset & 0xFFFF);
}

void PVMPatchBranch(PVMEmitter *Emitter, U32 From, U32 To, PVMBranchType Type)
{
    U32 Offset = To - From - 1;
    PVMCurrentChunk(Emitter)->Code[From] |= Type & (Offset >> 16);
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
        if (Tmp.As.Register.ID != Dst->As.Register.ID)
            WriteOp16(Emitter, PVM_OP(MOV64, Dst->As.Register.ID, Tmp.As.Register.ID));
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

void PVMEmitLoad(PVMEmitter *Emitter, const VarLocation *Dst, U64 Integer, IntegralType IntType)
{
    VarLocation Target;
    if (VAR_MEM == Dst->LocationType)
        Target = PVMAllocRegister(Emitter, IntType);

    ChunkWriteMovImm(PVMCurrentChunk(Emitter), Target.As.Register.ID, Integer, IntType);

    if (VAR_MEM == Dst->LocationType)
    {
        LoadFromReg(Emitter, Dst->As.Memory, Target.As.Register);
        PVMFreeRegister(Emitter, Target.As.Register);
    }
}


void PVMEmitAddImm(PVMEmitter *Emitter, const VarLocation *Dst, I16 Imm)
{
    VarLocation Target;
    bool IsOwning = PVMEmitIntoReg(Emitter, &Target, Dst);

    if (IS_SMALL_IMM(Imm))
    {
        WriteOp16(Emitter, PVM_OP(ADDQI, Target.As.Register.ID, Imm));
    }
    else 
    {
        WriteOp16(Emitter, PVM_OP(ADDI, Target.As.Register.ID, Imm));
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
DEFINE_INTEGER_BINARY_OP(PVMEmitMul, MUL);
DEFINE_INTEGER_BINARY_OP(PVMEmitIMul, IMUL);
DEFINE_INTEGER_BINARY_OP(PVMEmitDiv, DIV);
DEFINE_INTEGER_BINARY_OP(PVMEmitIDiv, IDIV);
DEFINE_INTEGER_BINARY_OP(PVMEmitMod, MOD);

#undef DEFINE_INTEGER_BINARY_OP


bool PVMEmitSetCC(PVMEmitter *Emitter, TokenType Op, const VarLocation *Dst, const VarLocation *Src) 
{
#define UNREACH 0
#define NOP 1
#define NOPWARN 2
#define RDRSGWARN 6
#define RDRSG 7
#define RDGRSWARN 8
#define RDGRS 9
#define RDRSWARN 10
#define RDRS 11
#define RD 12
#define RS 13
#define RDWARN 14
#define RSWARN 15
#define SET(SignPrefix, BitPostfix)\
do {\
    switch (Op) {\
    case TOKEN_EQUAL:           WriteOp16(Emitter, PVM_OP(SEQ ## BitPostfix, RdID, RsID)); break;\
    case TOKEN_LESS_GREATER:    WriteOp16(Emitter, PVM_OP(SNE ## BitPostfix, RdID, RsID)); break;\
    case TOKEN_LESS:            WriteOp16(Emitter, PVM_OP(SignPrefix ## SLT ## BitPostfix, RdID, RsID)); break;\
    case TOKEN_GREATER:         WriteOp16(Emitter, PVM_OP(SignPrefix ## SGT ## BitPostfix, RdID, RsID)); break;\
    case TOKEN_GREATER_EQUAL:   WriteOp16(Emitter, PVM_OP(SignPrefix ## SGE ## BitPostfix, RsID, RdID)); break;\
    case TOKEN_LESS_EQUAL:      WriteOp16(Emitter, PVM_OP(SignPrefix ## SLE ## BitPostfix, RsID, RdID)); break;\
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


    static U16 OpcodeLut[TYPE_COUNT][TYPE_COUNT] = {
        [TYPE_I64] = {
            [TYPE_U64] = NOPWARN,
            [TYPE_I64] = NOP,
            [TYPE_U32] = PVM_OP(MOVSEX64_32, RSWARN, 0),
            [TYPE_I32] = PVM_OP(MOVSEX64_32, RS, 0),
            [TYPE_U16] = PVM_OP(MOVSEX64_16, RSWARN, 0),
            [TYPE_I16] = PVM_OP(MOVSEX64_16, RS, 0),
            [TYPE_U8]  = PVM_OP(MOVSEX64_8, RSWARN, 0),
            [TYPE_I8]  = PVM_OP(MOVSEX64_8, RS, 0),
        },
        [TYPE_U64] = {
            [TYPE_I64] = NOPWARN,
            [TYPE_U64] = NOP,
            [TYPE_I32] = PVM_OP(MOVZEX64_32, RSWARN, 0),
            [TYPE_U32] = PVM_OP(MOVZEX64_32, RS, 0),
            [TYPE_I16] = PVM_OP(MOVZEX64_16, RSWARN, 0),
            [TYPE_U16] = PVM_OP(MOVZEX64_16, RS, 0),
            [TYPE_I8]  = PVM_OP(MOVZEX64_8, RSWARN, 0),
            [TYPE_U8]  = PVM_OP(MOVZEX64_8, RS, 0),
        },
        [TYPE_I32] = {
            [TYPE_U64] = PVM_OP(MOVZEX64_32, RDWARN, 0),
            [TYPE_I64] = PVM_OP(MOVSEX64_32, RD, 0),
            [TYPE_U32] = NOPWARN, 
            [TYPE_I32] = NOP,
            [TYPE_U16] = PVM_OP(MOVSEX32_16, RSWARN, 0),
            [TYPE_I16] = PVM_OP(MOVSEX32_16, RS, 0),
            [TYPE_U8]  = PVM_OP(MOVSEX32_8, RSWARN, 0),
            [TYPE_I8]  = PVM_OP(MOVSEX32_8, RS, 0),
        },
        [TYPE_U32] = {
            [TYPE_I64] = PVM_OP(MOVSEX64_32, RDWARN, 0),
            [TYPE_U64] = PVM_OP(MOVZEX64_32, RD, 0),
            [TYPE_I32] = NOPWARN, 
            [TYPE_U32] = NOP,
            [TYPE_I16] = PVM_OP(MOVZEX32_16, RSWARN, 0),
            [TYPE_U16] = PVM_OP(MOVZEX32_16, RS, 0),
            [TYPE_I8]  = PVM_OP(MOVZEX32_8, RSWARN, 0),
            [TYPE_U8]  = PVM_OP(MOVZEX32_8, RS, 0),        
        },
        [TYPE_I16] = {
            [TYPE_U64] = PVM_OP(MOVZEX64_16, RDWARN, 0),
            [TYPE_I64] = PVM_OP(MOVSEX64_16, RD, 0),
            [TYPE_U32] = PVM_OP(MOVZEX32_16, RDWARN, 0), 
            [TYPE_I32] = PVM_OP(MOVSEX32_16, RD, 0),
            [TYPE_U16] = PVM_OP(MOVSEX32_16, RDRSWARN, 0),
            [TYPE_I16] = PVM_OP(MOVSEX32_16, RDRS, 0),
            [TYPE_U8]  = PVM_OP(MOVSEX32_8, RDGRSWARN, 0),
            [TYPE_I8]  = PVM_OP(MOVSEX32_8, RDGRS, 0),
        },
        [TYPE_U16] = {
            [TYPE_I64] = PVM_OP(MOVSEX64_16, RDWARN, 0),
            [TYPE_U64] = PVM_OP(MOVZEX64_16, RD, 0),
            [TYPE_I32] = PVM_OP(MOVSEX32_16, RDWARN, 0), 
            [TYPE_U32] = PVM_OP(MOVZEX32_16, RD, 0),
            [TYPE_I16] = PVM_OP(MOVZEX32_16, RDRSWARN, 0),
            [TYPE_U16] = PVM_OP(MOVZEX32_16, RDRS, 0),
            [TYPE_I8]  = PVM_OP(MOVZEX32_8, RDGRSWARN, 0),
            [TYPE_U8]  = PVM_OP(MOVZEX32_8, RDGRS, 0),
        },
        [TYPE_I8] = {
            [TYPE_U64] = PVM_OP(MOVZEX64_8, RDWARN, 0),
            [TYPE_I64] = PVM_OP(MOVSEX64_8, RD, 0),
            [TYPE_U32] = PVM_OP(MOVZEX32_8, RDWARN, 0), 
            [TYPE_I32] = PVM_OP(MOVSEX32_8, RD, 0),
            [TYPE_U16] = PVM_OP(MOVSEX32_8, RDRSGWARN, 0),
            [TYPE_I16] = PVM_OP(MOVSEX32_8, RDRSG, 0),
            [TYPE_U8]  = PVM_OP(MOVSEX32_8, RDRSWARN, 0),
            [TYPE_I8]  = PVM_OP(MOVSEX32_8, RDRS, 0),
        },
        [TYPE_U8] = {
            [TYPE_I64] = PVM_OP(MOVSEX64_8, RDWARN, 0),
            [TYPE_U64] = PVM_OP(MOVZEX64_8, RD, 0),
            [TYPE_I32] = PVM_OP(MOVSEX32_8, RDWARN, 0), 
            [TYPE_U32] = PVM_OP(MOVZEX32_8, RD, 0),
            [TYPE_I16] = PVM_OP(MOVZEX32_8, RDRSGWARN, 0),
            [TYPE_U16] = PVM_OP(MOVZEX32_8, RDRSG, 0),
            [TYPE_I8]  = PVM_OP(MOVZEX32_8, RDRSWARN, 0),
            [TYPE_U8]  = PVM_OP(MOVZEX32_8, RDRS, 0),
        },
    };
    U16 Opcode = OpcodeLut[Rd.As.Register.Type][Rs.As.Register.Type];
    U16 Instruction = Opcode & 0xFF00;

    bool NoWarning = true;
    if (NOP == Opcode)
    {
        /* nothing to do */
    }
    else if (NOPWARN == Opcode)
    {
        /* TODO: warning */
        NoWarning = false;
    }
    else if (UNREACH == Opcode)
    {
        PASCAL_UNREACHABLE("Invalid Type for SetCC: %s and %s\n", 
                IntegralTypeToStr(Rd.As.Register.Type),
                IntegralTypeToStr(Rs.As.Register.Type)
        );
    }
    else switch (PVM_GET_RD(Opcode))
    {
    case RDWARN: NoWarning = false;
    case RD:
    {
        WriteOp16(Emitter, Instruction | (RdID << 4) | RdID);
    } break;
    case RSWARN: NoWarning = false;
    case RS:
    {
        WriteOp16(Emitter, Instruction | (RsID << 4) | RsID);
    } break;
    case RDRSWARN: NoWarning = false;
    case RDRS:
    {
        WriteOp16(Emitter, Instruction | (RsID << 4) | RsID);
        WriteOp16(Emitter, Instruction | (RdID << 4) | RdID);
    } break;
    case RDGRSWARN: NoWarning = false;
    case RDGRS:
    {
        WriteOp16(Emitter, Instruction | (RsID << 4) | RsID);
        if (IntegralTypeIsSigned(Rd.As.Register.Type))
            WriteOp16(Emitter, PVM_OP(MOVZEX32_16, RdID, RdID));
        else
            WriteOp16(Emitter, PVM_OP(MOVSEX32_16, RdID, RdID));
    } break;
    case RDRSGWARN: NoWarning = false;
    case RDRSG:
    {
        WriteOp16(Emitter, Instruction | (RdID << 4) | RdID);
        if (IntegralTypeIsSigned(Rs.As.Register.Type))
            WriteOp16(Emitter, PVM_OP(MOVZEX32_16, RsID, RsID));
        else
            WriteOp16(Emitter, PVM_OP(MOVSEX32_16, RsID, RsID));
    } break;
    }


    if (Bits64)
    {
        if (Signed)
            SET(I, 64);
        else
            SET(,64);
    }
    else if (Signed)
    {
        SET(I, );
    }
    else
    {
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
    return NoWarning;

#undef UNREACH
#undef NOP
#undef NOPWARN
#undef RDRSGWARN
#undef RDRSG
#undef RDGRSWARN
#undef RDGRS
#undef RDRSWARN
#undef RDRS
#undef RD
#undef RS
#undef RDWARN
#undef RSWARN
#undef SET
}




/* stack allocation */
VarMemory PVMEQueueStackAllocation(PVMEmitter *Emitter, U32 Size, IntegralType Type)
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





/* subroutine */
void PVMEmitSaveCallerRegs(PVMEmitter *Emitter, UInt ReturnRegID)
{
    U16 SaveReglist = Emitter->Reglist & ~((U16)1 << ReturnRegID);
    if (SaveReglist & 0xFF)
        WriteOp16(Emitter, PVM_REGLIST(PSHH, SaveReglist & 0xFF));
    if (SaveReglist >> 8)
        WriteOp16(Emitter, PVM_REGLIST(PSHH, SaveReglist >> 8));

    if (Emitter->NumSavelist > PVM_MAX_CALL_IN_EXPR)
    {
        PASCAL_UNREACHABLE("TODO: make the limit on number of calls in expr dynamic or larger.");
    }
    Emitter->SavedRegisters[Emitter->NumSavelist++] = SaveReglist;
}


U32 PVMEmitCall(PVMEmitter *Emitter, VarSubroutine *Callee)
{
    U32 CurrentLocation = PVMCurrentChunk(Emitter)->Count;
    U32 Location = Callee->Location - CurrentLocation - 1;
    WriteOp32(Emitter, PVM_BSR(Location >> 16), Location & 0xFFFF);
    return CurrentLocation;
}


void PVMEmitUnsaveCallerRegs(PVMEmitter *Emitter)
{
    U16 Restorelist = Emitter->SavedRegisters[--Emitter->NumSavelist];
    if (Restorelist >> 8)
        WriteOp16(Emitter, PVM_REGLIST(POPH, Restorelist >> 8));
    if (Restorelist & 0xFF)
        WriteOp16(Emitter, PVM_REGLIST(POPL, Restorelist & 0xFF));
}



/* exit/return */
void PVMEmitExit(PVMEmitter *Emitter)
{
    WriteOp16(Emitter, PVM_SYS(EXIT));
}






