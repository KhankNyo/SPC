#ifndef PASCAL_PVM2_ISA_H
#define PASCAL_PVM2_ISA_H


#include "Common.h"

typedef enum PVMOp 
{
    OP_SYS,

    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_IMUL,
    OP_IDIV,
    OP_NEG,

    OP_SEQ,
    OP_SNE,
    OP_SGT,
    OP_SLT,
    OP_ISGT,
    OP_ISLT,

    OP_BNZ,
    OP_BEZ,
    OP_BR,
    OP_BSR,

    OP_PSHL,
    OP_PSHH,
    OP_POPL,
    OP_POPH,

    
    OP_FADD,
    OP_FSUB,
    OP_FMUL,
    OP_FDIV,
    OP_FNEG,

    OP_FSEQ,
    OP_FSNE,
    OP_FSGT,
    OP_FSLT,


    OP_MOV64,
    OP_MOV32,
    OP_MOV16,
    OP_MOV8,
    OP_MOVI,
    OP_FMOV,

    OP_MOVSEX64_32,
    OP_MOVSEX64_16,
    OP_MOVSEX64_8,
    OP_MOVSEX32_16,
    OP_MOVSEX32_8,
    OP_MOVSEXP_32,
    OP_MOVSEXP_16,
    OP_MOVSEXP_8,

    OP_MOVZEX64_32,
    OP_MOVZEX64_16,
    OP_MOVZEX64_8,
    OP_MOVZEX32_16,
    OP_MOVZEX32_8,
    OP_MOVZEXP_32,
    OP_MOVZEXP_16,
    OP_MOVZEXP_8,

    OP_LD8,
    OP_ST8,
    OP_LD16,
    OP_ST16,
    OP_LD32,
    OP_ST32,
    OP_LD64,
    OP_ST64,

    OP_LD8L,
    OP_ST8L,
    OP_LD16L,
    OP_ST16L,
    OP_LD32L,
    OP_ST32L,
    OP_LD64L,
    OP_ST64L,


    OP_LDF32,
    OP_STF32,
    OP_LDF64,
    OP_STF64,
    OP_LDF32L,
    OP_STF32L,
    OP_LDF64L,
    OP_STF64L,

} PVMOp;

typedef enum PVMSysOp
{
    OP_SYS_EXIT,
} PVMSysOp;

typedef enum PVMImmType 
{
    IMMTYPE_U16,
    IMMTYPE_U32,
    IMMTYPE_U48,
    IMMTYPE_U64,

    IMMTYPE_I16,
    IMMTYPE_I32,
    IMMTYPE_I48,
} PVMImmType;




#define PVM_OP32(Ins, Rd, Rs)\
    (BIT_POS32(OP_ ## Ins, 8, 8)\
    | BIT_POS32(Rd, 4, 4)\
    | BIT_POS32(Rs, 4, 0))

#define PVM_REGLIST(Ins, List)\
    (BIT_POS32(OP_ ## Ins, 8, 8)\
    | BIT_POS32(List, 8, 0))

#define PVM_MOVI(Rd, ImmType) PVM_OP32(MOVI, Rd, IMMTYPE_ ## ImmType)


#define PVM_GET_OP(OpcodeHalf) (PVMOp)(((Opcode) >> 8) & 0xFF)
#define PVM_GET_RD(OpcodeHalf) (((Opcode) >> 4) & 0xF)
#define PVM_GET_RS(OpcodeHalf) ((Opcode) & 0xF)
#define PVM_GET_REGLIST(OpcodeHalf) ((Opcode) & 0xFF)
#define PVM_GET_IMMTYPE(OpcodeHalf) (PVMImmType)((OpcodeHalf) & 0xF)

#define PVM_GET_SYS_OP(OpcodeHalf) (PVMSysOp)((OpcodeHalf) & 0xFF)

#if PVM_LITTLE_ENDIAN
#  define PVM_LEAST_SIGNIF_BYTE 0
#else /* PVM_BIG_ENDIAN */
#  define PVM_LEAST_SIGNIF_BYTE (sizeof(U64) - 1)
#endif /* PVM_LITTLE_ENDIAN */


#define PVM_BR_OFFSET_SIZE 24
#define PVM_BCC_OFFSET_SIZE 20

#define PVM_REG_COUNT 16
#define PVM_FREG_COUNT 16

#define PVM_REG_GP 13
#define PVM_REG_FP 14
#define PVM_REG_SP 15


#endif /* PASCAL_PVM2_ISA_H */

