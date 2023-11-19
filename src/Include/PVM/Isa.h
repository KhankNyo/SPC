#ifndef PASCAL_PVM_ISA_H
#define PASCAL_PVM_ISA_H


#include "Common.h"
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
 * F*:              Floating point registers, F64
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
 * BranchIf:        [ 0010 ][  CC  ][    RA   ][              Imm21              ]
 * BranchAlways:    [ 0011 ][  CC  ][                     Imm26                  ]
 *
 * LimmRd:          [ 0100 ][ Mode ][    RD   ][              Imm21              ]
 * ImmRd:           [ 0101 ][ Mode ][    Op   ][  RD  ][          Imm16          ]
 * 
 *
 * Floating point ops:
 * FData:           [ 1000 ][ Mode ][    Op   ][  FD  ][  FA  ][  FB  ][  00000  ]
 * FMem:            [ 1001 ][ Mode ][    FD   ][              Imm21              ]
 */
 /* 
 * 3/ Instructions:
 *  3.0/Resv:
 *      Mode:
 *      00:
 *      01:
 *      10:
 *      11:
 */
 /*  
 *  3.1/Data:
 *      Mode:
 *      00: Arith
 *          Ins:
 *          00000:  ADD RD, RA, RB
 *                      RD := RA + RB
 *          00001:  SUB RD, RA, RB
 *                      RD := RA - RB
 *          00010:  NEG RD, RA
 *                      RD := ~RA + 1;
 *
 *      01: Special
 *          00000:  (S)MUL HI, RA, RB, LO
 *                  if (S)
 *                      Product := SExPtr(RA) * SExPtr(RB)
 *                      HI := Product.HI
 *                      LO := Product.LO
 *                  else
 *                      Product := RA * RB
 *                      HI := Product.HI
 *                      LO := Product.LO
 *
 *          00001:  (S)DIVP RD, RA, RB, RR
 *                  if (RB.Ptr == 0)
 *                      DivisionBy0Exception()
 *                  if (S)
 *                      RD.SPtr  := RA.SPtr / RB.SPtr
 *                  else
 *                      RD.Ptr   := RA.SPtr / RB.SPtr
 *                  RR.Ptr := Remainder
 *
 *          00010:  (S)DIV RD, RA, RB, RR
 *                  if (RB.Ptr == 0)
 *                      DivisionBy0Exception()
 *                  if (S)
 *                      RD.SWord := RA.SWord / RB.SWord
 *                  else 
 *                      RD.Word  := RA.Word / RB.Word
 *                  RR := Remainder
 *
 *      10: Cmp
 *      11: Transfer
 *          00000:  MOV RD, RS
 *                  RD := RS;
 */
 /*
 *  3.2/BranchIf:
 *      Mode:
 *      00: BEZ RA, Offset20
 *          if (RA == 0)
 *              PC += SExPtr(Offset20)
 *      01: BNZ RA, Offset20
 *          if (RA != 0)
 *              PC += SExPtr(Offset20)
 */
 /*
 *  3.3/BranchAlways, Ret:
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
 */
 /*
 *  3.4/LimmRd:
 *      Mode:
 *      00:
 *      01:
 *      10:
 *      11:
 */
 /*  
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
 *          00101: LHUI RD, Imm16 
 *              RD.Ptr.Upper := Imm16 << 48
 *          00110: ORHLI
 *              RD.Ptr.Upper |= Imm16 << 32
 *
 *      01: Mem:
 *          00000:  LDRS RD, [FP + Imm16]
 *              RD.Ptr := [FP + Imm16]
 *          00001:  LDFS FD, [FP + Imm16]
 *              FD := [FP + Imm16]
 *          00010:  STRS RD, [FP + Imm16]
 *              [FP + Imm16] := RD.Ptr
 *          00011:  STFS RD, [FP + Imm16]
 *              [FP + Imm16] := FD
 *              
 *          00100:  PSHL {R0..R15}
 *                  for i := 0 to 16 do 
 *                      if 1 == REGLIST[i] then
 *                          [++SP] := Ri.Ptr;
 *          00101:  POPL {R0..R15}
 *                  for i := 0 to 16 do 
 *                      if 1 == REGLIST[15 - i] then
 *                          Ri.Ptr := [SP--];
 *          00110:  PSHU {R16..R31}
 *                  for i := 16 to 32 do 
 *                      if 1 == REGLIST[i] then
 *                          [++SP] := Ri.Ptr;
 *          00111:  POPU {R16..R31}
 *                  for i := 16 to 32 do 
 *                      if 1 == REGLIST[15 - i] then
 *                          Ri.Ptr := [SP--];
 *          01000:  ADDSP Imm21
 *                  SP += SExPtr(Imm21)
 *
 *          01001:  LDG RD, [Imm16]
 *                  RD.Ptr := GlobalTable[Imm16]
 *          01010:  STG RD, [Imm16]
 *                  GlobalTable[Imm16] := RD.Ptr;
 *
 *      10:
 *      11:
 */
 /* 
 *  4/ ABI:
 *   4.0/ Function Call and return values
 *      Let Rx := General Register x
 *          Fx := Floating Point Register x
 *      Arguments:  R0..R7, rest are on stack in the order from left to right
 *                  F0..F7, rest are on stack in the order from left to right
 *      Return:     R0 or F0
 */
 /*
 *   4.1/ Caller and Callee saved registers:
 *      R00..R15 are Caller saved registers (i.e. temporaries, the function being call can freely use these without saving)
 *      R16..R31 are Callee saved registers (i.e. the function being called must save them before using)
 */
