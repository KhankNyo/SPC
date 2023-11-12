
#include <string.h>

#include "Common.h"
#include "Pascal.h"
#include "Memory.h"
#include "Tokenizer.h"
#include "Parser.h"

#include "PVM/Disassembler.h"
#include "PVM/PVM.h"
#include "PVM/CodeChunk.h"
#include "PVM/Codegen.h"


#define MB 1024*1024



int PascalMain(int argc, const U8 *const *argv)
{
    if (argc <= 1)
    {
        return PascalRepl();
    }
    if (argc < 3)
    {
        const U8 *name = argc == 0 
            ? (const U8*)"Pascal"
            : argv[0];

        PascalPrintUsage(stderr, name);
        return PASCAL_EXIT_FAILURE;
    }

    const U8 *InFileName = argv[1];
    const U8 *OutFileName = argv[2];
    return PascalRunFile(InFileName, OutFileName);
}



void PascalPrintUsage(FILE *f, const U8 *ProgramName)
{
    fprintf(f, "Usage: %s InputName.pas OutPutName\n",
            ProgramName
    );
}






