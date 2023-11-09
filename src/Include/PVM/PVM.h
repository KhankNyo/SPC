#ifndef PASCAL_VM_H
#define PASCAL_VM_H


#include "CodeChunk.h"

/*---------------------------------------------------------------------*/
/*
 * 1/ Notation:
 * [b..a]:          bits from index a to index b
 * SExWord(val):    sign extend value to signed word from word
 * SExPtr(val):     sign extend value to signed ptr from word
 * PC:              Program Counter, has size of Ptr, always aligned to Word boundary
 * FP:              Frame Pointer, has size of Ptr, always aligned on Ptr boundary
 * SP:              Stack Pointer, has size of Ptr, always aligned on Ptr boundary
 * R*:              General purpose registers, assumed to be Word size if there are no postfix
 *
 * 2/ Pascal VM opcode format:
 * U32 Opcode:      [31..28][27..26][  25..21 ][20..16][15..11][10..6 ][5][ 4..0 ] 
 * Resv:            [ 0000 ][ Mode ][              Depends on each Mode          ] 
 * Data:            [ 0001 ][ Mode ][    Op   ][  RD  ][  RA  ][  RB  ][   Sh    ] Arith
 *                  [ 0001 ][  01  ][  00000  ][  RD  ][  RA  ][  RB  ][S][ 0000 ] Special: (S)Mul
 *                  [ 0001 ][  01  ][(S)Div(P)][  RD  ][  RA  ][  RB  ][S][  RR  ] Special: (S)Div(P)
 *                  [ 0001 ][  10  ][    Op   ][  RD  ][  RA  ][  RB  ][  00000  ] Cmp
 *                  [ 0001 ][  11  ][    Op   ][  RD  ][  RA  ][    0000000000   ] Transfer
 *
 * BranchIf:        [ 0010 ][  CC  ][    RA   ][  RB  ][           Imm16         ]
 * BranchSignedIf:  [ 0011 ][  0C  ][    RA   ][  RB  ][           Imm16         ]
 * BranchAlways:    [ 0011 ][  1C  ][                     Imm26                  ]
 *
 * LimmRd:          [ 0100 ][ Mode ][    RD   ][              Imm20              ]
 * ImmRd:           [ 0101 ][ Mode ][    Op   ][  RD  ][          Imm16          ]
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
 *          00000:  ADD RD, RA, RB
 *                      RD := RA + RB
 *          00001:  SUB RD, RA, RB
 *                      RD := RA - RB
 *
 *          00010:  (S)MUL HI, RA, RB, LO
 *                  if (S)
 *                      Product := SExPtr(RA) * SExPtr(RB)
 *                      HI := Product.HI
 *                      LO := Product.LO
 *                  else
 *                      Product := RA * RB
 *                      HI := Product.HI
 *                      LO := Prod_ARITHuct.LO
 *
 *          00011:  (S)DIVP RD, RA, RB, RR
 *                  if (RB.Ptr == 0)
 *                      DivisionBy0Exception()
 *                  if (S)
 *                      RD.SPtr  := RA.SPtr / RB.SPtr
 *                  else
 *                      RD.Ptr   := RA.SPtr / RB.SPtr
 *                  RR.Word  := Remainder
 *
 *          00100:  (S)DIV RD, RA, RB, RR
 *                  if (RB.Ptr == 0)
 *                      DivisionBy0Exception()
 *                  if (S)
 *                      RD.SWord := RA.SWord / RB.SWord
 *                  else 
 *                      RD.Word  := RA.Word / RB.Word
 *                  RR.Word  := Remainder
 *      01: 
 *      10:
 *      11:
 *
 *  3.2/BranchIf:
 *      CC:
 *      00: BLT RA, RB, Offset16
 *          if (RA < RB)
 *              PC += SExPtr(Offset16)
 *      01: BGT RA, RB, Offset16
 *          if (RA > RB)
 *              PC += SExPtr(Offset16)
 *      10: BEQ RA, RB, Offset16
 *          if (RA == RB)
 *              PC += SExPtr(Offset16)
 *      11: BNE RA, RB, Offset16
 *          if (RA != RB)
 *              PC += SExPtr(Offset16)
 *
 *  3.3/BranchSignedIf:
 *      Mode:
 *      00: BSLT RA, RB, Offset16
 *          if (SExWord(RA) < SExWord(RB))
 *              PC += SExPtr(Offset16)
 *      01: BSGT RA, RB, Offset16 
 *          if (SExWord(RA) > SExWord(RB))
 *              PC += SExPtr(Offset16)
 *
 *  3.3.5/BranchAlways, Ret:
 *      Mode:
 *      10: B Offset26
 *          PC += SExPtr(Offset26)
 *
 *      11: BSR Offset26, RET
 *          if (SExPtr(Offset26) == -1) Do Ret:
 *              PC, FP = RetStackPop(.Addr, .Frame);
 *          else Do BranchSubroutine:
 *              RetStackPush(.Addr = PC, .Frame = FP);
 *              PC += SExPtr(Offset26);
 *
 *  3.4/LimmRd:
 *      Mode:
 *      00:
 *      01:
 *      10:
 *      11:
 *
 *  3.5/ImmRd:
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
 *              RD += SExWord(Imm16)
 *          00001: SUB RD, Imm16
 *              RD -= SExWord(Imm16)
 *          00010: LDI RD, Imm16
 *              RD = SExWord(Imm16)
 *          00011: LUI RD, Imm16
 *              RD := Imm16 << 16
 *          00100: ORI RD, Imm16
 *              RD |= Imm16
 * 4/ ABI:
 *  4.0/ Function Call and return values
 *      Let Rx = General Register x
 *          Fx = Floating Point Register x
 *      Arguments:  R0..R7, rest are on stack from the order of left to right
 *                  F0..F7, rest are on stack from the order of left to right
 *      Return:     R0 or F0
 *
 *
 */