/*---------------------------------------------------------------------*/



#define PVM_MODE_SIZE 2
typedef enum PVMIns
{
    PVM_RESV = 0,
        PVM_RE_SYS = PVM_RESV,
        

    PVM_DI = 1 << PVM_MODE_SIZE,
        PVM_IDAT_ARITH = PVM_DI,
        PVM_IDAT_SPECIAL,
        PVM_IDAT_CMP,
        PVM_IDAT_TRANSFER,

    PVM_BRIF = 2 << PVM_MODE_SIZE,
        PVM_BRIF_EZ = PVM_BRIF,
        PVM_BRIF_NZ,

    PVM_BALT = 3 << PVM_MODE_SIZE,
        PVM_BALT_AL = PVM_BALT,
        PVM_BALT_SR,

    PVM_IRD = 4 << PVM_MODE_SIZE,
        PVM_IRD_ARITH = PVM_IRD,
        PVM_IRD_MEM,

    PVM_FDAT = 8 << PVM_MODE_SIZE,
        PVM_FDAT_ARITH = PVM_FDAT,
        PVM_FDAT_SPECIAL,
        PVM_FDAT_CMP, 
        PVM_FDAT_TRANSFER,

    PVM_FMEM = 9 << PVM_MODE_SIZE,
        PVM_FMEM_LDF = PVM_FMEM,
} PVMIns;

typedef enum PVMArith 
{
    PVM_ARITH_ADD = 0,
    PVM_ARITH_SUB,
    PVM_ARITH_NEG,
} PVMArith;

typedef enum PVMSpecial
{
    PVM_SPECIAL_MUL = 0,
    PVM_SPECIAL_DIVP,
    PVM_SPECIAL_DIV,
    PVM_SPECIAL_D2P,
        PVM_SPECIAL_P2D = PVM_SPECIAL_D2P,
} PVMSpecial;

