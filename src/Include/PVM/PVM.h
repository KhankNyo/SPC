#ifndef PASCAL_PVM2_H
#define PASCAL_PVM2_H



#include "PVM/Chunk.h"
#include "PVM/Isa.h"
#include "PascalString.h"



typedef struct PVMSaveFrame 
{
    U16 *IP;
    PVMPTR FP;
} PVMSaveFrame;


typedef struct PascalVM 
{
    PVMGPR R[PVM_REG_COUNT];
    PVMFPR F[PVM_REG_COUNT];
    bool FloatCondition;
    PascalStr TmpStr;

    struct {
        PVMPTR Start;
        PVMPTR End;
    } Stack;
    struct {
        PVMSaveFrame *Val;
        PVMSaveFrame *Start;
        int SizeLeft;
    } RetStack;

    bool SingleStepMode;
    struct {
        int Line;
        U32 PC;
    } Error;
} PascalVM;

PascalVM PVMInit(U32 StackSize, UInt RetStackSize);
void PVMDeinit(PascalVM *PVM);


typedef enum PVMReturnValue 
{
    PVM_NO_ERROR = 0,
    PVM_ILLEGAL_INSTRUCTION,
    PVM_DIVISION_BY_0,
    PVM_CALLSTACK_OVERFLOW,
} PVMReturnValue;
PVMReturnValue PVMInterpret(PascalVM *PVM, PVMChunk *Code);

/* Same as PVMInterpret, but handles and prints error to stdout */
bool PVMRun(PascalVM *PVM, PVMChunk *Code);

void PVMDumpState(FILE *f, const PascalVM *PVM, UInt RegPerLine);







#endif /* PASCAL_PVM2_H */

