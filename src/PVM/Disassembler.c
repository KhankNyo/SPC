
#include <stdarg.h>
#include "PVM/Isa.h"
#include "PVM/Disassembler.h"

static const char *sIntReg[PVM_REG_COUNT] = {
    "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", 
    "r8", "r9", "r10", "r11", "r12", 
    [PVM_REG_GP] = "rgp", 
    [PVM_REG_FP] = "rfp",
    [PVM_REG_SP] = "rsp", 
};

static const char *sFltReg[PVM_FREG_COUNT] = {
    "f0", "f1", "f2", "f3", "f4", "f5", "f6", "f7", 
    "f8", "f9", "f10", "f11", "f12", "f13", "f14", "f15",
};

static const UInt sBytesPerLine = 4;
static const UInt sAddrPad = 9;
static const UInt sMnemonicPad = 13;
static const UInt sCommentPad = 20;

typedef struct ImmediateInfo
{
    U32 Addr;
    UInt SexIndex, Count;
    U64 Imm;
} ImmediateInfo;


void PVMDisasm(FILE *f, const PVMChunk *Chunk, const char *Name)
{
    const char *Fmt = "=====================";
    fprintf(f, "%s %s %s\n", Fmt, Name, Fmt);

    U32 i = 0;
    const LineDebugInfo *Last = NULL;
    while (i < Chunk->Count)
    {
        if (i == Chunk->EntryPoint)
        {
            fprintf(f, "\n %s Entry Point %s\n", Fmt, Fmt);
        }

        const LineDebugInfo *Info = ChunkGetConstDebugInfo(Chunk, i);
        if (NULL != Info && Last != Info)
        {
            fputc('\n', f);
            Last = Info;
            if (Info->IsSubroutine)
            {
                fprintf(f, "\n %s Subroutine %s\n", Fmt, Fmt);
            }

            for (UInt k = 0; k < Info->Count; k++)
            {
                fprintf(f, "[line %d]: \"%.*s\"\n", 
                        Info->Line[k], Info->SrcLen[k], Info->Src[k]
                );
            }
        }

        i = PVMDisasmSingleInstruction(f, Chunk, i);
    }
}



static void PrintPaddedMnemonic(FILE *f, int Pad, const char *Mnemonic)
{
    fprintf(f, "%*s   %-*s", 3*sBytesPerLine - Pad, "", sMnemonicPad, Mnemonic);
}

static void PrintComment(FILE *f, int Pad, const char *Fmt, ...)
{
    va_list Args;
    va_start(Args, Fmt);
    fprintf(f, "%*s ", sCommentPad - Pad, "#");
    vfprintf(f, Fmt, Args);
    va_end(Args);
}


static void DisasmRdRs(FILE *f, const char *Mnemonic, const char *RegSet[], U16 Opcode)
{
    const char *Rd = RegSet[PVM_GET_RD(Opcode)];
    const char *Rs = RegSet[PVM_GET_RS(Opcode)];

    int Pad = fprintf(f, "%02x %02x", Opcode >> 8, Opcode & 0xFF);
    PrintPaddedMnemonic(f, Pad, Mnemonic);
    fprintf(f, "%s, %s\n", Rd, Rs);
}

static void DisasmInter(FILE *f, const char *Mnemonic, const char **RdSet, const char **RsSet, U16 Opcode)
{
    const char *Rd = RdSet[PVM_GET_RD(Opcode)];
    const char *Rs = RsSet[PVM_GET_RS(Opcode)];

    int Pad = fprintf(f, "%02x %02x", Opcode >> 8, Opcode & 0xFF);
    PrintPaddedMnemonic(f, Pad, Mnemonic);
    fprintf(f, "%s, %s\n", Rd, Rs);
}

static void DisasmFscc(FILE *f, const char *Mnemonic, U16 Opcode)
{
    const char *Fd = sFltReg[PVM_GET_RD(Opcode)];
    const char *Fs = sFltReg[PVM_GET_RS(Opcode)];

    int Pad = fprintf(f, "%02x %02x", Opcode >> 8, Opcode & 0xFF);
    PrintPaddedMnemonic(f, Pad, Mnemonic);
    fprintf(f, "%s, %s\n", Fd, Fs);
}