typedef enum PVMCmp 
{
    PVM_CMP_SEQB = 0,
    PVM_CMP_SNEB,
    PVM_CMP_SLTB,
    PVM_CMP_SGTB,

    PVM_CMP_SEQH,
    PVM_CMP_SNEH,
    PVM_CMP_SLTH,
    PVM_CMP_SGTH,

    PVM_CMP_SEQW,
    PVM_CMP_SNEW,
    PVM_CMP_SLTW,
    PVM_CMP_SGTW,

    PVM_CMP_SEQP,
    PVM_CMP_SNEP,
    PVM_CMP_SLTP,
    PVM_CMP_SGTP,


    PVM_CMP_SSLTB,
    PVM_CMP_SSGTB,

    PVM_CMP_SSLTH,
    PVM_CMP_SSGTH,

    PVM_CMP_SSLTW,
    PVM_CMP_SSGTW,
    
    PVM_CMP_SSLTP,
    PVM_CMP_SSGTP,
} PVMCmp;

typedef enum PVMTransfer 
{
    PVM_TRANSFER_MOV = 0,
} PVMTransfer;

typedef enum PVMIRDArith 
{
    PVM_IRD_ADD = 0,
    PVM_IRD_SUB,

    PVM_IRD_LDI,
    PVM_IRD_LDZI,
    PVM_IRD_ORUI,
    PVM_IRD_LDZHLI,
    PVM_IRD_LDHLI,
    PVM_IRD_ORHUI,
} PVMIRDArith;

typedef enum PVMIRDMem
{
    PVM_IRD_LDRS = 0,
    PVM_IRD_LDFS,
    PVM_IRD_STRS,
    PVM_IRD_STFS,

    PVM_IRD_PSHL,
    PVM_IRD_POPL,
    PVM_IRD_PSHU,
    PVM_IRD_POPU,

    PVM_IRD_ADDSPI,

    PVM_IRD_LDG,
    PVM_IRD_STG,
} PVMIRDMem;

typedef enum PVMSysOp 
{
    PVM_SYS_EXIT = 0,
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
    PVM_REG_ARGCOUNT,

    PVM_REG_RET = PVM_REG_ARG0,
} PVMArgReg;





#define PVM_REG_COUNT 32
#define PVM_MAX_OP_COUNT ((U32)1 << 5)
#define PVM_MAX_SYS_COUNT ((U32)1 << 26)



/* Setters */

