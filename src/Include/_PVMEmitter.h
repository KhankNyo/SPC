#ifndef PASCAL_VM2_EMITTER_H
#define PASCAL_VM2_EMITTER_H

#include "PVM/_Chunk.h"
#include "_Variable.h"
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
    UInt CurrentScopeDepth;
    U32 StackSpace;
    struct {
        VarLocation SP, FP, GP;
    } Reg;
} PVMEmitter;

typedef enum PVMBranchType
{
    BRANCHTYPE_UNCONDITIONAL = 0xFF,
    BRANCHTYPE_CONDITIONAL = 0x0F,
} PVMBranchType;


PVMEmitter PVMEmitterInit(PVMChunk *Chunk);
void PVMEmitterDeinit(PVMEmitter *Emitter);


U32 PVMGetCurrentLocation(PVMEmitter *Emitter);
/* TODO: weird semantics between these 2 functions */
void PVMFreeRegister(PVMEmitter *Emitter, VarRegister Reg);
VarLocation PVMAllocRegister(PVMEmitter *Emitter, IntegralType Type);


/* Branching instructions */
#define PVMMarkBranchTarget(pEmitter) PVMGetCurrentLocation(pEmitter)
/* returns the offset of the branch instruction for later patching */
U32 PVMEmitBranchIfFalse(PVMEmitter *Emitter, const VarLocation *Condition);
/* returns the offset of the branch instruction for patching if necessary */
U32 PVMEmitBranch(PVMEmitter *Emitter, U32 To);
void PVMPatchBranch(PVMEmitter *Emitter, U32 From, U32 To, PVMBranchType Type);
void PVMPatchBranchToCurrent(PVMEmitter *Emitter, U32 From, PVMBranchType Type);


/* move and load */
void PVMEmitMov(PVMEmitter *Emitter, const VarLocation *Dst, const VarLocation *Src);
void PVMEmitLoad(PVMEmitter *Emitter, const VarLocation *Dest, U64 Integer, IntegralType IntType);

/* arith instructions */
void PVMEmitAddImm(PVMEmitter *Emitter, const VarLocation *Dst, I16 Imm);
void PVMEmitAdd(PVMEmitter *Emitter, const VarLocation *Dst, const VarLocation *Src);
void PVMEmitSub(PVMEmitter *Emitter, const VarLocation *Dst, const VarLocation *Src);
void PVMEmitMul(PVMEmitter *Emitter, const VarLocation *Dst, const VarLocation *Src);
void PVMEmitDiv(PVMEmitter *Emitter, const VarLocation *Dst, const VarLocation *Src);
void PVMEmitIMul(PVMEmitter *Emitter, const VarLocation *Dst, const VarLocation *Src);
void PVMEmitIDiv(PVMEmitter *Emitter, const VarLocation *Dst, const VarLocation *Src);
void PVMEmitMod(PVMEmitter *Emitter, const VarLocation *Dst, const VarLocation *Src);
/* returns true if there are no warning */
bool PVMEmitSetCC(PVMEmitter *Emitter, TokenType Op, const VarLocation *Dst, const VarLocation *Right);



/* stack instructions */
VarMemory PVMQueueStackAllocation(PVMEmitter *Emitter, U32 Size);
void PVMCommitStackAllocation(PVMEmitter *Emitter);

/* call instructions */
void PVMEmitSaveCallerRegs(PVMEmitter *Emitter, UInt ReturnRegID);
/* returns the location of the call instruction in case it needs a patch later on */
U32 PVMEmitCall(PVMEmitter *Emitter, VarSubroutine *Callee);
void PVMEmitUnsaveCallerRegs(PVMEmitter *Emitter);

/* exit/return */
void PVMEmitExit(PVMEmitter *Emitter);


#endif /* PASCAL_VM2_EMITTER_H */

