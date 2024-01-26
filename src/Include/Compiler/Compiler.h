#ifndef PASCAL_COMPILER_H
#define PASCAL_COMPILER_H



#include "Common.h"
#include "Vartab.h"
#include "PVM/Chunk.h"
#include "Data.h"

#define PVM_MAX_FUNCTION_COUNT 1024
#define PVM_MAX_SCOPE_COUNT 16              /* 15 nested functions */
#define PVM_MAX_VAR_PER_SCOPE (1 << 10)     /* limit on the allocator */
#define PVM_INITIAL_VAR_PER_SCOPE 32


typedef const U8 *(*PascalReplLineCallbackFn)(void *);
typedef enum PVMCallConv 
{
    CALLCONV_MSX64 = 0,
} PVMCallConv;

typedef enum PascalCompileMode 
{
    PASCAL_COMPMODE_PROGRAM = 0,
    PASCAL_COMPMODE_REPL,
} PascalCompileMode;

typedef enum PascalReplStatus 
{
    PASCAL_REPL_ERROR = 0,
    PASCAL_REPL_REQLINE,
    PASCAL_REPL_FINISHED,
} PascalReplStatus;



struct PascalCompileFlags 
{
    PVMCallConv CallConv;
    PascalCompileMode CompMode;
};



struct PascalCompiler 
{
    PascalCompileFlags Flags;

    PVMEmitter Emitter;
    PascalTokenizer Lexer;
    Token Curr, Next;

    PascalGPA InternalAlloc;
    PascalArena InternalArena;
    PascalVartab *Locals[PVM_MAX_SCOPE_COUNT];
    PascalVartab Global;
    I32 Scope;
    U32 StackSize, TemporarySize;

    struct {
        StringView Crt;
        StringView System;
    } Builtins;
    Token EmptyToken;

    TmpIdentifiers Idens;
    VarLocation *Lhs;

    /* TODO: dynamic */
    /* for exit statement to see which subroutine it's currently in 
     * during compilation of closures,
     * TODO: add closures */
    CompilerFrame Subroutine[PVM_MAX_SCOPE_COUNT];

    U32 Breaks[256];
    U32 BreakCount;
    bool InLoop;

    struct {
        struct {
            U32 CallSite;
            const U32 *SubroutineLocation;
        } *Data;
        U32 Count, Cap;
    } SubroutineReferences;

    U32 Line;
    U32 EntryPoint;
    FILE *LogFile;
    bool Error, Panic;

    PascalReplLineCallbackFn ReplCallback;
    void *ReplUserData;
};

PascalCompiler PascalCompilerInit(
        PascalCompileFlags Flags, const PascalVartab *PredefinedIdentifiers, 
        FILE *LogFile, PVMChunk *OutChunk
);
void PascalCompilerDeinit(PascalCompiler *Compiler);
void PascalCompilerReset(PascalCompiler *Compiler, bool PreserveFunctions);


/* NOTE: when executing callback, previous line SHOULD NOT BE FREED, 
 * because variables from the new line might reference ones from previous lines */
/* returns true if compilation is successful, false otherwise */
bool PascalCompileRepl(
        PascalCompiler *Compiler, const U8 *Line,  
        PascalReplLineCallbackFn Callback, void *CallbackData
);

/* returns true if compilation is successful, false otherwise */
bool PascalCompileProgram(
        PascalCompiler *Compiler, const U8 *Source
);






#endif /* PASCAL_VM_COMPILER_H */

