#ifndef PASCAL_VM_DISASSEMBLER_H
#define PASCAL_VM_DISASSEMBLER_H



#include "PVM.h"


void PVMDisasm(FILE *f, const CodeChunk *Chunk, const char *ChunkName);
void PVMDisasmInstruction(FILE *f, PVMWord Addr, PVMWord Opcode);


#endif /* PASCAL_VM_DISASSEMBLER_H */

