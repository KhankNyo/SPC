
#include "PVM/PVM.h"


static const char *sIntRegName[] = 
{
    "R0", "R1", "R2", "R3", "R4", "R5", "R6", "R7",
    "R8", "R9", "R10", "R11", "R12", "R13", "R14", "R15",
    "R16", "R17", "R18", "R19", "R20", "R21", "R22", "R23",
    "R24", "R25", "R26", "R27", "R28", "R29", "R30", "R31",
};
static const char *sFloatRegName[] = 
{
    "F0", "F1", "F2", "F3", "F4", "F5", "F6", "F7",
    "F8", "F9", "F10", "F11", "F12", "F13", "F14", "F15",
    "F16", "F17", "F18", "F19", "F20", "F21", "F22", "F23",
    "F24", "F25", "F26", "F27", "F28", "F29", "F30", "F31",
};

typedef enum ImmType 
{
    IMM_UNSIGNED,
    IMM_SIGNED,
    IMM_INDIRECT,
    IMM_SIGNED_INDIRECT,
} ImmType;

static void DisasmInstruction(FILE *f, PVMWord Addr, PVMWord Opcode);

static void DisasmIDatArith(FILE *f, PVMWord Opcode);
static void DisasmIDatSpecial(FILE *f, PVMWord Opcode);
static void DisasmIDatCmp(FILE *f, PVMWord Opcode);
static void DisasmIDatTransfer(FILE *f, PVMWord Opcode);


static void DisasmFDatArith(FILE *f, PVMWord Opcode);
static void DisasmFDatSpecial(FILE *f, PVMWord Opcode);
static void DisasmFDatCmp(FILE *f, PVMWord Opcode);
static void DisasmFDatTransfer(FILE *f, PVMWord Opcode);
static void DisasmFMem(FILE *f, const char *Mnemonic, PVMWord Opcode);

static void DisasmTransferIns(FILE *f, const char *Mnemonic, const char *Rd, const char *Ra);
static void DisasmArithIns(FILE *f, const char *Mnemonic, const char *Rd, const char *Ra, const char *Rb);
static void DisasmIDatExOper(FILE *f, const char *Mnemonic, const char *Rd, const char *Ra, const char *Rb, const char *Sh);


static void DisasmBrIf(FILE *f, const char *Mnemonic, PVMWord Addr, PVMWord Opcode);
static void DisasmBAlt(FILE *f, const char *Mnemonic, PVMWord Addr, PVMWord Opcode);

static void DisasmImmRdArith(FILE *f, PVMWord Opcode);
static void DisasmImmRdMem(FILE *f, PVMWord Opcode);
static void DisasmImmReg(FILE *f, const char *Mnemonic, const char *RegisterSet[32], PVMWord Opcode, ImmType Type);
static void DisasmRegList(FILE *f, const char *Mnemonic, PVMWord Opcode, bool IsUpper);
static void DisasmLongImm(FILE *f, const char *Mnemonic, PVMWord Opcode, ImmType Type);

static void DisasmSysOp(FILE *f, PVMWord Addr, PVMWord Opcode);
static void PrintAddr(FILE *f, PVMWord Addr);
static void PrintHexCode(FILE *f, PVMWord Opcode);




void PVMDisasm(FILE *f, const CodeChunk *Chunk, const char *ChunkName)
{
    fprintf(f, "<========== %s ==========>\n", ChunkName);
    for (PVMWord i = 0; i < Chunk->Count; i++)
    {
        DisasmInstruction(f, i, Chunk->Code[i]);
    }
}



