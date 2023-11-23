
#include <time.h>
#include <stdarg.h>

#include "Memory.h"
#include "PVM/_PVM.h"
#include "PVM/_Disassembler.h"


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
    PVMDumpState(stdout, PVM, 8);


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
#define ASSIGNMENT 
#define INTEGER_BINARY_OP(Operator, Opc, RegType)\
    PVM->R[PVM_GET_RD(Opc)]RegType GLUE(Operator,=) PVM->R[PVM_GET_RS(Opc)]RegType
#define INTEGER_SET_IF(Operator, Opc, RegType)\
    PVM->R[PVM_GET_RD(Opc)]RegType = PVM->R[PVM_GET_RD(Opc)]RegType Operator PVM->R[PVM_GET_RS(Opc)]RegType

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
            *(++SP.DWord) = PVM->R[i].DWord;\
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
            PVM->R[(Base + (PVM_REG_COUNT/2)-1) - i].DWord = *(SP.DWord--);\
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

#define LOAD_INTEGER_FROM_STACK(Opc, IP, DestSize, AddrMode)\
    memcpy(&PVM->R[PVM_GET_RD(Opc)]DestSize, &FP AddrMode [GET_LONG_IMM(Opc, IP)], sizeof PVM->R[0]DestSize)
#define STORE_INTEGER_TO_STACK(Opc, IP, SrcSize, AddrMode)\
    memcpy(&FP AddrMode [GET_LONG_IMM(Opc, IP)], &PVM->R[PVM_GET_RD(Opc)]SrcSize, sizeof PVM->R[0]SrcSize)

#define LOAD_INTEGER_FROM_GLOBAL(Opc, IP, DestSize)\
    memcpy(&PVM->R[PVM_GET_RD(Opc)]DestSize, &Chunk->Global.Data.As.u8[GET_LONG_IMM(Opc, IP)], sizeof PVM->R[0]DestSize)
#define STORE_INTEGER_TO_GLOBAL(Opc, IP, SrcSize)\
    memcpy(&Chunk->Global.Data.As.u8[GET_LONG_IMM(Opc, IP)], &PVM->R[PVM_GET_RD(Opc)]SrcSize, sizeof PVM->R[0]SrcSize)

#define LOAD_FLOAT_FROM_GLOBAL(Opc, IP, DestSize)\
    memcpy(&PVM->F[PVM_GET_RD(Opc)]DestSize, &Chunk->Global.Data.As.u8[GET_LONG_IMM(Opc, IP)], sizeof PVM->F[0]DestSize)
#define STORE_FLOAT_TO_GLOBAL(Opc, IP, SrcSize)\
    memcpy(&Chunk->Global.Data.As.u8[GET_LONG_IMM(Opc, IP)], &PVM->F[PVM_GET_RD(Opc)]SrcSize, sizeof PVM->F[0]SrcSize)

#define LOAD_FLOAT_FROM_STACK(Opc, IP, DestSize, AddrMode)\
    memcpy(&PVM->F[PVM_GET_RD(Opc)]DestSize, &FP AddrMode [GET_LONG_IMM(Opc, IP)], sizeof PVM->F[0]DestSize)
