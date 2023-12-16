

#include <string.h>
#include <time.h>

#include "Common.h"

#include "Pascal.h"
#include "Memory.h"

#include "PVM/PVM.h"
#include "Compiler/Compiler.h"


static bool GetCommandLine(PascalVM *PVM, char *Buf, USize Bufsz);

#define STREQU(Buffer, Literal) (0 == strncmp(Buffer, Literal, sizeof(Literal) - 1))

int PascalRepl(void)
{
    MemInit(1024*1024);
    PascalArena Program = ArenaInit(1024*1024, 4);
    PascalGPA Permanent = GPAInit(4 * 1024 * 1024);

    PascalVM PVM = PVMInit(1024, 128);
    PVMChunk Chunk = ChunkInit(1024);
    PascalVartab Identifiers = VartabPredefinedIdentifiers(MemGetAllocator(), 1024);
    
    PascalCompileFlags Flags = { 
        .CompMode = COMPMODE_REPL,
    };

    int RetVal = PASCAL_EXIT_SUCCESS;
    static char Tmp[1024] = { 0 };
    while (GetCommandLine(&PVM, Tmp, sizeof Tmp))
    {
        USize SourceLen = strlen(Tmp);
        U8 *CurrentSource = ArenaAllocate(&Program, SourceLen + 1);
        memcpy(CurrentSource, Tmp, SourceLen);
        CurrentSource[SourceLen] = '\0';

        if (PascalCompile(CurrentSource, Flags, &Identifiers, &Permanent, stderr, &Chunk))
        {
            PVMRun(&PVM, &Chunk);
        }
        ChunkReset(&Chunk, true);
    }


    GPADeinit(&Permanent);
    ArenaDeinit(&Program);
    MemDeinit();
    return RetVal;
}


static bool GetCommandLine(PascalVM *PVM, char *Buf, USize Bufsz)
{
    FILE *Out = stdout, *In = stdin;
    do {
        printf("\n>> ");
        if (NULL == fgets(Buf, Bufsz, In))
            return false;

        if ('.' == Buf[0])
        {
            Buf[strlen(Buf) - 1] = '\0'; /* remove pesky newline character */

            if (STREQU(&Buf[1], "Debug"))
            {
                if (PVM->SingleStepMode)
                    fprintf(Out, "Disabled debug mode.\n");
                else 
                    fprintf(Out, "Enabled debug mode.\n");
                PVM->SingleStepMode = !PVM->SingleStepMode;
            }
            else if (STREQU(&Buf[1], "Quit"))
            {
                fprintf(Out, "Quitting...\n");
                return false;
            }
            else if (STREQU(&Buf[1], "Disasm"))
            {
                if (PVM->Disassemble)
                    fprintf(Out, "Disabled disassembly.\n");
                else 
                    fprintf(Out, "Enabled disassembly.\n");
                PVM->Disassemble = !PVM->Disassemble;
            }
            else 
            {
                fprintf(Out, "Unknown Repl command: '%s'\n", &Buf[1]);
            }
            Buf[0] = '\n';
        }
    } while ('\n' == Buf[0]);
    return true;
}



