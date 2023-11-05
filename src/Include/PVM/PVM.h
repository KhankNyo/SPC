#ifndef PASCAL_VM_H
#define PASCAL_VM_H


#include "CodeChunk.h"

/*---------------------------------------------------------------------*/
/*
 * 1/ Notation:
 * [b..a]: bits from index a to index b
 *
 * 2/ Pascal VM opcode format:
 * U32 Opcode:      [31..28][27..22][21..16][15..10][9..4][3..0] 
 * Resv:            [                  TODO                    ]
 * Data:            [ 0001 ][ Ins  ][  RD  ][  S0  ][ S1 ][0000]
 * BranchIf:        [ 0010 ][  cc  ][  RA  ][  RB  ][   Imm10  ]
 * ImmRD:           [ 0011 ][ Ins  ][  RD  ][       Imm16      ]
 * 
 * 3/ Instructions:
 *  3.0/ Resv:
 *      
 *  3.1/ Data:
 *      ADD RD, RS0, RS1
 *          RD := RS0 + RS1
 *      SUB RD, RS0, RS1
 *          RD := RS0 - RS1
 *      
 *  3.2/ BranchIf:
 *      BRIFcc RA, RB, Imm10
 *          compare RA and RB according to cc 
 *          if condition is true 
 *              PC += SignExt32(Imm10 << 2)
 *
 *  3.3/ ImmRD:
 *      ADDI RD, Imm16
 *          RD += SignExt32(Imm16)
 *      SUBI RD, Imm16
 *          RD -= SignExt32(Imm16)
 *      LDI RD, Imm16
 *          RD := SignExt32(Imm16)
 *      LUI RD, Imm16 
 *          RD := Imm16 << 16
 *
 *      ORI RD, Imm16
 *          RD |= Imm16
 *
 *
 *
 */
/*---------------------------------------------------------------------*/

typedef enum PVMIns
{
    PVM_RESV = 0,
        

    PVM_DATA = 1 << 4,
        PVM_DI_ADD = PVM_DATA,
        PVM_DI_SUB,
        PVM_DI_COUNT,

    PVM_BRIF = 2 << 4,
        PVM_BRIF_EQ = PVM_BRIF,
        PVM_BRIF_COUNT,

    PVM_IRD = 3 << 4,
        PVM_IRD_ADD = PVM_IRD,
        PVM_IRD_SUB,
        PVM_IRD_LDI,
        PVM_IRD_LUI,
        PVM_IRD_ORI,
        PVM_IRD_COUNT,

    PVM_INS_COUNT,
} PVMIns;

#define PVM_MAX_INS_COUNT (1u << 6)
PASCAL_STATIC_ASSERT(PVM_DI_COUNT < (PVM_DATA | PVM_MAX_INS_COUNT), "Too many instructions");
PASCAL_STATIC_ASSERT(PVM_IRD_COUNT < (PVM_IRD | PVM_MAX_INS_COUNT), "Too many instructions");


#define PVM_DATA_INS(Mnemonic, Rd, Rs0, Rs1)\
    (BIT_POS32(PVM_DI_ ## Mnemonic, 10, 22)\
    | BIT_POS32(Rd, 6, 16)\
    | BIT_POS32(Rs0, 6, 10)\
    | BIT_POS32(Rs1, 6, 4))

#define PVM_IRD_INS(Mnemonic, Rd, Imm16)\
    (BIT_POS32(PVM_IRD_ ## Mnemonic, 10, 22)\
     | BIT_POS32(Rd, 6, 16)\
     | BIT_POS32(Imm16, 16, 0))

#define PVM_BRANCH_IF(CC, Ra, Rb, Offset10)\
    (BIT_POS32(PVM_BRIF_ ## CC, 10, 22)\
     | BIT_POS32(Ra, 6, 16)\
     | BIT_POS32(Rb, 6, 10)\
     | BIT_POS32(Offset10, 10, 0))


#define PVM_GET_INS(U32_Opcode) ((PVMIns)(((U32)(U32_Opcode)) >> 22))

#define PVM_GET_DI_RD(U32_Opcode) BIT_AT32(U32_Opcode, 6, 16)
#define PVM_GET_DI_RS0(U32_Opcode) BIT_AT32(U32_Opcode, 6, 10)
#define PVM_GET_DI_RS1(U32_Opcode) BIT_AT32(U32_Opcode, 6, 4)

#define PVM_GET_BRIF_RA(U32_Opcode) PVM_GET_DI_RD(U32_Opcode)
#define PVM_GET_BRIF_RB(U32_Opcode) PVM_GET_DI_RS0(U32_Opcode)
#define PVM_GET_BRIF_IMM10(U32_Opcode) BIT_SEX32(U32_Opcode, 9)

#define PVM_GET_IRD_RD(U32_Opcode) PVM_GET_DI_RD(U32_Opcode)
#define PVM_GET_IRD_IMM16(U32_Opcode) ((U32_Opcode) & 0xFFFF)


void PVMDisasm(FILE *f, const CodeChunk *Chunk);





#endif /* PASCAL_VM_H */

