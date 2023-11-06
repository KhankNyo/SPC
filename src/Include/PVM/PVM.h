#ifndef PASCAL_VM_H
#define PASCAL_VM_H


#include "CodeChunk.h"

/*---------------------------------------------------------------------*/
/*
 * 1/ Notation:
 * [b..a]:          bits from index a to index b
 * SExWord(val):    sign extend value to signed word from word
 * SExPtr(val):     sign extend value to signed ptr from word
 *
 * 2/ Pascal VM opcode format:
 * U32 Opcode:      [31..28][27..26][25..21][20..16][15..10][ 9..5 ][ 4..0 ] 
 * Resv:            [ 0000 ][ Mode ][         Depends on each Mode         ] 
 * Data:            [ 0001 ][ Mode ][  Op  ][  RD  ][  RA  ][  RB  ][  Sh  ]
 * BranchIf:        [ 0010 ][  CC  ][  RA  ][  RB  ][         Imm16        ]
 * BranchSignedIf:  [ 0011 ][  0C  ][  RA  ][  RB  ][         Imm16        ]
 * BranchAlways:    [ 0011 ][  1C  ][                 Imm26                ]
 *
 * LimmRd:          [ 0100 ][ Mode ][  RD  ][             Imm20            ]
 * ImmRd:           [ 0101 ][ Mode ][  Op  ][  RD  ][         Imm16        ]
 * 
 * 
 * 3/ Instructions:
 *  3.0/Resv:
 *      Mode:
 *      00:
 *      01:
 *      10:
 *      11:
 *
 *  3.1/Data:
 *      Mode:
 *      00: Arith
 *          Ins:
 *          00000:  ADD RD, RA, RB:
 *                      RD := RA + RB
 *          00001:  SUB RD, RA, RB:
 *                      RD := RA - RB
 *      01: 
 *      10:
 *      11:
 *
 *  3.2/BranchIf:
 *      CC:
 *      00: BEQ RA, RB, Offset16
 *          if (RA == RB)
 *              PC += SExPtr(Offset16)
 *              
 *      01: BNE RA, RB, Offset16
 *          if (RA != RB)
 *              PC += SExPtr(Offset16)
 *
 *      10: BLT RA, RB, Offset16
 *          if (RA < RB)
 *              PC += SExPtr(Offset16)
 *
 *      11: BGT RA, RB, Offset16
 *          if (RA > RB)
 *              PC += SExPtr(Offset16)
 *
 *  3.3/BranchSignedIf:
 *      Mode:
 *      00: BSLT RA, RB, Offset16
 *          if (SExWord(RA) < SExWord(RB))
 *              PC += SExPtr(Offset16)
 *
 *      01: BSGT RA, RB, Offset16 
 *          if (SExWord(RA) > SExWord(RB))
 *              PC += SExPtr(Offset16)
 *
 *      10: B Offset26
 *          PC += SExPtr(Offset26)
 *
 *      11: BSR Offset26
 *          ReturnValueStack++ = PC
 *          PC += SExPtr(Offset26)
 */
/*
 *  3.2/LimmRd:
 *      Mode:
 *      00:
 *      01:
 *      10:
 *      11:
 *
 *  3.3/ImmRd:
 *      Mode:
 *      00: Arith:
 *          00000: ADD RD, Imm16
 *              RD += SExWord(Imm16)
 *          00001: SUB RD, Imm16
 *              RD += SExWord(Imm16)
 *          00010: LDI RD, Imm16
 *              RD := SExWord(Imm16)
 *          00011: LUI RD, Imm16
 *              RD := Imm16 << 16
 *          00100: ORI RD, Imm16
 *              RD |= Imm16
 *      01:
 *      10:
 *      11:
 *
 *  3.4/LimmRd:
 *      Mode:
 *      00: Arith:
 *          00000: ADD RD, Imm16
 *          00001: SUB RD, Imm16
 *          00010: LDI RD, Imm16
 *          00011: LUI RD, Imm16
 *          00100: ORI RD, Imm16
 *
* */
/*---------------------------------------------------------------------*/



#define PVM_MODE_SIZE 2
typedef enum PVMIns
{
    PVM_RESV = 0,
        PVM_RE_SYS = PVM_RESV,
        PVM_RE_COUNT,
        

    PVM_DI = 1 << PVM_MODE_SIZE,
        PVM_DI_ARITH = PVM_DI,
        PVM_DI_COUNT,

    PVM_BRIF = 2 << PVM_MODE_SIZE,
        PVM_BRIF_EQ = PVM_BRIF,
        PVM_BRIF_NE,
        PVM_BRIF_LT,
        PVM_BRIF_GT,
        PVM_BRIF_COUNT,

    PVM_BALT = 3 << PVM_MODE_SIZE,
        PVM_BALT_SGT = PVM_BALT,
        PVM_BALT_SLT,
        PVM_BALT_AL,
        PVM_BALT_SR,
        PVM_BALT_COUNT,

    PVM_IRD = 4 << PVM_MODE_SIZE,
        PVM_IRD_ARITH = PVM_IRD,
        PVM_IRD_COUNT,

    PVM_INS_COUNT,
} PVMIns;

typedef enum PVMDIArith 
{
    PVM_DI_ADD = 0,
    PVM_DI_SUB,
} PVMDIArith;

typedef enum PVMIRDArith 
{
    PVM_IRD_ADD = 0,
    PVM_IRD_SUB,
    PVM_IRD_LDI,
    PVM_IRD_LUI,
    PVM_IRD_ORI,
} PVMIRDArith;

typedef enum PVMSysOp 
{
    PVM_SYS_EXIT = 0,
    PVM_SYS_COUNT,
} PVMSysOp;



