#ifndef PASCAL_VM_EMITTER_H
#define PASCAL_VM_EMITTER_H


#include "Common.h"
#include "IntegralTypes.h"
#include "Tokenizer.h"
#include "Variable.h"
#include "PVM/CodeChunk.h"
#include "PVMCompiler.h"



#define PVM_CONDITIONAL_BRANCH 21
#define PVM_UNCONDITIONAL_BRANCH 26

typedef struct PVMEmitter 
{
    CodeChunk *Chunk;

    UInt SpilledRegCount;
    U32 RegisterList;
    U32 SavedRegisters[PVM_MAX_SCOPE_COUNT];

    UInt CurrentScopeDepth;
    U32 EntryPoint;
    U32 SP;

    U32 GlobalDataSize;
    U32 VarCount;
    U32 GlobalCount;
} PVMEmitter;


PVMEmitter PVMEmitterInit(CodeChunk *Chunk);
void PVMEmitterDeinit(PVMEmitter *Emitter);


GlobalVar PVMEmitGlobalSpace(PVMEmitter *Emitter, U32 Size);


U32 PVMEmitCode(PVMEmitter *Emitter, U32 Instruction);
void PVMEmitGlobal(PVMEmitter *Emitter, GlobalVar Global);
void PVMEmitDebugInfo(PVMEmitter *Emitter, const U8 *Src, U32 Line);
void PVMUpdateDebugInfo(PVMEmitter *Emitter, UInt LineLen);
bool PVMEmitIntoReg(PVMEmitter *Emitter, VarLocation *Target, const VarLocation *Src);
U32 PVMEmitBranchIfFalse(PVMEmitter *Emitter, const VarLocation *Condition);
void PVMPatchBranch(PVMEmitter *Emitter, U32 StreamOffset, U32 Location, UInt ImmSize);
void PVMPatchBranchToCurrent(PVMEmitter *Emitter, U32 StreamOffset, UInt ImmSize); 
U32 PVMEmitBranch(PVMEmitter *Emitter, U32 Location);
void PVMEmitMov(PVMEmitter *Emitter, const VarLocation *Dest, const VarLocation *Src);
void PVMEmitLoad(PVMEmitter *Emitter, const VarLocation *Dest, U64 Integer, IntegralType IntegerType);
void PVMEmitAddImm(PVMEmitter *Emitter, const VarLocation *Dest, I16 Imm);
void PVMEmitAdd(PVMEmitter *Emitter, const VarLocation *Dest, const VarLocation *Left, const VarLocation *Right);
void PVMEmitSub(PVMEmitter *Emitter, const VarLocation *Dest, const VarLocation *Left, const VarLocation *Right);
void PVMEmitMul(PVMEmitter *Emitter, const VarLocation *Dest, const VarLocation *Left, const VarLocation *Right);
void PVMEmitDiv(PVMEmitter *Emitter, 
        const VarLocation *Dividend, const VarLocation *Remainder, 
        const VarLocation *Left, const VarLocation *Right
);
void PVMEmitSetCC(PVMEmitter *Emitter, TokenType Op, 
        const VarLocation *Dest, 
        const VarLocation *Left, const VarLocation *Right
);
void PVMEmitPush(PVMEmitter *Emitter, UInt RegID);
void PVMEmitPop(PVMEmitter *Emitter, UInt RegID);
void PVMEmitAddSp(PVMEmitter *Emitter, I32 Offset);
void PVMEmitSaveCallerRegs(PVMEmitter *Emitter);
void PVMEmitCall(PVMEmitter *Emitter, FunctionVar *Callee);
void PVMEmitUnsaveCallerRegs(PVMEmitter *Emitter);
void PVMEmitReturn(PVMEmitter *Emitter);
void PVMEmitExit(PVMEmitter *Emitter);

U32 PVMAllocateStackSpace(PVMEmitter *Emitter, UInt Size);

VarLocation PVMAllocateRegister(PVMEmitter *Emitter, IntegralType Type);
void PVMFreeRegister(PVMEmitter *Emitter, const VarLocation *Register);
void PVMMarkRegisterAsAllocated(PVMEmitter *Emitter, UInt RegID);
bool PVMRegisterIsFree(PVMEmitter *Emitter, UInt RegID);
void PVMSaveRegister(PVMEmitter *Emitter, UInt RegID);





#endif /* PASCAL_VM_EMITTER_H */

