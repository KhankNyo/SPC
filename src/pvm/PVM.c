
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
    *PVM = (PascalVM){ 0 };
}


PVMReturnValue PVMInterpret(PascalVM *PVM, const CodeChunk *Chunk)
{
#define REG(Opcode, InstructionType, Reg) (PVM->R[GLUE(PVM_ ##InstructionType, _GET_ ##Reg) (Opcode)])
#define IRD_SIGNED_IMM(Opcode) (I32)(I16)PVM_IRD_GET_IMM(Opcode)

#define DI_IBINARY_OP(Operation, Opcode, RegType)\
do {\
    REG(Opcode, DI, RD)RegType = REG(Opcode, DI, RA)RegType Operation REG(Opcode, DI, RB)RegType;\
} while(0)

#define BRANCH_IF(Operation, IP, Opcode, RegType)\
do {\
    if (REG(Opcode, BRIF, RA)RegType Operation REG(Opcode, BRIF, RB)RegType) {\
        (IP) += (PVMSPtr)(I32)PVM_BRIF_GET_IMM(Opcode);\
        /* TODO: verify jump target */\
    }\
} while(0)

    PVMWord *IP = Chunk->Data;
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
            PVMDIArith Op = PVM_DI_GET_OP(Opcode);
            switch (Op)
            {
            case PVM_DI_ADD: DI_IBINARY_OP(+, Opcode, .Word.First); break;
            case PVM_DI_SUB: DI_IBINARY_OP(-, Opcode, .Word.First); break;
            }
        } break;

        case PVM_IRD_ARITH:
        {
            PVMIRDArith Op = PVM_IRD_GET_OP(Opcode);
            switch (Op)
            {
            case PVM_IRD_ADD: REG(Opcode, IRD, RD).Word.First += IRD_SIGNED_IMM(Opcode); break;
            case PVM_IRD_SUB: REG(Opcode, IRD, RD).Word.First -= IRD_SIGNED_IMM(Opcode); break;
            case PVM_IRD_LDI: REG(Opcode, IRD, RD).Word.First = IRD_SIGNED_IMM(Opcode); break;
            case PVM_IRD_LUI: REG(Opcode, IRD, RD).Word.First = PVM_IRD_GET_IMM(Opcode) << 16; break;
            case PVM_IRD_ORI: REG(Opcode, IRD, RD).Word.First |= PVM_IRD_GET_IMM(Opcode); break;
            }
        } break;




        case PVM_BRIF_EQ: BRANCH_IF(==, IP, Opcode, .Word.First); break;
        case PVM_BRIF_NE: BRANCH_IF(!=, IP, Opcode, .Word.First); break;
        case PVM_BRIF_GT: BRANCH_IF(>, IP, Opcode, .Word.First); break;
        case PVM_BRIF_LT: BRANCH_IF(<, IP, Opcode, .Word.First); break;
        case PVM_BALT_SGT: BRANCH_IF(==, IP, Opcode, .Word.First); break;
        case PVM_BALT_SLT: BRANCH_IF(!=, IP, Opcode, .Word.First); break;
        case PVM_BALT_AL: 
        {
            IP += (PVMSPtr)(I32)PVM_BAL_GET_IMM(Opcode);
            /* TODO: verify jump target */
        } break;
        case PVM_BALT_SR:
        {
            PVMSPtr SignedImm = PVM_BSR_GET_IMM(Opcode);
            if (-1 == SignedImm) /* return instruction */
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

        case PVM_RE_COUNT:
        case PVM_DI_COUNT:
        case PVM_IRD_COUNT:
        case PVM_INS_COUNT:
        {
            PASCAL_UNREACHABLE("PVM: *_COUNT are not instructions\n");
        } break;

        }
    }
CallstackUnderflow:
    return PVM_CALLSTACK_UNDERFLOW;
CallstackOverflow:
    return PVM_CALLSTACK_OVERFLOW;
Out:
    return PVM_NO_ERROR;

#undef REG
#undef IRD_SIGNED_IMM
#undef DI_IBINARY_OP
#undef BRIF
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

    fprintf(f, "\n===================== F32 REGISTERS ======================");
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


