
#include "PVM/_Isa.h"
#include "PVM/_Disassembler.h"

static const char *sIntReg[PVM_REG_COUNT] = {
    "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", 
    "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
};

static const char *sFltReg[PVM_FREG_COUNT] = {
    "f0", "f1", "f2", "f3", "f4", "f5", "f6", "f7", 
    "f8", "f9", "f10", "f11", "f12", "f13", "f14", "f15",
};

static const int sBytesPerLine = 4;


void PVMDisasm(FILE *f, const PVMChunk *Chunk, const char *Name)
{
    const char *Fmt = "===============";
    fprintf(f, "%s %s %s\n", Fmt, Name, Fmt);

    U32 i = 0;
    while (i < Chunk->Count)
    {
        i = PVMDisasmSingleInstruction(f, Chunk, i);
    }
}




static void DisasmRdRs(FILE *f, const char *Mnemonic, const char *RegSet[], U16 Opcode)
{
    const char *Rd = RegSet[PVM_GET_RD(Opcode)];
    const char *Rs = RegSet[PVM_GET_RS(Opcode)];

    int Pad = fprintf(f, "%02x %02x", Opcode >> 8, Opcode & 0xFF);
    fprintf(f, "%*s%6s %s, %s\n", 3*sBytesPerLine - Pad, "", 
            Mnemonic, Rd, Rs
    );
}

static void DisasmFscc(FILE *f, const char *Mnemonic, U16 Opcode)
{
    const char *Rd = sIntReg[PVM_GET_RD(Opcode)];
    const char *Fd = sFltReg[PVM_GET_RD(Opcode)];
    const char *Fs = sFltReg[PVM_GET_RS(Opcode)];

    int Pad = fprintf(f, "%02x %02x", Opcode >> 8, Opcode & 0xFF);
    fprintf(f, "%*s%6s %s, %s, %s\n", 3*sBytesPerLine - Pad, "", 
            Mnemonic, Rd, Fd, Fs
    );
}

static U32 DisasmBcc(FILE *f, const char *Mnemonic, U16 Opcode, const PVMChunk *Chunk, U32 Addr)
{
    const char *Rd = sIntReg[PVM_GET_RD(Opcode)];
    I32 BrOffset = BitSex32Safe(
            ((Opcode & 0xF) << 16) | (U32)(Chunk->Code[Addr]), 
            PVM_BCC_OFFSET_SIZE - 1
    );

    int Pad = fprintf(f, "%02x %02x %02x %02x", 
            Opcode >> 8, Opcode & 0xFF, Chunk->Code[Addr] >> 8, Chunk->Code[Addr] & 0xFF
    );
    fprintf(f, "%*s%6s %s, [%u]\n", 3*sBytesPerLine - Pad, "",
            Mnemonic, Rd, Addr + BrOffset
    );
    return Addr + 2;
}


static U32 DisasmBr(FILE *f, const char *Mnemonic, U16 Opcode, const PVMChunk *Chunk, U32 Addr)
{
    const char *Rd = sIntReg[PVM_GET_RD(Opcode)];
    I32 BrOffset = BitSex32Safe(
            (((U32)Opcode & 0xFF) << 16) | (U32)(Chunk->Code[Addr]), 
            PVM_BR_OFFSET_SIZE - 1
    );

    int Pad = fprintf(f, "%02x %02x %02x %02x", 
            Opcode >> 8, Opcode & 0xFF, Chunk->Code[Addr] >> 8, Chunk->Code[Addr] & 0xFF
    );
    Addr += 2;
    fprintf(f, "%*s%6s %s, [%u]\n", 3*sBytesPerLine - Pad, "",
            Mnemonic, Rd, Addr + BrOffset
    );
    return Addr;
}


static void DisasmMnemonic(FILE *f, const char *Mnemonic, U16 Opcode)
{
    int Pad = fprintf(f, "%02x %02x", Opcode >> 8, Opcode & 0xFF);
    fprintf(f, "%*s%6s\n", 3*sBytesPerLine - Pad, "", 
            Mnemonic
    );
}

static U32 DisasmSysOp(FILE *f, const PVMChunk *Chunk, U32 Addr, U16 Opcode)
{
    (void)Chunk;
    switch (PVM_GET_SYS_OP(Opcode))
    {
    case OP_SYS_EXIT:
    {
        DisasmMnemonic(f, "exit", Opcode);
        return Addr + 1;
    } break;
    }
}

