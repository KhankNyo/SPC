
#include <time.h>
#include <stdarg.h>

#include "Memory.h"
#include "Common.h"
#include "PVM/_PVM.h"
#include "PVM/_Disassembler.h"
#include "PVM/Debugger.h"


PascalVM PVMInit(U32 StackSize, UInt RetStackSize)
{
    PascalVM PVM = {
        .F = { 0 },
        .R = { 0 },
        .Stack.Start.Raw = MemAllocateArray(PVM.Stack.Start.DWord[0], StackSize),
        .RetStack.Start = MemAllocateArray(PVM.RetStack.Start[0], RetStackSize),
        .RetStack.SizeLeft = RetStackSize,

        .Error = { 0 }, 
        .SingleStepMode = false,
    };
    PVM.Stack.End.Raw = PVM.Stack.Start.DWord + StackSize;
    PVM.RetStack.Val = PVM.RetStack.Start;
    return PVM;
}

void PVMDeinit(PascalVM *PVM)
{
    MemDeallocateArray(PVM->Stack.Start.Raw);
    MemDeallocateArray(PVM->RetStack.Start);
    *PVM = (PascalVM){ 0 };
}


static void RuntimeError(PascalVM *PVM, const char *Fmt, ...)
{
    va_list Args;
    va_start(Args, Fmt);
    fprintf(stdout, "Runtime Error: [line %d]:\n\t", PVM->Error.Line);
    fprintf(stdout, Fmt, Args);
    va_end(Args);
}


bool PVMRun(PascalVM *PVM, PVMChunk *Chunk)
{
    PVMDisasm(stdout, Chunk, "Compiled Code");
    fprintf(stdout, "Press Enter to execute...\n");
    getc(stdin);

    bool NoError = false;
    double Start = clock();
    PVMReturnValue Ret = PVMInterpret(PVM, Chunk);
    double End = clock();
    PVMDumpState(stdout, PVM, 4);


    switch (Ret)
    {
    case PVM_NO_ERROR:
    {
        fprintf(stdout, "Finished execution.\n"
                "Time elapsed: %f ms\n", (End - Start) * 1000 / CLOCKS_PER_SEC
        );
        NoError = true;
    } break;
    case PVM_CALLSTACK_OVERFLOW:
    {
        RuntimeError(PVM, "Callstack overflow");
    } break;
    case PVM_DIVISION_BY_0:
    {
        RuntimeError(PVM, "Integer division by 0");
    } break;
    case PVM_ILLEGAL_INSTRUCTION:
    {
        RuntimeError(PVM, "IllegalInstruction");
    } break;
    }
    return NoError;
}




PVMReturnValue PVMInterpret(PascalVM *PVM, PVMChunk *Chunk)
{
#define FP() PVM->R[PVM_REG_FP]
#define SP() PVM->R[PVM_REG_SP]

#define ASSIGNMENT 

#define INTEGER_BINARY_OP(Operator, Opc, RegType)\
    PVM->R[PVM_GET_RD(Opc)]RegType GLUE(Operator,=) PVM->R[PVM_GET_RS(Opc)]RegType
#define INTEGER_SET_IF(Operator, Opc, RegType)\
    PVM->R[PVM_GET_RD(Opc)].Word.First = PVM->R[PVM_GET_RD(Opc)]RegType Operator PVM->R[PVM_GET_RS(Opc)]RegType

#define GET_BR_IMM(Opc, IP) \
    BitSex32Safe(*IP++ | (((U32)(Opc) & 0xFF) << 16), PVM_BR_OFFSET_SIZE - 1)
#define GET_BCC_IMM(Opc, IP) \
    BitSex32Safe(*IP++ | (((U32)(Opc) & 0xF) << 16), PVM_BCC_OFFSET_SIZE - 1)
#define GET_LONG_IMM(Opc, IP) \
    (*IP++ | (((U32)(Opc) & 0xF) << 16))

#define FLOAT_BINARY_OP(Operator, Opc, RegType)\
    PVM->F[PVM_GET_RD(Opc)]RegType GLUE(Operator,=) PVM->F[PVM_GET_RS(Opc)]RegType
#define FLOAT_SET_IF(Operator, Opc, IRegType, RegType)\
    PVM->R[PVM_GET_RD(Opc)]IRegType = PVM->F[PVM_GET_RD(Opc)]RegType Operator PVM->F[PVM_GET_RS(Opc)]RegType

#define PUSH_MULTIPLE(Base, Opc) do{\
    UInt RegList = PVM_GET_REGLIST(Opc);\
    UInt Base_ = Base;\
    UInt i = Base_;\
    while (RegList && i < Base_ + PVM_REG_COUNT/2) {\
        if (RegList & 1) {\
            *(++SP().Ptr.DWord) = PVM->R[i].DWord;\
        }\
        i++;\
        RegList >>= 1;\
    }\
} while (0)

