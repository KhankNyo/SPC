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
    OP_MOD,
    OP_NEG,

    OP_ADDI,
    OP_ADDPI,
    OP_ADDQI,

    OP_SHL,
    OP_SHR,
    OP_ASR,
    OP_VSHL,
    OP_VSHR,
    OP_VASR,

    OP_SEQ,
    OP_SNE,
    OP_SGT,
    OP_SLT,
    OP_SGE,
    OP_SLE,
    OP_ISGT,
    OP_ISLT,
    OP_ISGE,
    OP_ISLE,

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
    OP_FSGE,
    OP_FSLE,


    OP_MOV64,
    OP_MOV32,
    OP_MOV16,
    OP_MOV8,
    OP_MOVI,
    OP_MOVQI,
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


    OP_ADD64,
    OP_SUB64,
    OP_DIV64,
    OP_MUL64,
    OP_IDIV64,
    OP_IMUL64,
    OP_MOD64,
    OP_NEG64,

    OP_SEQ64,
    OP_SNE64,
    OP_SLT64,
    OP_SGT64,
    OP_SLE64,
    OP_SGE64,
    OP_ISLT64,
    OP_ISGT64,
    OP_ISLE64,
    OP_ISGE64,

    OP_SEQP,
    OP_SNEP,
    OP_SLTP,
    OP_SGTP,
    OP_SLEP,
    OP_SGEP,
    OP_ISLTP,
    OP_ISGTP,
    OP_ISLEP,
    OP_ISGEP,
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




#define PVM_OP(Ins, Rd, Rs)\
    (BIT_POS32(OP_ ## Ins, 8, 8)\
    | BIT_POS32(Rd, 4, 4)\
    | BIT_POS32(Rs, 4, 0))

#define PVM_REGLIST(Ins, List)\
    (BIT_POS32(OP_ ## Ins, 8, 8)\
    | BIT_POS32(List, 8, 0))

#define PVM_SYS(Op)\
    (BIT_POS32(OP_SYS, 8, 8)\
     | BIT_POS32(OP_SYS_ ## Op, 8, 0))

#define PVM_MOVI(Rd, ImmType) PVM_OP(MOVI, Rd, IMMTYPE_ ## ImmType)

#define PVM_B(Condition, Rd, Imm4)\
    (BIT_POS32(OP_B ## Condition, 8, 8)\
     | BIT_POS32(Rd, 4, 4)\
     | BIT_POS32(Imm4, 4, 0))

#define PVM_BR(UpperByte)\
    (BIT_POS32(OP_BR, 8, 8)\
     | BIT_POS32(UpperByte, 8, 0))

#define PVM_BSR(UpperByte)\
    (BIT_POS32(OP_BSR, 8, 8)\
     | BIT_POS32(UpperByte, 8, 0))


#define PVM_GET_OP(OpcodeHalf) (PVMOp)(((Opcode) >> 8) & 0xFF)
#define PVM_GET_RD(OpcodeHalf) (((Opcode) >> 4) & 0xF)
#define PVM_GET_RS(OpcodeHalf) ((Opcode) & 0xF)
#define PVM_GET_REGLIST(OpcodeHalf) ((Opcode) & 0xFF)
#define PVM_GET_IMMTYPE(OpcodeHalf) (PVMImmType)((OpcodeHalf) & 0xF)

#define PVM_GET_SYS_OP(OpcodeHalf) (PVMSysOp)((OpcodeHalf) & 0xFF)

#define IS_SMALL_IMM(Integer) (-8 <= (I64)(Integer) && (I64)(Integer) <= 7)



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
#define PVM_ARGREG_COUNT 8
typedef enum PVMArgReg 
{
    PVM_ARGREG_0,
    PVM_ARGREG_1,
    PVM_ARGREG_2,
    PVM_ARGREG_3,
    PVM_ARGREG_4,
    PVM_ARGREG_5,
    PVM_ARGREG_6,
    PVM_ARGREG_7,
    PVM_RETREG = PVM_ARGREG_0,
} PVMArgReg;



typedef union PVMPTR 
{
    uintptr_t UInt;
    intptr_t Int;
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





#endif /* PASCAL_PVM2_ISA_H */