static void DisasmReglist(FILE *f, const char *Mnemonic, U16 Opcode, bool UpperRegisters)
{
    const char **Registers = sIntReg;
    if (UpperRegisters)
    {
        Registers = &sIntReg[PVM_REG_COUNT/2];
    }

    char RegisterListStr[256] = {0};
    char *Tmp = RegisterListStr;
    UInt SizeLeft = sizeof RegisterListStr;
    int Written = 0;

    UInt RegisterList = PVM_GET_REGLIST(Opcode);

#define REGISTER_SELECTED(Idx) ((RegisterList >> (Idx)) & 0x1)
    UInt i = 0;
    while (i < PVM_REG_COUNT/2)
    {
        if ((REGISTER_SELECTED(i) && SizeLeft > 0))
        {
            if (Written)
            {
                Written = snprintf(Tmp, SizeLeft, ", ");
                Tmp += Written;
                SizeLeft -= Written;
            }

            /* go through a line of selected reg */
            /* r1 ... r3 */
            if (REGISTER_SELECTED(i + 1))
            {
                Written = snprintf(Tmp, SizeLeft, "%s ... ", Registers[i]);
                Tmp += Written;
                SizeLeft -= Written;
                do {
                    i++;
                } while (REGISTER_SELECTED(i + 1));
            }

            Written = snprintf(Tmp, SizeLeft, "%s", Registers[i]);
            Tmp += Written;
            SizeLeft -= Written;
        }
        i++;
    }
#undef REGISTER_SELECTED

    int Pad = fprintf(f, "%02x %02x", Opcode >> 8, Opcode & 0xFF);
    fprintf(f, "%*s%6s { %s }\n", 3*sBytesPerLine - Pad, "", 
            Mnemonic, RegisterListStr
    );
}




U32 PVMDisasSingleInstruction(FILE *f, const PVMChunk *Chunk, U32 Addr)
{
    U16 Opcode = Chunk->Code[Addr];
    fprintf(f, "%08u: ", Addr);

    switch (PVM_GET_OP(Opcode))
    {
    case OP_SYS:
    {
        return DisasmSysOp(f, Chunk, Addr, Opcode);
    } break;
    case OP_ADD: DisasmRdRs(f, "add", sIntReg, Opcode); break;
    case OP_SUB: DisasmRdRs(f, "sub", sIntReg, Opcode); break;
    case OP_DIV: DisasmRdRs(f, "div", sIntReg, Opcode); break;
    case OP_MUL: DisasmRdRs(f, "mul", sIntReg, Opcode); break;
    case OP_IDIV: DisasmRdRs(f, "idiv", sIntReg, Opcode); break;
    case OP_IMUL: DisasmRdRs(f, "imul", sIntReg, Opcode); break;
    case OP_NEG: DisasmRdRs(f, "neg", sIntReg, Opcode); break;

    case OP_BNZ: return DisasmBcc(f, "bnz", Opcode, Chunk, Addr);
    case OP_BEZ: return DisasmBcc(f, "bez", Opcode, Chunk, Addr);
    case OP_BR: return DisasmBr(f, "br", Opcode, Chunk, Addr);
    case OP_BSR: return DisasmBr(f, "bsr", Opcode, Chunk, Addr);


    case OP_SEQ: DisasmRdRs(f, "seq", sIntReg, Opcode); break;
    case OP_SNE: DisasmRdRs(f, "sne", sIntReg, Opcode); break;
    case OP_SLT: DisasmRdRs(f, "slt", sIntReg, Opcode); break;
    case OP_SGT: DisasmRdRs(f, "sgt", sIntReg, Opcode); break;
    case OP_ISLT: DisasmRdRs(f, "islt", sIntReg, Opcode); break;
    case OP_ISGT: DisasmRdRs(f, "isgt", sIntReg, Opcode); break;


    case OP_PSHL: DisasmReglist(f, "pshl", Opcode, false); break;
    case OP_PSHH: DisasmReglist(f, "pshh", Opcode, true); break;
    case OP_POPL: DisasmReglist(f, "popl", Opcode, false); break;
    case OP_POPH: DisasmReglist(f, "pshh", Opcode, true); break;


    case OP_FADD: DisasmRdRs(f, "fadd", sFltReg, Opcode); break;
    case OP_FSUB: DisasmRdRs(f, "fsub", sFltReg, Opcode); break;
    case OP_FDIV: DisasmRdRs(f, "fdiv", sFltReg, Opcode); break;
    case OP_FMUL: DisasmRdRs(f, "fmul", sFltReg, Opcode); break;
    case OP_FNEG: DisasmRdRs(f, "fneg", sFltReg, Opcode); break;

    case OP_MOV: DisasmRdRs(f, "mov", sIntReg, Opcode); break;
    case OP_FMOV: DisasmRdRs(f, "fmov", sIntReg, Opcode); break;


    case OP_FSEQ: DisasmFscc(f, "fseq", Opcode); break;
    case OP_FSNE: DisasmFscc(f, "fsne", Opcode); break;
    case OP_FSLT: DisasmFscc(f, "fslt", Opcode); break;
    case OP_FSGT: DisasmFscc(f, "fsgt", Opcode); break;

    case OP_DWORD:
    {
    } break;
    }
    return Addr + 1;
}


