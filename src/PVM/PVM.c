
#include <time.h>
#include <stdarg.h>
#include <inttypes.h>

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
        .Condition = false,
        .Stack.Start.Raw = MemAllocateArray(PVM.Stack.Start.DWord[0], StackSize),
        .RetStack.Start = MemAllocateArray(PVM.RetStack.Start[0], RetStackSize),
        .RetStack.SizeLeft = RetStackSize,

        .LogFile = stderr,
        .Error = { 0 }, 
        .SingleStepMode = false,
        .Disassemble = false,
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


static void RuntimeError(const PascalVM *PVM, const char *Fmt, ...)
{
    va_list Args;
    va_start(Args, Fmt);
    if (NULL != PVM->LogFile)
    {
        fprintf(PVM->LogFile, "Runtime Error: [line %d]:\n\t", PVM->Error.Line);
        fprintf(PVM->LogFile, Fmt, Args);
    }
    va_end(Args);
}

static const PascalStr *RuntimeTypeToStr(IntegralType Type, PVMGPR Data)
{
#define INTEGER_TO_STR(Fmt, RegType) do {\
    int Len = snprintf(Str, PSTR_MAX_LEN, Fmt, Data RegType);\
    PStrSetLen(&Tmp, Len);\
} while (0)

    static PascalStr Tmp;
    char *Str = (char *)PStrGetPtr(&Tmp);
    PStrSetLen(&Tmp, 0);

    switch (Type)
    {
    case TYPE_STRING: return Data.Ptr.Raw;

    case TYPE_CHAR:
    {
        int Len = snprintf(Str, PSTR_MAX_LEN, "%c", Data.SWord.First);
        PStrSetLen(&Tmp, Len);
    } break;
    case TYPE_I8:  INTEGER_TO_STR("%"PRIi8, .SByte[PVM_LEAST_SIGNIF_BYTE]); break;
    case TYPE_I16: INTEGER_TO_STR("%"PRIi16, .SHalf.First); break;
    case TYPE_I32: INTEGER_TO_STR("%"PRIi32, .SWord.First); break;
    case TYPE_I64: INTEGER_TO_STR("%"PRIi64, .SDWord); break;

    case TYPE_U8:  INTEGER_TO_STR("%"PRIu8, .Byte[PVM_LEAST_SIGNIF_BYTE]); break;
    case TYPE_U16: INTEGER_TO_STR("%"PRIu16, .Half.First); break;
    case TYPE_U32: INTEGER_TO_STR("%"PRIu32, .Word.First); break;
    case TYPE_U64: INTEGER_TO_STR("%"PRIu64, .DWord); break;

    case TYPE_BOOLEAN:
    {
        if (Data.Word.First)
        {
            Tmp = PStrCopy((const U8*)"TRUE", sizeof("TRUE") - 1);
        }
        else
        {
            Tmp = PStrCopy((const U8*)"FALSE", sizeof("FALSE") - 1);
        }
    } break;
    case TYPE_POINTER:
    {
        int Len;
        if (NULL == Data.Ptr.Raw)
        {
            Len = snprintf(Str, PSTR_MAX_LEN, "nil");
        }
        else
        { 
            Len = snprintf(Str, PSTR_MAX_LEN, "%p", Data.Ptr.Raw);
        }
        PStrSetLen(&Tmp, Len);
    } break;
    case TYPE_F64:
    { 
        F64 f64;
        memcpy(&f64, &Data, sizeof f64);
        int Len = snprintf(Str, PSTR_MAX_LEN, "%f", f64);
        PStrSetLen(&Tmp, Len);
    } break;
    case TYPE_F32:
    {
        PVMFPR FltReg;
        memcpy(&FltReg, &Data, sizeof FltReg);
        F32 f32 = FltReg.Single;
        int Len = snprintf(Str, PSTR_MAX_LEN, "%f", f32);
        PStrSetLen(&Tmp, Len);
    } break;

    case TYPE_FUNCTION:
    case TYPE_COUNT:
    case TYPE_RECORD:
    case TYPE_INVALID:
    {
        PASCAL_UNREACHABLE("Invalid type in %s", __func__);
    } break;

    }

    return &Tmp;

#undef INTEGER_TO_STR
}


