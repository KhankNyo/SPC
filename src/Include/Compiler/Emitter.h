#ifndef PASCAL_VM2_EMITTER_H
#define PASCAL_VM2_EMITTER_H

#include "Variable.h"
#include "Tokenizer.h"

#include "PVM/Chunk.h"
#include "PVM/Isa.h"




#define PVM_STACK_ALIGNMENT sizeof(PVMGPR)
#define PVM_GLOBAL_ALIGNMENT sizeof(U32)


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

PVMEmitter PVMEmitterInit(PVMChunk *Chunk);
void PVMEmitterDeinit(PVMEmitter *Emitter);
void PVMEmitterReset(PVMEmitter *Emitter, bool PreserveFunctions);

void PVMSetEntryPoint(PVMEmitter *Emitter, U32 EntryPoint);
SaveRegInfo PVMEmitterBeginScope(PVMEmitter *Emitter);
void PVMEmitterEndScope(PVMEmitter *Emitter, SaveRegInfo PrevScope);
void PVMEmitDebugInfo(PVMEmitter *Emitter, const U8 *Src, U32 Len, U32 LineNum);
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
U32 PVMEmitBranchAndInc(PVMEmitter *Emitter, VarRegister Reg, I8 By, U32 To);
/* returns the offset of the branch instruction for patching if necessary */
U32 PVMEmitBranch(PVMEmitter *Emitter, U32 To);
void PVMPatchBranch(PVMEmitter *Emitter, U32 From, U32 To);
void PVMPatchBranchToCurrent(PVMEmitter *Emitter, U32 From);


/* move and load */
void PVMEmitMoveImm(PVMEmitter *Emitter, VarRegister Reg, I64 Immediate);
void PVMEmitMove(PVMEmitter *Emitter, const VarLocation *Dst, const VarLocation *Src);
void PVMEmitCopy(PVMEmitter *Emitter, const VarLocation *Dst, const VarLocation *Src);
/* returns true if the caller owns OutTarget, false otherwise, 
 * call PVMFreeRegister to dipose OutTarget if true is returned */
bool PVMEmitIntoRegLocation(PVMEmitter *Emitter, VarLocation *OutTarget, bool ReadOnly, const VarLocation *Src);
bool PVMEmitIntoReg(PVMEmitter *Emitter, VarRegister *OutTarget, bool ReadOnly, const VarLocation *Src);
void PVMEmitLoadAddr(PVMEmitter *Emitter, VarRegister Dst, VarMemory Src);
U32 PVMEmitLoadSubroutineAddr(PVMEmitter *Emitter, VarRegister Dst, U32 SubroutineAddr);
void PVMEmitLoadEffectiveAddr(PVMEmitter *Emitter, VarRegister Dst, VarMemory Src, I32 Offset);
VarLocation PVMEmitLoadArrayElement(PVMEmitter *Emitter, const VarLocation *Array, const VarLocation *Index);


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
void PVMEmitNeg(PVMEmitter *Emitter, VarRegister Dst, VarRegister Src, IntegralType Type);
void PVMEmitNot(PVMEmitter *Emitter, VarRegister Dst, VarRegister Src, IntegralType Type);
void PVMEmitAdd(PVMEmitter *Emitter, VarRegister Dst, const VarLocation *Src);
void PVMEmitSub(PVMEmitter *Emitter, VarRegister Dst, const VarLocation *Src);
void PVMEmitAnd(PVMEmitter *Emitter, VarRegister Dst, const VarLocation *Src);
void PVMEmitOr(PVMEmitter *Emitter, VarRegister Dst, const VarLocation *Src);
void PVMEmitXor(PVMEmitter *Emitter, VarRegister Dst, const VarLocation *Src);
void PVMEmitMul(PVMEmitter *Emitter, VarRegister Dst, const VarLocation *Src);
void PVMEmitDiv(PVMEmitter *Emitter, VarRegister Dst, const VarLocation *Src);
void PVMEmitMod(PVMEmitter *Emitter, VarRegister Dst, const VarLocation *Src);
void PVMEmitShl(PVMEmitter *Emitter, VarRegister Dst, const VarLocation *Src);
void PVMEmitShr(PVMEmitter *Emitter, VarRegister Dst, const VarLocation *Src);
void PVMEmitAsr(PVMEmitter *Emitter, VarRegister Dst, const VarLocation *Src);
/* returns flag */
VarLocation PVMEmitSetIfLessOrEqual(PVMEmitter *Emitter, 
        VarRegister A, VarRegister B, IntegralType CommonType
);
VarLocation PVMEmitSetIfGreaterOrEqual(PVMEmitter *Emitter, 
        VarRegister A, VarRegister B, IntegralType CommonType
);
VarLocation PVMEmitSetIfLess(PVMEmitter *Emitter, 
        VarRegister A, VarRegister B, IntegralType CommonType
);
VarLocation PVMEmitSetIfGreater(PVMEmitter *Emitter, 
        VarRegister A, VarRegister B, IntegralType CommonType
);
VarLocation PVMEmitSetIfEqual(PVMEmitter *Emitter, 
        VarRegister A, VarRegister B, IntegralType CommonType
);
VarLocation PVMEmitSetIfNotEqual(PVMEmitter *Emitter, 
        VarRegister A, VarRegister B, IntegralType CommonType
);
VarLocation PVMEmitMemEqu(PVMEmitter *Emitter, VarRegister PtrA, VarRegister PtrB, VarRegister Size);



/* subroutine arguments */
I32 PVMStartArg(PVMEmitter *Emitter, U32 ArgSize);
VarLocation PVMSetArg(PVMEmitter *Emitter, UInt ArgNumber, VarType Type, I32 *Base);
void PVMMarkArgAsOccupied(PVMEmitter *Emitter, const VarLocation *Arg);
VarLocation PVMSetReturnType(PVMEmitter *Emitter, VarType Type);


/* stack instructions */
void PVMEmitStackAllocation(PVMEmitter *Emitter, I32 Size);
void PVMEmitPush(PVMEmitter *Emitter, const VarLocation *Src);
void PVMEmitPop(PVMEmitter *Emitter, const VarLocation *Dst);
/* NOTE: does not advance the stack pointer nor emit any instructions to do so */
#define STACK_TOP -1 
VarLocation PVMCreateStackLocation(PVMEmitter *Emitter, VarType Type, int FpOffset);

/* global instructions */
U32 PVMGetGlobalOffset(PVMEmitter *Emitter);
VarMemory PVMEmitGlobalData(PVMEmitter *Emitter, const void *Data, U32 Size);
VarMemory PVMEmitGlobalSpace(PVMEmitter *Emitter, U32 Size);
/* note: data must've already been allocated */
void PVMInitializeGlobal(PVMEmitter *Emitter, VarMemory GLobal, const VarLiteral *Data, VarType Type);


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

