#ifndef PASCAL_VM_H
#define PASCAL_VM_H


#include "CodeChunk.h"
#include "Isa.h"


typedef union PVMGPR 
{
    PVMPtr Ptr;
    PVMSPtr SPtr;
    PVMSByte SByte[sizeof(PVMPtr)];
    PVMByte Byte[sizeof(PVMPtr)];
#if PASCAL_LITTLE_ENDIAN
    struct {
        PVMWord First, Second;
    } Word;
    struct {
        PVMSWord First, Second;
    } SWord;
    struct {
        PVMHalf First, Second, Third, Fourth;
    } Half;
    struct {
        PVMSHalf First, Second, Third, Fourth;
    } SHalf;
#else
    struct {
        PVMWord Second, First;
    } Word;
    struct {
        PVMHalf Fourth, Third, Second, First;
    } Half;
    struct {
        PVMSWord Second, First;
    } SWord;
    struct {
        PVMSHalf Fourth, Third, Second, First;
    } SHalf;
#endif /* PASCAL_LITTLE_ENDIAN */
} PVMGPR;
PASCAL_STATIC_ASSERT(sizeof(PVMGPR) == sizeof(PVMPtr), "Pack that damned struct");



typedef union PVMFPR 
{
    F64 Double;
    PVMPtr Ptr;
    F32 Single[2];
    PVMWord Word[2];
} PVMFPR;


typedef struct PVMSaveFrame 
{
    PVMWord *IP;
    PVMPtr *Frame;
} PVMSaveFrame;


typedef struct PascalVM 
{
    PVMGPR R[PVM_REG_COUNT];
    PVMFPR F[PVM_REG_COUNT];

    struct {
        PVMPtr *Start;
        PVMPtr *Ptr;
        PVMPtr *End;
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
    PVM_CALLSTACK_UNDERFLOW,
} PVMReturnValue;
PVMReturnValue PVMInterpret(PascalVM *PVM, const CodeChunk *Chunk);

/* Same as PVMInterpret, but handles and prints error to stdout */
bool PVMRun(PascalVM *PVM, const CodeChunk *Chunk);

void PVMDumpState(FILE *f, const PascalVM *PVM, UInt RegPerLine);




#endif /* PASCAL_VM_H */

