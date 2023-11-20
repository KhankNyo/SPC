
#include "PVMEmitter.h"




static CodeChunk *PVMCurrentChunk(PVMEmitter *Emitter)
{
    return Emitter->Chunk;
}


PVMEmitter PVMEmitterInit(CodeChunk *Chunk)
{
    PVMEmitter Emitter = {
        .Chunk = Chunk,
        .RegisterList = 0,
        .SpilledRegCount = 0,
        .GlobalDataSize = 0,
        .VarCount = 0,
        .EntryPoint = 0,
        .SavedRegisters = { 0 },
    };
    return Emitter;
}

void PVMEmitterDeinit(PVMEmitter *Emitter)
{
    PVMEmitExit(Emitter);
}


GlobalVar PVMEmitGlobalSpace(PVMEmitter *Emitter, U32 Size)
{
    GlobalVar Global = {
        .Size = Size,
        .Location = Emitter->GlobalDataSize,
    };
    Emitter->GlobalDataSize += Size / sizeof(PVMPtr);
    if (Size % sizeof(PVMPtr))
        Emitter->GlobalDataSize += 1;
    return Global;
}





U32 PVMEmitCode(PVMEmitter *Emitter, U32 Instruction)
{
    return ChunkWriteCode(PVMCurrentChunk(Emitter), Instruction);
}

void PVMEmitGlobal(PVMEmitter *Emitter, GlobalVar Global)
{
    PASCAL_UNREACHABLE("TODO: emit global");
}


void PVMEmitDebugInfo(PVMEmitter *Emitter, const U8 *Src, UInt Len, U32 Line)
{
    ChunkWriteDebugInfo(PVMCurrentChunk(Emitter), 
            Len, Src, Line
    );
}

bool PVMEmitIntoReg(PVMEmitter *Emitter, VarLocation *Target, const VarLocation *Src)
{
    switch (Src->LocationType)
    {
    case VAR_INVALID: PASCAL_UNREACHABLE("VAR_INVALID encountered"); break;

    case VAR_TMP_REG:
    case VAR_REG: 
    {
        *Target = *Src;
        return false;
    } break;
    case VAR_TMP_STK:
    case VAR_LOCAL:
    {
        *Target = PVMAllocateRegister(Emitter, Src->Type);
        PVMEmitCode(Emitter, PVM_IRD_MEM_INS(LDRS, Target->As.Reg.ID, Src->As.Local.FPOffset));
        return true;
    } break;
    case VAR_GLOBAL:
    {
        *Target = PVMAllocateRegister(Emitter, Src->Type);
        PVMEmitCode(Emitter, PVM_IRD_MEM_INS(LDG, Target->As.Reg.ID, Src->As.Global.Location));
        return true;
    } break;
    /* loads pointer to function */
    case VAR_FUNCTION:
    {
        PASCAL_UNREACHABLE("TODO: EmitIntoReg function pointer");
    } break;
    }
    return false;
}


U64 PVMEmitBranchIfFalse(PVMEmitter *Emitter, const VarLocation *Condition)
{
    VarLocation Target;
    bool IsOwning = PVMEmitIntoReg(Emitter, &Target, Condition);
    U64 CurrentOffset = PVMEmitCode(Emitter, PVM_BRIF_INS(EZ, Target.As.Reg.ID, -1));
    if (IsOwning)
    {
        PVMFreeRegister(Emitter, &Target);
    }
    return CurrentOffset;
}

U64 PVMEmitBranch(PVMEmitter *Emitter, U64 Location)
{
    I64 BrOffset = Location - PVMCurrentChunk(Emitter)->Count - 1;
    return PVMEmitCode(Emitter, PVM_BRAL_INS(BrOffset));
}

void PVMPatchBranch(PVMEmitter *Emitter, U32 StreamOffset, U32 Location, UInt ImmSize)
{
    CodeChunk *Chunk = PVMCurrentChunk(Emitter);
    U32 Instruction = Chunk->Code[StreamOffset];
    U32 Mask = ((U32)1 << ImmSize) - 1;

    U32 Offset = Location - StreamOffset - 1;
    Chunk->Code[StreamOffset] = (Instruction & ~Mask) | (Offset & Mask);
}

void PVMPatchBranchToCurrent(PVMEmitter *Emitter, U64 StreamOffset, UInt ImmSize)
{
    PVMPatchBranch(Emitter, StreamOffset, PVMCurrentChunk(Emitter)->Count, ImmSize);
}