static void DisasmInstruction(FILE *f, PVMWord Addr, PVMWord Opcode)
{
    PrintAddr(f, Addr);
    PrintHexCode(f, Opcode);

    PVMIns Ins = PVM_GET_INS(Opcode);
    switch (Ins)
    {
    case PVM_RE_SYS: DisasmSysOp(f, Addr, Opcode); break;

    case PVM_IDAT_ARITH: DisasmIDatArith(f, Opcode); break;
    case PVM_IDAT_SPECIAL: DisasmIDatSpecial(f, Opcode); break;
    case PVM_IDAT_CMP: DisasmIDatCmp(f, Opcode); break;
    case PVM_IDAT_TRANSFER: DisasmIDatTransfer(f, Opcode); break;

    case PVM_FDAT_ARITH: DisasmFDatArith(f, Opcode); break;
    case PVM_FDAT_SPECIAL: DisasmFDatSpecial(f, Opcode); break;
    case PVM_FDAT_CMP: DisasmFDatCmp(f, Opcode); break;
    case PVM_FDAT_TRANSFER: DisasmFDatTransfer(f, Opcode); break;
    case PVM_FMEM_LDF: DisasmFMem(f, "LDF", Opcode); break;

    case PVM_IRD_ARITH: DisasmImmRdArith(f, Opcode); break;
    case PVM_IRD_MEM: DisasmImmRdMem(f, Opcode); break;

    case PVM_BRIF_EZ: DisasmBrIf(f, "EZ", Addr, Opcode); break;
    case PVM_BRIF_NZ: DisasmBrIf(f, "NZ", Addr, Opcode); break;
    case PVM_BALT_AL: DisasmBAlt(f, "BAL", Addr, Opcode); break;
    case PVM_BALT_SR: DisasmBAlt(f, "BSR", Addr, Opcode); break;
    }
}








static void DisasmIDatArith(FILE *f, PVMWord Opcode)
{
    const char *Mnemonic = "???";
    const char *Rd = sIntRegName[PVM_IDAT_GET_RD(Opcode)];
    const char *Ra = sIntRegName[PVM_IDAT_GET_RA(Opcode)];
    const char *Rb = sIntRegName[PVM_IDAT_GET_RB(Opcode)];
    switch (PVM_IDAT_GET_ARITH(Opcode))
    {
    case PVM_ARITH_ADD: Mnemonic = "ADD"; break;
    case PVM_ARITH_SUB: Mnemonic = "SUB"; break;
    case PVM_ARITH_NEG: fprintf(f, "NEG %s, %s\n", Rd, Ra); return;
    }
    DisasmArithIns(f, Mnemonic, Rd, Ra, Rb);
}

static void DisasmIDatSpecial(FILE *f, PVMWord Opcode)
{    
    const char *Rd = sIntRegName[PVM_IDAT_GET_RD(Opcode)];
    const char *Ra = sIntRegName[PVM_IDAT_GET_RA(Opcode)];
    const char *Rb = sIntRegName[PVM_IDAT_GET_RB(Opcode)];
    switch (PVM_IDAT_GET_SPECIAL(Opcode))
    {
    case PVM_SPECIAL_MUL: 
    {
        if (PVM_IDAT_SPECIAL_SIGNED(Opcode))
            DisasmArithIns(f, "SMUL", Ra, Ra, Rb);
        else
            DisasmArithIns(f, "MUL", Ra, Ra, Rb);
    } break;

    case PVM_SPECIAL_DIVP:
    {
        const char *Rr = sIntRegName[PVM_IDAT_SPECIAL_GET_RR(Opcode)];
        if (PVM_IDAT_SPECIAL_SIGNED(Opcode))
            DisasmIDatExOper(f, "SDIVP", Rd, Ra, Rb, Rr);
        else
            DisasmIDatExOper(f, "DIVP", Rd, Ra, Rb, Rr);
    } break;

    case PVM_SPECIAL_DIV:
    {
        const char *Rr = sIntRegName[PVM_IDAT_SPECIAL_GET_RR(Opcode)];
        if (PVM_IDAT_SPECIAL_SIGNED(Opcode))
            DisasmIDatExOper(f, "SDIV", Rd, Ra, Rb, Rr);
        else
            DisasmIDatExOper(f, "DIV", Rd, Ra, Rb, Rr);
    } break;

    case PVM_SPECIAL_D2P: 
    {
        const char *Fa = sFloatRegName[PVM_FDAT_GET_FA(Opcode)];
        DisasmTransferIns(f, "D2P", Rd, Fa);
    } break;
    }
}

