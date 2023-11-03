
#include "Include/Common.h"
#include "Include/Pascal.h"
#include "Include/Memory.h"


static char *LoadFile(const char *FileName);
static void UnloadFile(char *FileContents);


int PascalMain(int argc, const char *const *argv)
{
    if (argc < 3)
    {
        const char *name = argc == 0 
            ? "Pascal"
            : argv[0];

        PascalPrintUsage(stderr, name);
        return PASCAL_EXIT_FAILURE;
    }

    const char *InFileName = argv[1];
    char *Source = LoadFile(InFileName);
    fprintf(stderr, "---------------------------\n");
    fprintf(stderr, "\"%s\"\n", Source);
    fprintf(stderr, "---------------------------\n");
    UnloadFile(Source);
    return PASCAL_EXIT_SUCCESS;
}



void PascalPrintUsage(FILE *f, const char *ProgramName)
{
    fprintf(f, "Usage: %s InputName.pas OutPutName\n",
            ProgramName
    );
}







static char *LoadFile(const char *FileName)
{
    FILE *File = fopen(FileName, "rb");
    if (NULL == File)
    {
        perror(FileName);
        return NULL;
    }

    fseek(File, 0, SEEK_END);
    USize Size = ftell(File);
    fseek(File, 0, SEEK_SET);
    char *Content = MemAllocate(Size + 1);

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


static void UnloadFile(char *FileContent)
{
    MemDeallocate(FileContent);
}

