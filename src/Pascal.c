
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


static bool PascalRun(const U8 *Source, PascalArena *Allocator);
static U8 *LoadFile(const U8 *FileName);
static void UnloadFile(U8 *FileContents);


int PascalMain(int argc, const U8 *const *argv)
{
    MemInit(1u << 16, 3);

    int ReturnValue = PASCAL_EXIT_SUCCESS;
    if (argc <= 1)
    {
        ReturnValue = PascalRepl();
        goto Return;
    }
    if (argc < 3)
    {
        const U8 *name = argc == 0 
            ? (const U8*)"Pascal"
            : argv[0];

        PascalPrintUsage(stderr, name);
        ReturnValue = PASCAL_EXIT_FAILURE;
        goto Return;
    }

    const U8 *InFileName = argv[1];
    const U8 *OutFileName = argv[2];
    ReturnValue = PascalRunFile(InFileName, OutFileName);


Return:
    MemDeinit();
    return ReturnValue;
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
        U8 *CurrentSource = ArenaAllocate(&Program, SourceLen + 1);
        memcpy(CurrentSource, Tmp, SourceLen);
        CurrentSource[SourceLen] = '\0';

        PascalRun(CurrentSource, &Scratch);

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
    if (NULL == Source)
        return PASCAL_EXIT_FAILURE;

    PascalArena Arena = ArenaInit(MB, 4);
    PascalRun(Source, &Arena);
    ArenaDeinit(&Arena);

    UnloadFile(Source);
    return PASCAL_EXIT_SUCCESS;
}



void PascalPrintUsage(FILE *f, const U8 *ProgramName)
{
    fprintf(f, "Usage: %s InputName.pas OutPutName\n",
            ProgramName
    );
}




static bool PascalRun(const U8 *Source, PascalArena *Arena)
{
    PascalParser Parser = ParserInit(Source, Arena, stderr);
    PascalAst *Ast = ParserGenerateAst(&Parser);
    if (NULL == Ast)
        goto ParseError;


    CodeChunk Code = CodeChunkInit(1024);
    if (!PVMCompile(&Code, Ast))
        goto CompileError;


    PVMDisasm(stdout, &Code, "Compiled expression");
    printf("Press Enter to execute...\n");
    getc(stdin);

    PascalVM VM = PVMInit(1024, 128);
    PVMReturnValue Ret = PVMInterpret(&VM, &Code);
    PVMDumpState(stdout, &VM, 6);

    ParserDestroyAst(Ast);
    CodeChunkDeinit(&Code);
    return Ret == PVM_NO_ERROR;

CompileError:
    ParserDestroyAst(Ast);
    CodeChunkDeinit(&Code);
ParseError:
    return false;
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

