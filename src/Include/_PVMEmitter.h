#ifndef PASCAL_VM2_EMITTER_H
#define PASCAL_VM2_EMITTER_H

#include "PVM/_Chunk.h"
#include "PVM/_Isa.h"
#include "Variable.h"
#include "Tokenizer.h"
#include "PVMCompiler.h"


#define PVM_MAX_CALL_IN_EXPR 32

typedef struct PVMEmitter 
{
    PVMChunk *Chunk;
    U16 Reglist;
    U32 NumSavelist;
    U16 SavedRegisters[PVM_MAX_CALL_IN_EXPR];

    U32 SpilledRegCount;
    U32 StackSpace;
    struct {
        VarLocation SP, FP, GP;
    } Reg;

    VarLocation ReturnValue;
    VarLocation ArgReg[PVM_ARGREG_COUNT];
} PVMEmitter;

typedef enum PVMBranchType
{
    BRANCHTYPE_UNCONDITIONAL    = 0x00FF,
    BRANCHTYPE_CONDITIONAL      = 0x000F,
} PVMBranchType;


PVMEmitter PVMEmitterInit(PVMChunk *Chunk);
void PVMEmitterDeinit(PVMEmitter *Emitter);
void PVMEmitterBeginScope(PVMEmitter *Emitter);
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
/* returns the offset of the branch instruction for patching if necessary */
U32 PVMEmitBranch(PVMEmitter *Emitter, U32 To);
void PVMPatchBranch(PVMEmitter *Emitter, U32 From, U32 To, PVMBranchType Type);
void PVMPatchBranchToCurrent(PVMEmitter *Emitter, U32 From, PVMBranchType Type);


/* move and load */
void PVMEmitMov(PVMEmitter *Emitter, const VarLocation *Dst, const VarLocation *Src);
void PVMEmitLoadImm(PVMEmitter *Emitter, VarRegister Register, U64 Integer);


/* arith instructions */
void PVMEmitAddImm(PVMEmitter *Emitter, const VarLocation *Dst, I16 Imm);
void PVMEmitAdd(PVMEmitter *Emitter, const VarLocation *Dst, const VarLocation *Src);
void PVMEmitSub(PVMEmitter *Emitter, const VarLocation *Dst, const VarLocation *Src);
void PVMEmitNeg(PVMEmitter *Emitter, const VarLocation *Dst, const VarLocation *Src);
void PVMEmitMul(PVMEmitter *Emitter, const VarLocation *Dst, const VarLocation *Src);
void PVMEmitDiv(PVMEmitter *Emitter, const VarLocation *Dst, const VarLocation *Src);
void PVMEmitIMul(PVMEmitter *Emitter, const VarLocation *Dst, const VarLocation *Src);
void PVMEmitIDiv(PVMEmitter *Emitter, const VarLocation *Dst, const VarLocation *Src);
void PVMEmitMod(PVMEmitter *Emitter, const VarLocation *Dst, const VarLocation *Src);
/* returns true if there are no warning */
bool PVMEmitSetCC(PVMEmitter *Emitter, TokenType Op, const VarLocation *Dst, const VarLocation *Right);


/* stack instructions */
VarMemory PVMQueueStackAllocation(PVMEmitter *Emitter, U32 Size, IntegralType Type);
void PVMCommitStackAllocation(PVMEmitter *Emitter);


/* global instructions */
VarMemory PVMEmitGlobalSpace(PVMEmitter *Emitter, U32 Size, IntegralType Type);


/* call instructions */
#define NO_RETURN_REG PVM_REG_COUNT
void PVMEmitSaveCallerRegs(PVMEmitter *Emitter, UInt ReturnRegID);
/* returns the location of the call instruction in case it needs a patch later on */
U32 PVMEmitCall(PVMEmitter *Emitter, VarSubroutine *Callee);
void PVMEmitUnsaveCallerRegs(PVMEmitter *Emitter);


/* exit/return */
void PVMEmitExit(PVMEmitter *Emitter);


#endif /* PASCAL_VM2_EMITTER_H */

