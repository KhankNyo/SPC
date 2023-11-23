#ifndef PASCAL_PVM2_DISASSEMBLER_H
#define PASCAL_PVM2_DISASSEMBLER_H


#include "Common.h"
#include "PVM/_Chunk.h"

void PVMDisasm(FILE *f, const PVMChunk *Code, const char *Name);
U32 PVMDisasmSingleInstruction(FILE *f, const PVMChunk *Code, U32 Addr);


#endif /* PASCAL_PVM2_DISASSEMBLER_H */

