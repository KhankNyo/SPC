
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
#define DI_BINARY_OP(Operation, Opcode, RegType) BINARY_OP(Operation, Opcode, DI, R, RegType)
#define FDAT_BINARY_OP(Operation, Opcode) BINARY_OP(Operation, Opcode, FDAT, F, NOTHING)
#define DI_DIVIDE_OP(Opcode, RegType)\
    do {\
        DI_BINARY_OP(/, Opcode, RegType);\
        R(Opcode, DI_SPECIAL, RR)RegType = R(Opcode, DI, RA)RegType % R(Opcode, DI, RB)RegType;\
    } while(0)

#define TEST_AND_SET(Operation, Opcode, InstructionType, RegisterSet, RegType)\
    R(Opcode, DI, RD).Ptr = \
        RegisterSet(Opcode, InstructionType, RegisterSet ## A)RegType \
        Operation \
        RegisterSet(Opcode, InstructionType, RegisterSet ## B)RegType
#define DI_TEST_AND_SET(Operation, Opcode, RegType) TEST_AND_SET(Operation, Opcode, DI, R, RegType)
#define FDAT_TEST_AND_SET(Operation, Opcode) TEST_AND_SET(Operation, Opcode, FDAT, F, NOTHING)

#define BRANCH_IF(Operation, IP, Opcode, RegType)\
do {\
    if (R(Opcode, BRIF, RA)RegType Operation R(Opcode, BRIF, RB)RegType) {\
        (IP) += (PVMSPtr)(I32)PVM_BRIF_GET_IMM(Opcode);\
        /* TODO: verify jump target */\
    }\
} while(0)



    PVMWord *IP = Chunk->Code;
    PVMPtr *FP = PVM->Stack.Start;
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
            case PVM_SYS_COUNT:
            {
                PASCAL_UNREACHABLE("PVM: SYS_COUNT is not an instruction\n");
            } break;
            }
        } break;

        case PVM_DI_ARITH:
        {
            switch ((PVMArith)PVM_DI_GET_OP(Opcode))
            {
            case PVM_ARITH_ADD: DI_BINARY_OP(+, Opcode, .Word.First); break;
            case PVM_ARITH_SUB: DI_BINARY_OP(-, Opcode, .Word.First); break;
            }
        } break;

        case PVM_DI_SPECIAL:
        {
            switch ((PVMSpecial)PVM_DI_GET_OP(Opcode))
            {            
            case PVM_SPECIAL_MUL:
            {
                if (PVM_DI_SPECIAL_SIGNED(Opcode))
                {
                    R(Opcode, DI, RD).SPtr = 
                        R(Opcode, DI, RA).SWord.First * R(Opcode, DI, RB).SWord.First;
                }
                else
                {
                    R(Opcode, DI, RD).Ptr = 
                        R(Opcode, DI, RA).Word.First * R(Opcode, DI, RB).Word.First;
                }
            } break;

            case PVM_SPECIAL_DIVP:
            {
                if (0 == R(Opcode, DI, RB).Ptr)
                {
                    /* TODO division by 0 exception */
                }

                if (PVM_DI_SPECIAL_SIGNED(Opcode))
                    DI_DIVIDE_OP(Opcode, .SPtr);
                else 
                    DI_DIVIDE_OP(Opcode, .Ptr);
            } break;

            case PVM_SPECIAL_DIV:
            {
                if (0 == R(Opcode, DI, RB).Word.First)
                {
                    /* TODO division by 0 exception */
                }

                if (PVM_DI_SPECIAL_SIGNED(Opcode))
                    DI_DIVIDE_OP(Opcode, .SWord.First);
                else 
                    DI_DIVIDE_OP(Opcode, .Word.First);
            } break;
            }
        } break;

        case PVM_DI_CMP:
        {
            switch ((PVMCmp)PVM_DI_GET_OP(Opcode))
            {
            case PVM_CMP_SEQB: DI_TEST_AND_SET(==, Opcode, .Byte[0]); break;
            case PVM_CMP_SNEB: DI_TEST_AND_SET(!=, Opcode, .Byte[0]); break;
            case PVM_CMP_SLTB: DI_TEST_AND_SET(<, Opcode, .Byte[0]); break;
            case PVM_CMP_SGTB: DI_TEST_AND_SET(>, Opcode, .Byte[0]); break;

            case PVM_CMP_SEQH: DI_TEST_AND_SET(==, Opcode, .Half.First); break;
            case PVM_CMP_SNEH: DI_TEST_AND_SET(!=, Opcode, .Half.First); break;
            case PVM_CMP_SLTH: DI_TEST_AND_SET(<, Opcode, .Half.First); break;
            case PVM_CMP_SGTH: DI_TEST_AND_SET(>, Opcode, .Half.First); break;

            case PVM_CMP_SEQW: DI_TEST_AND_SET(==, Opcode, .Word.First); break;
            case PVM_CMP_SNEW: DI_TEST_AND_SET(!=, Opcode, .Word.First); break;
            case PVM_CMP_SLTW: DI_TEST_AND_SET(<, Opcode, .Word.First); break;
            case PVM_CMP_SGTW: DI_TEST_AND_SET(>, Opcode, .Word.First); break;

            case PVM_CMP_SEQP: DI_TEST_AND_SET(==, Opcode, .Ptr); break;
            case PVM_CMP_SNEP: DI_TEST_AND_SET(!=, Opcode, .Ptr); break;
            case PVM_CMP_SLTP: DI_TEST_AND_SET(<, Opcode, .Ptr); break;
            case PVM_CMP_SGTP: DI_TEST_AND_SET(>, Opcode, .Ptr); break;


            case PVM_CMP_SSLTB: DI_TEST_AND_SET(<, Opcode, .SByte[0]); break;
            case PVM_CMP_SSGTB: DI_TEST_AND_SET(>, Opcode, .SByte[0]); break;

            case PVM_CMP_SSLTH: DI_TEST_AND_SET(<, Opcode, .SHalf.First); break;
            case PVM_CMP_SSGTH: DI_TEST_AND_SET(>, Opcode, .SHalf.First); break;

            case PVM_CMP_SSLTW: DI_TEST_AND_SET(<, Opcode, .SWord.First); break;
            case PVM_CMP_SSGTW: DI_TEST_AND_SET(>, Opcode, .SWord.First); break;

            case PVM_CMP_SSLTP: DI_TEST_AND_SET(<, Opcode, .SPtr); break;
            case PVM_CMP_SSGTP: DI_TEST_AND_SET(>, Opcode, .SPtr); break;
            }
        } break;

        case PVM_DI_TRANSFER:
        {
            switch ((PVMTransfer)PVM_DI_GET_OP(Opcode))
            {
            case PVM_DI_MOV: R(Opcode, DI, RD).Word.First = R(Opcode, DI, RA).Word.First; break;
            }
        } break;



        case PVM_IRD_ARITH:
        {
            PVMIRDArith Op = PVM_IRD_GET_OP(Opcode);
            switch (Op)
            {
            case PVM_IRD_ADD: R(Opcode, IRD, RD).Word.First += IRD_SIGNED_IMM(Opcode); break;
            case PVM_IRD_SUB: R(Opcode, IRD, RD).Word.First -= IRD_SIGNED_IMM(Opcode); break;
            case PVM_IRD_LDI: R(Opcode, IRD, RD).Word.First = IRD_SIGNED_IMM(Opcode); break;
            case PVM_IRD_LUI: R(Opcode, IRD, RD).Word.First = PVM_IRD_GET_IMM(Opcode) << 16; break;
            case PVM_IRD_ORI: R(Opcode, IRD, RD).Word.First |= PVM_IRD_GET_IMM(Opcode); break;
            }
        } break;



        case PVM_BRIF_EQ: BRANCH_IF(==, IP, Opcode, .Word.First); break;
        case PVM_BRIF_NE: BRANCH_IF(!=, IP, Opcode, .Word.First); break;
        case PVM_BRIF_GT: BRANCH_IF(>, IP, Opcode, .Word.First); break;
        case PVM_BRIF_LT: BRANCH_IF(<, IP, Opcode, .Word.First); break;
        case PVM_BRIF_SGT: BRANCH_IF(>, IP, Opcode, .SWord.First); break;
        case PVM_BRIF_SLT: BRANCH_IF(<, IP, Opcode, .SWord.First); break;
        case PVM_BALT_AL: 
        {
            IP += (PVMSPtr)(I32)PVM_BAL_GET_IMM(Opcode);
            /* TODO: verify jump target */
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
            switch ((PVMArith)PVM_FDAT_GET_OP(Opcode))
            {
            case PVM_ARITH_ADD: FDAT_BINARY_OP(+, Opcode); break;
            case PVM_ARITH_SUB: FDAT_BINARY_OP(-, Opcode); break;
            }
        } break;
        case PVM_FDAT_CMP:
        {
            switch ((PVMCmp)PVM_FDAT_GET_OP(Opcode))
            {
            case PVM_CMP_SEQB: FDAT_TEST_AND_SET(==, Opcode); break;
            case PVM_CMP_SNEB: FDAT_TEST_AND_SET(!=, Opcode); break;
            case PVM_CMP_SLTB: FDAT_TEST_AND_SET(<, Opcode); break;
            case PVM_CMP_SGTB: FDAT_TEST_AND_SET(>, Opcode); break;

            case PVM_CMP_SEQH: FDAT_TEST_AND_SET(==, Opcode); break;
            case PVM_CMP_SNEH: FDAT_TEST_AND_SET(!=, Opcode); break;
            case PVM_CMP_SLTH: FDAT_TEST_AND_SET(<, Opcode); break;
            case PVM_CMP_SGTH: FDAT_TEST_AND_SET(>, Opcode); break;

            case PVM_CMP_SEQW: FDAT_TEST_AND_SET(==, Opcode); break;
            case PVM_CMP_SNEW: FDAT_TEST_AND_SET(!=, Opcode); break;
            case PVM_CMP_SLTW: FDAT_TEST_AND_SET(<, Opcode); break;
            case PVM_CMP_SGTW: FDAT_TEST_AND_SET(>, Opcode); break;

            case PVM_CMP_SEQP: FDAT_TEST_AND_SET(==, Opcode); break;
            case PVM_CMP_SNEP: FDAT_TEST_AND_SET(!=, Opcode); break;
            case PVM_CMP_SLTP: FDAT_TEST_AND_SET(<, Opcode); break;
            case PVM_CMP_SGTP: FDAT_TEST_AND_SET(>, Opcode); break;


            case PVM_CMP_SSLTB: FDAT_TEST_AND_SET(<, Opcode); break;
            case PVM_CMP_SSGTB: FDAT_TEST_AND_SET(>, Opcode); break;

            case PVM_CMP_SSLTH: FDAT_TEST_AND_SET(<, Opcode); break;
            case PVM_CMP_SSGTH: FDAT_TEST_AND_SET(>, Opcode); break;

            case PVM_CMP_SSLTW: FDAT_TEST_AND_SET(<, Opcode); break;
            case PVM_CMP_SSGTW: FDAT_TEST_AND_SET(>, Opcode); break;

            case PVM_CMP_SSLTP: FDAT_TEST_AND_SET(<, Opcode); break;
            case PVM_CMP_SSGTP: FDAT_TEST_AND_SET(>, Opcode); break;
            }
        } break;
        case PVM_FDAT_TRANSFER:
        {
            switch ((PVMTransfer)PVM_FDAT_GET_OP(Opcode))
            {
            case PVM_DI_MOV: F(Opcode, FDAT, FD) = F(Opcode, FDAT, FA); break;
            }
        }
        case PVM_FMEM_LDF:
        {
            F(Opcode, FDAT, FD) = Chunk->DataSection[PVM_FMEM_GET_IMM(Opcode)];
        } break;
        case PVM_FDAT_SPECIAL:
        {
        } break;

        }
    }
CallstackUnderflow:
    return PVM_CALLSTACK_UNDERFLOW;
CallstackOverflow:
    return PVM_CALLSTACK_OVERFLOW;
Out:
    return PVM_NO_ERROR;


#undef IRD_SIGNED_IMM
#undef BINARY_OP
#undef DI_BINARY_OP
#undef DI_DIVIDE_OP
#undef DI_TEST_AND_SET
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
        fprintf(f, "[F%02d: %g]", i, PVM->F[i]);
    }

    fprintf(f, "\n===================== STACK ======================");
    fprintf(f, "\nSP: %p", (void*)PVM->Stack.Ptr);
    for (PVMPtr *Sp = PVM->Stack.Start; Sp < PVM->Stack.Ptr; Sp++)
    {
        fprintf(f, "%8p: [0x%08llx]\n", (void*)Sp, *Sp);
    }
}