static void DisasmIDatCmp(FILE *f, PVMWord Opcode)
{
    const char *MnemonicTable[] = {
        [PVM_CMP_SEQB] = "SEQB",
        [PVM_CMP_SNEB] = "SNEB",
        [PVM_CMP_SLTB] = "SLTB",
        [PVM_CMP_SGTB] = "SGTB",

        [PVM_CMP_SEQH] = "SEQH",
        [PVM_CMP_SNEH] = "SNEH",
        [PVM_CMP_SLTH] = "SLTH",
        [PVM_CMP_SGTH] = "SGTH",

        [PVM_CMP_SEQW] = "SEQW",
        [PVM_CMP_SNEW] = "SNEW",
        [PVM_CMP_SLTW] = "SLTW",
        [PVM_CMP_SGTW] = "SGTW",

        [PVM_CMP_SEQP] = "SEQP",
        [PVM_CMP_SNEP] = "SNEP",
        [PVM_CMP_SLTP] = "SLTP",
        [PVM_CMP_SGTP] = "SGTP",

        [PVM_CMP_SSLTB] = "SSLTB",
        [PVM_CMP_SSGTB] = "SSGTB",

        [PVM_CMP_SSLTH] = "SSLTH",
        [PVM_CMP_SSGTH] = "SSGTH",

        [PVM_CMP_SSLTW] = "SSLTW",
        [PVM_CMP_SSGTW] = "SSGTW",

        [PVM_CMP_SSLTP] = "SSLTP",
        [PVM_CMP_SSGTP] = "SSGTP",
    };
    const char *Mnemonic = "???";
    const char *Rd = sIntRegName[PVM_IDAT_GET_RD(Opcode)];
    const char *Ra = sIntRegName[PVM_IDAT_GET_RA(Opcode)];
    const char *Rb = sIntRegName[PVM_IDAT_GET_RB(Opcode)];

    UInt Op = PVM_GET_OP(Opcode);
    if (Op < STATIC_ARRAY_SIZE(MnemonicTable))
    {
        Mnemonic = MnemonicTable[Op];
    }
    DisasmArithIns(f, Mnemonic, Rd, Ra, Rb);
}



static void DisasmIDatTransfer(FILE *f, PVMWord Opcode)
{
    const char *Mnemonic = "???";
    const char *Rd = sIntRegName[PVM_IDAT_GET_RD(Opcode)];
    const char *Ra = sIntRegName[PVM_IDAT_GET_RA(Opcode)];

    switch (PVM_IDAT_GET_TRANSFER(Opcode))
    {
    case PVM_TRANSFER_MOV: Mnemonic = "MOV"; break;
    }

    DisasmTransferIns(f, Mnemonic, Rd, Ra);
}







static void DisasmFDatArith(FILE *f, PVMWord Opcode)
{
    const char *Mnemonic = "???";
    const char *Fd = sFloatRegName[PVM_FDAT_GET_FD(Opcode)];
    const char *Fa = sFloatRegName[PVM_FDAT_GET_FA(Opcode)];
    const char *Fb = sFloatRegName[PVM_FDAT_GET_FB(Opcode)];
    switch (PVM_FDAT_GET_ARITH(Opcode))
    {
    case PVM_ARITH_ADD: Mnemonic = "FADD"; break;
    case PVM_ARITH_SUB: Mnemonic = "FSUB"; break;
    case PVM_ARITH_NEG: fprintf(f, "FNEG %s, %s\n", Fd, Fa); return;
    }
    DisasmArithIns(f, Mnemonic, Fd, Fa, Fb);
}

static void DisasmFDatSpecial(FILE *f, PVMWord Opcode)
{
    const char *Mnemonic = "???";
    const char *Fd = sFloatRegName[PVM_FDAT_GET_FD(Opcode)];
    const char *Fa = sFloatRegName[PVM_FDAT_GET_FA(Opcode)];
    const char *Fb = sFloatRegName[PVM_FDAT_GET_FB(Opcode)];

    switch (PVM_FDAT_GET_SPECIAL(Opcode))
    {
    case PVM_SPECIAL_DIVP:
    case PVM_SPECIAL_DIV: Mnemonic = "FDIV"; break;
    case PVM_SPECIAL_MUL: Mnemonic = "FMUL"; break;
    case PVM_SPECIAL_P2D:
    {
        Fd = sIntRegName[PVM_IDAT_GET_RD(Opcode)];
        Mnemonic = "P2D";
    } break;
    }
    DisasmArithIns(f, Mnemonic, Fd, Fa, Fb);
}

static void DisasmFDatCmp(FILE *f, PVMWord Opcode)
{
    const char *Mnemonic = "???";
    const char *Fd = sFloatRegName[PVM_FDAT_GET_FD(Opcode)];
    const char *Fa = sFloatRegName[PVM_FDAT_GET_FA(Opcode)];
    const char *Fb = sFloatRegName[PVM_FDAT_GET_FB(Opcode)];
    switch (PVM_FDAT_GET_CMP(Opcode))
    {
    case PVM_CMP_SNEP: Mnemonic = "FSNE"; break;
    case PVM_CMP_SEQP: Mnemonic = "FSEQ"; break;
    case PVM_CMP_SLTP: Mnemonic = "FSLT"; break;
    case PVM_CMP_SGTP: Mnemonic = "FSGT"; break;
    default: break;
    }

    DisasmArithIns(f, Mnemonic, Fd, Fa, Fb);
}