void PVMEmitMov(PVMEmitter *Emitter, const VarLocation *Dest, const VarLocation *Src)
{
    VarLocation Tmp = { 0 };
    bool IsOwning = PVMEmitIntoReg(Emitter, &Tmp, Src);
    switch (Dest->LocationType)
    {
    case VAR_INVALID:
    case VAR_FUNCTION:
    {
        PASCAL_UNREACHABLE("PVMEmitMov: Invalid dest for mov");
    } break;

    case VAR_REG:
    case VAR_TMP_REG:
    {
        if (Dest->As.Reg.ID != Tmp.As.Reg.ID)
        {
            PVMEmitCode(Emitter, PVM_IDAT_TRANSFER_INS(MOV, Dest->As.Reg.ID, Tmp.As.Reg.ID));
        }
    } break;
    case VAR_LOCAL:
    case VAR_TMP_STK:
    {
        PVMEmitCode(Emitter, PVM_IRD_MEM_INS(STRS, Tmp.As.Reg.ID, Dest->As.Local.FPOffset));
    } break;
    case VAR_GLOBAL:
    {
        PVMEmitCode(Emitter, PVM_IRD_MEM_INS(STG, Tmp.As.Reg.ID, Dest->As.Global.Location));
    } break;
    }

    if (IsOwning)
    {
        PVMFreeRegister(Emitter, &Tmp);
    }
}

void PVMEmitLoad(PVMEmitter *Emitter, const VarLocation *Dest, U64 Integer, IntegralType IntegerType)
{
    /* TODO: sign type */
    if (IntegerType > Dest->Type)
        IntegerType = Dest->Type;

    CodeChunk *Current = PVMCurrentChunk(Emitter);

    
    VarLocation Target;
    bool IsOwning = false;
    switch (Dest->LocationType)
    {
    case VAR_REG:
    case VAR_TMP_REG:
    {
        Target = *Dest;
    } break;
    default:
    {
        IsOwning = true;
        Target = PVMAllocateRegister(Emitter, Dest->Type);
    } break;
    }

    /* fast zeroing idiom */
    if (0 == Integer)
    {
        ChunkWriteCode(Current, PVM_IRD_ARITH_INS(LDI, Target.As.Reg.ID, 0));
        if (IntegerType == TYPE_I64 || IntegerType == TYPE_U64)
            ChunkWriteCode(Current, PVM_IRD_ARITH_INS(LDZHLI, Target.As.Reg.ID, 0));
    }
    else /* load the integer */
    {
        switch (IntegerType)
        {
        case TYPE_U8:
        case TYPE_I8:
        case TYPE_I16:
        {
            ChunkWriteCode(Current, PVM_IRD_ARITH_INS(LDI, Target.As.Reg.ID, Integer));
        } break;
        case TYPE_U16:
        {
            ChunkWriteCode(Current, PVM_IRD_ARITH_INS(LDZI, Target.As.Reg.ID, Integer));
        } break;
        case TYPE_U32:
        case TYPE_I32:
        {
            ChunkWriteCode(Current, PVM_IRD_ARITH_INS(LDZI, Target.As.Reg.ID, Integer));
            ChunkWriteCode(Current, PVM_IRD_ARITH_INS(ORUI, Target.As.Reg.ID, Integer >> 16));
        } break;
        case TYPE_U64:
        {
            ChunkWriteCode(Current, PVM_IRD_ARITH_INS(LDZI, Target.As.Reg.ID, Integer >> 16));
            ChunkWriteCode(Current, PVM_IRD_ARITH_INS(ORUI, Target.As.Reg.ID, Integer));
            ChunkWriteCode(Current, PVM_IRD_ARITH_INS(LDZHLI, Target.As.Reg.ID, Integer >> 32));
            if (Integer >> 48)
                ChunkWriteCode(Current, PVM_IRD_ARITH_INS(ORHUI, Target.As.Reg.ID, Integer >> 48));
        } break;
        case TYPE_I64:
        {
            ChunkWriteCode(Current, PVM_IRD_ARITH_INS(LDZI, Target.As.Reg.ID, Integer >> 16));
            ChunkWriteCode(Current, PVM_IRD_ARITH_INS(ORUI, Target.As.Reg.ID, Integer));
            if (Integer >> 48 == 0xFFFF)
            {
                ChunkWriteCode(Current, PVM_IRD_ARITH_INS(LDHLI, Target.As.Reg.ID, Integer >> 32));
            }
            else 
            {
                ChunkWriteCode(Current, PVM_IRD_ARITH_INS(LDZHLI, Target.As.Reg.ID, Integer >> 32));
                ChunkWriteCode(Current, PVM_IRD_ARITH_INS(ORHUI, Target.As.Reg.ID, Integer >> 48));
            }
        } break;
        default: PASCAL_UNREACHABLE("Invalid integer type: %d", IntegerType);
        }
    }

    if (IsOwning)
    {
        PVMEmitMov(Emitter, Dest, &Target);
        PVMFreeRegister(Emitter, &Target);
    }
}