#define POP_MULTIPLE(Base, Opc) do{\
    UInt RegList = PVM_GET_REGLIST(Opc);\
    UInt Base_ = Base;\
    UInt i = Base_;\
    while (RegList && i < Base_ + PVM_REG_COUNT/2) {\
        if (RegList & 0x80) {\
            PVM->R[(Base + (PVM_REG_COUNT/2)-1) - i].DWord = *(SP().Ptr.DWord--);\
        }\
        i++;\
        RegList = (RegList << 1) & 0xFF;\
    }\
} while (0)

#define VERIFY_STACK_ADDR(Addr) do {\
    if ((void*)(Addr) > (void*)PVM->Stack.End) {\
        /* TODO: verify stack addr */\
    }\
} while(0)

#define MOVE_INTEGER(Opc, DestSize, SrcSize)\
    PVM->R[PVM_GET_RD(Opc)]DestSize = PVM->R[PVM_GET_RS(Opc)]SrcSize
#define MOVE_FLOAT(Opc, DestSize, SrcSize)\
    PVM->F[PVM_GET_RD(Opc)]DestSize = PVM->F[PVM_GET_RS(Opc)]SrcSize

#define LOAD_INTEGER(Opc, ImmType, IP, DestSize, AddrMode)\
do {\
    U64 Offset = 0;\
    GET_SEX_IMM(Offset, ImmType, IP);\
    memcpy(&PVM->R[PVM_GET_RD(Opc)]DestSize, PVM->R[PVM_GET_RS(Opcode)]AddrMode + Offset, sizeof PVM->R[0]DestSize);\
} while(0)
#define STORE_INTEGER(Opc, ImmType, IP, SrcSize, AddrMode)\
do {\
    U64 Offset = 0;\
    GET_SEX_IMM(Offset, ImmType, IP);\
    memcpy(PVM->R[PVM_GET_RS(Opcode)]AddrMode + Offset, &PVM->R[PVM_GET_RD(Opc)]SrcSize, sizeof PVM->R[0]SrcSize);\
} while(0)

#define LOAD_FLOAT(Opc, ImmType, IP, DestSize, AddrMode)\
do {\
    U64 Offset = 0;\
    GET_SEX_IMM(Offset, ImmType, IP);\
    memcpy(&PVM->F[PVM_GET_RD(Opc)]DestSize, PVM->R[PVM_GET_RS(Opcode)]AddrMode + Offset, sizeof PVM->F[0]DestSize);\
} while(0)
#define STORE_FLOAT(Opc, ImmType, IP, SrcSize, AddrMode)\
do {\
    U64 Offset = 0;\
    GET_SEX_IMM(Offset, ImmType, IP);\
    memcpy(PVM->R[PVM_GET_RS(Opcode)]AddrMode + Offset, &PVM->F[PVM_GET_RD(Opc)]SrcSize, sizeof PVM->F[0]SrcSize);\
} while(0)