static void DisasmFDatTransfer(FILE *f, PVMWord Opcode)
{
    const char *Mnemonic = "???";
    const char *Fd = sFloatRegName[PVM_FDAT_GET_FD(Opcode)];
    const char *Fa = sFloatRegName[PVM_FDAT_GET_FA(Opcode)];

    switch (PVM_FDAT_GET_TRANSFER(Opcode))
    {
    case PVM_TRANSFER_MOV: Mnemonic = "FMOV"; break;
    }

    DisasmTransferIns(f, Mnemonic, Fd, Fa);
}


static void DisasmFMem(FILE *f, const char *Mnemonic, PVMWord Opcode)
{
    const char *Fd = sFloatRegName[PVM_FMEM_GET_FD(Opcode)];
    fprintf(f, "%s %s, %s\n", Mnemonic, Fd, "TODO: Operand of LDF");
}






static void DisasmTransferIns(FILE *f, const char *Mnemonic, const char *Rd, const char *Ra)
{
    fprintf(f, "%s %s, %s\n", Mnemonic, Rd, Ra);
}

static void DisasmArithIns(FILE *f, const char *Mnemonic, const char *Rd, const char *Ra, const char *Rb)
{
    fprintf(f, "%s %s, %s, %s\n", 
            Mnemonic, Rd, Ra, Rb
    );
}

static void DisasmIDatExOper(FILE *f, const char *Mnemonic, const char *Rd, const char *Ra, const char *Rb, const char *Sh)
{
    fprintf(f, "%s %s, %s, %s, %s\n", 
            Mnemonic, Rd, Ra, Rb, Sh
    );
}

static void DisasmBrIf(FILE *f, const char *Mnemonic, PVMWord Addr, PVMWord Opcode)
{
    const char *Ra = sIntRegName[PVM_BRIF_GET_RA(Opcode)];
    PVMWord BranchTarget = Addr + 1 + PVM_BRIF_GET_IMM(Opcode);

    fprintf(f, "B%s %s, [%x]\n", 
            Mnemonic, Ra, BranchTarget
    );
}


static void DisasmBAlt(FILE *f, const char *Mnemonic, PVMWord Addr, PVMWord Opcode)
{
    PVMWord BranchTarget = Addr + 1 + PVM_BAL_GET_IMM(Opcode);
    if (PVM_GET_INS(Opcode) == PVM_BALT_SR && BranchTarget == Addr)
    {
        fprintf(f, "RET\n");
    }
    else
    {
        fprintf(f, "%s [%x]\n", Mnemonic, BranchTarget);
    }
}




static void DisasmImmRdArith(FILE *f, PVMWord Opcode)
{
    switch (PVM_IRD_GET_ARITH(Opcode))
    {
    case PVM_IRD_ADD: DisasmImmReg(f, "ADD", sIntRegName, Opcode, IMM_SIGNED); break;
    case PVM_IRD_SUB: DisasmImmReg(f, "SUB", sIntRegName, Opcode, IMM_SIGNED); break;

    case PVM_IRD_LDI: DisasmImmReg(f, "LDI", sIntRegName, Opcode, IMM_SIGNED); break;
    case PVM_IRD_LDZI: DisasmImmReg(f, "LDZI", sIntRegName, Opcode, IMM_UNSIGNED); break;
    case PVM_IRD_ORUI: DisasmImmReg(f, "ORUI", sIntRegName, Opcode, IMM_UNSIGNED); break;
    case PVM_IRD_LDZHLI: DisasmImmReg(f, "LDZHLI", sIntRegName, Opcode, IMM_UNSIGNED); break;
    case PVM_IRD_LDHLI: DisasmImmReg(f, "LDHLI", sIntRegName, Opcode, IMM_SIGNED); break;
    case PVM_IRD_ORHUI: DisasmImmReg(f, "ORHUI", sIntRegName, Opcode, IMM_UNSIGNED); break;
    }
}