#define PVM_REG_COUNT 32
#define PVM_MAX_INS_COUNT ((U32)1 << 2)
#define PVM_MAX_SYS_COUNT ((U32)1 << 26)

PASCAL_STATIC_ASSERT(PVM_RE_COUNT <= PVM_DI, "Too many Resv instructions");
PASCAL_STATIC_ASSERT(PVM_DI_COUNT <= PVM_BRIF, "Too many Data instructions");
PASCAL_STATIC_ASSERT(PVM_BRIF_COUNT <= PVM_BALT, "Too many BranchIf instructions");
PASCAL_STATIC_ASSERT(PVM_BALT_COUNT <= PVM_IRD, "Too many BranchAlt instructions");
PASCAL_STATIC_ASSERT(PVM_IRD_COUNT <= PVM_INS_COUNT, "Too many ImmRd instructions");

PASCAL_STATIC_ASSERT(PVM_SYS_COUNT < PVM_MAX_SYS_COUNT, "Too many SysOp instructions");


/* Setters */

#define PVM_DI_ARITH_INS(Mnemonic, Rd, Ra, Rb, Sh)\
    (BIT_POS32(PVM_DI_ARITH, 6, 26)\
    | BIT_POS32(PVM_DI_ ## Mnemonic, 5, 21)\
    | BIT_POS32(Rd, 5, 16)\
    | BIT_POS32(Ra, 5, 11)\
    | BIT_POS32(Rb, 5, 6)\
    | BIT_POS32(Sh, 6, 0))

#define PVM_IRD_ARITH_INS(Mnemonic, Rd, Imm16)\
    (BIT_POS32(PVM_IRD_ARITH, 6, 26)\
    | BIT_POS32(PVM_IRD_ ## Mnemonic, 5, 21)\
    | BIT_POS32(Rd, 5, 16)\
    | BIT_POS32(Imm16, 16, 0))\

#define PVM_BRANCH_IF(CC, Ra, Rb, Offset16)\
    (BIT_POS32(PVM_BRIF_ ## CC, 6, 26)\
     | BIT_POS32(Ra, 5, 21)\
     | BIT_POS32(Rb, 5, 16)\
     | BIT_POS32(Offset16, 16, 0))

#define PVM_SYS_INS(Mnemonic)\
    (BIT_POS32(PVM_RE_SYS, 6, 26)\
    | BIT_POS32(PVM_SYS_ ## Mnemonic, 26, 0))



/* Getters */

#define PVM_GET_INS(OpcodeWord) ((PVMIns)BIT_AT32(OpcodeWord, 6, 26))

#define PVM_DI_GET_OP(OpcodeWord) BIT_AT32(OpcodeWord, 5, 21)
#define PVM_DI_GET_RD(OpcodeWord) BIT_AT32(OpcodeWord, 5, 16)
#define PVM_DI_GET_RA(OpcodeWord) BIT_AT32(OpcodeWord, 5, 11)
#define PVM_DI_GET_RB(OpcodeWord) BIT_AT32(OpcodeWord, 5, 6)
#define PVM_DI_GET_SH(OpcodeWord) BIT_AT32(OpcodeWord, 6, 0)

#define PVM_BRIF_GET_RA(OpcodeWord) PVM_DI_GET_OP(OpcodeWord)
#define PVM_BRIF_GET_RB(OpcodeWord) PVM_DI_GET_RD(OpcodeWord)
#define PVM_BRIF_GET_IMM(OpcodeWord) BIT_SEX32(BIT_AT32(OpcodeWord, 16, 0), 15)

#define PVM_BAL_GET_IMM(OpcodeWord) BIT_SEX32(BIT_AT32(OpcodeWord, 26, 0), 25)
#define PVM_BSR_GET_IMM(OpcodeWord) PVM_BAL_GET_IMM(OpcodeWord)

#define PVM_IRD_GET_OP(OpcodeWord) PVM_DI_GET_OP(OpcodeWord)
#define PVM_IRD_GET_RD(OpcodeWord) PVM_DI_GET_RD(OpcodeWord)
#define PVM_IRD_GET_IMM(OpcodeWord) BIT_AT32(OpcodeWord, 16, 0)


#define PVM_GET_SYS_OP(OpcodeWord) (PVMSysOp)BIT_AT32(OpcodeWord, 26, 0)


typedef uintptr_t PVMPtr;
typedef U32 PVMWord;
typedef U16 PVMHalf;
typedef U8 PVMByte;

typedef intptr_t PVMSPtr;
typedef I32 PVMSWord;
typedef I16 PVMSHalf;
typedef I8 PVMSByte;




typedef union PVMGPR 
{
    PVMPtr Ptr;
    PVMSPtr SPtr;
    PVMSByte SByte[8];
    PVMByte Byte[8];
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


typedef struct PVMSaveFrame 
{
    PVMWord *IP;
    PVMPtr *Frame;
} PVMSaveFrame;


typedef struct PascalVM 
{
    PVMGPR R[PVM_REG_COUNT];
    F64 F[PVM_REG_COUNT];

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
} PascalVM;

PascalVM PVMInit(U32 StackSize, UInt RetStackSize);
void PVMDeinit(PascalVM *PVM);


typedef enum PVMReturnValue 
{
    PVM_NO_ERROR = 0,
    PVM_CALLSTACK_OVERFLOW,
    PVM_CALLSTACK_UNDERFLOW,
} PVMReturnValue;

PVMReturnValue PVMInterpret(PascalVM *PVM, const CodeChunk *Chunk);


void PVMDumpState(FILE *f, const PascalVM *PVM, UInt RegPerLine);




#endif /* PASCAL_VM_H */