#define PVM_IDAT_ARITH_INS(Mnemonic, Rd, Ra, Rb, Sh)\
    (BIT_POS32(PVM_IDAT_ARITH, 6, 26)\
    | BIT_POS32(PVM_ARITH_ ## Mnemonic, 5, 21)\
    | BIT_POS32(Rd, 5, 16)\
    | BIT_POS32(Ra, 5, 11)\
    | BIT_POS32(Rb, 5, 6)\
    | BIT_POS32(Sh, 6, 0))

#define PVM_IDAT_SPECIAL_INS(Mnemonic, Rd, Ra, Rb, Signed, Rr)\
    (BIT_POS32(PVM_IDAT_SPECIAL, 6, 26)\
    | BIT_POS32(PVM_SPECIAL_ ## Mnemonic, 5, 21)\
    | BIT_POS32(Rd, 5, 16)\
    | BIT_POS32(Ra, 5, 11)\
    | BIT_POS32(Rb, 5, 6)\
    | BIT_POS32(Signed, 1, 5)\
    | BIT_POS32(Rr, 5, 0))

#define PVM_IDAT_CMP_INS(Mnemonic, Rd, Ra, Rb)\
    (BIT_POS32(PVM_IDAT_CMP, 6, 26)\
    | BIT_POS32(PVM_CMP_ ## Mnemonic, 5, 21)\
    | BIT_POS32(Rd, 5, 16)\
    | BIT_POS32(Ra, 5, 11)\
    | BIT_POS32(Rb, 5, 6))

#define PVM_IDAT_TRANSFER_INS(Mnemonic, Rd, Ra)\
    (BIT_POS32(PVM_IDAT_TRANSFER, 6, 26)\
    | BIT_POS32(PVM_TRANSFER_ ## Mnemonic, 5, 21)\
    | BIT_POS32(Rd, 5, 16)\
    | BIT_POS32(Ra, 5, 11))



#define PVM_IRD_ARITH_INS(Mnemonic, Rd, Imm16)\
    (BIT_POS32(PVM_IRD_ARITH, 6, 26)\
    | BIT_POS32(PVM_IRD_ ## Mnemonic, 5, 21)\
    | BIT_POS32(Rd, 5, 16)\
    | BIT_POS32(Imm16, 16, 0))\

#define PVM_IRD_MEM_INS(Mnemonic, Rd, Imm16)\
    (BIT_POS32(PVM_IRD_MEM, 6, 26)\
     | BIT_POS32(PVM_IRD_ ## Mnemonic, 5, 21)\
     | BIT_POS32(Rd, 5, 16)\
     | BIT_POS32(Imm16, 16, 0))
#define PVM_ADDSP_INS(Offset) (PVM_IRD_MEM_INS(ADDSPI, 0, 0) | BIT_POS32(Offset, 21, 0))

#define PVM_BRIF_INS(CC, Ra, Offset21)\
    (BIT_POS32(PVM_BRIF_ ## CC, 6, 26)\
     | BIT_POS32(Ra, 5, 21)\
     | BIT_POS32(Offset21, 21, 0))


#define PVM_BRAL_INS(Offset26)\
    (BIT_POS32(PVM_BALT_AL, 6, 26)\
     | BIT_POS32(Offset26, 26, 0))

#define PVM_BSR_INS(Offset26)\
    (BIT_POS32(PVM_BALT_SR, 6, 26)\
     | BIT_POS32(Offset26, 26, 0))

#define PVM_RET_INS PVM_BSR_INS((U32)-1)

#define PVM_SYS_INS(Mnemonic)\
    (BIT_POS32(PVM_RE_SYS, 6, 26)\
    | BIT_POS32(PVM_SYS_ ## Mnemonic, 26, 0))




#define PVM_FDAT_ARITH_INS(Mnemonic, Fd, Fa, Fb)\
    (BIT_POS32(PVM_FDAT_ARITH, 6, 26)\
     | BIT_POS32(PVM_ARITH_ ## Mnemonic, 5, 21)\
     | BIT_POS32(Fd, 5, 16)\
     | BIT_POS32(Fa, 5, 11)\
     | BIT_POS32(Fb, 5, 10)

#define PVM_FDAT_CMP_INS(Mnemonic, Rd, Fa, Fb)\
    (BIT_POS32(PVM_FDAT_CMP, 6, 26)\
     | BIT_POS32(PVM_CMP_ ## Mnemonic, 5, 21)\
     | BIT_POS32(Rd, 5, 16)\
     | BIT_POS32(Fa, 5, 11)\
     | BIT_POS32(Fb, 5, 6)

#define PVM_FMEM_INS(Mnemonic, Fd, Offset20)\
    (BIT_POS32(PVM_FMEM_ ## Mnemonic, 6, 26)\
     | BIT_POS32(Fd, 5, 21)\
     | BIT_POS32(Offset20, 20, 0)


/* Getters */

#define PVM_GET_INS(OpcodeWord) ((PVMIns)BIT_AT32(OpcodeWord, 6, 26))
#define PVM_GET_OP(OpcodeWord) (BIT_AT32(OpcodeWord, 5, 21))

#define PVM_IDAT_GET_ARITH(OpcodeWord)        (PVMArith)PVM_GET_OP(OpcodeWord)
#define PVM_IDAT_GET_SPECIAL(OpcodeWord)      (PVMSpecial)PVM_GET_OP(OpcodeWord)
#define PVM_IDAT_GET_CMP(OpcodeWord)          (PVMCmp)PVM_GET_OP(OpcodeWord)
#define PVM_IDAT_GET_TRANSFER(OpcodeWord)     (PVMTransfer)PVM_GET_OP(OpcodeWord)

#define PVM_IDAT_GET_RD(OpcodeWord) BIT_AT32(OpcodeWord, 5, 16)
#define PVM_IDAT_GET_RA(OpcodeWord) BIT_AT32(OpcodeWord, 5, 11)
#define PVM_IDAT_GET_RB(OpcodeWord) BIT_AT32(OpcodeWord, 5, 6)
#define PVM_IDAT_GET_SH(OpcodeWord) BIT_AT32(OpcodeWord, 6, 0)


#define PVM_IDAT_SPECIAL_SIGNED(OpcodeWord) ((OpcodeWord) & ((U32)1 << 6))
#define PVM_IDAT_SPECIAL_GET_RR(OpcodeWord) BIT_AT32(OpcodeWord, 5, 0)

#define PVM_BRIF_GET_RA(OpcodeWord) PVM_GET_OP(OpcodeWord)
#define PVM_BRIF_GET_IMM(OpcodeWord) BIT_SEX32(BIT_AT32(OpcodeWord, 21, 0), 20)

#define PVM_BAL_GET_IMM(OpcodeWord) (I32)BIT_SEX32(BIT_AT32(OpcodeWord, 26, 0), 25)
#define PVM_BSR_GET_IMM(OpcodeWord) PVM_BAL_GET_IMM(OpcodeWord)


#define PVM_IRD_GET_ARITH(OpcodeWord) (PVMIRDArith)PVM_GET_OP(OpcodeWord)
#define PVM_IRD_GET_MEM(OpcodeWord) (PVMIRDMem)PVM_GET_OP(OpcodeWord)
#define PVM_IRD_GET_RD(OpcodeWord) PVM_IDAT_GET_RD(OpcodeWord)
#define PVM_IRD_GET_FD(OpcodeWord) PVM_IDAT_GET_RD(OpcodeWord)
#define PVM_IRD_GET_IMM(OpcodeWord) BIT_AT32(OpcodeWord, 16, 0)

#define PVM_LIRD_GET_RD(OpcodeWord) BIT_AT32(OpcodeWord, 5, 21)
#define PVM_LIRD_GET_IMM(OpcodeWord) BIT_AT32(OpcodeWord, 20, 0)


#define PVM_GET_SYS_OP(OpcodeWord) (PVMSysOp)BIT_AT32(OpcodeWord, 26, 0)



#define PVM_FDAT_GET_ARITH(OpcodeWord)        PVM_IDAT_GET_ARITH(OpcodeWord)
#define PVM_FDAT_GET_SPECIAL(OpcodeWord)      PVM_IDAT_GET_SPECIAL(OpcodeWord)
#define PVM_FDAT_GET_CMP(OpcodeWord)          PVM_IDAT_GET_CMP(OpcodeWord)
#define PVM_FDAT_GET_TRANSFER(OpcodeWord)     PVM_IDAT_GET_TRANSFER(OpcodeWord)

#define PVM_FDAT_GET_FD(OpcodeWord) PVM_IDAT_GET_RD(OpcodeWord)
#define PVM_FDAT_GET_FA(OpcodeWord) PVM_IDAT_GET_RA(OpcodeWord)
#define PVM_FDAT_GET_FB(OpcodeWord) PVM_IDAT_GET_RB(OpcodeWord)

#define PVM_FMEM_GET_FD(OpcodeWord) PVM_LIRD_GET_RD(OpcodeWord)
#define PVM_FMEM_GET_IMM(OpcodeWord) PVM_LIRD_GET_IMM(OpcodeWord)









typedef uintptr_t PVMPtr;
typedef U32 PVMWord;
typedef U16 PVMHalf;
typedef U8 PVMByte;

typedef intptr_t PVMSPtr;
typedef I32 PVMSWord;
typedef I16 PVMSHalf;
typedef I8 PVMSByte;




#endif /* PASCAL_PVM_ISA_H */


