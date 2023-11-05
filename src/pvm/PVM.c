
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
static void DisasmInstruction(FILE *f, U32 Addr, U32 Opcode);
static void DisasmDataIns(FILE *f, const char *Mnemonic, U32 Addr, U32 Opcode);
static void DisasmBrIf(FILE *f, const char *Mnemonic, U32 Addr, U32 Opcode);
static void DisasmImmRd(FILE *f, const char *Mnemonic, U32 Addr, U32 Opcode, bool IsSigned);
static void PrintAddr(FILE *f, U32 Addr);


void PVMDisasm(FILE *f, const CodeChunk *Chunk)
{
    for (U32 i = 0; i < Chunk->Count; i++)
    {
        DisasmInstruction(f, i, Chunk->Data[i]);
    }
}



static void DisasmInstruction(FILE *f, U32 Addr, U32 Opcode)
{
    PVMIns Ins = PVM_GET_INS(Opcode);
    switch (Ins)
    {
    case PVM_RESV:

    case PVM_DI_ADD: DisasmDataIns(f, "ADD", Addr, Opcode); break;
    case PVM_DI_SUB: DisasmDataIns(f, "SUB", Addr, Opcode); break;

    case PVM_BRIF_EQ: DisasmBrIf(f, "EQ", Addr, Opcode); break;

    case PVM_IRD_ADD: DisasmImmRd(f, "ADD", Addr, Opcode, true); break;
    case PVM_IRD_SUB: DisasmImmRd(f, "SUB", Addr, Opcode, true); break;
    case PVM_IRD_LDI: DisasmImmRd(f, "LDI", Addr, Opcode, true); break;
    case PVM_IRD_LUI: DisasmImmRd(f, "LUI", Addr, Opcode, false); break;
    case PVM_IRD_ORI: DisasmImmRd(f, "ORI", Addr, Opcode, false); break;

    case PVM_IRD_COUNT:
    case PVM_BRIF_COUNT:
    case PVM_DI_COUNT:
    case PVM_INS_COUNT:
    {
        PASCAL_UNREACHABLE("Counting enums are not instructions");
    } break;
    }
}



static void DisasmDataIns(FILE *f, const char *Mnemonic, U32 Addr, U32 Opcode)
{
    const char *Rd = sRegName[PVM_GET_DI_RD(Opcode)];
    const char *Rs0 = sRegName[PVM_GET_DI_RS0(Opcode)];
    const char *Rs1 = sRegName[PVM_GET_DI_RS1(Opcode)];

    PrintAddr(f, Addr);
    fprintf(f, "DI_%s %s, %s, %s\n", 
            Mnemonic, Rd, Rs0, Rs1
    );
}

static void DisasmBrIf(FILE *f, const char *Mnemonic, U32 Addr, U32 Opcode)
{
    const char *Ra = sRegName[PVM_GET_BRIF_RA(Opcode)];
    const char *Rb = sRegName[PVM_GET_BRIF_RB(Opcode)];
    U32 BranchTarget = Addr + PVM_GET_BRIF_IMM10(Opcode);

    PrintAddr(f, Addr);
    fprintf(f, "BRIF_%s %s, %s, [%u]\n", 
            Mnemonic, Ra, Rb, BranchTarget
    );
}

static void DisasmImmRd(FILE *f, const char *Mnemonic, U32 Addr, U32 Opcode, bool IsSigned)
{
    const char *Rd = sRegName[PVM_GET_IRD_RD(Opcode)];
    int Immediate = IsSigned 
        ? (int)(I16)PVM_GET_IRD_IMM16(Opcode)
        : (int)PVM_GET_IRD_IMM16(Opcode);

    PrintAddr(f, Addr);
    fprintf(f, "IRD_%s %s, %d\n", 
            Mnemonic, Rd, Immediate
    );
}


static void PrintAddr(FILE *f, U32 Addr)
{
    fprintf(f, "%8x:    ", Addr);
}