void PVMEmitAddImm(PVMEmitter *Emitter, const VarLocation *Dest, I16 Imm)
{
    VarLocation Target;
    bool IsOwning = PVMEmitIntoReg(Emitter, &Target, Dest);

    PVMEmitCode(Emitter, PVM_IRD_ARITH_INS(ADD, Target.As.Reg.ID, Imm));

    if (IsOwning)
    {
        PVMEmitMov(Emitter, Dest, &Target);
        PVMFreeRegister(Emitter, &Target);
    }
}


void PVMEmitAdd(PVMEmitter *Emitter, const VarLocation *Dest, const VarLocation *Left, const VarLocation *Right)
{
    VarLocation Target, LeftReg, RightReg;
    bool IsOwningTarget, IsOwningLeft, IsOwningRight;
    IsOwningTarget = PVMEmitIntoReg(Emitter, &Target, Dest);
    IsOwningLeft = PVMEmitIntoReg(Emitter, &LeftReg, Left);
    IsOwningRight = PVMEmitIntoReg(Emitter, &RightReg, Right);

    PVMEmitCode(Emitter, PVM_IDAT_ARITH_INS(ADD, Target.As.Reg.ID, LeftReg.As.Reg.ID, RightReg.As.Reg.ID, 0));

    if (IsOwningTarget)
    {
        PVMEmitMov(Emitter, Dest, &Target);
        PVMFreeRegister(Emitter, &Target);
    }
    if (IsOwningLeft)
        PVMFreeRegister(Emitter, &LeftReg);
    if (IsOwningRight)
        PVMFreeRegister(Emitter, &RightReg);
}


void PVMEmitSub(PVMEmitter *Emitter, const VarLocation *Dest, const VarLocation *Left, const VarLocation *Right)
{
    VarLocation Target, LeftReg, RightReg;
    bool IsOwningTarget, IsOwningLeft, IsOwningRight;
    IsOwningTarget = PVMEmitIntoReg(Emitter, &Target, Dest);
    IsOwningLeft = PVMEmitIntoReg(Emitter, &LeftReg, Left);
    IsOwningRight = PVMEmitIntoReg(Emitter, &RightReg, Right);

    PVMEmitCode(Emitter, PVM_IDAT_ARITH_INS(SUB, Target.As.Reg.ID, LeftReg.As.Reg.ID, RightReg.As.Reg.ID, 0));

    if (IsOwningTarget)
    {
        PVMEmitMov(Emitter, Dest, &Target);
        PVMFreeRegister(Emitter, &Target);
    }
    if (IsOwningLeft)
        PVMFreeRegister(Emitter, &LeftReg);
    if (IsOwningRight)
        PVMFreeRegister(Emitter, &RightReg);
}

void PVMEmitMul(PVMEmitter *Emitter, const VarLocation *Dest, const VarLocation *Left, const VarLocation *Right)
{
    VarLocation Target, LeftReg, RightReg;
    bool IsOwningTarget, IsOwningLeft, IsOwningRight;
    IsOwningTarget = PVMEmitIntoReg(Emitter, &Target, Dest);
    IsOwningLeft = PVMEmitIntoReg(Emitter, &LeftReg, Left);
    IsOwningRight = PVMEmitIntoReg(Emitter, &RightReg, Right);

    PVMEmitCode(Emitter, 
            PVM_IDAT_SPECIAL_INS(MUL, 
                Target.As.Reg.ID, LeftReg.As.Reg.ID, RightReg.As.Reg.ID, 
                false, 0
            )
    );

    if (IsOwningTarget)
    {
        PVMEmitMov(Emitter, Dest, &Target);
        PVMFreeRegister(Emitter, &Target);
    }
    if (IsOwningLeft)
        PVMFreeRegister(Emitter, &LeftReg);
    if (IsOwningRight)
        PVMFreeRegister(Emitter, &RightReg);
}

