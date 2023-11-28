

#include <string.h>
#include <time.h>

#include "Common.h"

#include "Pascal.h"
#include "Memory.h"

#include "PVM/PVM.h"
#include "PVMCompiler.h"


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
    

    int RetVal = PASCAL_EXIT_SUCCESS;
    static char Tmp[1024] = { 0 };
    while (GetCommandLine(&PVM, Tmp, sizeof Tmp))
    {
        USize SourceLen = strlen(Tmp);
        U8 *CurrentSource = ArenaAllocate(&Program, SourceLen + 1);
        memcpy(CurrentSource, Tmp, SourceLen);
        CurrentSource[SourceLen] = '\0';

        PascalCompileFlags Flags = { 0 };
        if (PVMCompile(CurrentSource, Flags, &Identifiers, &Chunk, &Permanent, stderr))
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
    do {
        printf("\n>> ");
        if (NULL == fgets(Buf, Bufsz, stdin))
            return false;

        if ('.' == Buf[0])
        {
            Buf[strlen(Buf) - 1] = '\0'; /* remove pesky newline character */

            if (STREQU(&Buf[1], "Debug"))
            {
                if (PVM->SingleStepMode)
                    fprintf(stdout, "Disabled debug mode.\n");
                else 
                    fprintf(stdout, "Enabled debug mode.\n");
                PVM->SingleStepMode = !PVM->SingleStepMode;
            }
            else if (STREQU(&Buf[1], "Quit"))
            {
                fprintf(stdout, "Quitting...\n");
                return false;
            }
            else 
            {
                fprintf(stdout, "Unknown Repl command: '%s'\n", &Buf[1]);
            }
            Buf[0] = '\n';
        }
    } while ('\n' == Buf[0]);
    return true;
}



