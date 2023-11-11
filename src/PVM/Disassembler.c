
#include "PVM/PVM.h"


static const char *sRegName[] = 
{
    "R0", "R1", "R2", "R3", "R4", "R5", "R6", "R7",
    "R8", "R9", "R10", "R11", "R12", "R13", "R14", "R15",
    "R16", "R17", "R18", "R19", "R20", "R21", "R22", "R23",
    "R24", "R25", "R26", "R27", "R28", "R29", "R30", "R31",
    "R32", "R33", "R34", "R35", "R36", "R37", "R38", "R39",
    "R40", "R41", "R42", "R43", "R44", "R45", "R46", "R47",
    "R48", "R49", "R50", "R51", "R52", "R53", "R54", "R55",
    "R56", "R57", "R58", "R59", "R60", "R61", "R62", "R63",
};
static void DisasmInstruction(FILE *f, PVMWord Addr, PVMWord Opcode);

static void DisasmDataInsArith(FILE *f, PVMWord Opcode);
static void DisasmDataInsSpecial(FILE *f, PVMWord Opcode);
static void DisasmDataInsCmp(FILE *f, PVMWord Opcode);
static void DisasmDataInsTransfer(FILE *f, PVMWord Opcode);
static void DisasmDataIns(FILE *f, const char *Mnemonic, const char *Rd, const char *Ra, const char *Rb);
static void DisasmDataExOper(FILE *f, const char *Mnemonic, const char *Rd, const char *Ra, const char *Rb, const char *Sh);


static void DisasmFDatArith(FILE *f, PVMWord Opcode);
static void DisasmFDatSpecial(FILE *f, PVMWord Opcode);
static void DisasmFDatCmp(FILE *f, PVMWord Opcode);
static void DisasmFDatTransfer(FILE *f, PVMWord Opcode);

static void DisasmBrIf(FILE *f, const char *Mnemonic, PVMWord Addr, PVMWord Opcode);
static void DisasmBAlt(FILE *f, const char *Mnemonic, PVMWord Addr, PVMWord Opcode);

static void DisasmImmRdArith(FILE *f, PVMWord Opcode);
static void DisasmImmRd(FILE *f, const char *Mnemonic, PVMWord Opcode, bool IsSigned);

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

    case PVM_DI_ARITH: DisasmDataInsArith(f, Opcode); break;
    case PVM_DI_SPECIAL: DisasmDataInsSpecial(f, Opcode); break;
    case PVM_DI_CMP: DisasmDataInsCmp(f, Opcode); break;
    case PVM_DI_TRANSFER: DisasmDataInsTransfer(f, Opcode); break;

    case PVM_FDAT_ARITH: DisasmFDatArith(f, Opcode); break;
    case PVM_FDAT_SPECIAL: DisasmFDatSpecial(f, Opcode); break;
    case PVM_FDAT_CMP: DisasmFDatCmp(f, Opcode); break;
    case PVM_FDAT_TRANSFER: DisasmFDatTransfer(f, Opcode); break;

    case PVM_IRD_ARITH: DisasmImmRdArith(f, Opcode); break;

    case PVM_BRIF_EQ: DisasmBrIf(f, "EQ", Addr, Opcode); break;
    case PVM_BRIF_NE: DisasmBrIf(f, "NE", Addr, Opcode); break;
    case PVM_BRIF_LT: DisasmBrIf(f, "LT", Addr, Opcode); break;
    case PVM_BRIF_GT: DisasmBrIf(f, "GT", Addr, Opcode); break;
    case PVM_BRIF_SLT: DisasmBrIf(f, "SLT", Addr, Opcode); break;
    case PVM_BRIF_SGT: DisasmBrIf(f, "SGT", Addr, Opcode); break;
    case PVM_BALT_AL: DisasmBAlt(f, "BAL", Addr, Opcode); break;
    case PVM_BALT_SR: DisasmBAlt(f, "BSR", Addr, Opcode); break;

    }
}








static void DisasmDataInsArith(FILE *f, PVMWord Opcode)
{
    const char *Rd = sRegName[PVM_DI_GET_RD(Opcode)];
    const char *Ra = sRegName[PVM_DI_GET_RA(Opcode)];
    const char *Rb = sRegName[PVM_DI_GET_RB(Opcode)];

    switch ((PVMArith)PVM_DI_GET_OP(Opcode))
    {
    case PVM_ARITH_ADD: DisasmDataIns(f, "ADD", Rd, Ra, Rb); break;
    case PVM_ARITH_SUB: DisasmDataIns(f, "SUB", Rd, Ra, Rb); break;
    {
        fprintf(f, "???\n");
    } break;

    }
}

static void DisasmDataInsSpecial(FILE *f, PVMWord Opcode)
{    
    const char *Rd = sRegName[PVM_DI_GET_RD(Opcode)];
    const char *Ra = sRegName[PVM_DI_GET_RA(Opcode)];
    const char *Rb = sRegName[PVM_DI_GET_RB(Opcode)];
    switch ((PVMSpecial)PVM_DI_GET_OP(Opcode))
    {
    case PVM_SPECIAL_MUL: 
    {
        if (PVM_DI_SPECIAL_SIGNED(Opcode))
            DisasmDataIns(f, "SMUL", Ra, Ra, Rb);
        else
            DisasmDataIns(f, "MUL", Ra, Ra, Rb);
    } break;

    case PVM_SPECIAL_DIVP:
    {
        const char *Rr = sRegName[PVM_DI_SPECIAL_GET_RR(Opcode)];
        if (PVM_DI_SPECIAL_SIGNED(Opcode))
            DisasmDataExOper(f, "SDIVP", Rd, Ra, Rb, Rr);
        else
            DisasmDataExOper(f, "DIVP", Rd, Ra, Rb, Rr);
    } break;

    case PVM_SPECIAL_DIV:
    {
        const char *Rr = sRegName[PVM_DI_SPECIAL_GET_RR(Opcode)];
        if (PVM_DI_SPECIAL_SIGNED(Opcode))
            DisasmDataExOper(f, "SDIV", Rd, Ra, Rb, Rr);
        else
            DisasmDataExOper(f, "DIV", Rd, Ra, Rb, Rr);
    } break;
    }
}

