#ifndef PVM_DEBUGGER_H
#define PVM_DEBUGGER_H


#include "PVM.h"

void PVMDebugPause(const PascalVM *PVM, const PVMChunk *Chunk, 
        const U16 *IP, const PVMPTR SP, const PVMPTR FP
);


#endif /* PVM_DEBUGGER_H */