/*---------------------------------------------------------------------*/



#define PVM_MODE_SIZE 2
typedef enum PVMIns
{
    PVM_RESV = 0,
        PVM_RE_SYS = PVM_RESV,
        PVM_RE_COUNT,
        

    PVM_DI = 1 << PVM_MODE_SIZE,
        PVM_DI_ARITH = PVM_DI,
        PVM_DI_SPECIAL,
        PVM_DI_CMP,
        PVM_DI_TRANSFER,
        PVM_DI_COUNT,

    PVM_BRIF = 2 << PVM_MODE_SIZE,
        PVM_BRIF_EQ = PVM_BRIF,
        PVM_BRIF_NE,
        PVM_BRIF_LT,
        PVM_BRIF_GT,
        PVM_BRIF_COUNT,

    PVM_BALT = 3 << PVM_MODE_SIZE,
        PVM_BRIF_SGT = PVM_BALT,
        PVM_BRIF_SLT,
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
    PVM_DI_ARITH_COUNT,
} PVMDIArith;

typedef enum PVMDISpecial
{
    PVM_DI_MUL = 0,
    PVM_DI_DIVP,
    PVM_DI_DIV,
} PVMDISpecial;

typedef enum PVMDICmp 
{
    PVM_DI_SEQB = 0,
    PVM_DI_SNEB,
    PVM_DI_SLTB,
    PVM_DI_SGTB,

    PVM_DI_SEQH,
    PVM_DI_SNEH,
    PVM_DI_SLTH,
    PVM_DI_SGTH,

    PVM_DI_SEQW,
    PVM_DI_SNEW,
    PVM_DI_SLTW,
    PVM_DI_SGTW,

    PVM_DI_SEQP,
    PVM_DI_SNEP,
    PVM_DI_SLTP,
    PVM_DI_SGTP,


    PVM_DI_SSLTB,
    PVM_DI_SSGTB,

    PVM_DI_SSLTH,
    PVM_DI_SSGTH,

    PVM_DI_SSLTW,
    PVM_DI_SSGTW,
    
    PVM_DI_SSLTP,
    PVM_DI_SSGTP,
} PVMDICmp;

typedef enum PVMDITransfer 
{
    PVM_DI_MOV = 0,
} PVMDITransfer;

typedef enum PVMIRDArith 
{
    PVM_IRD_ADD = 0,
    PVM_IRD_SUB,
    PVM_IRD_LDI,
    PVM_IRD_LUI,
    PVM_IRD_ORI,
    PVM_IRD_ARITH_COUNT,
} PVMIRDArith;

typedef enum PVMSysOp 
{
    PVM_SYS_EXIT = 0,
    PVM_SYS_COUNT,
} PVMSysOp;



typedef enum PVMArgReg 
{
    PVM_REG_ARG0 = 0,
    PVM_REG_ARG1,
    PVM_REG_ARG2,
    PVM_REG_ARG3,
    PVM_REG_ARG4,
    PVM_REG_ARG5,
    PVM_REG_ARG6,
    PVM_REG_ARG7,

    PVM_REG_RET = PVM_REG_ARG0,
} PVMArgReg;





