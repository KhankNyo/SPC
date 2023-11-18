

#include <inttypes.h>

#include "Common.h"

#include "PVM/PVM.h"
#include "PVM/Debugger.h"
#include "PVM/Disassembler.h"


void PVMDebugPause(const PascalVM *PVM, const CodeChunk *Chunk, 
        const PVMWord *IP, const PVMPtr *SP, const PVMPtr *FP)
{
    fprintf(stderr, "\n ==================== Debugger ==================== \n");

    fprintf(stderr, 
            "IP: [%p]: ",
            (const void*)IP
    );
    PVMDisasmInstruction(stderr, IP - Chunk->Code, *IP);

    fprintf(stderr, 
            "SP: [%p]: %08"PRIx64"\n"
            "FP: [%p]\n",
            (const void*)SP, *SP,
            (const void*)FP
    );

    PVMDumpState(stderr, PVM, 6);
    fprintf(stderr, "Press Enter to continue.\n");

    char Dummy[4];
    fgets(Dummy, sizeof Dummy, stdin);
}

