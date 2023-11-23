#ifndef PASCAL_PVM2_H
#define PASCAL_PVM2_H



#include "PVM/_Chunk.h"
#include "PVM/_Isa.h"


typedef union PVMPTR 
{
    uintptr_t UInt;
    void *Raw;
    U64 *DWord;
    I64 *SDWord;
    U32 *Word;
    I32 *SWord;
    U16 *Half;
    I16 *SHalf;
    U8 *Byte;
    I8 *SByte;
} PVMPTR;

typedef union PVMFPR 
{
    F64 Double;
    U64 DWord;
    F32 Single;
    U32 Word[2];
} PVMFPR;



typedef union PVMGPR 
{
    PVMPTR Ptr;
    U64 DWord;
    I64 SDWord;
    I8 SByte[sizeof(U64)];
    U8 Byte[sizeof(U64)];
#if PASCAL_LITTLE_ENDIAN
    struct {
        U32 First, Second;
    } Word;
    struct {
        I32 First, Second;
    } SWord;
    struct {
        U16 First, Second, Third, Fourth;
    } Half;
    struct {
        I16 First, Second, Third, Fourth;
    } SHalf;
#else
    struct {
        U32 Second, First;
    } Word;
    struct {
        U16 Fourth, Third, Second, First;
    } Half;
    struct {
        I32 Second, First;
    } SWord;
    struct {
        I16 Fourth, Third, Second, First;
    } SHalf;
#endif /* PASCAL_LITTLE_ENDIAN */
} PVMGPR;
PASCAL_STATIC_ASSERT(sizeof(PVMGPR) == sizeof(U64), "Pack that damned struct");



typedef struct PVMSaveFrame 
{
    U16 *IP;
    PVMPTR FP;
} PVMSaveFrame;


typedef struct PascalVM 
{
    PVMGPR R[PVM_REG_COUNT];
    PVMFPR F[PVM_REG_COUNT];

    struct {
        PVMPTR Start;
        PVMPTR Ptr;
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

