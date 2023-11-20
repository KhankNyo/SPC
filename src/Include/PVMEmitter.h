#ifndef PASCAL_VM_EMITTER_H
#define PASCAL_VM_EMITTER_H


#include "Common.h"
#include "Ast.h"


#include "PVMCompiler.h"
#include "IntegralTypes.h"


typedef struct LocalVar 
{
    UInt Size;
    U32 FPOffset;
} LocalVar;
typedef struct GlobalVar 
{
    UInt Size;
    U32 Location;
} GlobalVar;
typedef struct FunctionVar
{
    U32 Location;
    IntegralType ReturnType;
    bool HasReturnType;
} FunctionVar;
typedef struct RegisterVar 
{
    VarID ID;
} RegisterVar;

typedef enum VarLocationType 
{
    VAR_INVALID = 0,
    VAR_REG,
    VAR_LOCAL,
    VAR_GLOBAL,
    VAR_FUNCTION,

    VAR_TMP_REG,
    VAR_TMP_STK,
} VarLocationType;

typedef struct VarLocation 
{
    VarID ID;
    VarLocationType LocationType;
    IntegralType IntegralType;
    union {
        RegisterVar Reg;
        
        LocalVar Local;
        GlobalVar Global;
        FunctionVar Function;
    } As;
} VarLocation;






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
    VarLocation Vars[PVM_MAX_VAR_PER_SCOPE];
} PVMEmitter;


PVMEmitter PVMEmitterInit(CodeChunk *Chunk);
void PVMEmitterDeinit(PVMEmitter *Emitter);


VarLocation *PVMGetLocationOf(PVMEmitter *Emitter, VarID ID);

U32 PVMEmitCode(PVMEmitter *Emitter, U32 Instruction);
void PVMEmitGlobal(PVMEmitter *Emitter, GlobalVar Global);
void PVMEmitDebugInfo(PVMEmitter *Emitter, const AstStmt *BaseStmt);
bool PVMEmitIntoReg(PVMEmitter *Emitter, VarLocation *Target, const VarLocation *Src);
U64 PVMEmitBranchIfFalse(PVMEmitter *Emitter, const VarLocation *Condition);
void PVMPatchBranch(PVMEmitter *Emitter, U32 StreamOffset, U32 Location, UInt ImmSize);
void PVMPatchBranchToCurrent(PVMEmitter *Emitter, U64 StreamOffset, UInt ImmSize); 
U64 PVMEmitBranch(PVMEmitter *Emitter, U64 Location);
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
void PVMEmitCall(PVMEmitter *Emitter, U32 CalleeID);
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

