

#include <inttypes.h>

#include "Common.h"

#include "PVM/_PVM.h"
#include "PVM/_Disassembler.h"
#include "PVM/Debugger.h"


void PVMDebugPause(const PascalVM *PVM, const PVMChunk *Chunk, 
        const U16 *IP, const PVMPTR SP, const PVMPTR FP
)
{
    fprintf(stderr, "\n ==================== Debugger ==================== \n");

    fprintf(stderr, 
            "IP: [%p]: \n",
            (const void*)IP
    );
    PVMDisasmSingleInstruction(stderr, Chunk, IP - Chunk->Code);

    fprintf(stderr, 
            "SP: [%p]: %08"PRIx64"\n"
            "FP: [%p]\n",
            SP.Raw, SP.UInt,
            FP.Raw
    );

    PVMDumpState(stderr, PVM, 6);
    fprintf(stderr, "Press Enter to continue.\n");

    char Dummy[4];
    fgets(Dummy, sizeof Dummy, stdin);
}

