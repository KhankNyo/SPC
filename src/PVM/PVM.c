
#include <time.h>
#include <stdarg.h>

#include "Memory.h"
#include "Common.h"
#include "PVM/PVM.h"
#include "PVM/Disassembler.h"
#include "PVM/Debugger.h"
#include "PascalString.h"


PascalVM PVMInit(U32 StackSize, UInt RetStackSize)
{
    PascalVM PVM = {
        .F = { 0 },
        .R = { 0 },
        .FloatCondition = false,
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
#define FLOAT_SET_IF(Operator, Opc, RegType)\
    PVM->FloatCondition = PVM->F[PVM_GET_RD(Opc)]RegType Operator PVM->F[PVM_GET_RS(Opc)]RegType

#define PUSH_MULTIPLE(RegType, Base, Opc) do{\
    UInt RegList = PVM_GET_REGLIST(Opc);\
    UInt Base_ = Base;\
    UInt i = Base_;\
    while (RegList && i < Base_ + PVM_REG_COUNT/2) {\
        if (RegList & 1) {\
            *(++SP().Ptr.DWord) = PVM->RegType[i].DWord;\
        }\
        i++;\
        RegList >>= 1;\
    }\
} while (0)

#define POP_MULTIPLE(RegType, Base, Opc) do{\
    UInt RegList = PVM_GET_REGLIST(Opc);\
    UInt Base_ = Base;\
    UInt i = Base_;\
    while (RegList && i < Base_ + PVM_REG_COUNT/2) {\
        if (RegList & 0x80) {\
            PVM->RegType[(Base + (PVM_REG_COUNT/2)-1) - i].DWord = *(SP().Ptr.DWord--);\
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
#define MOVE_INTER(Opc, Rd, RdSize, Rs, RsSize)\
    PVM->Rd[PVM_GET_RD(Opc)]RdSize = PVM->Rs[PVM_GET_RS(Opc)]RsSize

#define LOAD_INTEGER(Opc, ImmType, IP, DestSize, AddrMode, Type, Cast)\
do {\
    U64 Offset = 0;\
    GET_SEX_IMM(Offset, ImmType, IP);\
    memcpy(&PVM->R[PVM_GET_RD(Opc)]DestSize, PVM->R[PVM_GET_RS(Opcode)]AddrMode + Offset, sizeof Type);\
    PVM->R[PVM_GET_RD(Opcode)]DestSize = (Cast PVM->R[PVM_GET_RD(Opcode)]DestSize);\
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








    U16 *IP = Chunk->Code + Chunk->EntryPoint;
    SP().Ptr = PVM->Stack.Start;
    FP().Ptr = SP().Ptr;
    PVM->R[PVM_REG_GP].Ptr.Raw = Chunk->Global.Data.As.Raw;
    PVMReturnValue ReturnValue = PVM_NO_ERROR;
    U32 StreamOffset = 0;

    while (1)
    {
#ifdef PVM_DEBUGGER
        if (PVM->SingleStepMode)
        {
            PVMDebugPause(PVM, Chunk, IP, SP().Ptr, FP().Ptr);
        }
#endif /* PVM_DEBUGGER */

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
            case OP_SYS_WRITE:
            {
                U32 ArgCount = PVM->R[0].Word.First;
                PascalStr **Ptr = SP().Ptr.Raw;
                Ptr--;
                for (U32 i = 0; i < ArgCount; i++)
                {
                    PASCAL_ASSERT(NULL != *Ptr, "Unreachable");
                    const PascalStr *PStr = *Ptr;
                    fprintf(stdout, "%.*s", 
                            (int)PStrGetLen(PStr), PStrGetConstPtr(PStr)
                    );
                    Ptr--;
                    PASCAL_ASSERT((void*)Ptr > FP().Ptr.Raw, "Unreachable");
                }
                /* callee does the cleanup */
                /* TODO: this is outrageously unsafe */
                SP().Ptr.Raw = Ptr;
            } break;
            }
        } break;

        case OP_ADD: INTEGER_BINARY_OP(+, Opcode, .Word.First); break;
        case OP_SUB: INTEGER_BINARY_OP(-, Opcode, .Word.First); break;
        case OP_MUL: INTEGER_BINARY_OP(*, Opcode, .Word.First); break;
        case OP_IMUL: INTEGER_BINARY_OP(*, Opcode, .SWord.First); break;
        case OP_AND: INTEGER_BINARY_OP(&, Opcode, .Word.First); break;
        case OP_OR:  INTEGER_BINARY_OP(|, Opcode, .Word.First); break;
        case OP_XOR: INTEGER_BINARY_OP(^, Opcode, .Word.First); break;
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
        case OP_NOT:
        {
            PVM->R[PVM_GET_RD(Opcode)].Word.First = ~PVM->R[PVM_GET_RS(Opcode)].Word.First;
        } break;
        case OP_VSHL: INTEGER_BINARY_OP(<<, Opcode, .Word.First) & 0x1F; break;
        case OP_VSHR: INTEGER_BINARY_OP(>>, Opcode, .Word.First) & 0x1F; break;
        case OP_VASR: INTEGER_BINARY_OP(>>, Opcode, .SWord.First) & 0x1F; break;
        case OP_QSHL: 
        {
            PVM->R[PVM_GET_RD(Opcode)].Word.First <<= PVM_GET_RS(Opcode);
        } break;
        case OP_QSHR: 
        {
            PVM->R[PVM_GET_RD(Opcode)].Word.First >>= PVM_GET_RS(Opcode);
        } break;
        case OP_QASR: 
        {
            PVM->R[PVM_GET_RD(Opcode)].SWord.First >>= PVM_GET_RS(Opcode);
        } break;
        case OP_ADDQI:
        {
            PVM->R[PVM_GET_RD(Opcode)].Word.First += BIT_SEX32(PVM_GET_RS(Opcode), 3); 
        } break;


        case OP_ADDI:
        {
            U32 Imm = 0;
            GET_SEX_IMM(Imm, PVM_GET_RS(Opcode), IP);
            PVM->R[PVM_GET_RD(Opcode)].Word.First += Imm;
        } break;


        case OP_SADD: 
        {
            PascalStr *Dst = PVM->R[PVM_GET_RD(Opcode)].Ptr.Raw;
            PascalStr *Src = PVM->R[PVM_GET_RS(Opcode)].Ptr.Raw;
            PascalStr *TmpStr = &PVM->TmpStr;

            /* We take the addr of TmpStr and put it into Rd because later on,
             * the compiler should have emitted code to copy the string pointed by Rd to wherever
             *
             * This is a shitty way to do ensure that dst is not modified and is unsafe
             */
            /* TODO: this does not work in an expr like: a + b + (a + b) */
            /* TODO: something better? */
            if (TmpStr != Dst)
            {
                /* reset TmpStr */
                PStrSetLen(TmpStr, 0);
                PStrCopyInto(TmpStr, Dst);
                PVM->R[PVM_GET_RD(Opcode)].Ptr.Raw = TmpStr;
            }
            PStrConcat(TmpStr, Src);
        } break;
        case OP_SCPY:
        {
            PascalStr *Dst = PVM->R[PVM_GET_RD(Opcode)].Ptr.Raw;
            const PascalStr *Src = PVM->R[PVM_GET_RS(Opcode)].Ptr.Raw;
            if (Src != Dst)
            {
                PStrCopyInto(Dst, Src);
            }
        } break;
        case OP_STRLT:
        {
            PascalStr *Dst = PVM->R[PVM_GET_RD(Opcode)].Ptr.Raw;
            PascalStr *Src = PVM->R[PVM_GET_RS(Opcode)].Ptr.Raw;
            PVM->R[PVM_GET_RD(Opcode)].Ptr.UInt = PStrIsLess(Dst, Src);
        } break;
        case OP_STRGT:
        {
            PascalStr *Dst = PVM->R[PVM_GET_RD(Opcode)].Ptr.Raw;
            PascalStr *Src = PVM->R[PVM_GET_RS(Opcode)].Ptr.Raw;
            PVM->R[PVM_GET_RD(Opcode)].Ptr.UInt = PStrIsLess(Src, Dst);
        } break;
        case OP_STREQU:
        {
            PascalStr *Dst = PVM->R[PVM_GET_RD(Opcode)].Ptr.Raw;
            PascalStr *Src = PVM->R[PVM_GET_RS(Opcode)].Ptr.Raw;
            PVM->R[PVM_GET_RD(Opcode)].Ptr.UInt = PStrEqu(Dst, Src);
        } break;
        case OP_SETEZ: 
        {
            PVM->R[PVM_GET_RD(Opcode)].Word.First = 0 == PVM->R[PVM_GET_RS(Opcode)].Word.First;
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
        case OP_BEZ:
        {
            I32 Offset = GET_BCC_IMM(Opcode, IP);
            if (0 == PVM->R[PVM_GET_RD(Opcode)].Word.First)
            {
                /* TODO: bound chk */
                IP += Offset;
            }
        } break;


        case OP_PSHL: PUSH_MULTIPLE(R, 0, Opcode); break;
        case OP_POPL: POP_MULTIPLE(R, 0, Opcode); break;
        case OP_PSHH: PUSH_MULTIPLE(R, 8, Opcode); break;
        case OP_POPH: POP_MULTIPLE(R, 8, Opcode); break;
        case OP_FPSHL: PUSH_MULTIPLE(F, 0, Opcode); break;
        case OP_FPOPL: POP_MULTIPLE(F, 0, Opcode); break;
        case OP_FPSHH: PUSH_MULTIPLE(F, 8, Opcode); break;
        case OP_FPOPH: POP_MULTIPLE(F, 8, Opcode); break;


        case OP_FADD: FLOAT_BINARY_OP(+, Opcode, .Single); break;
        case OP_FSUB: FLOAT_BINARY_OP(-, Opcode, .Single); break;
        case OP_FMUL: FLOAT_BINARY_OP(*, Opcode, .Single); break;
        case OP_FDIV: FLOAT_BINARY_OP(/, Opcode, .Single); break;
        case OP_FNEG:
        {
            PVM->F[PVM_GET_RD(Opcode)].Single = -PVM->F[PVM_GET_RS(Opcode)].Single;
        } break;
        case OP_FSEQ: FLOAT_SET_IF(==, Opcode, .Single); break;
        case OP_FSNE: FLOAT_SET_IF(!=, Opcode, .Single); break;
        case OP_FSLT: FLOAT_SET_IF(<, Opcode, .Single); break;
        case OP_FSGT: FLOAT_SET_IF(>, Opcode, .Single); break;
        case OP_FSLE: FLOAT_SET_IF(<=, Opcode, .Single); break;
        case OP_FSGE: FLOAT_SET_IF(>=, Opcode, .Single); break;

        case OP_FADD64: FLOAT_BINARY_OP(+, Opcode, .Double); break;
        case OP_FSUB64: FLOAT_BINARY_OP(-, Opcode, .Double); break;
        case OP_FMUL64: FLOAT_BINARY_OP(*, Opcode, .Double); break;
        case OP_FDIV64: FLOAT_BINARY_OP(/, Opcode, .Double); break;
        case OP_FNEG64:
        {
            PVM->F[PVM_GET_RD(Opcode)].Double = -PVM->F[PVM_GET_RS(Opcode)].Double;
        } break;
        case OP_FSEQ64: FLOAT_SET_IF(==, Opcode, .Double); break;
        case OP_FSNE64: FLOAT_SET_IF(!=, Opcode, .Double); break;
        case OP_FSLT64: FLOAT_SET_IF(<, Opcode, .Double); break;
        case OP_FSGT64: FLOAT_SET_IF(>, Opcode, .Double); break;
        case OP_FSLE64: FLOAT_SET_IF(<=, Opcode, .Double); break;
        case OP_FSGE64: FLOAT_SET_IF(>=, Opcode, .Double); break;
        case OP_GETFCC: PVM->R[PVM_GET_RD(Opcode)].Word.First = PVM->FloatCondition; break;


        case OP_MOV32:       MOVE_INTEGER(Opcode, .Word.First, .Word.First); break;
        case OP_MOVZEX32_8:  MOVE_INTEGER(Opcode, .Word.First, .Byte[PVM_LEAST_SIGNIF_BYTE]); break;
        case OP_MOVZEX32_16: MOVE_INTEGER(Opocde, .Word.First, .Half.First); break;
        case OP_MOV64:       MOVE_INTEGER(Opcode, .DWord, .DWord); break;
        case OP_MOVZEX64_8:  MOVE_INTEGER(Opcode, .DWord, .Byte[PVM_LEAST_SIGNIF_BYTE]); break;
        case OP_MOVZEX64_16: MOVE_INTEGER(Opcode, .DWord, .Half.First); break;
        case OP_MOVZEX64_32: MOVE_INTEGER(Opcode, .DWord, .Word.First); break;
        case OP_MOVSEX64_32: MOVE_INTEGER(Opcode, .SDWord, .SWord.First); break;
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
        case OP_FMOV:   MOVE_FLOAT(Opcode, .Single, .Single); break;
        case OP_FMOV64: MOVE_FLOAT(Opcode, .Double, .Double); break;


        case OP_F32TOF64:   MOVE_FLOAT(Opcode, .Double, .Single); break;
        case OP_F64TOF32:   MOVE_FLOAT(Opcode, .Single, .Double); break;
        case OP_F64TOI64:   MOVE_INTER(Opcode, R, .SDWord, F, .Double); break;
        case OP_I64TOF64:   MOVE_INTER(Opcode, F, .Double, R, .SDWord); break;
        case OP_I64TOF32:   MOVE_INTER(Opcode, F, .Single, R, .SDWord); break;
        case OP_U64TOF64:   MOVE_INTER(Opcode, F, .Double, R, .DWord); break;
        case OP_U64TOF32:   MOVE_INTER(Opcode, F, .Single, R, .DWord); break;
        case OP_U32TOF32:   MOVE_INTER(Opcode, F, .Single, R, .Word.First); break;
        case OP_U32TOF64:   MOVE_INTER(Opcode, F, .Double, R, .Word.First); break;
        case OP_I32TOF32:   MOVE_INTER(Opcode, F, .Single, R, .SWord.First); break;
        case OP_I32TOF64:   MOVE_INTER(Opcode, F, .Double, R, .SWord.First); break;


        case OP_LD32:       LOAD_INTEGER(Opcode, IMMTYPE_I16, IP, .Word.First,  .Ptr.Byte, (U32), (U32)); break;
        case OP_LD64:       LOAD_INTEGER(Opcode, IMMTYPE_I16, IP, .DWord,       .Ptr.Byte, (U64), (U64)); break;
        case OP_LDZEX32_8:  LOAD_INTEGER(Opcode, IMMTYPE_I16, IP, .Word.First,  .Ptr.Byte, (U8),  (U32)(U8)); break;
        case OP_LDZEX32_16: LOAD_INTEGER(Opcode, IMMTYPE_I16, IP, .Word.First,  .Ptr.Byte, (U16), (U32)(U16)); break;
        case OP_LDZEX64_8:  LOAD_INTEGER(Opcode, IMMTYPE_I16, IP, .DWord,       .Ptr.Byte, (U8),  (U64)(U8)); break;
        case OP_LDZEX64_16: LOAD_INTEGER(Opcode, IMMTYPE_I16, IP, .DWord,       .Ptr.Byte, (U16), (U64)(U16)); break;
        case OP_LDZEX64_32: LOAD_INTEGER(Opcode, IMMTYPE_I16, IP, .DWord,       .Ptr.Byte, (U32), (U64)(U32)); break;
        case OP_LDSEX32_8:  LOAD_INTEGER(Opcode, IMMTYPE_I16, IP, .SWord.First, .Ptr.Byte, (U8),  (I32)(I8)); break;
        case OP_LDSEX32_16: LOAD_INTEGER(Opcode, IMMTYPE_I16, IP, .SWord.First, .Ptr.Byte, (U16), (I32)(I16)); break;
        case OP_LDSEX64_8:  LOAD_INTEGER(Opcode, IMMTYPE_I16, IP, .SDWord,      .Ptr.Byte, (U8),  (I64)(I8)); break;
        case OP_LDSEX64_16: LOAD_INTEGER(Opcode, IMMTYPE_I16, IP, .SDWord,      .Ptr.Byte, (U16), (I64)(I32)); break;
        case OP_LDSEX64_32: LOAD_INTEGER(Opcode, IMMTYPE_I16, IP, .SDWord,      .Ptr.Byte, (U32), (I64)(I32)); break;

        case OP_LD32L:       LOAD_INTEGER(Opcode, IMMTYPE_I32, IP, .Word.First, .Ptr.Byte, (U32), (U32)); break;
        case OP_LD64L:       LOAD_INTEGER(Opcode, IMMTYPE_I32, IP, .DWord,      .Ptr.Byte, (U64), (U64)); break;
        case OP_LDZEX32_8L:  LOAD_INTEGER(Opcode, IMMTYPE_I32, IP, .Word.First, .Ptr.Byte, (U8),  (U32)(U8)); break;
        case OP_LDZEX32_16L: LOAD_INTEGER(Opcode, IMMTYPE_I32, IP, .Word.First, .Ptr.Byte, (U16), (U32)(U16)); break;
        case OP_LDZEX64_8L:  LOAD_INTEGER(Opcode, IMMTYPE_I32, IP, .DWord,      .Ptr.Byte, (U8),  (U64)(U8)); break;
        case OP_LDZEX64_16L: LOAD_INTEGER(Opcode, IMMTYPE_I32, IP, .DWord,      .Ptr.Byte, (U16), (U64)(U16)); break;
        case OP_LDZEX64_32L: LOAD_INTEGER(Opcode, IMMTYPE_I32, IP, .DWord,      .Ptr.Byte, (U32), (U64)(U32)); break;
        case OP_LDSEX32_8L:  LOAD_INTEGER(Opcode, IMMTYPE_I32, IP, .SWord.First,.Ptr.Byte, (U8),  (I32)(I8)); break;
        case OP_LDSEX32_16L: LOAD_INTEGER(Opcode, IMMTYPE_I32, IP, .SWord.First,.Ptr.Byte, (U16), (I32)(I16)); break;
        case OP_LDSEX64_8L:  LOAD_INTEGER(Opcode, IMMTYPE_I32, IP, .SDWord,     .Ptr.Byte, (U8),  (I64)(I8)); break;
        case OP_LDSEX64_16L: LOAD_INTEGER(Opcode, IMMTYPE_I32, IP, .SDWord,     .Ptr.Byte, (U16), (I64)(I16)); break;
        case OP_LDSEX64_32L: LOAD_INTEGER(Opcode, IMMTYPE_I32, IP, .SDWord,     .Ptr.Byte, (U32), (I64)(I32)); break;

        case OP_LEA:
        {
            U64 Offset = 0;
            GET_SEX_IMM(Offset, IMMTYPE_I16, IP);
            PVM->R[PVM_GET_RD(Opcode)].Ptr.UInt = PVM->R[PVM_GET_RS(Opcode)].Ptr.UInt + Offset;
        } break;
        case OP_LEAL:
        {
            U64 Offset = 0;
            GET_SEX_IMM(Offset, IMMTYPE_I32, IP);
            PVM->R[PVM_GET_RD(Opcode)].Ptr.UInt = PVM->R[PVM_GET_RS(Opcode)].Ptr.UInt + Offset;
        } break;

        case OP_ST8:  STORE_INTEGER(Opcode, IMMTYPE_I16, IP, .Byte[PVM_LEAST_SIGNIF_BYTE], .Ptr.Byte); break;
        case OP_ST16: STORE_INTEGER(Opcode, IMMTYPE_I16, IP, .Half.First, .Ptr.Byte); break;
        case OP_ST32: STORE_INTEGER(Opcode, IMMTYPE_I16, IP, .Word.First, .Ptr.Byte); break;
        case OP_ST64: STORE_INTEGER(Opcode, IMMTYPE_I16, IP, .DWord, .Ptr.Byte); break;
        case OP_ST8L:  STORE_INTEGER(Opcode, IMMTYPE_I32, IP, .Byte[PVM_LEAST_SIGNIF_BYTE], .Ptr.Byte); break;
        case OP_ST16L: STORE_INTEGER(Opcode, IMMTYPE_I32, IP, .Half.First, .Ptr.Byte); break;
        case OP_ST32L: STORE_INTEGER(Opcode, IMMTYPE_I32, IP, .Word.First, .Ptr.Byte); break;
        case OP_ST64L: STORE_INTEGER(Opcode, IMMTYPE_I32, IP, .DWord, .Ptr.Byte); break;

        case OP_LDF32: LOAD_FLOAT(Opcode, IMMTYPE_I16, IP, .Single, .Ptr.Byte); break;
        case OP_STF32: STORE_FLOAT(Opcode, IMMTYPE_I16, IP, .Single, .Ptr.Byte); break;
        case OP_LDF64: LOAD_FLOAT(Opcode, IMMTYPE_I16, IP, .Double, .Ptr.Byte); break;
        case OP_STF64: STORE_FLOAT(Opcode, IMMTYPE_I16, IP, .Double, .Ptr.Byte); break;
        case OP_LDF32L: LOAD_FLOAT(Opcode, IMMTYPE_I32, IP, .Single, .Ptr.Byte); break;
        case OP_STF32L: STORE_FLOAT(Opcode, IMMTYPE_I32, IP, .Single, .Ptr.Byte); break;
        case OP_LDF64L: LOAD_FLOAT(Opcode, IMMTYPE_I32, IP, .Double, .Ptr.Byte); break;
        case OP_STF64L: STORE_FLOAT(Opcode, IMMTYPE_I32, IP, .Double, .Ptr.Byte); break;


        case OP_ADD64: INTEGER_BINARY_OP(+, Opcode, .DWord); break;
        case OP_SUB64: INTEGER_BINARY_OP(-, Opcode, .DWord); break;
        case OP_MUL64: INTEGER_BINARY_OP(*, Opcode, .DWord); break;
        case OP_IMUL64: INTEGER_BINARY_OP(*, Opcode, .SDWord); break;
        case OP_AND64: INTEGER_BINARY_OP(&, Opcode, .DWord); break;
        case OP_OR64:  INTEGER_BINARY_OP(|, Opcode, .DWord); break;
        case OP_XOR64: INTEGER_BINARY_OP(^, Opcode, .DWord); break;
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
        case OP_NOT64:
        {
            PVM->R[PVM_GET_RD(Opcode)].DWord = ~PVM->R[PVM_GET_RS(Opcode)].DWord;
        } break;
        case OP_VSHL64: INTEGER_BINARY_OP(<<, Opcode, .DWord) & 0x3F; break;
        case OP_VSHR64: INTEGER_BINARY_OP(>>, Opcode, .DWord) & 0x3F; break;
        case OP_VASR64: INTEGER_BINARY_OP(>>, Opcode, .SDWord) & 0x3F; break;
        case OP_QSHL64: 
        {
            PVM->R[PVM_GET_RD(Opcode)].DWord <<= PVM_GET_RS(Opcode);
        } break;
        case OP_QSHR64: 
        {
            PVM->R[PVM_GET_RD(Opcode)].DWord >>= PVM_GET_RS(Opcode);
        } break;
        case OP_QASR64: 
        {
            PVM->R[PVM_GET_RD(Opcode)].SDWord >>= PVM_GET_RS(Opcode);
        } break;

        case OP_SETEZ64: 
        {
            PVM->R[PVM_GET_RD(Opcode)].DWord = 0 == PVM->R[PVM_GET_RS(Opcode)].DWord;
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
    if (NULL == Info)
    {
        return ReturnValue;
    }

    if (Info->Count > 0)
    {
        PVM->Error.Line = Info->Line[Info->Count - 1];
    }
    else 
    {
        PVM->Error.Line = Info->Line[0];
    }
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
    fprintf(f, "\nR%d: Stack pointer; R%d: Frame Pointer; R%d: Global Pointer\n",
            PVM_REG_SP, PVM_REG_FP, PVM_REG_GP
    );
    for (UInt i = 0; i < PVM_REG_COUNT; i++)
    {
        if (i % RegPerLine == 0)
        {
            fprintf(f, "\n");
        }
        fprintf(f, "[R%02d: %llx]", i, PVM->R[i].DWord);
    }

    fprintf(f, "\n===================== F%d REGISTERS ======================\n", (int)sizeof(PVM->F[0])*8);
    fprintf(f, "[Fnn: Double|Single]\n");
    for (UInt i = 0; i < PVM_REG_COUNT; i++)
    {
        if (i % RegPerLine == 0)
        {
            fprintf(f, "\n");
        }
        fprintf(f, "[F%02d: %g|%g]", i, PVM->F[i].Double, PVM->F[i].Single);
    }

    fprintf(f, "\n===================== STACK|GLOBAL ======================\n");
    for (PVMPTR Sp = PVM->Stack.Start; Sp.DWord <= PVM->R[PVM_REG_SP].Ptr.DWord; Sp.DWord++)
    {
        fprintf(f, "S: %8p: [0x%08llx]\n", Sp.Raw, *Sp.DWord);
    }
    fputc('\n', f);
}