#define GET_SEX_IMM(U64_OutVariable, ImmType, IP)\
do {\
    UInt Count = 0;\
    UInt SexIndex = 0; /* Sign EXtend */\
    switch (ImmType) {\
    case IMMTYPE_I16: SexIndex = 15; FALLTHROUGH;\
    case IMMTYPE_U16: Count = 1; break;\
    case IMMTYPE_I32: SexIndex = 31; FALLTHROUGH;\
    case IMMTYPE_U32: Count = 2; break;\
    case IMMTYPE_I48: SexIndex = 47; FALLTHROUGH;\
    case IMMTYPE_U48: Count = 3; break;\
    case IMMTYPE_U64: Count = 4; break;\
    }\
    for (UInt i = 0; i < Count; i++) {\
        U64_OutVariable |= (U64)*IP++ << i*16;\
    }\
    if (SexIndex)\
        U64_OutVariable = BitSex64(U64_OutVariable, SexIndex);\
} while(0)








    U16 *IP = Chunk->Code;
    SP().Ptr = PVM->Stack.Start;
    FP().Ptr = SP().Ptr;
    PVM->R[PVM_REG_GP].Ptr.Raw = Chunk->Global.Data.As.Raw;
    PVMReturnValue ReturnValue = PVM_NO_ERROR;
    U32 StreamOffset = 0;

    while (1)
    {
        if (PVM->SingleStepMode)
        {
            PVMDebugPause(PVM, Chunk, IP, SP().Ptr, FP().Ptr);
        }

        UInt Opcode = *IP++;
        switch (PVM_GET_OP(Opcode))
        {
        case OP_SYS:
        {
            switch (PVM_GET_SYS_OP(Opcode))
            {
            case OP_SYS_EXIT:
            {
                if (PVM->RetStack.Val == PVM->RetStack.Start)
                    goto Exit;

                PVM->RetStack.Val--;
                IP = PVM->RetStack.Val->IP;
                SP().Ptr = FP().Ptr;
                FP().Ptr = PVM->RetStack.Val->FP;
                PVM->RetStack.SizeLeft++;
            } break;
            }
        } break;

        case OP_ADD: INTEGER_BINARY_OP(+, Opcode, .Word.First); break;
        case OP_SUB: INTEGER_BINARY_OP(-, Opcode, .Word.First); break;
        case OP_MUL: INTEGER_BINARY_OP(*, Opcode, .Word.First); break;
        case OP_IMUL: INTEGER_BINARY_OP(*, Opcode, .SWord.First); break;
        case OP_DIV:
        {
            if (0 == PVM->R[PVM_GET_RS(Opcode)].Word.First)
                goto DivisionBy0;
            INTEGER_BINARY_OP(/, Opcode, .Word.First);
        } break;
        case OP_IDIV:
        {
            if (0 == PVM->R[PVM_GET_RS(Opcode)].SWord.First)
                goto DivisionBy0;
            INTEGER_BINARY_OP(/, Opcode, .SWord.First);
        } break;
        case OP_MOD:
        {
            if (0 == PVM->R[PVM_GET_RS(Opcode)].Word.First)
                goto DivisionBy0;
            INTEGER_BINARY_OP(%, Opcode, .Word.First);
        } break;
        case OP_NEG:
        {
            PVM->R[PVM_GET_RD(Opcode)].Word.First = -PVM->R[PVM_GET_RS(Opcode)].Word.First;
        } break;
        case OP_VSHL: INTEGER_BINARY_OP(<<, Opcode, .Word.First); break;
        case OP_VSHR: INTEGER_BINARY_OP(>>, Opcode, .Word.First); break;
        case OP_VASR: INTEGER_BINARY_OP(>>, Opcode, .SWord.First); break;
        case OP_SHL:
        {
            PVM->R[PVM_GET_RD(Opcode)].Word.First <<= PVM_GET_RS(Opcode);
        } break;
        case OP_SHR:
        {
            PVM->R[PVM_GET_RD(Opcode)].Word.First >>= PVM_GET_RS(Opcode);
        } break;
        case OP_ASR:
        {
            PVM->R[PVM_GET_RD(Opcode)].SWord.First >>= PVM_GET_RS(Opcode);
        } break;
        case OP_ADDQI:
        {
            PVM->R[PVM_GET_RD(Opcode)].Word.First += PVM_GET_RS(Opcode); 
        } break;


        case OP_ADDI:
        {
            U32 Imm = 0;
            GET_SEX_IMM(Imm, PVM_GET_RS(Opcode), IP);
            PVM->R[PVM_GET_RD(Opcode)].Word.First += Imm;
        } break;


        case OP_SEQ: INTEGER_SET_IF(==, Opcode, .Word.First); break;
        case OP_SNE: INTEGER_SET_IF(!=, Opcode, .Word.First); break;
        case OP_SLT: INTEGER_SET_IF(<, Opcode, .Word.First); break;
        case OP_SGT: INTEGER_SET_IF(>, Opcode, .Word.First); break;
        case OP_ISLT: INTEGER_SET_IF(<, Opcode, .SWord.First); break;
        case OP_ISGT: INTEGER_SET_IF(>, Opcode, .SWord.First); break;
        case OP_SLE: INTEGER_SET_IF(<=, Opcode, .Word.First); break;
        case OP_SGE: INTEGER_SET_IF(>=, Opcode, .Word.First); break;
        case OP_ISLE: INTEGER_SET_IF(<=, Opcode, .SWord.First); break;
        case OP_ISGE: INTEGER_SET_IF(>=, Opcode, .SWord.First); break;

        case OP_BR:
        {
            I32 Offset = GET_BR_IMM(Opcode, IP);
            IP += Offset;
            PASCAL_ASSERT(IP < Chunk->Code + Chunk->Count, "Unreachable");
        } break;
        case OP_BSR:
        {
            if (0 == PVM->RetStack.SizeLeft)
                goto CallStackOverflow;

            I32 Offset = GET_BR_IMM(Opcode, IP);
            PVM->RetStack.Val->IP = IP;
            PVM->RetStack.Val->FP = FP().Ptr;
            PVM->RetStack.Val++;
            PVM->RetStack.SizeLeft--;

            IP += Offset;
        } break;
        case OP_BNZ:
        {
            I32 Offset = GET_BCC_IMM(Opcode, IP);
            if (PVM->R[PVM_GET_RD(Opcode)].Word.First)
            {
                /* TODO: bound chk */
                IP += Offset;
            }
        } break;
        case OP_BEZ:
        {
            I32 Offset = GET_BCC_IMM(Opcode, IP);
            if (0 == PVM->R[PVM_GET_RD(Opcode)].Word.First)
            {
                /* TODO: bound chk */
                IP += Offset;
            }
        } break;


        case OP_PSHL: PUSH_MULTIPLE(0, Opcode); break;
        case OP_POPL: POP_MULTIPLE(0, Opcode); break;
        case OP_PSHH: PUSH_MULTIPLE(8, Opcode); break;
        case OP_POPH: POP_MULTIPLE(8, Opcode); break;


        case OP_FADD: FLOAT_BINARY_OP(+, Opcode, .Single); break;
        case OP_FSUB: FLOAT_BINARY_OP(-, Opcode, .Single); break;
        case OP_FMUL: FLOAT_BINARY_OP(*, Opcode, .Single); break;
        case OP_FDIV: FLOAT_BINARY_OP(/, Opcode, .Single); break;
        case OP_FNEG:
        {
            PVM->F[PVM_GET_RD(Opcode)].Single = -PVM->F[PVM_GET_RS(Opcode)].Single;
        } break;

        case OP_FSEQ: FLOAT_SET_IF(==, Opcode, .Word.First, .Single); break;
        case OP_FSNE: FLOAT_SET_IF(!=, Opcode, .Word.First, .Single); break;
        case OP_FSLT: FLOAT_SET_IF(<, Opcode, .Word.First, .Single); break;
        case OP_FSGT: FLOAT_SET_IF(>, Opcode, .Word.First, .Single); break;
        case OP_FSLE: FLOAT_SET_IF(<=, Opcode, .Word.First, .Single); break;
        case OP_FSGE: FLOAT_SET_IF(>=, Opcode, .Word.First, .Single); break;


        case OP_MOV64:       MOVE_INTEGER(Opcode, .DWord, .DWord); break;
        case OP_MOV32:       MOVE_INTEGER(Opcode, .Word.First, .Word.First); break;
        case OP_MOV16:       MOVE_INTEGER(Opcode, .Half.First, .Half.First); break;
        case OP_MOV8:        MOVE_INTEGER(Opcode, .Byte[PVM_LEAST_SIGNIF_BYTE], .Byte[PVM_LEAST_SIGNIF_BYTE]); break;
        case OP_MOVSEX64_32: MOVE_INTEGER(Opcode, .SDWord, .SWord.First); break;
        case OP_MOVSEX64_16: MOVE_INTEGER(Opcode, .SDWord, .SHalf.First); break;
        case OP_MOVSEX64_8:  MOVE_INTEGER(Opcode, .SDWord, .SByte[PVM_LEAST_SIGNIF_BYTE]); break;
        case OP_MOVSEX32_16: MOVE_INTEGER(Opcode, .SWord.First, .SHalf.First); break;
        case OP_MOVSEX32_8:  MOVE_INTEGER(Opcode, .SWord.First, .SByte[PVM_LEAST_SIGNIF_BYTE]); break;
        case OP_MOVSEXP_32:  MOVE_INTEGER(Opcode, .Ptr.Int, .SWord.First); break;
        case OP_MOVSEXP_16:  MOVE_INTEGER(Opcode, .Ptr.Int, .SWord.First); break;
        case OP_MOVSEXP_8:   MOVE_INTEGER(Opcode, .Ptr.Int, .SByte[PVM_LEAST_SIGNIF_BYTE]); break;

        case OP_MOVZEX64_32: MOVE_INTEGER(Opcode, .DWord, .Word.First); break;
        case OP_MOVZEX64_16: MOVE_INTEGER(Opcode, .DWord, .SHalf.First); break;
        case OP_MOVZEX64_8:  MOVE_INTEGER(Opcode, .DWord, .Byte[PVM_LEAST_SIGNIF_BYTE]); break;
        case OP_MOVZEX32_16: MOVE_INTEGER(Opcode, .Word.First, .SHalf.First); break;
        case OP_MOVZEX32_8:  MOVE_INTEGER(Opcode, .Word.First, .Byte[PVM_LEAST_SIGNIF_BYTE]); break;
        case OP_MOVZEXP_32:  MOVE_INTEGER(Opcode, .Ptr.UInt, .SWord.First); break;
        case OP_MOVZEXP_16:  MOVE_INTEGER(Opcode, .Ptr.UInt, .SWord.First); break;
        case OP_MOVZEXP_8:   MOVE_INTEGER(Opcode, .Ptr.UInt, .SByte[PVM_LEAST_SIGNIF_BYTE]); break;
        case OP_MOVI:
        {
            U64 Imm = 0;
            GET_SEX_IMM(Imm, PVM_GET_IMMTYPE(Opcode), IP);
            PVM->R[PVM_GET_RD(Opcode)].DWord = Imm;
        } break;
        case OP_MOVQI:
        {
            I32 Imm = BIT_SEX32(PVM_GET_RS(Opcode), 3);
            PVM->R[PVM_GET_RD(Opcode)].SWord.First = Imm;
        } break;
        case OP_FMOV: FLOAT_BINARY_OP(ASSIGNMENT, Opcode, .Single); break;
        case OP_FMOV64: FLOAT_BINARY_OP(ASSIGNMENT, Opcode, .Double); break;


        case OP_LD8:  LOAD_INTEGER(Opcode, IMMTYPE_U16, IP, .Byte[PVM_LEAST_SIGNIF_BYTE], .Ptr.Byte); break;
        case OP_LD16: LOAD_INTEGER(Opcode, IMMTYPE_U16, IP, .Half.First, .Ptr.Byte); break;
        case OP_LD32: LOAD_INTEGER(Opcode, IMMTYPE_U16, IP, .Word.First, .Ptr.Byte); break;
        case OP_LD64: LOAD_INTEGER(Opcode, IMMTYPE_U16, IP, .DWord, .Ptr.Byte); break;
        case OP_ST8:  STORE_INTEGER(Opcode, IMMTYPE_U16, IP, .Byte[PVM_LEAST_SIGNIF_BYTE], .Ptr.Byte); break;
        case OP_ST16: STORE_INTEGER(Opcode, IMMTYPE_U16, IP, .Half.First, .Ptr.Byte); break;
        case OP_ST32: STORE_INTEGER(Opcode, IMMTYPE_U16, IP, .Word.First, .Ptr.Byte); break;
        case OP_ST64: STORE_INTEGER(Opcode, IMMTYPE_U16, IP, .DWord, .Ptr.Byte); break;

        case OP_LD8L:  LOAD_INTEGER(Opcode, IMMTYPE_U32, IP, .Byte[PVM_LEAST_SIGNIF_BYTE], .Ptr.Byte); break;
        case OP_LD16L: LOAD_INTEGER(Opcode, IMMTYPE_U32, IP, .Half.First, .Ptr.Byte); break;
        case OP_LD32L: LOAD_INTEGER(Opcode, IMMTYPE_U32, IP, .Word.First, .Ptr.Byte); break;
        case OP_LD64L: LOAD_INTEGER(Opcode, IMMTYPE_U32, IP, .DWord, .Ptr.Byte); break;
        case OP_ST8L:  STORE_INTEGER(Opcode, IMMTYPE_U32, IP, .Byte[PVM_LEAST_SIGNIF_BYTE], .Ptr.Byte); break;
        case OP_ST16L: STORE_INTEGER(Opcode, IMMTYPE_U32, IP, .Half.First, .Ptr.Byte); break;
        case OP_ST32L: STORE_INTEGER(Opcode, IMMTYPE_U32, IP, .Word.First, .Ptr.Byte); break;
        case OP_ST64L: STORE_INTEGER(Opcode, IMMTYPE_U32, IP, .DWord, .Ptr.Byte); break;


        case OP_LDF32: LOAD_FLOAT(Opcode, IMMTYPE_U16, IP, .Single, .Ptr.Byte); break;
        case OP_STF32: STORE_FLOAT(Opcode, IMMTYPE_U16, IP, .Single, .Ptr.Byte); break;
        case OP_LDF64: LOAD_FLOAT(Opcode, IMMTYPE_U16, IP, .Double, .Ptr.Byte); break;
        case OP_STF64: STORE_FLOAT(Opcode, IMMTYPE_U16, IP, .Double, .Ptr.Byte); break;

        case OP_LDF32L: LOAD_FLOAT(Opcode, IMMTYPE_U32, IP, .Single, .Ptr.Byte); break;
        case OP_STF32L: STORE_FLOAT(Opcode, IMMTYPE_U32, IP, .Single, .Ptr.Byte); break;
        case OP_LDF64L: LOAD_FLOAT(Opcode, IMMTYPE_U32, IP, .Double, .Ptr.Byte); break;
        case OP_STF64L: STORE_FLOAT(Opcode, IMMTYPE_U32, IP, .Double, .Ptr.Byte); break;


        case OP_ADD64: INTEGER_BINARY_OP(+, Opcode, .DWord); break;
        case OP_SUB64: INTEGER_BINARY_OP(-, Opcode, .DWord); break;
        case OP_MUL64: INTEGER_BINARY_OP(*, Opcode, .DWord); break;
        case OP_IMUL64: INTEGER_BINARY_OP(*, Opcode, .SDWord); break;
        case OP_DIV64: 
        {
            if (0 == PVM->R[PVM_GET_RS(Opcode)].DWord)
                goto DivisionBy0;
            INTEGER_BINARY_OP(/, Opcode, .DWord);
        } break;
        case OP_IDIV64: 
        {
            if (0 == PVM->R[PVM_GET_RS(Opcode)].DWord)
                goto DivisionBy0;
            INTEGER_BINARY_OP(/, Opcode, .SDWord);
        } break;
        case OP_MOD64:
        {
            if (0 == PVM->R[PVM_GET_RS(Opcode)].DWord)
                goto DivisionBy0;
            INTEGER_BINARY_OP(%, Opcode, .DWord);
        } break;
        case OP_NEG64: 
        {
            PVM->R[PVM_GET_RD(Opcode)].DWord = -PVM->R[PVM_GET_RS(Opcode)].DWord;
        } break;

        case OP_SEQ64: INTEGER_SET_IF(==, Opcode, .DWord); break;
        case OP_SNE64: INTEGER_SET_IF(!=, Opcode, .DWord); break;
        case OP_SLT64: INTEGER_SET_IF(<, Opcode, .DWord); break;
        case OP_SGT64: INTEGER_SET_IF(>, Opcode, .DWord); break;
        case OP_ISLT64: INTEGER_SET_IF(<, Opcode, .SDWord); break;
        case OP_ISGT64: INTEGER_SET_IF(>, Opcode, .SDWord); break;
        case OP_SLE64: INTEGER_SET_IF(<=, Opcode, .DWord); break;
        case OP_SGE64: INTEGER_SET_IF(>=, Opcode, .DWord); break;
        case OP_ISLE64: INTEGER_SET_IF(<=, Opcode, .SDWord); break;
        case OP_ISGE64: INTEGER_SET_IF(>=, Opcode, .SDWord); break;
        }
    }
