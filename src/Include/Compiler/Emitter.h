#ifndef PASCAL_VM2_EMITTER_H
#define PASCAL_VM2_EMITTER_H

#include "Variable.h"
#include "Tokenizer.h"
#include "Compiler.h"

#include "PVM/Chunk.h"
#include "PVM/Isa.h"




#define PVM_MAX_CALL_IN_EXPR 32

struct PVMEmitter 
{
    PVMChunk *Chunk;
    U32 Reglist;
    U32 NumSavelist;
    U32 SavedRegisters[PVM_MAX_CALL_IN_EXPR];

    U32 SpilledRegCount;
    U32 StackSpace, ArgSpace;
    struct {
        VarLocation SP, FP, GP;
        VarLocation Flag;
    } Reg;

    VarLocation ReturnValue;
    VarLocation ArgReg[PVM_ARGREG_COUNT];
};

typedef enum PVMBranchType
{
    BRANCHTYPE_UNCONDITIONAL    = 0x00FF,
    BRANCHTYPE_FLAG             = 0x00FF,
    BRANCHTYPE_CONDITIONAL      = 0x000F,
    BRANCHTYPE_INC              = 0x0000,
} PVMBranchType;


PVMEmitter PVMEmitterInit(PVMChunk *Chunk);
void PVMEmitterDeinit(PVMEmitter *Emitter);

void PVMSetEntryPoint(PVMEmitter *Emitter, U32 EntryPoint);
void PVMEmitterBeginScope(PVMEmitter *Emitter);
void PVMEmitSaveFrame(PVMEmitter *Emitter);
void PVMEmitterEndScope(PVMEmitter *Emitter);
void PVMEmitDebugInfo(PVMEmitter *Emitter, 
        const U8 *Src, U32 Len, U32 LineNum
);
void PVMUpdateDebugInfo(PVMEmitter *Emitter, U32 LineLen, bool IsSubroutine);


U32 PVMGetCurrentLocation(PVMEmitter *Emitter);
/* TODO: weird semantics between these 2 functions */
void PVMFreeRegister(PVMEmitter *Emitter, VarRegister Reg);
VarLocation PVMAllocateRegister(PVMEmitter *Emitter, IntegralType Type);
void PVMMarkRegisterAsAllocated(PVMEmitter *Emitter, U32 RegID);


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
void PVMPatchBranch(PVMEmitter *Emitter, U32 From, U32 To, PVMBranchType Type);
void PVMPatchBranchToCurrent(PVMEmitter *Emitter, U32 From, PVMBranchType Type);


/* move and load */
void PVMEmitMov(PVMEmitter *Emitter, VarLocation *Dst, const VarLocation *Src);
/* returns true if the caller owns OutTarget, false otherwise, 
 * call PVMFreeRegister to dipose OutTarget if true is returned */
bool PVMEmitIntoReg(PVMEmitter *Emitter, VarLocation *OutTarget, const VarLocation *Src);
void PVMEmitLoadAddr(PVMEmitter *Emitter, const VarLocation *Dst, const VarLocation *Src);
void PVMEmitLoadEffectiveAddr(PVMEmitter *Emitter, const VarLocation *Dst, const VarLocation *Base, I32 Offset);
void PVMEmitDerefPtr(PVMEmitter *Emitter, VarLocation *Dst, const VarLocation *Ptr);
void PVMEmitStoreToPtr(PVMEmitter *Emitter, const VarLocation *Ptr, const VarLocation *Src);


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
void PVMEmitAddImm(PVMEmitter *Emitter, VarLocation *Dst, I16 Imm);
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
VarLocation PVMSetParamType(PVMEmitter *Emitter, UInt ArgNumber, IntegralType ParamType);
VarLocation PVMSetArgType(PVMEmitter *Emitter, UInt ArgNumber, IntegralType ArgType);
void PVMMarkArgAsOccupied(PVMEmitter *Emitter, VarLocation *Arg);
VarLocation PVMSetReturnType(PVMEmitter *Emitter, IntegralType ReturnType);
/* accepts Pointer to VarLocation as args */
void PVMEmitPushMultiple(PVMEmitter *Emitter, int Count, ...);


/* stack instructions */
U32 PVMGetStackOffset(PVMEmitter *Emitter);
VarMemory PVMQueueStackArg(PVMEmitter *Emitter, U32 Size);
void PVMEmitStackAllocation(PVMEmitter *Emitter, I32 Size);

/* global instructions */
U32 PVMGetGlobalOffset(PVMEmitter *Emitter);
VarMemory PVMEmitGlobalData(PVMEmitter *Emitter, const void *Data, U32 Size);
VarMemory PVMEmitGlobalSpace(PVMEmitter *Emitter, U32 Size);
#define PVMEmitGlobalAllocation(pEmitter, U32Size) PVMEmitGlobalSpace(pEmitter, U32Size)


/* call instructions */
#define NO_RETURN_REG PVM_REG_COUNT
void PVMEmitSaveCallerRegs(PVMEmitter *Emitter, UInt ReturnRegID);
/* returns the location of the call instruction in case it needs a patch later on */
U32 PVMEmitCall(PVMEmitter *Emitter, const VarSubroutine *Callee);
void PVMEmitUnsaveCallerRegs(PVMEmitter *Emitter, UInt ReturnRegID);


/* exit/return */
void PVMEmitExit(PVMEmitter *Emitter);


/* system calls */
void PVMEmitWrite(PVMEmitter *Emitter);


#endif /* PASCAL_VM2_EMITTER_H */