void PVMEmitDiv(PVMEmitter *Emitter, const VarLocation *Dividend, const VarLocation *Remainder, const VarLocation *Left, const VarLocation *Right)
{
    VarLocation Target, TargetRemainder, LeftReg, RightReg;
    bool IsOwningTarget, IsOwningRemainder, IsOwningLeft, IsOwningRight;
    IsOwningTarget = PVMEmitIntoReg(Emitter, &Target, Dividend);
    IsOwningRemainder = PVMEmitIntoReg(Emitter, &TargetRemainder, Remainder);
    IsOwningLeft = PVMEmitIntoReg(Emitter, &LeftReg, Left);
    IsOwningRight = PVMEmitIntoReg(Emitter, &RightReg, Right);

    PVMEmitCode(Emitter, 
            PVM_IDAT_SPECIAL_INS(DIV, 
                Target.As.Reg.ID, LeftReg.As.Reg.ID, RightReg.As.Reg.ID, 
                false, TargetRemainder.As.Reg.ID
            )
    );

    if (IsOwningTarget)
    {
        PVMEmitMov(Emitter, Dividend, &Target);
        PVMFreeRegister(Emitter, &Target);
    }
    if (IsOwningRemainder)
    {
        PVMEmitMov(Emitter, Remainder, &TargetRemainder);
        PVMFreeRegister(Emitter, &TargetRemainder);
    }
    if (IsOwningLeft)
        PVMFreeRegister(Emitter, &LeftReg);
    if (IsOwningRight)
        PVMFreeRegister(Emitter, &RightReg);
}