static void DisasmDataInsCmp(FILE *f, PVMWord Opcode)
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
    PVMCmp Op = PVM_DI_GET_OP(Opcode);
    if ((UInt)Op < STATIC_ARRAY_SIZE(MnemonicTable))
    {
        const char *Rd = sRegName[PVM_DI_GET_RD(Opcode)];
        const char *Ra = sRegName[PVM_DI_GET_RA(Opcode)];
        const char *Rb = sRegName[PVM_DI_GET_RB(Opcode)];
        DisasmDataIns(f, MnemonicTable[Op], Rd, Ra, Rb);
    }
    else
    {
        fprintf(f, "???\n");
    }
}



static void DisasmDataInsTransfer(FILE *f, PVMWord Opcode)
{
    const char *Mnemonic = "???";
    const char *Rd = sRegName[PVM_DI_GET_RD(Opcode)];
    const char *Ra = sRegName[PVM_DI_GET_RA(Opcode)];

    switch ((PVMTransfer)PVM_DI_GET_OP(Opcode))
    {
    case PVM_DI_MOV: Mnemonic = "MOV"; break;
    }

    fprintf(f, "%s %s, %s\n", Mnemonic, Rd, Ra);
}







static void DisasmFDatArith(FILE *f, PVMWord Opcode)
{
    const char *Mnemonic = "???";
    const char *Rd = sRegName[PVM_FDAT_GET_FD(Opcode)];
    const char *Ra = sRegName[PVM_FDAT_GET_FA(Opcode)];
    const char *Rs = sRegName[PVM_FDAT_GET_FB(Opcode)];

    switch (PVM_FDAT_GET_OP(Opcode))
    {
    case PVM_ARITH_ADD: Mnemonic = "ADD"; break;
    case PVM_ARITH_SUB: Mnemonic = "SUB"; break;
    }
}

static void DisasmFDatSpecial(FILE *f, PVMWord Opcode)
{
}

static void DisasmFDatCmp(FILE *f, PVMWord Opcode)
{
}

static void DisasmFDatTransfer(FILE *f, PVMWord Opcode)
{
}












static void DisasmDataIns(FILE *f, const char *Mnemonic, const char *Rd, const char *Ra, const char *Rb)
{
    fprintf(f, "%s %s, %s, %s\n", 
            Mnemonic, Rd, Ra, Rb
    );
}

static void DisasmDataExOper(FILE *f, const char *Mnemonic, const char *Rd, const char *Ra, const char *Rb, const char *Sh)
{
    fprintf(f, "%s %s, %s, %s, %s\n", 
            Mnemonic, Rd, Ra, Rb, Sh
    );
}

static void DisasmBrIf(FILE *f, const char *Mnemonic, PVMWord Addr, PVMWord Opcode)
{
    const char *Ra = sRegName[PVM_BRIF_GET_RA(Opcode)];
    const char *Rb = sRegName[PVM_BRIF_GET_RB(Opcode)];
    PVMWord BranchTarget = Addr + 1 + PVM_BRIF_GET_IMM(Opcode);

    fprintf(f, "B%s %s, %s, [%u]\n", 
            Mnemonic, Ra, Rb, BranchTarget
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
        fprintf(f, "%s [%u]\n", Mnemonic, BranchTarget);
    }
}




static void DisasmImmRdArith(FILE *f, PVMWord Opcode)
{
    switch ((PVMIRDArith)PVM_IRD_GET_OP(Opcode))
    {
    case PVM_IRD_ADD: DisasmImmRd(f, "ADD", Opcode, true); break;
    case PVM_IRD_SUB: DisasmImmRd(f, "SUB", Opcode, true); break;
    case PVM_IRD_LDI: DisasmImmRd(f, "LDI", Opcode, true); break;
    case PVM_IRD_LUI: DisasmImmRd(f, "LUI", Opcode, false); break;
    case PVM_IRD_ORI: DisasmImmRd(f, "ORI", Opcode, false); break;
    }
}



static void DisasmImmRd(FILE *f, const char *Mnemonic, PVMWord Opcode, bool IsSigned)
{
    const char *Rd = sRegName[PVM_IRD_GET_RD(Opcode)];
    int Immediate = IsSigned 
        ? (int)(I16)PVM_IRD_GET_IMM(Opcode)
        : (int)PVM_IRD_GET_IMM(Opcode);

    fprintf(f, "%s %s, %d\n", 
            Mnemonic, Rd, Immediate
    );
}



static void DisasmSysOp(FILE *f, PVMWord Addr, PVMWord Opcode)
{
    PVMSysOp SysOp = PVM_GET_SYS_OP(Opcode);

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
    fprintf(f, "%08x  ", Opcode);
}