DivisionBy0:
    ReturnValue = PVM_DIVISION_BY_0;
    goto Exit;
CallStackOverflow:
    ReturnValue = PVM_CALLSTACK_OVERFLOW;
    goto Exit;

Exit:
    StreamOffset = IP - Chunk->Code;
    LineDebugInfo *Info = ChunkGetDebugInfo(Chunk, StreamOffset);
    if (Info->Count > 0)
        PVM->Error.Line = Info->Line[Info->Count - 1];
    else 
        PVM->Error.Line = Info->Line[0];
    PVM->Error.PC = StreamOffset;
    return ReturnValue;

#undef LOAD_FLOAT
#undef STORE_INTEGER
#undef STORE_INTEGER
#undef LOAD_INTEGER
#undef VERIFY_STACK_ADDR
#undef PUSH_MULTIPLE
#undef POP_MULTIPLE
#undef FLOAT_SET_IF
#undef FLOAT_BINARY_OP
#undef GET_BR_IMM
#undef GET_BCC_IMM
#undef INTEGER_SET_IF
#undef INTEGER_BINARY_OP
#undef ASSIGNMENT
}


void PVMDumpState(FILE *f, const PascalVM *PVM, UInt RegPerLine)
{
    fprintf(f, "\n===================== INT REGISTERS ======================");
    fprintf(f, "\nR%d: Stack pointer; R%d: Frame Pointer; R%d: Global Pointer",
            PVM_REG_SP, PVM_REG_FP, PVM_REG_GP
    );
    for (UInt i = 0; i < PVM_REG_COUNT; i++)
    {
        if (i % RegPerLine == 0)
        {
            fprintf(f, "\n");
        }
        fprintf(f, "[R%02d: %lld]", i, PVM->R[i].DWord);
    }

    fprintf(f, "\n===================== F%d REGISTERS ======================", (int)sizeof(PVM->F[0])*8);
    for (UInt i = 0; i < PVM_REG_COUNT; i++)
    {
        if (i % RegPerLine == 0)
        {
            fprintf(f, "\n");
        }
        fprintf(f, "[F%02d: %g]", i, PVM->F[i].Double);
    }

    fprintf(f, "\n===================== STACK ======================\n");
    for (PVMPTR Sp = PVM->Stack.Start; Sp.DWord <= PVM->R[PVM_REG_SP].Ptr.DWord; Sp.DWord++)
    {
        fprintf(f, "%8p: [0x%08llx]\n", Sp.Raw, *Sp.DWord);
    }
    fputc('\n', f);
}





