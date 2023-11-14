
#include <string.h>
#include <time.h>
#include "Common.h"

#include "Pascal.h"
#include "Memory.h"
#include "Parser.h"

#include "PVM/PVM.h"
#include "PVM/CodeGen.h"
#include "PVM/Disassembler.h"




static U8 *LoadFile(const U8 *FileName, U32 MemorySize);
static void UnloadFile(U8 *FileContents);
static bool PascalRun(const U8 *Source, PascalArena *Arena);



int PascalRunFile(const U8 *InFileName, const U8 *OutFileName)
{
    U8 *Source = LoadFile(InFileName, 1024*1024);
    if (NULL == Source)
        return PASCAL_EXIT_FAILURE;

    PascalArena Arena = ArenaInit(1024*1024, 4);
    PascalRun(Source, &Arena);
    ArenaDeinit(&Arena);

    UnloadFile(Source);
    return PASCAL_EXIT_SUCCESS;
}



static U8 *LoadFile(const U8 *FileName, U32 MemorySize)
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

    MemInit(Size + MemorySize);
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
    (void)FileContent;
    MemDeinit();
}


static bool PascalRun(const U8 *Source, PascalArena *Arena)
{
    PascalVartab Identifiers = VartabPredefinedIdentifiers(MemGetAllocator(), 1024);    
    PascalParser Parser = ParserInit(Source, &Identifiers, Arena, stderr);
    PascalAst *Ast = ParserGenerateAst(&Parser);
    if (NULL == Ast)
        goto ParseError;


    CodeChunk Code = ChunkInit(1024);
    if (!PVMCompile(&Code, Ast))
        goto CompileError;


    PVMDisasm(stdout, &Code, "Compiled expression");
    printf("Press Enter to execute...\n");
    getc(stdin);

    PascalVM VM = PVMInit(1024, 128);
    PVMReturnValue Ret = PVMInterpret(&VM, &Code);
    PVMDumpState(stdout, &VM, 6);

    ParserDestroyAst(Ast);
    ChunkDeinit(&Code);
    return Ret == PVM_NO_ERROR;

CompileError:
    ParserDestroyAst(Ast);
    ChunkDeinit(&Code);
ParseError:
    return false;
}