bool PVMRun(PascalVM *PVM, PVMChunk *Chunk)
{
    if (PVM->Disassemble)
    {
        PVMDisasm(PVM->LogFile, Chunk, "Compiled Code");
        fprintf(PVM->LogFile, "Press Enter to execute...\n");
        getc(stdin);
    }

    bool NoError = false;
    double Start = clock();
    PVMReturnValue Ret = PVMInterpret(PVM, Chunk);
    double End = clock();

    if (PVM->Disassemble)
        PVMDumpState(PVM->LogFile, PVM, 4);


    if (NULL != PVM->LogFile)
    {
        switch (Ret)
        {
        case PVM_NO_ERROR:
        {
            fprintf(PVM->LogFile, "Finished execution.\n"
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
    PVM->Condition = PVM->R[PVM_GET_RD(Opc)]RegType Operator PVM->R[PVM_GET_RS(Opc)]RegType

#define GET_BR_IMM(Opc, IP) \
    BitSex32Safe(((U32)*IP++ << 8) | (((U32)(Opc) & 0xFF)), 23) /* sign extend at bit 23 */
#define GET_BCC_IMM(Opc, IP) \
    BitSex32Safe(((U32)*IP++ << 4) | (((U32)(Opc) & 0xF)), 19) /* sign extend at bit 19 */

#define FLOAT_BINARY_OP(Operator, Opc, RegType)\
    PVM->F[PVM_GET_RD(Opc)]RegType GLUE(Operator,=) PVM->F[PVM_GET_RS(Opc)]RegType
#define FLOAT_SET_IF(Operator, Opc, RegType)\
    PVM->Condition = PVM->F[PVM_GET_RD(Opc)]RegType Operator PVM->F[PVM_GET_RS(Opc)]RegType

/* starting from R(Base) to R(Base + 8) */
#define PUSH_MULTIPLE(RegType, Base, Opc) do{\
    UInt RegList = PVM_GET_REGLIST(Opc);\
    UInt Base_ = Base;\
    UInt i = Base_;\
    while (RegList && i < Base_ + PVM_REG_COUNT/2) {\
        if (RegList & 1) {\
            *(++SP().Ptr.DWord) = PVM->RegType[i].DWord;\
            /* TODO: check stack */\
        }\
        i++;\
        RegList >>= 1;\
    }\
} while (0)

/* starting from R(Base + 8) to R(Base) */
#define POP_MULTIPLE(RegType, Base, Opc) do{\
    UInt RegList = PVM_GET_REGLIST(Opc);\
    UInt Base_ = Base;\
    UInt i = Base_;\
    while (RegList && i < Base_ + PVM_REG_COUNT/2) {\
        if (RegList & 0x80) {\
            PVM->RegType[(Base + (PVM_REG_COUNT/2)-1) - i].DWord = *(SP().Ptr.DWord--);\
            /* TODO: check stack */\
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
    FP().Ptr = PVM->Stack.Start;
    SP().Ptr.Byte = PVM->Stack.Start.Byte - sizeof(PVMGPR);
    PVM->R[PVM_REG_GP].Ptr.Raw = Chunk->Global.Data.As.Raw;
    PVMReturnValue ReturnValue = PVM_NO_ERROR;
    U32 StreamOffset = 0;

    while (1)
    {
        if (PVM->SingleStepMode)
        {
            PVMDebugPause(PVM, Chunk, IP);
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
                /* global scope, exit */
                if (PVM->RetStack.Val == PVM->RetStack.Start)
                    goto Exit;

                /* stack scope, return */
                PVM->RetStack.Val--;
                IP = PVM->RetStack.Val->IP;
                SP().Ptr.Byte = FP().Ptr.Byte - sizeof(PVMGPR);
                FP().Ptr = PVM->RetStack.Val->FP;
                PVM->RetStack.SizeLeft++;
            } break;
            case OP_SYS_ENTER:
            {
                FP().Ptr.Byte = SP().Ptr.Byte + sizeof(PVMGPR);
                U32 StackSize = 0;
                GET_SEX_IMM(StackSize, IMMTYPE_U32, IP);
                SP().Ptr.Byte += StackSize;

                if (SP().Ptr.Byte >= PVM->Stack.End.Byte)
                {
                    /* TODO: check stack */
                }
            } break;
            case OP_SYS_WRITE:
            {
                U32 ArgCount = PVM->R[0].Word.First;
                PVMGPR *Ptr = SP().Ptr.Raw;

                /* TODO: make this more clear */
                Ptr -= ArgCount*2 - 1;
                PVMGPR *Cleanup = Ptr;
                PASCAL_ASSERT((void*)Ptr >= FP().Ptr.Raw, "Unreachable: %d", ArgCount);

                FILE *OutFile = PVM->R[1].Ptr.Raw;
                PASCAL_NONNULL(OutFile);
                for (U32 i = 0; i < ArgCount; i++)
                {
                    PVMGPR Value = (*Ptr++);
                    IntegralType Type = (*Ptr++).DWord;
                    const PascalStr *PStr = RuntimeTypeToStr(Type, Value);
                    fprintf(OutFile, "%.*s", 
                            (int)PStrGetLen(PStr), PStrGetConstPtr(PStr)
                    );
                }
                /* callee does the cleanup */
                SP().Ptr.Raw = Cleanup - 1;
                PASCAL_ASSERT((void*)Ptr >= FP().Ptr.Raw, "Unreachable: %d", ArgCount);
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
        case OP_MEMCPY:
        {
            void *Dst = PVM->R[PVM_GET_RD(Opcode)].Ptr.Raw;
            const void *Src = PVM->R[PVM_GET_RS(Opcode)].Ptr.Raw;
            U32 Size = 0;
            GET_SEX_IMM(Size, IMMTYPE_U32, IP);
            memcpy(Dst, Src, Size);
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
            /* TODO: bound chk */
            IP += Offset;
            PASCAL_ASSERT(IP < Chunk->Code + Chunk->Count, "Unreachable");
        } break;
        case OP_CALL:
        {
            if (0 == PVM->RetStack.SizeLeft)
                goto CallStackOverflow;

            I32 Offset = GET_BR_IMM(Opcode, IP);
            /* save frame */
            PVM->RetStack.Val->IP = IP;
            PVM->RetStack.Val->FP = FP().Ptr;
            PVM->RetStack.Val++;
            PVM->RetStack.SizeLeft--;

            /* TODO: bound chk */

            IP += Offset;
        } break;
        case OP_CALLPTR:
        {
            if (0 == PVM->RetStack.SizeLeft)
                goto CallStackOverflow;

            PVM->RetStack.Val->IP = IP;
            PVM->RetStack.Val->FP = FP().Ptr;
            PVM->RetStack.Val++;
            PVM->RetStack.SizeLeft--;

            /* TODO: bound chk */

            IP = PVM->R[PVM_GET_RD(Opcode)].Ptr.Raw;
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
        case OP_BNZ:
        {
            I32 Offset = GET_BCC_IMM(Opcode, IP);
            if (PVM->R[PVM_GET_RD(Opcode)].Word.First)
            {
                /* TODO: bound chk */
                IP += Offset;
            }
        } break;
        case OP_BCT:
        {
            I32 Offset = GET_BR_IMM(Opcode, IP);
            if (PVM->Condition)
            {
                /* TODO: bound chk */
                IP += Offset;
            }
        } break;
        case OP_BCF:
        {
            I32 Offset = GET_BR_IMM(Opcode, IP);
            if (!PVM->Condition)
            {
                /* TODO: bound check */
                IP += Offset;
            }
        } break;
        case OP_BRI:
        {
            I32 Offset = (I16)*IP++;
            /* TODO: bound check */
            IP += Offset;
            PVM->R[PVM_GET_RD(Opcode)].DWord += BitSex64(PVM_GET_RS(Opcode), 3);
        } break;
        case OP_LDRIP:
        {
            I32 Offset = 0; 
            GET_SEX_IMM(Offset, PVM_GET_IMMTYPE(Opcode), IP);
            PVM->R[PVM_GET_RD(Opcode)].Ptr.Raw = IP + Offset;
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

        case OP_GETFLAG: PVM->R[PVM_GET_RD(Opcode)].Word.First = PVM->Condition; break;
        case OP_SETFLAG: PVM->Condition = 0 != PVM->R[PVM_GET_RD(Opcode)].Word.First; break;
        case OP_SETNFLAG: PVM->Condition = 0 == PVM->R[PVM_GET_RD(Opcode)].Word.First; break;
        case OP_NEGFLAG: PVM->Condition = !PVM->Condition; break;


        case OP_MOV32:       MOVE_INTEGER(Opcode, .Word.First, .Word.First); break;
        case OP_MOVZEX32_8:  MOVE_INTEGER(Opcode, .Word.First, .Byte[PVM_LEAST_SIGNIF_BYTE]); break;
        case OP_MOVZEX32_16: MOVE_INTEGER(Opcode, .Word.First, .Half.First); break;
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
            PVM->R[PVM_GET_RD(Opcode)].SDWord = Imm;
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
        case OP_ADDQI64:
        {
            PVM->R[PVM_GET_RD(Opcode)].DWord += BIT_SEX32(PVM_GET_RS(Opcode), 3); 
        } break;
        case OP_ADDI64:
        {
            U64 Imm = 0;
            GET_SEX_IMM(Imm, PVM_GET_RS(Opcode), IP);
            PVM->R[PVM_GET_RD(Opcode)].DWord += Imm;
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

        default:
        {
            goto IllegalInstruction;
        } break;
        }
    }
DivisionBy0:
    ReturnValue = PVM_DIVISION_BY_0;
    goto Exit;
CallStackOverflow:
    ReturnValue = PVM_CALLSTACK_OVERFLOW;
    goto Exit;
IllegalInstruction:
    ReturnValue = PVM_ILLEGAL_INSTRUCTION;
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

    fprintf(f, "\n===================== STACK ======================");
    for (PVMPTR Sp = PVM->Stack.Start; Sp.DWord <= PVM->R[PVM_REG_SP].Ptr.DWord; Sp.DWord++)
    {
        fprintf(f, "\nS: %8p: [0x%08llx]", Sp.Raw, *Sp.DWord);
        int Pad = sizeof(" <- SP");
        if (Sp.Raw == PVM->R[PVM_REG_SP].Ptr.Raw)
        {
            fprintf(f, " <- SP");
            Pad = 1;
        }
        if (Sp.Raw == PVM->R[PVM_REG_FP].Ptr.Raw)
            fprintf(f, "%*s<- FP", Pad, "");
    }
    fputc('\n', f);
}