#define PVM_REG_COUNT 32
#define PVM_MAX_OP_COUNT ((U32)1 << 5)
#define PVM_MAX_SYS_COUNT ((U32)1 << 26)

PASCAL_STATIC_ASSERT(PVM_RE_COUNT <= PVM_DI, "Too many Resv instructions");
PASCAL_STATIC_ASSERT(PVM_DI_COUNT <= PVM_BRIF, "Too many Data instructions");
PASCAL_STATIC_ASSERT(PVM_BRIF_COUNT <= PVM_BALT, "Too many BranchIf instructions");
PASCAL_STATIC_ASSERT(PVM_BALT_COUNT <= PVM_IRD, "Too many BranchAlt instructions");
PASCAL_STATIC_ASSERT(PVM_IRD_COUNT <= PVM_INS_COUNT, "Too many ImmRd instructions");

PASCAL_STATIC_ASSERT(PVM_SYS_COUNT < PVM_MAX_SYS_COUNT, "Too many SysOp instructions");
PASCAL_STATIC_ASSERT(PVM_DI_ARITH_COUNT < PVM_MAX_OP_COUNT, "Too many op for DataIns Arith instruction");
PASCAL_STATIC_ASSERT(PVM_IRD_ARITH_COUNT < PVM_MAX_OP_COUNT, "Too many op for ImmRd Arith instruction");


/* Setters */

#define PVM_DI_ARITH_INS(Mnemonic, Rd, Ra, Rb, Sh)\
    (BIT_POS32(PVM_DI_ARITH, 6, 26)\
    | BIT_POS32(PVM_DI_ ## Mnemonic, 5, 21)\
    | BIT_POS32(Rd, 5, 16)\
    | BIT_POS32(Ra, 5, 11)\
    | BIT_POS32(Rb, 5, 6)\
    | BIT_POS32(Sh, 6, 0))

#define PVM_DI_SPECIAL_INS(Mnemonic, Rd, Ra, Rb, Signed, Rr)\
    (BIT_POS32(PVM_DI_SPECIAL, 6, 26)\
    | BIT_POS32(PVM_DI_ ## Mnemonic, 5, 21)\
    | BIT_POS32(Rd, 5, 16)\
    | BIT_POS32(Ra, 5, 11)\
    | BIT_POS32(Rb, 5, 6)\
    | BIT_POS32(Signed, 1, 5)\
    | BIT_POS32(Rr, 5, 0))

#define PVM_DI_CMP_INS(Mnemonic, Rd, Ra, Rb)\
    (BIT_POS32(PVM_DI_CMP, 6, 26)\
    | BIT_POS32(PVM_DI_ ## Mnemonic, 5, 21)\
    | BIT_POS32(Rd, 5, 16)\
    | BIT_POS32(Ra, 5, 11)\
    | BIT_POS32(Rb, 5, 6))

#define PVM_DI_TRANSFER_INS(Mnemonic, Rd, Ra)\
    (BIT_POS32(PVM_DI_TRANSFER, 6, 26)\
    | BIT_POS32(PVM_DI_ ## Mnemonic, 5, 21)\
    | BIT_POS32(Rd, 5, 16)\
    | BIT_POS32(Ra, 5, 11))



#define PVM_IRD_ARITH_INS(Mnemonic, Rd, Imm16)\
    (BIT_POS32(PVM_IRD_ARITH, 6, 26)\
    | BIT_POS32(PVM_IRD_ ## Mnemonic, 5, 21)\
    | BIT_POS32(Rd, 5, 16)\
    | BIT_POS32(Imm16, 16, 0))\


#define PVM_BRANCH_IF_INS(CC, Ra, Rb, Offset16)\
    (BIT_POS32(PVM_BRIF_ ## CC, 6, 26)\
     | BIT_POS32(Ra, 5, 21)\
     | BIT_POS32(Rb, 5, 16)\
     | BIT_POS32(Offset16, 16, 0))


#define PVM_LONGBRANCH_INS(Offset26)\
    (BIT_POS32(PVM_BALT_AL, 6, 26)\
     | BIT_POS32(Offset26, 26, 0))

#define PVM_BSR_INS(Offset26)\
    (BIT_POS32(PVM_BALT_SR, 6, 26)\
     | BIT_POS32(Offset26, 26, 0))

#define PVM_RET_INS PVM_BSR_INS((U32)-1)

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

#define PVM_DI_SPECIAL_SIGNED(OpcodeWord) ((OpcodeWord) & ((U32)1 << 6))
#define PVM_DI_SPECIAL_GET_RR(OpcodeWord) BIT_AT32(OpcodeWord, 5, 0)

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

