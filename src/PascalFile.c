
#include <string.h>
#include <time.h>
#include "Common.h"

#include "Pascal.h"
#include "Memory.h"

#include "PVM/PVM.h"
#include "Compiler/Compiler.h"





static U8 *LoadFile(const U8 *FileName, U32 MemorySize);
static void UnloadFile(U8 *FileContents);



int PascalRunFile(const U8 *InFileName, const U8 *OutFileName)
{
    U8 *Source = LoadFile(InFileName, 1024*1024);
    if (NULL == Source)
        return PASCAL_EXIT_FAILURE;

    int Status = PASCAL_EXIT_SUCCESS;
    PascalVartab Predefined = VartabPredefinedIdentifiers(MemGetAllocator(), 1024);
    PascalCompileFlags Flags = { 
        .CompMode = PASCAL_COMPMODE_PROGRAM, 
        .CallConv = CALLCONV_MSX64 
    };
    PVMChunk Chunk = ChunkInit(1024);
    PascalCompiler Compiler = PascalCompilerInit(Flags, &Predefined, stderr, &Chunk);
    if (PascalCompileProgram(&Compiler, Source))
    {
        PascalVM PVM = PVMInit(1024, 128);
        //PVM.SingleStepMode = true;
        PVM.Disassemble = true;
        PVMRun(&PVM, &Chunk);
    }
    else
    {
        Status = PASCAL_EXIT_FAILURE;
    }


    PascalCompilerDeinit(&Compiler);
    UnloadFile(Source);
    return Status;
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


