#ifndef PASCAL_PVM2_ISA_H
#define PASCAL_PVM2_ISA_H


#include "Common.h"

typedef enum PVMOp 
{
    OP_SYS,

    OP_SADD,
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_IMUL,
    OP_IDIV,
    OP_MOD,
    OP_NEG,
    OP_NOT,

    OP_AND,
    OP_OR,
    OP_XOR,

    OP_ADDI,
    OP_ADDQI,

    OP_QSHL,
    OP_QSHR,
    OP_QASR,
    OP_VSHL,
    OP_VSHR,
    OP_VASR,

    OP_STRLT,
    OP_STREQ,
    OP_STRCPY,
    OP_SEQ,
    OP_SLT,
    OP_ISLT,
    OP_SETEZ,

    OP_BEZ,
    OP_BNZ,
    OP_BR,
    OP_CALL,
    OP_CALLPTR,
    OP_BCT,
    OP_BCF,
    OP_BRI,
    OP_LDRIP,

    OP_PSHL,
    OP_PSHH,
    OP_POPL,
    OP_POPH,
    OP_FPSHL,
    OP_FPSHH,
    OP_FPOPL,
    OP_FPOPH,

    
    OP_FADD,
    OP_FSUB,
    OP_FMUL,
    OP_FDIV,
    OP_FNEG,
    OP_FSEQ,
    OP_FSGT,
    OP_FSLT,
    OP_FSNE,
    OP_FSGE,
    OP_FSLE,

    OP_FADD64,
    OP_FSUB64,
    OP_FMUL64,
    OP_FDIV64,
    OP_FNEG64,
    OP_FSEQ64,
    OP_FSGT64,
    OP_FSLT64,
    OP_FSNE64,
    OP_FSGE64,
    OP_FSLE64,

    OP_GETFLAG,
    OP_GETNFLAG,
    OP_SETFLAG,
    OP_SETNFLAG,
    OP_NEGFLAG,


    OP_MEMCPY,
    OP_VMEMCPY,
    OP_VMEMEQU,

    OP_MOV32,
    OP_MOVZEX32_8,
    OP_MOVZEX32_16,
    OP_MOV64,
    OP_MOVZEX64_8,
    OP_MOVZEX64_16,
    OP_MOVZEX64_32,
    OP_MOVSEX64_32,
    OP_MOVI,
    OP_MOVQI,
    OP_FMOV,
    OP_FMOV64,


    OP_F32TOF64,
    OP_F64TOF32,

    OP_F64TOI64,
    OP_I64TOF64,
    OP_I64TOF32,
    OP_U64TOF64,
    OP_U64TOF32,
    OP_I32TOF32,
    OP_I32TOF64,
    OP_U32TOF32,
    OP_U32TOF64,


    OP_LD32,
    OP_LD64,
    OP_LDZEX32_8,
    OP_LDZEX32_16,
    OP_LDZEX64_8,
    OP_LDZEX64_16,
    OP_LDZEX64_32,
    OP_LDSEX32_8,
    OP_LDSEX32_16,
    OP_LDSEX64_8,
    OP_LDSEX64_16,
    OP_LDSEX64_32,

    OP_LD32L,
    OP_LD64L,
    OP_LDZEX32_8L,
    OP_LDZEX32_16L,
    OP_LDZEX64_8L,
    OP_LDZEX64_16L,
    OP_LDZEX64_32L,
    OP_LDSEX32_8L,
    OP_LDSEX32_16L,
    OP_LDSEX64_8L,
    OP_LDSEX64_16L,
    OP_LDSEX64_32L,

    OP_LEA,
    OP_LEAL,

    OP_ST8,
    OP_ST16,
    OP_ST32,
    OP_ST64,

    OP_ST8L,
    OP_ST16L,
    OP_ST32L,
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
    OP_NOT64,
    OP_AND64,
    OP_OR64,
    OP_XOR64,

    OP_QSHL64,
    OP_QSHR64,
    OP_QASR64,
    OP_VSHL64,
    OP_VSHR64,
    OP_VASR64,
    OP_ADDI64,
    OP_ADDQI64,


    OP_SEQ64,
    OP_SLT64,
    OP_ISLT64,
    OP_SETEZ64,
} PVMOp;

/* sys ops uses Pascal calling convention, 
 * including callee cleanup */
typedef enum PVMSysOp
{
    OP_SYS_EXIT,
    OP_SYS_ENTER,
    OP_SYS_WRITE,
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
#define PVM_OP_ALT(Ins, OperandIs64Bit, Rd, Rs)\
    ((OperandIs64Bit)? \
        PVM_OP(Ins ## 64, Rd, Rs) \
        : PVM_OP(Ins, Rd, Rs))

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

#define PVM_BR(LowerByte)\
    (BIT_POS32(OP_BR, 8, 8)\
     | BIT_POS32(LowerByte, 8, 0))

#define PVM_CALL(LowerByte)\
    (BIT_POS32(OP_CALL, 8, 8)\
     | BIT_POS32(LowerByte, 8, 0))

#define PVM_BR_COND(Cond, LowerByte)\
    (BIT_POS32(OP_BC ## Cond, 8, 8)\
     | BIT_POS32(LowerByte, 8, 8))

#define PVM_IMM_OP(Opcode, Rd, ImmType)\
    (BIT_POS32(OP_ ## Opcode, 8, 8)\
     | BIT_POS32(Rd, 4, 4)\
     | BIT_POS32(ImmType, 4, 0))


#define PVM_GET_OP(OpcodeHalf) (PVMOp)(((OpcodeHalf) >> 8) & 0xFF)
#define PVM_GET_RD(OpcodeHalf) (((OpcodeHalf) >> 4) & 0xF)
#define PVM_GET_RS(OpcodeHalf) ((OpcodeHalf) & 0xF)
#define PVM_GET_REGLIST(OpcodeHalf) ((OpcodeHalf) & 0xFF)
#define PVM_GET_IMMTYPE(OpcodeHalf) (PVMImmType)((OpcodeHalf) & 0xF)

#define PVM_GET_SYS_OP(OpcodeHalf) (PVMSysOp)((OpcodeHalf) & 0xFF)

#define IS_SMALL_IMM(Integer) (-8 <= (I64)(Integer) && (I64)(Integer) <= 7)



#if PASCAL_LITTLE_ENDIAN == 1
#  define PVM_LEAST_SIGNIF_BYTE 0
#else /* PVM_BIG_ENDIAN */
#  define PVM_LEAST_SIGNIF_BYTE (sizeof(U64) - 1)
#endif /* PVM_LITTLE_ENDIAN */


#define PVM_BR_OFFSET_SIZE 24
#define PVM_BCC_OFFSET_SIZE 20
#define PVM_BRANCH_INS_SIZE 2

#define PVM_REG_COUNT 16
#define PVM_FREG_COUNT 16

#define PVM_REG_GP 13
#define PVM_REG_FP 14
#define PVM_REG_SP 15
#define PVM_ARGREG_COUNT 4
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

    PVM_ARGREG_F0 = PVM_REG_COUNT,
    PVM_ARGREG_F1,
    PVM_ARGREG_F2,
    PVM_ARGREG_F3,
    PVM_ARGREG_F4,
    PVM_ARGREG_F5,
    PVM_ARGREG_F6,
    PVM_ARGREG_F7,
    PVM_FRETREG = PVM_ARGREG_F0,
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

