#ifndef PASCAL_COMPILER_DATA_H
#define PASCAL_COMPILER_DATA_H 



#include "Common.h"
#include "Variable.h"
#include "Tokenizer.h"
//#include "Compiler.h"
#include "Emitter.h"

struct CompilerFrame 
{
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


#define EMITTER() (&Compiler->Emitter)


PascalCompiler CompilerInit(const U8 *Source, 
        PascalCompileFlags Flags,
        PascalVartab *PredefinedIdentifiers, 
        PVMChunk *Chunk, /* out */
        PascalGPA *GlobalAlloc, FILE *LogFile
);
void CompilerDeinit(PascalCompiler *Compiler);



bool IsAtGlobalScope(const PascalCompiler *Compiler);
PascalVartab *CurrentScope(PascalCompiler *Compiler);

void CompilerPushScope(PascalCompiler *Compiler, PascalVartab *Scope);
PascalVartab *CompilerPopScope(PascalCompiler *Compiler);
void CompilerPushSubroutine(PascalCompiler *Compiler, SubroutineData *Subroutine);
void CompilerPopSubroutine(PascalCompiler *Compiler);
VarLocation *CompilerAllocateVarLocation(PascalCompiler *Compiler);


void PushSubroutineReference(PascalCompiler *Compiler, const U32 *SubroutineLocation, U32 CallSite);
void ResolveSubroutineReferences(PascalCompiler *Compiler);

void CompilerResetTmp(PascalCompiler *Compiler);
void CompilerPushTmp(PascalCompiler *Compiler, Token Identifier);
TmpIdentifiers CompilerSaveTmp(PascalCompiler *Compiler);
void CompilerUnsaveTmp(PascalCompiler *Compiler, const TmpIdentifiers *Save);
UInt CompilerGetTmpCount(const PascalCompiler *Compiler);
Token *CompilerGetTmp(PascalCompiler *Compiler, UInt Idx);

void CompilerInitDebugInfo(PascalCompiler *Compiler, const Token *From);
void CompilerEmitDebugInfo(PascalCompiler *Compiler, const Token *From);

bool NextTokenIs(const PascalCompiler *Compiler, TokenType Type);
bool IsAtEnd(const PascalCompiler *Compiler);
bool IsAtStmtEnd(const PascalCompiler *Compiler);
void ConsumeToken(PascalCompiler *Compiler);
bool ConsumeIfNextIs(PascalCompiler *Compiler, TokenType Type);
bool ConsumeOrError(PascalCompiler *Compiler, TokenType Expected, const char *Fmt, ...);

/* find an identifier, returns NULL if it doesn't exist, 
 * NOTE: DOES NOT report error when the search failed  */
PascalVar *FindIdentifier(PascalCompiler *Compiler, const Token *Identifier);
/* find an identifier, reutrns NULL if it doesn't exist,
 * NOTE: DOES report error when the search failed */
PascalVar *GetIdenInfo(PascalCompiler *Compiler, const Token *Identifier, const char *ErrFmt, ...);

/* never returns NULL */
PascalVar *DefineAtScope(PascalCompiler *Compiler, PascalVartab *Scope,
        const Token *Identifier, VarType Type, VarLocation *Location
);
/* never returns NULL */
PascalVar *DefineParameter(PascalCompiler *Compiler,
        const Token *Identifier, VarType Type, VarLocation *Location
);
/* never returns NULL */
PascalVar *DefineIdentifier(PascalCompiler *Compiler, 
        const Token *Identifier, VarType Type, VarLocation *Location
);
/* never returns NULL */
PascalVar *DefineGlobal(PascalCompiler *Compiler,
        const Token *Identifier, VarType Type, VarLocation *Location
);


VarType *CompilerCopyType(PascalCompiler *Compiler, VarType Type);


#endif /* PASCAL_COMPILER_DATA_H */

