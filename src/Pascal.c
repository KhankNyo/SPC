
#include <string.h>

#include "Common.h"
#include "Pascal.h"
#include "Memory.h"
#include "Tokenizer.h"
#include "Parser.h"

#include "PVM/Disassembler.h"
#include "PVM/PVM.h"
#include "PVM/CodeChunk.h"


#define MB 1024*1024

static PascalAst Compile(const U8 *Source, PascalArena *Allocator);
static U8 *LoadFile(const U8 *FileName);
static void UnloadFile(U8 *FileContents);


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



int PascalRepl(void)
{

    PascalArena Scratch = ArenaInit(1 << 15, 2);
    PascalArena Program = ArenaInit(MB, 4);
    PascalArena Permanent = ArenaInit(MB, 4);


    int RetVal = PASCAL_EXIT_SUCCESS;
    while (1)
    {
        static char Tmp[1024] = { 0 };
        do {
            printf("\n>> ");
            if (NULL == fgets(Tmp, sizeof Tmp, stdin))
            {
                RetVal = PASCAL_EXIT_FAILURE;
                goto Cleanup;
            }
        } while ('\n' == Tmp[0]);


        if (0 == strncmp(Tmp, "Quit", sizeof ("Quit") - 1))
            break;

        USize SourceLen = strlen(Tmp);
        PASCAL_ASSERT(SourceLen < sizeof Tmp, "Unreachable");
        U8 *CurrentSource = ArenaAllocate(&Program, SourceLen + 1);
        memcpy(CurrentSource, Tmp, SourceLen);
        CurrentSource[SourceLen] = '\0';

        Compile(CurrentSource, &Scratch);

        ArenaReset(&Scratch);
    }


Cleanup:
    ArenaDeinit(&Scratch);
    ArenaDeinit(&Program);
    ArenaDeinit(&Permanent);

    return RetVal;
}



int PascalRunFile(const U8 *InFileName, const U8 *OutFileName)
{
    U8 *Source = LoadFile(InFileName);

    PascalArena Allocator = ArenaInit(1024 * 1024, 4);
    Compile(Source, &Allocator);
    ArenaDeinit(&Allocator);

    UnloadFile(Source);
    return PASCAL_EXIT_SUCCESS;
}



void PascalPrintUsage(FILE *f, const U8 *ProgramName)
{
    fprintf(f, "Usage: %s InputName.pas OutPutName\n",
            ProgramName
    );
}




static PascalAst Compile(const U8 *Source, PascalArena *Allocator)
{
#if 0
    PascalParser Parser = ParserInit(Source, Allocator, stderr);
    PascalAst Ast = ParserGenerateAst(&Parser);
    PAstPrint(stdout, &Ast);
    return Ast;
#endif
    CodeChunk Chunk = CodeChunkInit(1024);
    PascalVM PVM = PVMInit(0, 0);

    CodeChunkWrite(&Chunk, PVM_IRD_ARITH_INS(LDI, 1, 10));
    CodeChunkWrite(&Chunk, PVM_IRD_ARITH_INS(ADD, 0, 1));
    CodeChunkWrite(&Chunk, PVM_BRANCH_IF(NE, 0, 1, -2));
    CodeChunkWrite(&Chunk, PVM_SYS_INS(EXIT));

    PVMDisasm(stderr, &Chunk);
    PVMInterpret(&PVM, &Chunk);
    PVMDumpState(stderr, &PVM, 6);

    PVMDeinit(&PVM);
    CodeChunkDeinit(&Chunk);
    return (PascalAst) { 0 };
}



static U8 *LoadFile(const U8 *FileName)
{
    FILE *File = fopen((const char *)FileName, "rb");
    if (NULL == File)
    {
        perror((const char *)FileName);
        return NULL;
    }

    fseek(File, 0, SEEK_END);
    USize Size = ftell(File);
    fseek(File, 0, SEEK_SET);
    U8 *Content = MemAllocate(Size + 1);

    USize ReadSize = fread(Content, 1, Size, File);
    if (Size != ReadSize)
    {
        fprintf(stderr, "Expected to read %llu characters from '%s,' read %llu instead\n",
                Size, FileName, ReadSize
        );
    }

    Content[Size] = '\0';
    fclose(File);
    return Content;
}


static void UnloadFile(U8 *FileContent)
{
    MemDeallocate(FileContent);
}