static U32 DisasmBcc(FILE *f, const char *Mnemonic, U16 Opcode, const PVMChunk *Chunk, U32 Addr)
{
    const char *Rd = sIntReg[PVM_GET_RD(Opcode)];
    I32 BrOffset = BitSex32Safe(
            ((U32)(Opcode & 0xF) << 16) | (U32)(Chunk->Code[Addr + 1]), 
            PVM_BCC_OFFSET_SIZE - 1
    );

    int Pad = fprintf(f, "%02x %02x %02x %02x", 
            Opcode >> 8, Opcode & 0xFF, 
            Chunk->Code[Addr + 1] >> 8, Chunk->Code[Addr + 1] & 0xFF
    );
    Addr += 2;
    PrintPaddedMnemonic(f, Pad, Mnemonic);
    fprintf(f, "%s, [%u]\n", Rd, Addr + BrOffset);
    return Addr;
}


static U32 DisasmBr(FILE *f, const char *Mnemonic, U16 Opcode, const PVMChunk *Chunk, U32 Addr)
{
    I32 BrOffset = BitSex32Safe(
            (((U32)Opcode & 0xFF) << 16) | (U32)(Chunk->Code[Addr + 1]), 
            PVM_BR_OFFSET_SIZE - 1
    );

    int Pad = fprintf(f, "%02x %02x %02x %02x", 
            Opcode >> 8, Opcode & 0xFF, Chunk->Code[Addr + 1] >> 8, Chunk->Code[Addr + 1] & 0xFF
    );

    Addr += 2;
    U32 SubroutineLocation = Addr + BrOffset;
    const LineDebugInfo *SubroutineInfo = ChunkGetConstDebugInfo(Chunk, SubroutineLocation);

    PrintPaddedMnemonic(f, Pad, Mnemonic);
    Pad = fprintf(f, "[%u]", Addr + BrOffset);
    if (NULL != SubroutineInfo)
    {
        PrintComment(f, Pad, "line %d: '%.*s'\n", 
                SubroutineInfo->Line[0], SubroutineInfo->SrcLen[0], SubroutineInfo->Src[0]
        );
    }
    else 
    {
        fputc('\n', f);
    }
    return Addr;
}


static U32 DisasmBri(FILE *f, const char *Mnemonic, U16 Opcode, const PVMChunk *Chunk, U32 Addr)
{
    I32 BrOffset = (I16)Chunk->Code[Addr + 1];

    int Pad = fprintf(f, "%02x %02x %02x %02x",
            Opcode >> 8, Opcode & 0xFF, Chunk->Code[Addr + 1] >> 8, Chunk->Code[Addr + 1] & 0xFF
    );

    Addr += 2;
    U32 Location = Addr + BrOffset;
    const LineDebugInfo *Info = ChunkGetConstDebugInfo(Chunk, Location);

    PrintPaddedMnemonic(f, Pad, Mnemonic);
    Pad = fprintf(f, "%s, [%u]", sIntReg[PVM_GET_RD(Opcode)], Location);
    if (NULL != Info)
    {
        PrintComment(f, Pad, "line %d: '%.*s'\n", Info->Line[0], Info->SrcLen[0], Info->Src[0]);
    }
    else
    {
        fputc('\n', f);
    }
    return Addr;
}


static void DisasmMnemonic(FILE *f, const char *Mnemonic, U16 Opcode)
{
    int Pad = fprintf(f, "%02x %02x", Opcode >> 8, Opcode & 0xFF);
    PrintPaddedMnemonic(f, Pad, Mnemonic);
    fputc('\n', f);
}

static U32 DisasmSysOp(FILE *f, const PVMChunk *Chunk, U32 Addr, U16 Opcode)
{
    (void)Chunk;
    switch (PVM_GET_SYS_OP(Opcode))
    {
    case OP_SYS_EXIT:
    {
        DisasmMnemonic(f, "exit", Opcode);
    } break;
    case OP_SYS_WRITE:
    {
        DisasmMnemonic(f, "write", Opcode);
    } break;
    }
    return Addr + 1;
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
    PrintPaddedMnemonic(f, Pad, Mnemonic);
    fprintf(f, "{ %s }\n", RegisterListStr);
}

