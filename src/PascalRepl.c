

#include <string.h>
#include <time.h>

#include "Common.h"

#include "Pascal.h"
#include "Memory.h"

#include "PVM/PVM.h"
#include "Compiler/Compiler.h"


static bool GetCommandLine(const char *Prompt, PascalVM *PVM, PascalCompiler *Compiler, char *Buf, USize Bufsz);

#define STREQU(Buffer, Literal) (0 == strncmp(Buffer, Literal, sizeof(Literal) - 1))
#define LINE_SIZE 1024


typedef struct NewlineData
{
    PascalCompiler *Compiler;
    PascalArena *Source;
    PascalVM *PVM;
    char LineBuffer[LINE_SIZE];
} NewlineData;

static U8 *SaveLine(PascalArena *SaveArena, const char *Line, USize LineLength)
{
    U8 *CurrentLine = ArenaAllocate(SaveArena, LineLength + 1);
    memcpy(CurrentLine, Line, LineLength);
    CurrentLine[LineLength] = '\0';
    return CurrentLine;
}

static const U8 *NewlineCallback(void *NewlineCallbackData)
{
    NewlineData *Data = NewlineCallbackData;
    if (GetCommandLine("... ", Data->PVM, Data->Compiler, Data->LineBuffer, sizeof Data->LineBuffer))
    {
        return SaveLine(Data->Source, Data->LineBuffer, strlen(Data->LineBuffer));
    }
    return NULL;
}


int PascalRepl(void)
{
    MemInit(1024*1024);
    PascalArena Program = ArenaInit(1024*1024, 4);

    PascalVM PVM = PVMInit(1024, 128);
    PVMChunk Chunk = ChunkInit(1024);
    PascalVartab Global = VartabPredefinedIdentifiers(MemGetAllocator(), 1024);

    PascalCompileFlags Flags = {
        .CompMode = PASCAL_COMPMODE_REPL,
        .CallConv = CALLCONV_MSX64,
    };
    PascalCompiler Compiler = PascalCompilerInit(Flags, &Global, stderr, &Chunk);
    NewlineData Data = { 
        .PVM = &PVM,
        .Source = &Program,
        .Compiler = &Compiler,
        .LineBuffer = { 0 }, 
    };

    while (GetCommandLine(">>> ", &PVM, &Compiler, Data.LineBuffer, sizeof Data.LineBuffer))
    {
        const U8 *CurrentLine = SaveLine(&Program, 
                Data.LineBuffer, strlen(Data.LineBuffer)
        );
        if (PascalCompileRepl(&Compiler, CurrentLine, NewlineCallback, &Data))
        {
            PVMRun(&PVM, &Chunk);
        }
        PascalCompilerReset(&Compiler, true);
        fputc('\n', stdout);
    }

    PascalCompilerDeinit(&Compiler);
    ArenaDeinit(&Program);
    MemDeinit();
    return PASCAL_EXIT_SUCCESS;
}


static bool GetCommandLine(const char *Prompt, PascalVM *PVM, PascalCompiler *Compiler, char *Buf, USize Bufsz)
{
    (void)Compiler;
    FILE *Out = stdout, *In = stdin;
    do {
        printf("%s", Prompt);
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



