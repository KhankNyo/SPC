#ifndef PASCAL_VM2_EMITTER_H
#define PASCAL_VM2_EMITTER_H

#include "Variable.h"
#include "Tokenizer.h"

#include "PVM/Chunk.h"
#include "PVM/Isa.h"




#define PVM_MAX_CALL_IN_EXPR 32


struct SaveRegInfo 
{
    U32 Size;
    U32 Regs;
    U32 RegLocation[PVM_REG_COUNT + PVM_FREG_COUNT];
};

struct PVMEmitter 
{
    PVMChunk *Chunk;
    U32 Reglist;

    U32 SpilledRegCount;
    U32 StackSpace;
    struct {
        VarLocation SP, FP, GP;
        VarLocation Flag;
    } Reg;

    bool ShouldEmit;
    VarLocation ReturnValue;
};

typedef enum PVMPatchType
{
    PATCHTYPE_SUBROUTINE_ADDR       = 0xFFFF,
    PATCHTYPE_BRANCH_UNCONDITIONAL  = 0x00FF,
    PATCHTYPE_BRANCH_FLAG           = 0x00FF,
    PATCHTYPE_BRANCH_CONDITIONAL    = 0x000F,
    PATCHTYPE_BRANCH_INC            = 0x0000,
} PVMPatchType;


PVMEmitter PVMEmitterInit(PVMChunk *Chunk);
void PVMEmitterDeinit(PVMEmitter *Emitter);
void PVMEmitterReset(PVMEmitter *Emitter, bool PreserveFunctions);

void PVMSetEntryPoint(PVMEmitter *Emitter, U32 EntryPoint);
SaveRegInfo PVMEmitterBeginScope(PVMEmitter *Emitter);
void PVMEmitSaveFrame(PVMEmitter *Emitter);
void PVMEmitterEndScope(PVMEmitter *Emitter, SaveRegInfo PrevScope);
void PVMEmitDebugInfo(PVMEmitter *Emitter, 
        const U8 *Src, U32 Len, U32 LineNum
);
void PVMUpdateDebugInfo(PVMEmitter *Emitter, U32 LineLen, bool IsSubroutine);


U32 PVMGetCurrentLocation(PVMEmitter *Emitter);
/* TODO: weird semantics between these 2 functions */
/* NOTE: FreeRegister does not free non-persistent registers, 
 * use PVMMarkRegisterAsFreed to free persistent registers (parameter regs for example) */
void PVMFreeRegister(PVMEmitter *Emitter, VarRegister Reg);
VarRegister PVMAllocateRegister(PVMEmitter *Emitter, IntegralType Type);
VarLocation PVMAllocateRegisterLocation(PVMEmitter *Emitter, VarType Type);
void PVMMarkRegisterAsAllocated(PVMEmitter *Emitter, U32 RegID);
void PVMMarkRegisterAsFreed(PVMEmitter *Emitter, UInt Reg);
bool PVMRegisterIsFree(PVMEmitter *Emitter, UInt Reg);


/* Branching instructions */
#define PVMMarkBranchTarget(pEmitter) PVMGetCurrentLocation(pEmitter)
/* returns the offset of the branch instruction for later patching */
U32 PVMEmitBranchIfFalse(PVMEmitter *Emitter, const VarLocation *Condition);
U32 PVMEmitBranchIfTrue(PVMEmitter *Emitter, const VarLocation *Condition);
U32 PVMEmitBranchOnFalseFlag(PVMEmitter *Emitter);
U32 PVMEmitBranchOnTrueFlag(PVMEmitter *Emitter);
U32 PVMEmitBranchAndInc(PVMEmitter *Emitter, VarRegister Reg, I8 By, U32 To);
/* returns the offset of the branch instruction for patching if necessary */
U32 PVMEmitBranch(PVMEmitter *Emitter, U32 To);
void PVMPatchBranch(PVMEmitter *Emitter, U32 From, U32 To, PVMPatchType Type);
void PVMPatchBranchToCurrent(PVMEmitter *Emitter, U32 From, PVMPatchType Type);


/* move and load */
void PVMEmitMov(PVMEmitter *Emitter, VarLocation *Dst, const VarLocation *Src);
void PVMEmitCopy(PVMEmitter *Emitter, const VarLocation *Dst, const VarLocation *Src);
/* returns true if the caller owns OutTarget, false otherwise, 
 * call PVMFreeRegister to dipose OutTarget if true is returned */
bool PVMEmitIntoRegLocation(PVMEmitter *Emitter, VarLocation *OutTarget, bool ReadOnly, const VarLocation *Src);
bool PVMEmitIntoReg(PVMEmitter *Emitter, VarRegister *OutTarget, bool ReadOnly, const VarLocation *Src);
void PVMEmitLoadAddr(PVMEmitter *Emitter, VarRegister Dst, VarMemory Src);
U32 PVMEmitLoadSubroutineAddr(PVMEmitter *Emitter, VarRegister Dst, U32 SubroutineAddr);
void PVMEmitLoadEffectiveAddr(PVMEmitter *Emitter, VarRegister Dst, VarMemory Src, I32 Offset);