static ImmediateInfo GetImmFromImmType(const PVMChunk *Chunk, U32 Addr, PVMImmType ImmType)
{
    ImmediateInfo Info = {
        .Count = 1,
        .SexIndex = 0,
    };
    switch (ImmType)
    {
    default:
    case IMMTYPE_I16: Info.SexIndex = 15; FALLTHROUGH;
    case IMMTYPE_U16: break;
    case IMMTYPE_I32: Info.SexIndex = 31; FALLTHROUGH;
    case IMMTYPE_U32: Info.Count = 2; break;
    case IMMTYPE_I48: Info.SexIndex = 47; FALLTHROUGH;
    case IMMTYPE_U48: Info.Count = 3; break;
    case IMMTYPE_U64: Info.Count = 4; break;
    }

    /* get imm */
    Info.Imm = Chunk->Code[Addr++];
    for (UInt i = 1; i < Info.Count; i++)
    {
        Info.Imm |= (U64)Chunk->Code[Addr++] << i * 16;
    }
    if (Info.SexIndex)
        Info.Imm = BitSex64(Info.Imm, Info.SexIndex);
    Info.Addr = Addr;
    return Info;
}

static void DisplayImmBytes(FILE *f, const ImmediateInfo Info)
{
    /* display hex dump of immediate as how it is represented in memory */
    for (UInt i = 0; i < Info.Count; i++)
    {
        if (i % 2 == 0)
        {
            fprintf(f, "\n%*s  ", sAddrPad, "");
        }
        UInt BitIndex = 16*(Info.Count - i - 1);
        U8 High = Info.Imm >> (BitIndex + 8);
        U8 Lo = (Info.Imm >> BitIndex) & 0xFF;
        fprintf(f, "%02x %02x ", High, Lo);
    }
    fputc('\n', f);
}


static U32 DisasmRdImm(FILE *f, const char *Mnemonic, const PVMChunk *Chunk, U32 Addr, U16 Opcode)
{
    ImmediateInfo Info = GetImmFromImmType(Chunk, Addr + 1, PVM_GET_IMMTYPE(Opcode));
    char LongMnemonic[64] = { 0 };
    snprintf(LongMnemonic, sizeof LongMnemonic, "%s%s%d", Mnemonic, 
            Info.SexIndex ? "sex64_" : "zex64_", Info.Count*16
    );

    /* display instruction */
    const char *Rd = sIntReg[PVM_GET_RD(Opcode)];
    int Pad = fprintf(f, "%02x %02x", Opcode >> 8, Opcode & 0xFF);

    PrintPaddedMnemonic(f, Pad, LongMnemonic);
    Pad = fprintf(f, "%s, 0x%llx", Rd, Info.Imm);
    PrintComment(f, Pad, "decimal %lld", Info.Imm);
    DisplayImmBytes(f, Info);
    return Info.Addr;
}

static U32 DisasmMem(FILE *f, 
        const char *Mnemonic, const char **RegSet, 
        U16 Opcode, PVMImmType ImmType, const PVMChunk *Chunk, U32 Addr)
{
    ImmediateInfo Info = GetImmFromImmType(Chunk, Addr + 1, ImmType);

    const char *Rd = RegSet[PVM_GET_RD(Opcode)];
    const char *Rs = sIntReg[PVM_GET_RS(Opcode)];
    int Pad = fprintf(f, "%02x %02x", Opcode >> 8, Opcode & 0xFF);
    PrintPaddedMnemonic(f, Pad, Mnemonic);
    fprintf(f, "%s, [%s + %lld]", Rd, Rs, (I64)Info.Imm);

    DisplayImmBytes(f, Info);
    return Info.Addr;
}