#define STORE_FLOAT_TO_STACK(Opc, IP, SrcSize, AddrMode)\
    memcpy(&FP AddrMode [GET_LONG_IMM(Opc, IP)], &PVM->F[PVM_GET_RD(Opc)]SrcSize, sizeof PVM->F[0]SrcSize)







    U16 *IP = Chunk->Code;
    PVMPTR FP = { .Raw = PVM->Stack.Start.Raw };
    PVMPTR SP = FP;

    while (1)
    {
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
                FP = PVM->RetStack.Val->FP;
                PVM->RetStack.SizeLeft++;
            } break;
            }
        } break;

        case OP_ADD: INTEGER_BINARY_OP(+, Opcode, .Word.First); break;
        case OP_SUB: INTEGER_BINARY_OP(-, Opcode, .Word.First); break;
        case OP_MUL: INTEGER_BINARY_OP(*, Opcode, .Word.First); break;
        case OP_DIV:
        {
            if (0 == PVM->R[PVM_GET_RS(Opcode)].Word.First)
                goto DivisionBy0;

            INTEGER_BINARY_OP(/, Opcode, .Word.First);
        } break;
        case OP_IMUL: INTEGER_BINARY_OP(*, Opcode, .SWord.First); break;
        case OP_IDIV:
        {
            if (0 == PVM->R[PVM_GET_RS(Opcode)].SWord.First)
                goto DivisionBy0;

            INTEGER_BINARY_OP(/, Opcode, .SWord.First);
        } break;
        case OP_NEG:
        {
            PVM->R[PVM_GET_RD(Opcode)].Word.First = -PVM->R[PVM_GET_RS(Opcode)].Word.First;
        } break;

        case OP_SEQ: INTEGER_SET_IF(==, Opcode, .Word.First); break;
        case OP_SNE: INTEGER_SET_IF(!=, Opcode, .Word.First); break;
        case OP_SLT: INTEGER_SET_IF(<, Opcode, .Word.First); break;
        case OP_SGT: INTEGER_SET_IF(>, Opcode, .Word.First); break;
        case OP_ISLT: INTEGER_SET_IF(<, Opcode, .SWord.First); break;
        case OP_ISGT: INTEGER_SET_IF(>, Opcode, .SWord.First); break;


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

            PVM->RetStack.Val->IP = IP;
            PVM->RetStack.Val->FP = FP;
            PVM->RetStack.Val++;
            PVM->RetStack.SizeLeft--;

            I32 Offset = GET_BR_IMM(Opcode, IP);
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


        case OP_MOV: INTEGER_BINARY_OP(ASSIGNMENT, Opcode, .Word.First); break;
        case OP_MOVI:
        {
            switch (PVM_GET_IMMTYPE(Opcode))
            {
            case IMMTYPE_I16:
            {
                PVM->R[PVM_GET_RD(Opcode)].SWord.First = (I32)(I16)*IP++;
            } break;
            case IMMTYPE_I32:
            {
                U32 Imm = *IP++;
                Imm |= (U32)*IP++ << 16;
                PVM->R[PVM_GET_RD(Opcode)].SWord.First = (I64)(I32)Imm;
            } break;
            case IMMTYPE_I48:
            {
                U64 Imm = *IP++;
                Imm |= (U32)*IP++ << 16;
                Imm |= (U64)*IP++ << 32;
                PVM->R[PVM_GET_RD(Opcode)].SDWord = BitSex64(Imm, 47);
            } break;
            case IMMTYPE_U16:
            {
                U32 Imm = *IP++;
                Imm |= (U32)*IP++ << 16;
                PVM->R[PVM_GET_RD(Opcode)].Word.First = Imm;
            } break;
            case IMMTYPE_U32:
            {
                U32 Imm = *IP++;
                Imm |= (U32)*IP++ << 16;
                PVM->R[PVM_GET_RD(Opcode)].Word.First = Imm;
            } break;
            case IMMTYPE_U48:
            {
                U32 Imm = *IP++;
                Imm |= (U32)*IP++ << 16;
                Imm |= (U64)*IP++ << 32;
                PVM->R[PVM_GET_RD(Opcode)].DWord = Imm;
            } break;
            case IMMTYPE_U64:
            {
                U32 Imm = *IP++;
                Imm |= (U32)*IP++ << 16;
                Imm |= (U64)*IP++ << 32;
                Imm |= (U64)*IP++ << 48;
                PVM->R[PVM_GET_RD(Opcode)].DWord = Imm;
            } break;
            }
        }
        case OP_FMOV: FLOAT_BINARY_OP(ASSIGNMENT, Opcode, .Single); break;


        case OP_LDRS: LOAD_INTEGER_FROM_STACK(Opcode, IP, .Word.First, .Word); break;
        case OP_LDRG: LOAD_INTEGER_FROM_GLOBAL(Opcode, IP, .Word.First); break;
        case OP_STRS: STORE_INTEGER_TO_STACK(Opcode, IP, .Word.First, .Word); break;
        case OP_STRG: STORE_INTEGER_TO_GLOBAL(Opcode, IP, .Word.First); break;

        case OP_LDFS: LOAD_FLOAT_FROM_STACK(Opcode, IP, .Single, .Word); break;
        case OP_LDFG: LOAD_FLOAT_FROM_GLOBAL(Opcode, IP, .Single); break;
        case OP_STFS: STORE_FLOAT_TO_STACK(Opcode, IP, .Single, .Word); break;
        case OP_STFG: STORE_FLOAT_TO_GLOBAL(Opcode, IP, .Single); break;

        case OP_DWORD:
        {
        } break;

        }
    }
DivisionBy0:
    return PVM_DIVISION_BY_0;
CallStackOverflow:
    return PVM_CALLSTACK_OVERFLOW;
Exit:
    return PVM_NO_ERROR;

#undef LOAD_FLOAT_FROM_STACK
#undef STORE_INTEGER_TO_STACK
#undef LOAD_FLOAT_FROM_GLOBAL
#undef STORE_INTEGER_TO_GLOBAL
#undef LOAD_INTEGER_FROM_GLOBAL
#undef STORE_INTEGER_TO_STACK
#undef LOAD_INTEGER_FROM_STACK
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

    fprintf(f, "\n===================== STACK ======================");
    fprintf(f, "\nSP: %8p\n", (void*)PVM->Stack.Ptr.Raw);
    for (PVMPTR Sp = PVM->Stack.Start; Sp.DWord <= PVM->Stack.Ptr.DWord; Sp.DWord++)
    {
        fprintf(f, "%8p: [0x%08llx]\n", Sp.Raw, *Sp.DWord);
    }
    fputc('\n', f);
}





