
#include "Include/Common.h"
#include "Include/Pascal.h"
#include "Include/Memory.h"
#include "Include/Tokenizer.h"


static U8 *LoadFile(const U8 *FileName);
static void UnloadFile(U8 *FileContents);


int PascalMain(int argc, const U8 *const *argv)
{
    if (argc < 3)
    {
        const U8 *name = argc == 0 
            ? (const U8*)"Pascal"
            : argv[0];

        PascalPrintUsage(stderr, name);
        return PASCAL_EXIT_FAILURE;
    }

    const U8 *InFileName = argv[1];
    U8 *Source = LoadFile(InFileName);

    Tokenizer Lexer = TokenizerInit(Source);
    Token Current;
    do {
        Current = TokenizerGetToken(&Lexer);
        fprintf(stdout, "Line %u: Type %s: ", 
                Current.Line,
                TokenTypeToStr(Current.Type)
        );
        if (Current.Type == TOKEN_STRING_LITERAL)
        {
            fprintf(stdout, "\"%s\"\n", PStrGetPtr(&Current.Literal.Str));
            PStrDeinit(&Current.Literal.Str);
        }
        else
        {
            fprintf(stdout, "\"%.*s\"\n", Current.Len, Current.Str);
        }
    } while (Current.Type != TOKEN_EOF);

    UnloadFile(Source);
    return PASCAL_EXIT_SUCCESS;
}



void PascalPrintUsage(FILE *f, const U8 *ProgramName)
{
    fprintf(f, "Usage: %s InputName.pas OutPutName\n",
            ProgramName
    );
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