/* type conversion */
void PVMEmitIntegerTypeConversion(PVMEmitter *Emitter, 
        VarRegister Dst, IntegralType DstType, 
        VarRegister Src, IntegralType SrcType
);
void PVMEmitFloatTypeConversion(PVMEmitter *Emitter,
        VarRegister Dst, IntegralType DstType, 
        VarRegister Src, IntegralType SrcType
);
void PVMEmitIntToFltTypeConversion(PVMEmitter *Emitter,
        VarRegister Dst, IntegralType DstType, 
        VarRegister Src, IntegralType SrcType
);

/* arith instructions */
void PVMEmitAddImm(PVMEmitter *Emitter, VarLocation *Dst, I64 Imm);
void PVMEmitAdd(PVMEmitter *Emitter, VarLocation *Dst, const VarLocation *Src);
void PVMEmitSub(PVMEmitter *Emitter, VarLocation *Dst, const VarLocation *Src);
void PVMEmitNeg(PVMEmitter *Emitter, VarLocation *Dst, const VarLocation *Src);
void PVMEmitNot(PVMEmitter *Emitter, VarLocation *Dst, const VarLocation *Src);
void PVMEmitAnd(PVMEmitter *Emitter, VarLocation *Dst, const VarLocation *Src);
void PVMEmitOr(PVMEmitter *Emitter, VarLocation *Dst, const VarLocation *Src);
void PVMEmitXor(PVMEmitter *Emitter, VarLocation *Dst, const VarLocation *Src);
void PVMEmitMul(PVMEmitter *Emitter, VarLocation *Dst, const VarLocation *Src);
void PVMEmitDiv(PVMEmitter *Emitter, VarLocation *Dst, const VarLocation *Src);
void PVMEmitIMul(PVMEmitter *Emitter, VarLocation *Dst, const VarLocation *Src);
void PVMEmitIDiv(PVMEmitter *Emitter, VarLocation *Dst, const VarLocation *Src);
void PVMEmitMod(PVMEmitter *Emitter, VarLocation *Dst, const VarLocation *Src);
void PVMEmitShl(PVMEmitter *Emitter, VarLocation *Dst, const VarLocation *Src);
void PVMEmitShr(PVMEmitter *Emitter, VarLocation *Dst, const VarLocation *Src);
void PVMEmitAsr(PVMEmitter *Emitter, VarLocation *Dst, const VarLocation *Src);
/* returns flag */
VarLocation PVMEmitSetFlag(PVMEmitter *Emitter, TokenType Op, const VarLocation *Left, const VarLocation *Right);



/* subroutine arguments */
I32 PVMStartArg(PVMEmitter *Emitter, U32 ArgSize);
VarLocation PVMSetParam(PVMEmitter *Emitter, UInt ArgNumber, VarType Type, I32 *Base);
VarLocation PVMSetArg(PVMEmitter *Emitter, UInt ArgNumber, VarType Type, I32 *Base);
void PVMMarkArgAsOccupied(PVMEmitter *Emitter, const VarLocation *Arg);

VarLocation PVMSetReturnType(PVMEmitter *Emitter, VarType Type);
/* accepts Pointer to VarLocation as args */
void PVMEmitPushMultiple(PVMEmitter *Emitter, int Count, ...);


/* stack instructions */
VarMemory PVMQueueStackArg(PVMEmitter *Emitter, U32 Size);
void PVMEmitStackAllocation(PVMEmitter *Emitter, I32 Size);

/* global instructions */
U32 PVMGetGlobalOffset(PVMEmitter *Emitter);
/* note: data must've already been allocated */
void PVMInitializeGlobal(PVMEmitter *Emitter, const VarLocation *Global, const VarLiteral *Data, USize Size, IntegralType Type);
VarMemory PVMEmitGlobalData(PVMEmitter *Emitter, const void *Data, U32 Size);
VarMemory PVMEmitGlobalSpace(PVMEmitter *Emitter, U32 Size);
#define PVMEmitGlobalAllocation(pEmitter, U32Size) PVMEmitGlobalSpace(pEmitter, U32Size)


/* call instructions */
#define NO_RETURN_REG (2*PVM_REG_COUNT)
SaveRegInfo PVMEmitSaveCallerRegs(PVMEmitter *Emitter, UInt ReturnRegID);
bool PVMRegIsSaved(SaveRegInfo Saved, UInt RegID);
VarLocation PVMRetreiveSavedCallerReg(PVMEmitter *Emitter, SaveRegInfo Saved, UInt RegID, VarType Type);

/* returns the location of the call instruction in case it needs a patch later on */
U32 PVMEmitCall(PVMEmitter *Emitter, U32 Location);
void PVMEmitCallPtr(PVMEmitter *Emitter, const VarLocation *Ptr);
void PVMEmitUnsaveCallerRegs(PVMEmitter *Emitter, UInt ReturnRegID, SaveRegInfo Save);


/* enter and exit/return */
U32 PVMEmitEnter(PVMEmitter *Emitter);
void PVMPatchEnter(PVMEmitter *Emitter, U32 Location, U32 StackSize);
void PVMEmitExit(PVMEmitter *Emitter);


/* system calls */
void PVMEmitWrite(PVMEmitter *Emitter);


#endif /* PASCAL_VM2_EMITTER_H */

