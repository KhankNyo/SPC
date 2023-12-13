#ifndef PASCAL_COMPILER_DATA_H
#define PASCAL_COMPILER_DATA_H 



#include "Common.h"
#include "Variable.h"
#include "Tokenizer.h"
#include "Compiler.h"
#include "Emitter.h"

struct CompilerFrame 
{
    U32 Location;
    VarSubroutine *Current; /* VarLocation never resizes, this is safe */
};


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

    struct {
        Token Crt;
    } Builtins;
    Token EmptyToken;

    struct {
        /* TODO: dynamic */
        Token Array[256];
        U32 Count, Cap;
    } Iden;

    struct {
        VarLocation **Location;
        U32 Count, Cap;
    } Var;

    /* TODO: dynamic */
    CompilerFrame Subroutine[PVM_MAX_SCOPE_COUNT];


    I32 Scope;
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



U32 CompilerGetSizeOfType(PVMCompiler *Compiler, IntegralType Type, const VarLocation *TypeInfo);
bool IsAtGlobalScope(const PVMCompiler *Compiler);
PascalVartab *CurrentScope(PVMCompiler *Compiler);

void CompilerPushScope(PVMCompiler *Compiler, PascalVartab *Scope);
PascalVartab *CompilerPopScope(PVMCompiler *Compiler);
void CompilerPushSubroutine(PVMCompiler *Compiler, VarSubroutine *Subroutine);
void CompilerPopSubroutine(PVMCompiler *Compiler);
VarLocation *CompilerAllocateVarLocation(PVMCompiler *Compiler);

void CompilerResetTmpIdentifier(PVMCompiler *Compiler);
void CompilerPushTmpIdentifier(PVMCompiler *Compiler, Token Identifier);
UInt CompilerGetTmpIdentifierCount(const PVMCompiler *Compiler);
Token *CompilerGetTmpIdentifier(PVMCompiler *Compiler, UInt Idx);

void CompilerInitDebugInfo(PVMCompiler *Compiler, const Token *From);
void CompilerEmitDebugInfo(PVMCompiler *Compiler, const Token *From);

void SubroutineDataPushParameter(PascalGPA *Allocator, VarSubroutine *Subroutine, PascalVar *Param);
void SubroutineDataPushRef(PascalGPA *Allocator, VarSubroutine *Subroutine, U32 CallSite);
void CompilerResolveSubroutineCalls(PVMCompiler *Compiler, 
        VarSubroutine *Subroutine, U32 SubroutineLocation
);


bool NextTokenIs(const PVMCompiler *Compiler, TokenType Type);
bool IsAtEnd(const PVMCompiler *Compiler);
bool IsAtStmtEnd(const PVMCompiler *Compiler);
void ConsumeToken(PVMCompiler *Compiler);
bool ConsumeIfNextIs(PVMCompiler *Compiler, TokenType Type);
bool ConsumeOrError(PVMCompiler *Compiler, TokenType Expected, const char *Fmt, ...);

PascalVar *FindIdentifier(PVMCompiler *Compiler, const Token *Identifier);
PascalVar *DefineAtScope(PVMCompiler *Compiler, PascalVartab *Scope,
        const Token *Identifier, IntegralType Type, VarLocation *Location
);
PascalVar *DefineParameter(PVMCompiler *Compiler,
        const Token *Identifier, IntegralType Type, VarLocation *Location
);
PascalVar *DefineIdentifier(PVMCompiler *Compiler, 
        const Token *Identifier, IntegralType Type, VarLocation *Location
);
PascalVar *DefineGlobal(PVMCompiler *Compiler,
        const Token *Identifier, IntegralType Type, VarLocation *Location
);

/* reports error if identifier is not found */
PascalVar *GetIdenInfo(PVMCompiler *Compiler, const Token *Identifier, const char *ErrFmt, ...);



#endif /* PASCAL_COMPILER_DATA_H */