static void DisasmImmRdMem(FILE *f, PVMWord Opcode)
{
    switch (PVM_IRD_GET_MEM(Opcode))
    {
    case PVM_IRD_LDRS: DisasmImmReg(f, "LDRS", sIntRegName, Opcode, IMM_INDIRECT); break;
    case PVM_IRD_LDFS: DisasmImmReg(f, "LDFS", sFloatRegName, Opcode, IMM_INDIRECT); break;
    case PVM_IRD_STRS: DisasmImmReg(f, "STRS", sIntRegName, Opcode, IMM_INDIRECT); break;
    case PVM_IRD_STFS: DisasmImmReg(f, "STFS", sFloatRegName, Opcode, IMM_INDIRECT); break;

    case PVM_IRD_POPU: DisasmRegList(f, "POP", Opcode, false); break;
    case PVM_IRD_POPL: DisasmRegList(f, "POP", Opcode, true); break;
    case PVM_IRD_PSHL: DisasmRegList(f, "PSH", Opcode, false); break;
    case PVM_IRD_PSHU: DisasmRegList(f, "PSH", Opcode, true); break;
    case PVM_IRD_ADDSPI: DisasmLongImm(f, "ADDSP", Opcode, true); break;
    }
}



static void DisasmImmReg(FILE *f, const char *Mnemonic, const char *RegisterSet[32], PVMWord Opcode, ImmType Type)
{
    const char *Rd = RegisterSet[PVM_IRD_GET_RD(Opcode)];
    U32 Immediate = PVM_IRD_GET_IMM(Opcode);

    switch (Type)
    {
    case IMM_SIGNED:
    {
        fprintf(f, "%s %s, %d\n", Mnemonic, Rd, (I32)Immediate);
    } break;
    case IMM_UNSIGNED:
    {
        fprintf(f, "%s %s, %u\n", Mnemonic, Rd, Immediate);
    } break;
    case IMM_INDIRECT:
    {
        fprintf(f, "%s %s, [%u]\n", Mnemonic, Rd, Immediate);
    } break;
    case IMM_SIGNED_INDIRECT:
    {
        fprintf(f, "%s %s, [%d]\n", Mnemonic, Rd, (I32)Immediate);
    } break;
    }
}

static void DisasmLongImm(FILE *f, const char *Mnemonic, PVMWord Opcode, ImmType Type)
{
    U32 Immediate = BIT_AT32(Opcode, 21, 0);
    switch (Type)
    {
    case IMM_SIGNED: fprintf(f, "%s %d\n", Mnemonic, (I32)BIT_SEX32(Immediate, 20)); break;
    case IMM_UNSIGNED: fprintf(f, "%s %u\n", Mnemonic, Immediate); break;
    case IMM_INDIRECT: fprintf(f, "%s [%u]\n", Mnemonic, Immediate); break;
    case IMM_SIGNED_INDIRECT: fprintf(f, "%s [%d]\n", Mnemonic, (I32)BIT_SEX32(Immediate, 20)); break;
    }
}


static void DisasmRegList(FILE *f, const char *Mnemonic, PVMWord Opcode, bool UpperRegisters)
{
    const char **Registers = sIntRegName;
    if (UpperRegisters)
    {
        Registers = &sIntRegName[16];
    }

    char RegisterListStr[256];
    UInt RegisterList = PVM_IRD_GET_IMM(Opcode);
    for (UInt i = 0; i < 16; i++)
    {
        if ((RegisterList >> i) & 0x1)
        {
            snprintf(RegisterListStr, sizeof RegisterListStr, "%s", Registers[i]);
            if (i != 15)
                snprintf(RegisterListStr, sizeof RegisterListStr, ", ");
        }
    }
    fprintf(f, "%s {%s}\n", Mnemonic, RegisterListStr);
}



static void DisasmSysOp(FILE *f, PVMWord Addr, PVMWord Opcode)
{
    PVMSysOp SysOp = PVM_GET_SYS_OP(Opcode);
    (void)Addr;

    switch (SysOp)
    {
    case PVM_SYS_EXIT:
    {
        fprintf(f, "SYS_EXIT\n");
    } break;

    }
}


static void PrintAddr(FILE *f, PVMWord Addr)
{
    fprintf(f, "%8x:    ", Addr);
}

static void PrintHexCode(FILE *f, PVMWord Opcode)
{
    fprintf(f, "%08X  ", Opcode);
}

