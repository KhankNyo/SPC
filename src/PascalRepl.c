

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
    
    PascalVartab Identifiers = VartabInit(1024);
    VartabSet(&Identifiers, (const U8*)"INTEGER", 7, TYPE_I16);
    VartabSet(&Identifiers, (const U8*)"REAL", 4, TYPE_F32);

    VartabSet(&Identifiers, (const U8*)"int8", 4, TYPE_I8);
    VartabSet(&Identifiers, (const U8*)"int16", 5, TYPE_I16);
    VartabSet(&Identifiers, (const U8*)"int32", 5, TYPE_I32);
    VartabSet(&Identifiers, (const U8*)"int64", 5, TYPE_I64);

    VartabSet(&Identifiers, (const U8*)"uint8", 4, TYPE_U8);
    VartabSet(&Identifiers, (const U8*)"uint16", 5, TYPE_U16);
    VartabSet(&Identifiers, (const U8*)"uint32", 5, TYPE_U32);
    VartabSet(&Identifiers, (const U8*)"uint64", 5, TYPE_U64);



    int RetVal = PASCAL_EXIT_SUCCESS;
    static char Tmp[1024] = { 0 };
    while (GetCommandLine(Tmp, sizeof Tmp))
    {
        USize SourceLen = strlen(Tmp);
        U8 *CurrentSource = ArenaAllocate(&Program, SourceLen + 1);
        memcpy(CurrentSource, Tmp, SourceLen);
        CurrentSource[SourceLen] = '\0';


        Parser = ParserInit(CurrentSource, &Identifiers, &Scratch, stderr);
        PascalAst *Ast = ParserGenerateAst(&Parser);
        if (NULL != Ast)
        {
            RunVM(&PVM, &Chunk, Ast);
        }
        ArenaReset(&Scratch);
        Chunk.Count = 0;
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


