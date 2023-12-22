#ifndef PASCAL_COMPILER_DATA_H
#define PASCAL_COMPILER_DATA_H 



#include "Common.h"
#include "Variable.h"
#include "Tokenizer.h"
#include "Compiler.h"
#include "Emitter.h"

struct CompilerFrame 
{
    U32 VarCount;
    const SubroutineData *Current;
};

struct TmpIdentifiers
{
    Token Array[64];
    U32 Count, Cap;
};

union PointeeTable 
{
    VarType *Pointee[TYPE_COUNT];
    struct {
        VarType *Lo[TYPE_POINTER - 1];
        union PointeeTable *Ptr;
        VarType *Hi[TYPE_COUNT - TYPE_POINTER];
    } Specific;
};

PASCAL_STATIC_ASSERT(
        (TYPE_POINTER - 1)*sizeof(void*) == offsetof(PointeeTable, Specific.Ptr), 
        "Pack the damn union"
);


struct PVMCompiler 
{
    PascalCompileFlags Flags;

    PVMEmitter Emitter;
    PascalTokenizer Lexer;
    Token Curr, Next;

    PascalGPA InternalAlloc;
    PascalGPA *GlobalAlloc;
    PascalVartab *Locals[PVM_MAX_SCOPE_COUNT];
    PascalVartab *Global;
    I32 Scope;
    U32 StackSize;

    struct {
        StringView Crt;
        StringView System;
    } Builtins;
    Token EmptyToken;

    TmpIdentifiers Idens;

    struct {
        VarLocation **Location;
        U32 Count, Cap;
    } Var;
    VarLocation Lhs;

    /* TODO: dynamic */
    /* for exit statement to see which subroutine it's currently in */
    CompilerFrame Subroutine[PVM_MAX_SCOPE_COUNT];

    U32 Breaks[256];
    U32 BreakCount;
    bool InLoop;

    struct {
        struct {
            U32 CallSite;
            PVMPatchType PatchType;
            const U32 *SubroutineLocation;
        } *Data;
        U32 Count, Cap;
    } SubroutineReferences;

    U32 EntryPoint;
    FILE *LogFile;
    bool Error, Panic;
};


#define EMITTER() (&Compiler->Emitter)


PVMCompiler CompilerInit(const U8 *Source, 
        PascalCompileFlags Flags,
        PascalVartab *PredefinedIdentifiers, 
        PVMChunk *Chunk, /* out */
        PascalGPA *GlobalAlloc, FILE *LogFile
);
void CompilerDeinit(PVMCompiler *Compiler);



bool IsAtGlobalScope(const PVMCompiler *Compiler);
PascalVartab *CurrentScope(PVMCompiler *Compiler);

void CompilerPushScope(PVMCompiler *Compiler, PascalVartab *Scope);
PascalVartab *CompilerPopScope(PVMCompiler *Compiler);
void CompilerPushSubroutine(PVMCompiler *Compiler, SubroutineData *Subroutine);
void CompilerPopSubroutine(PVMCompiler *Compiler);
VarLocation *CompilerAllocateVarLocation(PVMCompiler *Compiler);


void PushSubroutineReference(PVMCompiler *Compiler, 
        const U32 *SubroutineLocation, U32 CallSite, PVMPatchType PatchType
);
void ResolveSubroutineReferences(PVMCompiler *Compiler);

void CompilerResetTmp(PVMCompiler *Compiler);
void CompilerPushTmp(PVMCompiler *Compiler, Token Identifier);
TmpIdentifiers CompilerSaveTmp(PVMCompiler *Compiler);
void CompilerUnsaveTmp(PVMCompiler *Compiler, const TmpIdentifiers *Save);
UInt CompilerGetTmpCount(const PVMCompiler *Compiler);
Token *CompilerGetTmp(PVMCompiler *Compiler, UInt Idx);

void CompilerInitDebugInfo(PVMCompiler *Compiler, const Token *From);
void CompilerEmitDebugInfo(PVMCompiler *Compiler, const Token *From);

bool NextTokenIs(const PVMCompiler *Compiler, TokenType Type);
bool IsAtEnd(const PVMCompiler *Compiler);
bool IsAtStmtEnd(const PVMCompiler *Compiler);
void ConsumeToken(PVMCompiler *Compiler);
bool ConsumeIfNextIs(PVMCompiler *Compiler, TokenType Type);
bool ConsumeOrError(PVMCompiler *Compiler, TokenType Expected, const char *Fmt, ...);

/* find an identifier, returns NULL if it doesn't exist, 
 * NOTE: DOES NOT report error when the search failed  */
PascalVar *FindIdentifier(PVMCompiler *Compiler, const Token *Identifier);
/* find an identifier, reutrns NULL if it doesn't exist,
 * NOTE: DOES report error when the search failed */
PascalVar *GetIdenInfo(PVMCompiler *Compiler, const Token *Identifier, const char *ErrFmt, ...);

/* never returns NULL */
PascalVar *DefineAtScope(PVMCompiler *Compiler, PascalVartab *Scope,
        const Token *Identifier, VarType Type, VarLocation *Location
);
/* never returns NULL */
PascalVar *DefineParameter(PVMCompiler *Compiler,
        const Token *Identifier, VarType Type, VarLocation *Location
);
/* never returns NULL */
PascalVar *DefineIdentifier(PVMCompiler *Compiler, 
        const Token *Identifier, VarType Type, VarLocation *Location
);
/* never returns NULL */
PascalVar *DefineGlobal(PVMCompiler *Compiler,
        const Token *Identifier, VarType Type, VarLocation *Location
);


VarType *CompilerCopyType(PVMCompiler *Compiler, VarType Type);


#endif /* PASCAL_COMPILER_DATA_H */