void PVMEmitSetCC(PVMEmitter *Emitter, TokenType Op, const VarLocation *Dest, const VarLocation *Left, const VarLocation *Right)
{
    VarLocation Target, LeftReg, RightReg;
    bool IsOwningTarget, IsOwningLeft, IsOwningRight;

    IsOwningTarget = PVMEmitIntoReg(Emitter, &Target, Dest);
    IsOwningLeft = PVMEmitIntoReg(Emitter, &LeftReg, Left);
    IsOwningRight = PVMEmitIntoReg(Emitter, &RightReg, Right);

#define SET(Size, ...)\
    do {\
        switch (Op) {\
        case TOKEN_EQUAL:           PVMEmitCode(Emitter, PVM_IDAT_CMP_INS(SEQ ## Size, Target.As.Reg.ID, LeftReg.As.Reg.ID, RightReg.As.Reg.ID)); break;\
        case TOKEN_LESS_GREATER:    PVMEmitCode(Emitter, PVM_IDAT_CMP_INS(SNE ## Size, Target.As.Reg.ID, LeftReg.As.Reg.ID, RightReg.As.Reg.ID)); break;\
        case TOKEN_LESS:            PVMEmitCode(Emitter, PVM_IDAT_CMP_INS(SLT ## Size, Target.As.Reg.ID, LeftReg.As.Reg.ID, RightReg.As.Reg.ID)); break;\
        case TOKEN_GREATER:         PVMEmitCode(Emitter, PVM_IDAT_CMP_INS(SGT ## Size, Target.As.Reg.ID, LeftReg.As.Reg.ID, RightReg.As.Reg.ID)); break;\
        case TOKEN_GREATER_EQUAL:   PVMEmitCode(Emitter, PVM_IDAT_CMP_INS(SLT ## Size, Target.As.Reg.ID, RightReg.As.Reg.ID, LeftReg.As.Reg.ID)); break;\
        case TOKEN_LESS_EQUAL:      PVMEmitCode(Emitter, PVM_IDAT_CMP_INS(SGT ## Size, Target.As.Reg.ID, RightReg.As.Reg.ID, LeftReg.As.Reg.ID)); break;\
        default: {\
            PASCAL_UNREACHABLE(__VA_ARGS__);\
        } break;\
        }\
    }while(0)

#define SIGNEDSET(Size, ...)\
    do {\
        switch (Op) {\
        case TOKEN_EQUAL:           PVMEmitCode(Emitter, PVM_IDAT_CMP_INS(SEQ ## Size, Target.As.Reg.ID, LeftReg.As.Reg.ID, RightReg.As.Reg.ID)); break;\
        case TOKEN_LESS_GREATER:    PVMEmitCode(Emitter, PVM_IDAT_CMP_INS(SNE ## Size, Target.As.Reg.ID, LeftReg.As.Reg.ID, RightReg.As.Reg.ID)); break;\
        case TOKEN_LESS:            PVMEmitCode(Emitter, PVM_IDAT_CMP_INS(SSLT ## Size, Target.As.Reg.ID, LeftReg.As.Reg.ID, RightReg.As.Reg.ID)); break;\
        case TOKEN_GREATER:         PVMEmitCode(Emitter, PVM_IDAT_CMP_INS(SSGT ## Size, Target.As.Reg.ID, LeftReg.As.Reg.ID, RightReg.As.Reg.ID)); break;\
        case TOKEN_GREATER_EQUAL:   PVMEmitCode(Emitter, PVM_IDAT_CMP_INS(SSLT ## Size, Target.As.Reg.ID, RightReg.As.Reg.ID, LeftReg.As.Reg.ID)); break;\
        case TOKEN_LESS_EQUAL:      PVMEmitCode(Emitter, PVM_IDAT_CMP_INS(SSGT ## Size, Target.As.Reg.ID, RightReg.As.Reg.ID, LeftReg.As.Reg.ID)); break;\
        default: {\
            PASCAL_UNREACHABLE(__VA_ARGS__);\
        } break;\
        }\
    }while(0)



    switch (Dest->Type)
    {
    case TYPE_I64: SIGNEDSET(P, "(Signed) PVMEmitSetCC: PVMPtr: %s is not valid", TokenTypeToStr(Op)); break;
    case TYPE_I32: SIGNEDSET(W, "(Signed) PVMEmitSetCC: PVMWord: %s is not valid", TokenTypeToStr(Op)); break;
    case TYPE_I16: SIGNEDSET(H, "(Signed) PVMEmitSetCC: PVMHalf: %s is not valid", TokenTypeToStr(Op)); break;
    case TYPE_I8:  SIGNEDSET(B, "(Signed) PVMEmitSetCC: PVMByte: %s is not valid", TokenTypeToStr(Op)); break;
    case TYPE_U64: SET(P, "PVMEmitSetCC: PVMPtr: %s is not valid", TokenTypeToStr(Op)); break;
    case TYPE_U32: SET(W, "PVMEmitSetCC: PVMWord: %s is not valid", TokenTypeToStr(Op)); break;
    case TYPE_U16: SET(H, "PVMEmitSetCC: PVMHalf: %s is not valid", TokenTypeToStr(Op)); break;
    case TYPE_U8:  SET(B, "PVMEmitSetCC: PVMByte: %s is not valid", TokenTypeToStr(Op)); break;
    default: PASCAL_UNREACHABLE("Invalid type for setcc: %s", IntegralTypeToStr(Dest->Type)); break;
    }

    if (IsOwningTarget)
    {
        PVMEmitMov(Emitter, Dest, &Target);
        PVMFreeRegister(Emitter, &Target);
    }
    if (IsOwningLeft)
        PVMFreeRegister(Emitter, &LeftReg);
    if (IsOwningRight)
        PVMFreeRegister(Emitter, &RightReg);

#undef SET
#undef SIGNEDSET
}



void PVMEmitPush(PVMEmitter *Emitter, UInt RegID)
{
    if (RegID >= 16)
    {
        RegID -= 16;
        PVMEmitCode(Emitter, PVM_IRD_MEM_INS(PSHU, 0, 1 << RegID));
    }
    else 
    {
        PVMEmitCode(Emitter, PVM_IRD_MEM_INS(PSHL, 0, 1 << RegID));
    }
}


void PVMEmitPop(PVMEmitter *Emitter, UInt RegID)
{
    if (RegID >= 16)
    {
        RegID -= 16;
        PVMEmitCode(Emitter, PVM_IRD_MEM_INS(POPU, 0, 1 << RegID));
    }
    else 
    {
        PVMEmitCode(Emitter, PVM_IRD_MEM_INS(POPL, 0, 1 << RegID));
    }
}


void PVMEmitAddSp(PVMEmitter *Emitter, I32 Offset)
{
    if (INT21_MIN <= Offset && Offset <= INT21_MAX)
    {
        PVMEmitCode(Emitter, PVM_ADDSP_INS(Offset));
    }
    else 
    {
        PASCAL_UNREACHABLE("Immediate is too large to add to sp");
    }
}


void PVMEmitSaveCallerRegs(PVMEmitter *Emitter)
{
    /* check all locals in current scope and update their location */
    if (0 == Emitter->RegisterList)
        return;

    UInt RegList = 0;
    for (UInt i = 0; i < 16; i++)
    {
        if (!PVMRegisterIsFree(Emitter, i))
        {
            RegList |= (UInt)1 << i;
        }
    }

    Emitter->SavedRegisters[Emitter->CurrentScopeDepth] = RegList;
    PASCAL_UNREACHABLE("TODO: update location of saved variables");
    PVMEmitCode(Emitter, PVM_IRD_MEM_INS(PSHL, 0, RegList));
}

void PVMEmitCall(PVMEmitter *Emitter, FunctionVar *Function)
{
    /* TODO: function is assumed to have been defined before a call,
     *  though they can be forward declared without defined */

    PASCAL_ASSERT(NULL != Function, "TODO: forward decl");
    U32 CurrentLocation = PVMCurrentChunk(Emitter)->Count;
    PVMEmitCode(Emitter, PVM_BSR_INS(Function->Location - CurrentLocation - 1));
}

void PVMEmitUnsaveCallerRegs(PVMEmitter *Emitter)
{
    if (Emitter->SavedRegisters[Emitter->CurrentScopeDepth] == 0)
        return;

    /* pop all caller save regs */
    PVMEmitCode(Emitter, PVM_IRD_MEM_INS(POPL, 0, 
            Emitter->SavedRegisters[Emitter->CurrentScopeDepth]
    ));

    /* TODO: */
    PASCAL_UNREACHABLE("TODO: unsave caller regs");

    Emitter->SavedRegisters[Emitter->CurrentScopeDepth] = 0;
}


void PVMEmitReturn(PVMEmitter *Emitter)
{
    PVMAllocateStackSpace(Emitter, -Emitter->SP);
    PVMEmitCode(Emitter, PVM_RET_INS);
}


void PVMEmitExit(PVMEmitter *Emitter)
{
    PVMEmitCode(Emitter, PVM_SYS_INS(EXIT));
}






U32 PVMAllocateStackSpace(PVMEmitter *Emitter, UInt Slots)
{
    U32 CurrentSP = Emitter->SP;
    Emitter->SP += Slots;
    if (Slots != 0)
        PVMEmitAddSp(Emitter, (I32)Slots);
    return CurrentSP;
}


VarLocation PVMAllocateRegister(PVMEmitter *Emitter, IntegralType Type)
{
    PASCAL_ASSERT(Type != TYPE_INVALID, "Emitter received invalid type");
    for (UInt i = 0; i < PVM_REG_COUNT; i++)
    {
        /* found free reg */
        if (PVMRegisterIsFree(Emitter, i))
        {
            /* mark reg as allocated */
            PVMMarkRegisterAsAllocated(Emitter, i);
            return (VarLocation) {
                .LocationType = VAR_TMP_REG,
                .Type = Type,
                .As.Reg.ID = i,
            };
        }
    }


    /* spill register */
    UInt Reg = Emitter->SpilledRegCount % PVM_REG_COUNT;
    Emitter->SpilledRegCount++;
    PVMEmitPush(Emitter, Reg);

    return (VarLocation) {
        .LocationType = VAR_TMP_REG,
        .Type = Type,
        .As.Reg.ID = Reg,
    };
}


void PVMFreeRegister(PVMEmitter *Emitter, const VarLocation *Register)
{
    UInt Reg = (Emitter->SpilledRegCount - 1) % PVM_REG_COUNT;

    if (Emitter->SpilledRegCount > 0 && Reg == Register->As.Reg.ID)
    {
        Emitter->SpilledRegCount--;
        PVMEmitPop(Emitter, Reg);
    }
    else
    {
        Emitter->RegisterList &= ~(1u << Register->As.Reg.ID);
    }
}


void PVMMarkRegisterAsAllocated(PVMEmitter *Emitter, UInt RegID)
{
    Emitter->RegisterList |= (U32)1 << RegID;
}

bool PVMRegisterIsFree(PVMEmitter *Emitter, UInt RegID)
{
    return ((Emitter->RegisterList >> RegID) & 1u) == 0;
}

void PVMSaveRegister(PVMEmitter *Emitter, UInt RegID)
{
    PASCAL_UNREACHABLE("TODO: save register");
}




