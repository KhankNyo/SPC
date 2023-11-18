#ifndef PVM_DEBUGGER_H
#define PVM_DEBUGGER_H


#include "PVM.h"
void PVMDebugPause(const PascalVM *PVM, const CodeChunk *Chunk, 
        const PVMWord *IP, const PVMPtr *SP, const PVMPtr *FP
);


#endif /* PVM_DEBUGGER_H */

