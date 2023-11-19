
#include <stdarg.h>
#include <time.h>

#include "Memory.h"
#include "PVM/PVM.h"
#include "PVM/Debugger.h"
#include "PVM/Disassembler.h"




static void RuntimeError(PascalVM *PVM, const char *Fmt, ...);



PascalVM PVMInit(U32 StackSize, UInt RetStackSize)
{
    PascalVM PVM = { 0 };
    PVM.Stack.Start = MemAllocateArray(PVMPtr, StackSize);
    PVM.Stack.Ptr = PVM.Stack.Start;
    PVM.Stack.End = PVM.Stack.Start + StackSize;

    PVM.RetStack.Start = MemAllocateArray(PVMSaveFrame, RetStackSize);
    PVM.RetStack.Val = PVM.RetStack.Start;
    PVM.RetStack.SizeLeft = RetStackSize;
    PVM.SingleStepMode = false;
    return PVM;
}

void PVMDeinit(PascalVM *PVM)
{
    MemDeallocateArray(PVM->Stack.Start);
    MemDeallocateArray(PVM->RetStack.Start);
    *PVM = (PascalVM){ 0 };
}


bool PVMRun(PascalVM *PVM, const CodeChunk *Chunk)
{
    PVMDisasm(stdout, Chunk, "Compiled Code");
    fprintf(stdout, "Press Enter to execute...\n");
    getc(stdin);

    bool NoError = false;
    double Start = clock();
    PVMReturnValue Ret = PVMInterpret(PVM, Chunk);
    double End = clock();
    PVMDumpState(stdout, PVM, 6);


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
    case PVM_CALLSTACK_UNDERFLOW:
    {
        RuntimeError(PVM, "Callstack underflow");
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



/*============================================================================================
 *                                          WARNING:
 * The following code has the ability to cause cancer, eye sore, and even death to the soul.
 *                                  Viewer discretion advised.
 *============================================================================================*/
PVMReturnValue PVMInterpret(PascalVM *PVM, const CodeChunk *Chunk)
{
#define R(Opcode, InstructionType, RegType) (PVM->R[GLUE(PVM_ ##InstructionType, _GET_ ##RegType) (Opcode)])
#define F(Opcode, InstructionType, RegType) (PVM->F[GLUE(PVM_ ##InstructionType, _GET_ ##RegType) (Opcode)])

#define IRD_SIGNED_IMM(Opcode) (I32)(I16)PVM_IRD_GET_IMM(Opcode)


#define BINARY_OP(Operation, Opcode, InstructionType, RegisterSet, RegType)\
    RegisterSet(Opcode, InstructionType, RegisterSet ## D)RegType = \
        RegisterSet(Opcode, InstructionType, RegisterSet ## A)RegType \
        Operation \
        RegisterSet(Opcode, InstructionType, RegisterSet ## B)RegType
#define IDAT_BINARY_OP(Operation, Opcode, RegType) BINARY_OP(Operation, Opcode, IDAT, R, RegType)
#define IDAT_DIVIDE_OP(Opcode, RegType)\
    do {\
        IDAT_BINARY_OP(/, Opcode, RegType);\
        R(Opcode, IDAT_SPECIAL, RR)RegType = R(Opcode, IDAT, RA)RegType % R(Opcode, IDAT, RB)RegType;\
    } while(0)
#define FDAT_BINARY_OP(Operation, Opcode, RegType) BINARY_OP(Operation, Opcode, FDAT, F, RegType)


#define TEST_AND_SET(Operation, Opcode, InstructionType, RegisterSet, RegType)\
    R(Opcode, IDAT, RD).Ptr = \
        RegisterSet(Opcode, InstructionType, RegisterSet ## A)RegType \
        Operation \
        RegisterSet(Opcode, InstructionType, RegisterSet ## B)RegType
#define IDAT_TEST_AND_SET(Operation, Opcode, RegType) TEST_AND_SET(Operation, Opcode, IDAT, R, RegType)
#define FDAT_TEST_AND_SET(Operation, Opcode, RegType) TEST_AND_SET(Operation, Opcode, FDAT, F, RegType)


#define BRANCH_IF(Operation, IP, Opcode, RegType)\
do {\
    if (R(Opcode, BRIF, RA)RegType Operation 0) {\
        (IP) += (PVMSPtr)(I32)PVM_BRIF_GET_IMM(Opcode);\
        if (IP < Chunk->Code || CodeEnd <= IP) {\
            /* TODO: verify jump target */\
        }\
    }\
} while(0)


#define PUSH_MULTIPLE(Base, Opcode) do{\
    UInt RegList = PVM_IRD_GET_IMM(Opcode);\
    UInt Base_ = Base;\
    UInt i = Base_;\
    while (i < Base_ + 16) {\
        if (RegList & 1) {\
            *(++SP) = PVM->R[i].Ptr;\
        }\
        i++;\
        RegList >>= 1;\
    }\
} while (0)

#define POP_MULTIPLE(Base, Opcode) do{\
    UInt RegList = PVM_IRD_GET_IMM(Opcode);\
    UInt Base_ = Base;\
    UInt i = Base_;\
    while (i < Base_ + 16) {\
        if (RegList & 0x8000) {\
            PVM->R[(Base + 15) - i].Ptr = *(SP--);\
        }\
        i++;\
        RegList <<= 1;\
    }\
} while (0)

#define VERIFY_STACK_ADDR(Addr) do {\
    if ((void*)(Addr) > (void*)PVM->Stack.End) {\
        /* TODO: verify stack addr */\
    }\
} while(0)

    PVMWord *IP = Chunk->Code;
    PVMPtr *SP = PVM->Stack.Start;
    PVMPtr *FP = PVM->Stack.Start;
    const PVMWord *CodeEnd = IP + Chunk->Count;
    PVMReturnValue RetVal = PVM_NO_ERROR;
    while (1)
    {

#ifdef PVM_DEBUGGER
        if (PVM->SingleStepMode)
        {
            PVM->Stack.Ptr = SP;
            PVMDebugPause(PVM, Chunk, IP, SP, FP);
        }
#endif

        PVMWord Opcode = *IP++;
        switch (PVM_GET_INS(Opcode))
        {
        case PVM_RE_SYS:
        {
            PVMSysOp SysOp = PVM_GET_SYS_OP(Opcode);
            switch (SysOp)
            {
            case PVM_SYS_EXIT: goto Out;
            }
        } break;

        case PVM_IDAT_ARITH:
        {
            switch (PVM_IDAT_GET_ARITH(Opcode))
            {
            case PVM_ARITH_ADD: IDAT_BINARY_OP(+, Opcode, .Word.First); break;
            case PVM_ARITH_SUB: IDAT_BINARY_OP(-, Opcode, .Word.First); break;
            case PVM_ARITH_NEG: R(Opcode, IDAT, RD).Word.First = R(Opcode, IDAT, RA).Word.First; break;
            }
        } break;

        case PVM_IDAT_SPECIAL:
        {
            switch (PVM_IDAT_GET_SPECIAL(Opcode))
            {            
            case PVM_SPECIAL_MUL:
            {
                if (PVM_IDAT_SPECIAL_SIGNED(Opcode))
                {
                    R(Opcode, IDAT, RD).Ptr = 
                        R(Opcode, IDAT, RA).SWord.First * R(Opcode, IDAT, RB).SWord.First;
                }
                else
                {
                    R(Opcode, IDAT, RD).Ptr = 
                        R(Opcode, IDAT, RA).Word.First * R(Opcode, IDAT, RB).Word.First;
                }
            } break;

            case PVM_SPECIAL_DIVP:
            {
                if (0 == R(Opcode, IDAT, RB).Ptr)
                    goto DivisionBy0;

                if (PVM_IDAT_SPECIAL_SIGNED(Opcode))
                    IDAT_DIVIDE_OP(Opcode, .SPtr);
                else 
                    IDAT_DIVIDE_OP(Opcode, .Ptr);
            } break;

            case PVM_SPECIAL_DIV:
            {
                if (0 == R(Opcode, IDAT, RB).Word.First)
                    goto DivisionBy0;

                if (PVM_IDAT_SPECIAL_SIGNED(Opcode))
                    IDAT_DIVIDE_OP(Opcode, .SWord.First);
                else 
                    IDAT_DIVIDE_OP(Opcode, .Word.First);
            } break;

            case PVM_SPECIAL_D2P:
            {
                R(Opcode, IDAT, RD).Ptr = (PVMPtr)F(Opcode, FDAT, FD).Double;
            } break;
            }
        } break;

        case PVM_IDAT_CMP:
        {
            switch (PVM_IDAT_GET_CMP(Opcode))
            {
            case PVM_CMP_SEQB: IDAT_TEST_AND_SET(==, Opcode, .Byte[0]); break;
            case PVM_CMP_SNEB: IDAT_TEST_AND_SET(!=, Opcode, .Byte[0]); break;
            case PVM_CMP_SLTB: IDAT_TEST_AND_SET(<, Opcode, .Byte[0]); break;
            case PVM_CMP_SGTB: IDAT_TEST_AND_SET(>, Opcode, .Byte[0]); break;

            case PVM_CMP_SEQH: IDAT_TEST_AND_SET(==, Opcode, .Half.First); break;
            case PVM_CMP_SNEH: IDAT_TEST_AND_SET(!=, Opcode, .Half.First); break;
            case PVM_CMP_SLTH: IDAT_TEST_AND_SET(<, Opcode, .Half.First); break;
            case PVM_CMP_SGTH: IDAT_TEST_AND_SET(>, Opcode, .Half.First); break;

            case PVM_CMP_SEQW: IDAT_TEST_AND_SET(==, Opcode, .Word.First); break;
            case PVM_CMP_SNEW: IDAT_TEST_AND_SET(!=, Opcode, .Word.First); break;
            case PVM_CMP_SLTW: IDAT_TEST_AND_SET(<, Opcode, .Word.First); break;
            case PVM_CMP_SGTW: IDAT_TEST_AND_SET(>, Opcode, .Word.First); break;

            case PVM_CMP_SEQP: IDAT_TEST_AND_SET(==, Opcode, .Ptr); break;
            case PVM_CMP_SNEP: IDAT_TEST_AND_SET(!=, Opcode, .Ptr); break;
            case PVM_CMP_SLTP: IDAT_TEST_AND_SET(<, Opcode, .Ptr); break;
            case PVM_CMP_SGTP: IDAT_TEST_AND_SET(>, Opcode, .Ptr); break;


            case PVM_CMP_SSLTB: IDAT_TEST_AND_SET(<, Opcode, .SByte[0]); break;
            case PVM_CMP_SSGTB: IDAT_TEST_AND_SET(>, Opcode, .SByte[0]); break;

            case PVM_CMP_SSLTH: IDAT_TEST_AND_SET(<, Opcode, .SHalf.First); break;
            case PVM_CMP_SSGTH: IDAT_TEST_AND_SET(>, Opcode, .SHalf.First); break;

            case PVM_CMP_SSLTW: IDAT_TEST_AND_SET(<, Opcode, .SWord.First); break;
            case PVM_CMP_SSGTW: IDAT_TEST_AND_SET(>, Opcode, .SWord.First); break;

            case PVM_CMP_SSLTP: IDAT_TEST_AND_SET(<, Opcode, .SPtr); break;
            case PVM_CMP_SSGTP: IDAT_TEST_AND_SET(>, Opcode, .SPtr); break;
            }
        } break;

        case PVM_IDAT_TRANSFER:
        {
            switch (PVM_IDAT_GET_TRANSFER(Opcode))
            {
            case PVM_TRANSFER_MOV: R(Opcode, IDAT, RD).Word.First = R(Opcode, IDAT, RA).Word.First; break;
            }
        } break;



        case PVM_IRD_ARITH:
        {
            switch (PVM_IRD_GET_ARITH(Opcode))
            {
            case PVM_IRD_ADD: R(Opcode, IRD, RD).Word.First += IRD_SIGNED_IMM(Opcode); break;
            case PVM_IRD_SUB: R(Opcode, IRD, RD).Word.First -= IRD_SIGNED_IMM(Opcode); break;

            case PVM_IRD_LDI: R(Opcode, IRD, RD).Word.First = IRD_SIGNED_IMM(Opcode); break;
            case PVM_IRD_LDZI: R(Opcode, IRD, RD).Word.First = PVM_IRD_GET_IMM(Opcode); break;
            case PVM_IRD_ORUI: R(Opcode, IRD, RD).Word.First |= PVM_IRD_GET_IMM(Opcode) << 16; break;
            case PVM_IRD_LDZHLI: R(Opcode, IRD, RD).Word.Second = PVM_IRD_GET_IMM(Opcode); break;
            case PVM_IRD_LDHLI: R(Opcode, IRD, RD).Word.Second = IRD_SIGNED_IMM(Opcode); break;
            case PVM_IRD_ORHUI: R(Opcode, IRD, RD).Word.Second |= PVM_IRD_GET_IMM(Opcode) << 16; break;
            }
        } break;
        case PVM_IRD_MEM:
        {
            PVMPtr *Addr = FP + (PVMPtr)PVM_IRD_GET_IMM(Opcode);
            switch (PVM_IRD_GET_MEM(Opcode))
            {
            case PVM_IRD_LDRS: 
            {
                VERIFY_STACK_ADDR(Addr);
                R(Opcode, IRD, RD).Ptr = *Addr; 
            } break;
            case PVM_IRD_LDFS: 
            {
                VERIFY_STACK_ADDR(Addr);
                F(Opcode, IRD, FD).Ptr = *Addr;
            } break;
            case PVM_IRD_STRS: 
            {
                VERIFY_STACK_ADDR(Addr);
                *Addr = R(Opcode, IRD, RD).Ptr; 
            } break;
            case PVM_IRD_STFS: 
            {
                VERIFY_STACK_ADDR(Addr);
                *Addr = F(Opcode, IRD, FD).Ptr;
            } break;
            case PVM_IRD_PSHL: PUSH_MULTIPLE(0, Opcode); break;
            case PVM_IRD_POPL: POP_MULTIPLE(0, Opcode); break;
            case PVM_IRD_PSHU: PUSH_MULTIPLE(16, Opcode); break;
            case PVM_IRD_POPU: POP_MULTIPLE(16, Opcode); break;
            case PVM_IRD_ADDSPI:
            {
                SP += (PVMSPtr)(I32)BIT_SEX32(BIT_AT32(Opcode, 21, 0), 20);
            } break;
            }
        } break;




        case PVM_BRIF_EZ: BRANCH_IF(==, IP, Opcode, .Word.First); break;
        case PVM_BRIF_NZ: BRANCH_IF(!=, IP, Opcode, .Word.First); break;
        case PVM_BALT_AL: 
        {
            IP += (PVMSPtr)(I32)PVM_BAL_GET_IMM(Opcode);
            if (IP < Chunk->Code || CodeEnd <= IP)
            {
                /* TODO invalid branch target */
            }
        } break;
        case PVM_BALT_SR:
        {
            PVMSPtr SignedImm = (PVMSPtr)PVM_BSR_GET_IMM(Opcode);
            if (-1 == SignedImm) /* RET */
            {
                if (PVM->RetStack.Val == PVM->RetStack.Start)
                    goto CallstackUnderflow;

                PVM->RetStack.Val--;
                IP = PVM->RetStack.Val->IP;
                FP = PVM->RetStack.Val->Frame;
                PVM->RetStack.SizeLeft++;
            }
            else /* BSR */
            {
                if (0 == PVM->RetStack.SizeLeft)
                    goto CallstackOverflow;

                PVM->RetStack.Val->IP = IP;
                PVM->RetStack.Val->Frame = FP;
                PVM->RetStack.Val++;
                PVM->RetStack.SizeLeft--;
                IP += SignedImm;
            }
        } break;




        case PVM_FDAT_ARITH:
        {
            switch (PVM_FDAT_GET_ARITH(Opcode))
            {
            case PVM_ARITH_ADD: FDAT_BINARY_OP(+, Opcode, .Double); break;
            case PVM_ARITH_SUB: FDAT_BINARY_OP(-, Opcode, .Double); break;
            case PVM_ARITH_NEG: F(Opcode, FDAT, FD).Double = F(Opcode, FDAT, FA).Double; break;
            }
        } break;
        case PVM_FDAT_SPECIAL:
        {
            switch (PVM_FDAT_GET_SPECIAL(Opcode))
            {
            case PVM_SPECIAL_DIVP:
            case PVM_SPECIAL_DIV: FDAT_BINARY_OP(/, Opcode, .Double); break;
            case PVM_SPECIAL_MUL: FDAT_BINARY_OP(*, Opcode, .Double); break;
            case PVM_SPECIAL_P2D: F(Opcode, FDAT, FD).Double = R(Opcode, IDAT, RA).Ptr; break;
            }
        } break;
        case PVM_FDAT_CMP:
        {
            switch (PVM_FDAT_GET_CMP(Opcode))
            {
            default: goto IllegalInstruction;
            case PVM_CMP_SEQP: FDAT_TEST_AND_SET(==, Opcode, .Double); break;
            case PVM_CMP_SNEP: FDAT_TEST_AND_SET(!=, Opcode, .Double); break;
            case PVM_CMP_SLTP: FDAT_TEST_AND_SET(<, Opcode, .Double); break;
            case PVM_CMP_SGTP: FDAT_TEST_AND_SET(>, Opcode, .Double); break;
            }
        } break;
        case PVM_FDAT_TRANSFER:
        {
            switch (PVM_FDAT_GET_TRANSFER(Opcode))
            {
            case PVM_TRANSFER_MOV: F(Opcode, FDAT, FD) = F(Opcode, FDAT, FA); break;
            }
        } break;
        case PVM_FMEM_LDF:
        {
            /* TODO: check boundaries before loading */
            F(Opcode, FDAT, FD).Double = Chunk->DataSection.Data[PVM_FMEM_GET_IMM(Opcode)];
        } break;

        }
    }


IllegalInstruction:
    RetVal = PVM_ILLEGAL_INSTRUCTION;
    goto Out;
DivisionBy0:
    RetVal = PVM_DIVISION_BY_0;
    goto Out;
CallstackUnderflow:
    RetVal = PVM_CALLSTACK_UNDERFLOW;
    goto Out;
CallstackOverflow:
    RetVal = PVM_CALLSTACK_OVERFLOW;
Out:
    PVM->Error.Line = 1;
    PVM->Stack.Ptr = SP;
    return RetVal;


#undef IRD_SIGNED_IMM
#undef BINARY_OP
#undef IDAT_BINARY_OP
#undef IDAT_DIVIDE_OP
#undef IDAT_TEST_AND_SET
#undef BRANCH_IF
#undef FDAT_TEST_AND_SET
#undef FDAT_BINARY_OP
#undef TEST_AND_SET
#undef POP_MULTIPLE
#undef PUSH_MULTIPLE
#undef R
#undef F
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
        fprintf(f, "[R%02d: 0x%08lld]", i, PVM->R[i].Ptr);
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
    fprintf(f, "\nSP: %8p\n", (void*)PVM->Stack.Ptr);
    for (PVMPtr *Sp = PVM->Stack.Start; Sp <= PVM->Stack.Ptr; Sp++)
    {
        fprintf(f, "%8p: [0x%08llx]\n", (void*)Sp, *Sp);
    }
    fputc('\n', f);
}






static void RuntimeError(PascalVM *PVM, const char *Fmt, ...)
{
    va_list Args;
    va_start(Args, Fmt);
    fprintf(stdout, "Runtime Error: [line %d]:\n\t", PVM->Error.Line);
    fprintf(stdout, Fmt, Args);
    va_end(Args);
}


