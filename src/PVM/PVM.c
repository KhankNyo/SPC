
#include <time.h>
#include "Memory.h"
#include "PVM/PVM.h"






PascalVM PVMInit(U32 StackSize, UInt RetStackSize)
{
    PascalVM PVM = { 0 };
    PVM.Stack.Start = MemAllocateArray(PVMPtr, StackSize);
    PVM.Stack.Ptr = PVM.Stack.Start;
    PVM.Stack.End = PVM.Stack.Start + StackSize;

    PVM.RetStack.Start = MemAllocateArray(PVMSaveFrame, RetStackSize);
    PVM.RetStack.Val = PVM.RetStack.Start;
    PVM.RetStack.SizeLeft = RetStackSize;
    return PVM;
}

void PVMDeinit(PascalVM *PVM)
{
    MemDeallocateArray(PVM->Stack.Start);
    MemDeallocateArray(PVM->RetStack.Start);
    *PVM = (PascalVM){ 0 };
}



/*============================================================================================
 *                                          WARNING:
 * The following code has the ability to cause cancer, eye sore, and even death to the soul.
 *                                  Viewer discretion advised.
 *============================================================================================*/
PVMReturnValue PVMInterpret(PascalVM *PVM, const CodeChunk *Chunk)
{

#define NOTHING 

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
    UInt i = Base;\
    while (RegList) {\
        if (RegList & 1) {\
            *SP++ = PVM->R[i];\
            i++;\
        }\
    }\
} while (0)

#define POP_MULTIPLE(Base, Opcode) do{\
    UInt RegList = PVM_IRD_GET_IMM(Opcode);\
    UInt i = Base;\
    while (RegList) {\
        if (RegList & 1) {\
            *SP++ = PVM->R[i];\
            i++;\
        }\
    }\
} while (0)


    double start = clock();

    PVMWord *IP = Chunk->Code;
    PVMPtr *FP = PVM->Stack.Start;
    PVMPtr *SP = PVM->Stack.Start;
    const PVMWord *CodeEnd = IP + Chunk->Count;
    while (1)
    {
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
            if (Addr > PVM->Stack.End)
            {
                /* TODO: invalid access */
            }

            switch (PVM_IRD_GET_MEM(Opcode))
            {
            case PVM_IRD_LDRS: R(Opcode, IRD, RD).Ptr = *Addr; break;
            case PVM_IRD_LDFS: F(Opcode, IRD, FD).Ptr = *Addr; break;
            case PVM_IRD_STRS: *Addr = R(Opcode, IRD, RD).Ptr; break;
            case PVM_IRD_STFS: *Addr = F(Opcode, IRD, FD).Ptr; break;
            case PVM_TRANSFER_PSHL:
            {
                UInt RegList = PVM_IRD_GET_IMM(Opcode);
                UInt i = 0;
                while (RegList)
                {
                    if (RegList & 1)
                    {
                        *SP++ = PVM->R[i].Ptr;
                        i++;
                    }
                    RegList >>= 1;
                }
            } break;
            case PVM_TRANSFER_POPL:
            case PVM_TRANSFER_PSHU:
            {
                UInt RegList = PVM_IRD_GET_IMM(Opcode);
                UInt i = 16;
                while (RegList)
                {
                    if (RegList & 1)
                    {
                        *SP++ = PVM->R[i].Ptr;
                        i++;
                    }
                    RegList <<= 1;
                }
            } break;
            case PVM_TRANSFER_POPU:
            break;
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

                IP = PVM->RetStack.Val->IP;
                FP = PVM->RetStack.Val->Frame;
                PVM->RetStack.Val--;
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
    return PVM_ILLEGAL_INSTRUCTION;
DivisionBy0:
    return PVM_DIVISION_BY_0;
CallstackUnderflow:
    return PVM_CALLSTACK_UNDERFLOW;
CallstackOverflow:
    return PVM_CALLSTACK_OVERFLOW;
Out:
    printf("Finished in %g ms\n", (clock() - start) * 1000 / CLOCKS_PER_SEC);
    return PVM_NO_ERROR;


#undef IRD_SIGNED_IMM
#undef BINARY_OP
#undef IDAT_BINARY_OP
#undef IDAT_DIVIDE_OP
#undef IDAT_TEST_AND_SET
#undef BRANCH_IF
#undef FDAT_TEST_AND_SET
#undef FDAT_BINARY_OP
#undef TEST_AND_SET
#undef R
#undef F
#undef NOTHING
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
        fprintf(f, "[R%02d: 0x%08llx]", i, PVM->R[i].Ptr);
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
    fprintf(f, "\nSP: %p", (void*)PVM->Stack.Ptr);
    for (PVMPtr *Sp = PVM->Stack.Start; Sp < PVM->Stack.Ptr; Sp++)
    {
        fprintf(f, "%8p: [0x%08llx]\n", (void*)Sp, *Sp);
    }
}