static void DisasmRdSmallImm(FILE *f, const char *Mnemonic, U16 Opcode, bool IsSigned)
{
    const char *Rd = sIntReg[PVM_GET_RD(Opcode)];
    int Pad = fprintf(f, "%02x %02x", Opcode >> 8, Opcode & 0xFF);
    PrintPaddedMnemonic(f, Pad, Mnemonic);
    if (IsSigned)
    {
        fprintf(f, "%s, %d\n", Rd, BIT_SEX32(PVM_GET_RS(Opcode), 3));
    }
    else 
    {
        fprintf(f, "%s, %d\n", Rd, PVM_GET_RS(Opcode));
    }
}

static void DisasmSingleOperand(FILE *f, const char *Mnemonic, U16 Opcode)
{
    const char *Rd = sIntReg[PVM_GET_RD(Opcode)];
    int Pad = fprintf(f, "%02x %02x", Opcode >> 8, Opcode & 0xFF);
    PrintPaddedMnemonic(f, Pad, Mnemonic);
    fprintf(f, "%s\n", Rd);
}




U32 PVMDisasmSingleInstruction(FILE *f, const PVMChunk *Chunk, U32 Addr)
{
    U16 Opcode = Chunk->Code[Addr];
    fprintf(f, "%*u: ", sAddrPad, Addr);

    switch (PVM_GET_OP(Opcode))
    {
    case OP_SYS:
    {
        return DisasmSysOp(f, Chunk, Addr, Opcode);
    } break;
    case OP_SADD: DisasmRdRs(f, "sadd", sIntReg, Opcode); break;
    case OP_ADD: DisasmRdRs(f, "add", sIntReg, Opcode); break;
    case OP_SUB: DisasmRdRs(f, "sub", sIntReg, Opcode); break;
    case OP_MUL: DisasmRdRs(f, "mul", sIntReg, Opcode); break;
    case OP_IMUL: DisasmRdRs(f, "imul", sIntReg, Opcode); break;
    case OP_DIV: DisasmRdRs(f, "div", sIntReg, Opcode); break;
    case OP_IDIV: DisasmRdRs(f, "idiv", sIntReg, Opcode); break;
    case OP_MOD: DisasmRdRs(f, "mod", sIntReg, Opcode); break;
    case OP_NEG: DisasmRdRs(f, "neg", sIntReg, Opcode); break;
    case OP_NOT: DisasmRdRs(f, "not", sIntReg, Opcode); break;
    case OP_AND: DisasmRdRs(f, "and", sIntReg, Opcode); break;
    case OP_OR:  DisasmRdRs(f, "or", sIntReg, Opcode); break;
    case OP_XOR: DisasmRdRs(f, "xor", sIntReg, Opcode); break;
    case OP_VSHL: DisasmRdRs(f, "vshl", sIntReg, Opcode); break;
    case OP_VSHR: DisasmRdRs(f, "vshr", sIntReg, Opcode); break;
    case OP_VASR: DisasmRdRs(f, "vasr", sIntReg, Opcode); break;
    case OP_QSHL: DisasmRdSmallImm(f, "qshl", Opcode, false); break;
    case OP_QSHR: DisasmRdSmallImm(f, "qshr", Opcode, false); break;
    case OP_QASR: DisasmRdSmallImm(f, "qasr", Opcode, false); break;

    case OP_ADDI: return DisasmRdImm(f, "addi", Chunk, Addr, Opcode);
    case OP_ADDQI: DisasmRdSmallImm(f, "addqi", Opcode, true); break;


    case OP_BEZ: return DisasmBcc(f, "bez", Opcode, Chunk, Addr);
    case OP_BNZ: return DisasmBcc(f, "bnz", Opcode, Chunk, Addr);
    case OP_BR: return DisasmBr(f, "br", Opcode, Chunk, Addr);
    case OP_BSR: return DisasmBr(f, "bsr", Opcode, Chunk, Addr);
    case OP_BCT: return DisasmBr(f, "bct", Opcode, Chunk, Addr);
    case OP_BCF: return DisasmBr(f, "bcf", Opcode, Chunk, Addr);
    case OP_BRI: return DisasmBri(f, "bri", Opcode, Chunk, Addr);


    case OP_STRLT: DisasmRdRs(f, "strlt", sIntReg, Opcode); break;
    case OP_STRGT: DisasmRdRs(f, "strgt", sIntReg, Opcode); break;
    case OP_STREQU:DisasmRdRs(f, "strequ", sIntReg, Opcode); break;
    case OP_SCPY:  DisasmRdRs(f, "scpy", sIntReg, Opcode); break;
    case OP_SEQ: DisasmRdRs(f, "seq", sIntReg, Opcode); break;
    case OP_SNE: DisasmRdRs(f, "sne", sIntReg, Opcode); break;
    case OP_SLT: DisasmRdRs(f, "slt", sIntReg, Opcode); break;
    case OP_SGT: DisasmRdRs(f, "sgt", sIntReg, Opcode); break;
    case OP_ISLT: DisasmRdRs(f, "islt", sIntReg, Opcode); break;
    case OP_ISGT: DisasmRdRs(f, "isgt", sIntReg, Opcode); break;
    case OP_SLE: DisasmRdRs(f, "sle", sIntReg, Opcode); break;
    case OP_SGE: DisasmRdRs(f, "sge", sIntReg, Opcode); break;
    case OP_ISLE: DisasmRdRs(f, "isle", sIntReg, Opcode); break;
    case OP_ISGE: DisasmRdRs(f, "isge", sIntReg, Opcode); break;
    case OP_SETEZ: DisasmRdRs(f, "setez", sIntReg, Opcode); break;


    case OP_PSHL: DisasmReglist(f, "pshl", Opcode, false); break;
    case OP_PSHH: DisasmReglist(f, "pshh", Opcode, true); break;
    case OP_POPL: DisasmReglist(f, "popl", Opcode, false); break;
    case OP_POPH: DisasmReglist(f, "pshh", Opcode, true); break;
    case OP_FPSHL: DisasmReglist(f, "fpshl", Opcode, false); break;
    case OP_FPSHH: DisasmReglist(f, "fpshh", Opcode, true); break;
    case OP_FPOPL: DisasmReglist(f, "fpopl", Opcode, false); break;
    case OP_FPOPH: DisasmReglist(f, "fpshh", Opcode, true); break;


    case OP_LD32: return DisasmMem(f, "ld32", sIntReg, Opcode, IMMTYPE_I16, Chunk, Addr);
    case OP_LD64: return DisasmMem(f, "ld64", sIntReg, Opcode, IMMTYPE_I16, Chunk, Addr);
    case OP_LDZEX32_8: return DisasmMem(f, "ldzex32_8", sIntReg, Opcode, IMMTYPE_I16, Chunk, Addr);
    case OP_LDZEX32_16: return DisasmMem(f, "ldzex32_16", sIntReg, Opcode, IMMTYPE_I16, Chunk, Addr);
    case OP_LDZEX64_8: return DisasmMem(f, "ldzex64_8", sIntReg, Opcode, IMMTYPE_I16, Chunk, Addr);
    case OP_LDZEX64_16: return DisasmMem(f, "ldzex64_16", sIntReg, Opcode, IMMTYPE_I16, Chunk, Addr);
    case OP_LDZEX64_32: return DisasmMem(f, "ldzex64_32", sIntReg, Opcode, IMMTYPE_I16, Chunk, Addr);
    case OP_LDSEX32_8: return DisasmMem(f, "ldsex32_8", sIntReg, Opcode, IMMTYPE_I16, Chunk, Addr);
    case OP_LDSEX32_16: return DisasmMem(f, "ldsex32_16", sIntReg, Opcode, IMMTYPE_I16, Chunk, Addr);
    case OP_LDSEX64_8: return DisasmMem(f, "ldsex64_8", sIntReg, Opcode, IMMTYPE_I16, Chunk, Addr);
    case OP_LDSEX64_16: return DisasmMem(f, "ldsex64_16", sIntReg, Opcode, IMMTYPE_I16, Chunk, Addr);
    case OP_LDSEX64_32: return DisasmMem(f, "ldsex64_32", sIntReg, Opcode, IMMTYPE_I16, Chunk, Addr);

    case OP_LD32L: return DisasmMem(f, "ld32l", sIntReg, Opcode, IMMTYPE_I32, Chunk, Addr);
    case OP_LD64L: return DisasmMem(f, "ld64l", sIntReg, Opcode, IMMTYPE_I32, Chunk, Addr);
    case OP_LDZEX32_8L: return DisasmMem(f, "ldzex32_8l", sIntReg, Opcode, IMMTYPE_I32, Chunk, Addr);
    case OP_LDZEX32_16L: return DisasmMem(f, "ldzex32_16l", sIntReg, Opcode, IMMTYPE_I32, Chunk, Addr);
    case OP_LDZEX64_8L: return DisasmMem(f, "ldzex64_8l", sIntReg, Opcode, IMMTYPE_I32, Chunk, Addr);
    case OP_LDZEX64_16L: return DisasmMem(f, "ldzex64_16l", sIntReg, Opcode, IMMTYPE_I32, Chunk, Addr);
    case OP_LDZEX64_32L: return DisasmMem(f, "ldzex64_32l", sIntReg, Opcode, IMMTYPE_I32, Chunk, Addr);
    case OP_LDSEX32_8L: return DisasmMem(f, "ldsex32_8l", sIntReg, Opcode, IMMTYPE_I32, Chunk, Addr);
    case OP_LDSEX32_16L: return DisasmMem(f, "ldsex32_16l", sIntReg, Opcode, IMMTYPE_I32, Chunk, Addr);
    case OP_LDSEX64_8L: return DisasmMem(f, "ldsex64_8l", sIntReg, Opcode, IMMTYPE_I32, Chunk, Addr);
    case OP_LDSEX64_16L: return DisasmMem(f, "ldsex64_16l", sIntReg, Opcode, IMMTYPE_I32, Chunk, Addr);
    case OP_LDSEX64_32L: return DisasmMem(f, "ldsex64_32l", sIntReg, Opcode, IMMTYPE_I32, Chunk, Addr);

    case OP_LEA: return DisasmMem(f, "lea", sIntReg, Opcode, IMMTYPE_I16, Chunk, Addr);
    case OP_LEAL: return DisasmMem(f, "leal", sIntReg, Opcode, IMMTYPE_I32, Chunk, Addr);


    case OP_ST64: return DisasmMem(f, "st64", sIntReg, Opcode, IMMTYPE_I16, Chunk, Addr);
    case OP_ST32: return DisasmMem(f, "st32", sIntReg, Opcode, IMMTYPE_I16, Chunk, Addr);
    case OP_ST16: return DisasmMem(f, "st16", sIntReg, Opcode, IMMTYPE_I16, Chunk, Addr);
    case OP_ST8: return DisasmMem(f, "st8", sIntReg, Opcode, IMMTYPE_I16, Chunk, Addr);
    case OP_ST64L: return DisasmMem(f, "st64l", sIntReg, Opcode, IMMTYPE_I32, Chunk, Addr);
    case OP_ST32L: return DisasmMem(f, "st32l", sIntReg, Opcode, IMMTYPE_I32, Chunk, Addr);
    case OP_ST16L: return DisasmMem(f, "st16l", sIntReg, Opcode, IMMTYPE_I32, Chunk, Addr);
    case OP_ST8L: return DisasmMem(f, "st8l", sIntReg, Opcode, IMMTYPE_I32, Chunk, Addr);


    case OP_LDF32: return DisasmMem(f, "ldf32", sFltReg, Opcode, IMMTYPE_I16, Chunk, Addr);
    case OP_STF32: return DisasmMem(f, "stf32", sFltReg, Opcode, IMMTYPE_I16, Chunk, Addr);
    case OP_LDF32L: return DisasmMem(f, "ldf32l", sFltReg, Opcode, IMMTYPE_I32, Chunk, Addr);
    case OP_STF32L: return DisasmMem(f, "stf32l", sFltReg, Opcode, IMMTYPE_I32, Chunk, Addr);
 
    case OP_LDF64: return DisasmMem(f, "ldf64", sFltReg, Opcode, IMMTYPE_I16, Chunk, Addr);
    case OP_STF64: return DisasmMem(f, "stf64", sFltReg, Opcode, IMMTYPE_I16, Chunk, Addr);
    case OP_LDF64L: return DisasmMem(f, "ldf64l", sFltReg, Opcode, IMMTYPE_I32, Chunk, Addr);
    case OP_STF64L: return DisasmMem(f, "stf64l", sFltReg, Opcode, IMMTYPE_I32, Chunk, Addr);
       


    case OP_FADD: DisasmRdRs(f, "fadd", sFltReg, Opcode); break;
    case OP_FSUB: DisasmRdRs(f, "fsub", sFltReg, Opcode); break;
    case OP_FDIV: DisasmRdRs(f, "fdiv", sFltReg, Opcode); break;
    case OP_FMUL: DisasmRdRs(f, "fmul", sFltReg, Opcode); break;
    case OP_FNEG: DisasmRdRs(f, "fneg", sFltReg, Opcode); break;
    case OP_FSEQ: DisasmFscc(f, "fseq", Opcode); break;
    case OP_FSNE: DisasmFscc(f, "fsne", Opcode); break;
    case OP_FSLT: DisasmFscc(f, "fslt", Opcode); break;
    case OP_FSGT: DisasmFscc(f, "fsgt", Opcode); break;
    case OP_FSLE: DisasmFscc(f, "fsle", Opcode); break;
    case OP_FSGE: DisasmFscc(f, "fsge", Opcode); break;

    case OP_FADD64: DisasmRdRs(f, "fadd64", sFltReg, Opcode); break;
    case OP_FSUB64: DisasmRdRs(f, "fsub64", sFltReg, Opcode); break;
    case OP_FDIV64: DisasmRdRs(f, "fdiv64", sFltReg, Opcode); break;
    case OP_FMUL64: DisasmRdRs(f, "fmul64", sFltReg, Opcode); break;
    case OP_FNEG64: DisasmRdRs(f, "fneg64", sFltReg, Opcode); break;
    case OP_FSEQ64: DisasmFscc(f, "fseq64", Opcode); break;
    case OP_FSNE64: DisasmFscc(f, "fsne64", Opcode); break;
    case OP_FSLT64: DisasmFscc(f, "fslt64", Opcode); break;
    case OP_FSGT64: DisasmFscc(f, "fsgt64", Opcode); break;
    case OP_FSLE64: DisasmFscc(f, "fsle64", Opcode); break;
    case OP_FSGE64: DisasmFscc(f, "fsge64", Opcode); break;

    case OP_GETFLAG: DisasmSingleOperand(f, "getfcc", Opcode); break;



    case OP_MOV32: DisasmRdRs(f, "mov32", sIntReg, Opcode); break;
    case OP_MOVZEX32_8: DisasmRdRs(f, "movzex32_8", sIntReg, Opcode); break;
    case OP_MOVZEX32_16: DisasmRdRs(f, "movzex32_16", sIntReg, Opcode); break;
    case OP_MOV64: DisasmRdRs(f, "mov64", sIntReg, Opcode); break;
    case OP_MOVZEX64_8: DisasmRdRs(f, "movzex64_8", sIntReg, Opcode); break;
    case OP_MOVZEX64_16: DisasmRdRs(f, "movzex64_16", sIntReg, Opcode); break;
    case OP_MOVZEX64_32: DisasmRdRs(f, "movzex64_32", sIntReg, Opcode); break;
    case OP_MOVSEX64_32: DisasmRdRs(f, "movsex64_32", sIntReg, Opcode); break;

    case OP_FMOV: DisasmRdRs(f, "fmov", sFltReg, Opcode); break;
    case OP_FMOV64: DisasmRdRs(f, "fmov64", sFltReg, Opcode); break;
    case OP_MOVI: return DisasmRdImm(f, "movi", Chunk, Addr, Opcode);
    case OP_MOVQI: DisasmRdSmallImm(f, "movqi", Opcode, true); break;

    case OP_F64TOF32: DisasmRdRs(f, "f64tof32", sFltReg, Opcode); break;
    case OP_F32TOF64: DisasmRdRs(f, "f32tof64", sFltReg, Opcode); break;
    case OP_F64TOI64: DisasmInter(f, "f64toi64", sIntReg, sFltReg, Opcode); break;
    case OP_I64TOF32: DisasmInter(f, "i64tof32", sFltReg, sIntReg, Opcode); break;
    case OP_I64TOF64: DisasmInter(f, "i64tof64", sFltReg, sIntReg, Opcode); break;
    case OP_U64TOF64: DisasmInter(f, "u64tof64", sFltReg, sIntReg, Opcode); break;
    case OP_U64TOF32: DisasmInter(f, "u64tof32", sFltReg, sIntReg, Opcode); break;
    case OP_I32TOF32: DisasmInter(f, "i32tof32", sFltReg, sIntReg, Opcode); break;
    case OP_I32TOF64: DisasmInter(f, "i32tof64", sFltReg, sIntReg, Opcode); break;
    case OP_U32TOF64: DisasmInter(f, "u32tof64", sFltReg, sIntReg, Opcode); break;
    case OP_U32TOF32: DisasmInter(f, "u32tof32", sFltReg, sIntReg, Opcode); break;


    case OP_ADD64:  DisasmRdRs(f, "add64", sIntReg, Opcode); break;
    case OP_SUB64:  DisasmRdRs(f, "sub64", sIntReg, Opcode); break;
    case OP_MUL64:  DisasmRdRs(f, "mul64", sIntReg, Opcode); break;
    case OP_IMUL64: DisasmRdRs(f, "imul64", sIntReg, Opcode); break;
    case OP_DIV64:  DisasmRdRs(f, "div64", sIntReg, Opcode); break;
    case OP_IDIV64: DisasmRdRs(f, "idiv64", sIntReg, Opcode); break;
    case OP_MOD64:  DisasmRdRs(f, "mod64", sIntReg, Opcode); break;
    case OP_NEG64:  DisasmRdRs(f, "neg64", sIntReg, Opcode); break;
    case OP_NOT64:  DisasmRdRs(f, "not64", sIntReg, Opcode); break;
    case OP_AND64:  DisasmRdRs(f, "and64", sIntReg, Opcode); break;
    case OP_OR64:   DisasmRdRs(f, "or64", sIntReg, Opcode); break;
    case OP_XOR64:  DisasmRdRs(f, "xor64", sIntReg, Opcode); break;
    case OP_VSHL64: DisasmRdRs(f, "vshl64", sIntReg, Opcode); break;
    case OP_VSHR64: DisasmRdRs(f, "vshr64", sIntReg, Opcode); break;
    case OP_VASR64: DisasmRdRs(f, "vasr64", sIntReg, Opcode); break;
    case OP_QSHL64: DisasmRdSmallImm(f, "qshl", Opcode, false); break;
    case OP_QSHR64: DisasmRdSmallImm(f, "qshr", Opcode, false); break;
    case OP_QASR64: DisasmRdSmallImm(f, "qasr", Opcode, false); break;

    case OP_SEQ64: DisasmRdRs(f, "seq64", sIntReg, Opcode); break;
    case OP_SNE64: DisasmRdRs(f, "sne64", sIntReg, Opcode); break;
    case OP_SLT64: DisasmRdRs(f, "slt64", sIntReg, Opcode); break;
    case OP_SGT64: DisasmRdRs(f, "sgt64", sIntReg, Opcode); break;
    case OP_ISLT64: DisasmRdRs(f, "islt64", sIntReg, Opcode); break;
    case OP_ISGT64: DisasmRdRs(f, "isgt64", sIntReg, Opcode); break;
    case OP_SLE64: DisasmRdRs(f, "sle64", sIntReg, Opcode); break;
    case OP_SGE64: DisasmRdRs(f, "sge64", sIntReg, Opcode); break;
    case OP_ISLE64: DisasmRdRs(f, "isle64", sIntReg, Opcode); break;
    case OP_ISGE64: DisasmRdRs(f, "isge64", sIntReg, Opcode); break;
    case OP_SETEZ64: DisasmRdRs(f, "setez64", sIntReg, Opcode); break;
    }
    return Addr + 1;
}


