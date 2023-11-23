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


    OP_MOV,
    OP_MOVI,
    OP_FMOV,


    OP_LDRS,
    OP_STRS,
    OP_LDRG,
    OP_STRG,

    OP_LDFS,
    OP_STFS,
    OP_LDFG,
    OP_STFG,

    OP_DWORD,
} PVMOp;

typedef enum PVMSysOp
{
    OP_SYS_EXIT,
} PVMSysOp;

typedef enum PVMMovImmType 
{
    IMMTYPE_U16,
    IMMTYPE_U32,
    IMMTYPE_U48,
    IMMTYPE_U64,

    IMMTYPE_I16,
    IMMTYPE_I32,
    IMMTYPE_I48,
} PVMMovImmType;




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
#define PVM_GET_IMMTYPE(OpcodeHalf) (PVMMovImmType)((OpcodeHalf) & 0xF)

#define PVM_GET_SYS_OP(OpcodeHalf) (PVMSysOp)((OpcodeHalf) & 0xFF)


#define PVM_BR_OFFSET_SIZE 24
#define PVM_BCC_OFFSET_SIZE 20

#define PVM_REG_COUNT 16
#define PVM_FREG_COUNT 16


#endif /* PASCAL_PVM2_ISA_H */

