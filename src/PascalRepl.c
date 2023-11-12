

#include <string.h>
#include "Common.h"

#include "Pascal.h"
#include "Memory.h"

#include "Parser.h"
#include "PVM/CodeGen.h"
#include "PVM/Disassembler.h"


static bool GetCommandLine(char *Buf, USize Bufsz);
static bool RunVM(PascalVM *PVM, CodeChunk *Chunk, const PascalAst *Ast);

int PascalRepl(void)
{
    MemInit(1024*1024);
    PascalArena Program = ArenaInit(1024*1024, 4);
    PascalArena Scratch = ArenaInit(1024, 4);

    PascalVM PVM = PVMInit(1024, 128);
    CodeChunk Chunk = ChunkInit(1024);
    PascalParser Parser;



    int RetVal = PASCAL_EXIT_SUCCESS;
    static char Tmp[1024] = { 0 };
    while (GetCommandLine(Tmp, sizeof Tmp))
    {
        USize SourceLen = strlen(Tmp);
        U8 *CurrentSource = ArenaAllocate(&Program, SourceLen + 1);
        memcpy(CurrentSource, Tmp, SourceLen);
        CurrentSource[SourceLen] = '\0';


        Parser = ParserInit(CurrentSource, &Scratch, stderr);
        PascalAst *Ast = ParserGenerateAst(&Parser);
        if (NULL != Ast)
        {
            RunVM(&PVM, &Chunk, Ast);
        }
        ArenaReset(&Scratch);
    }


    ArenaDeinit(&Scratch);
    ArenaDeinit(&Program);
    MemDeinit();
    return RetVal;
}


static bool GetCommandLine(char *Buf, USize Bufsz)
{
    do {
        printf("\n>> ");
        if (NULL == fgets(Buf, Bufsz, stdin))
        {
            return false;
        }
    } while ('\n' == Buf[0]);

    if (0 == strncmp(Buf, "Quit", sizeof ("Quit") - 1))
        return false;
    return true;
}



static bool RunVM(PascalVM *PVM, CodeChunk *Chunk, const PascalAst *Ast)
{
    if (!PVMCompile(Chunk, Ast))
    {
        fprintf(stderr, "Compile Error\n");
        return false;
    }

    PVMDisasm(stdout, Chunk, "Compiled Code");

    printf("Press Enter to execute...\n");
    getc(stdin);
    PVMReturnValue Ret = PVMInterpret(PVM, Chunk);
    if (Ret != PVM_NO_ERROR)
    {
        return false;
    }

    PVMDumpState(stdout, PVM, 6);
    return true;
}


