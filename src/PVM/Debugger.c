

#include <inttypes.h>

#include "Common.h"

#include "PVM/PVM.h"
#include "PVM/Disassembler.h"
#include "PVM/Debugger.h"


void PVMDebugPause(const PascalVM *PVM, const PVMChunk *Chunk, const U16 *IP)
{
    fprintf(PVM->LogFile, "\n ==================== Debugger ==================== \n");

    fprintf(PVM->LogFile, 
            "IP: [%p]: \n",
            (const void*)IP
    );
    PVMDisasmSingleInstruction(PVM->LogFile, Chunk, IP - Chunk->Code);

    PVMDumpState(PVM->LogFile, PVM, 6);
    fprintf(PVM->LogFile, "Press Enter to continue.\n");

    char Dummy[4];
    fgets(Dummy, sizeof Dummy, stdin);
}

