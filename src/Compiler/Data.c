

#include <stdarg.h>

#include "StringView.h"

#include "Compiler/Data.h"
#include "Compiler/Error.h"
#include "Compiler/Builtins.h"



#define ARTIFICIAL_TOKEN(StrLiteral) (Token){\
    .Len = sizeof (StrLiteral) - 1,\
    .Str = (const U8*)(StrLiteral),\
    .Line = 0, .Type = TOKEN_IDENTIFIER, .LineOffset = 0\
}


PVMCompiler CompilerInit(const U8 *Source, 
        PascalCompileFlags Flags,
        PascalVartab *PredefinedIdentifiers, PVMChunk *Chunk, PascalGPA *GlobalAlloc, FILE *LogFile)
{
    PVMCompiler Compiler = {
        .Lexer = TokenizerInit(Source),
        .LogFile = LogFile,
        .Error = false,
        .Panic = false,
        .Scope = 0,
        .Flags = Flags,

        .GlobalAlloc = GlobalAlloc,
        .InternalAlloc = GPAInit(4 * 1024 * 1024),
        /* string literals are safe */
        .Builtins = {
            .Crt = STRVIEW_INIT_CSTR("crt", 3),
            .System = STRVIEW_INIT_CSTR("system", 6),
        },

        .Var = {
            .Cap = 256,
            .Count = 0,
        },
        .Global = PredefinedIdentifiers,
        .Idens = {
            .Cap = STATIC_ARRAY_SIZE(Compiler.Idens.Array),
            .Count = 0,
        },

        .Breaks = { 0 },
        .BreakCount = 0,
        .InLoop = false,

        .SubroutineReferences = { 0 },

        .Subroutine = { {0} },
        .Emitter = PVMEmitterInit(Chunk),
    };

    Compiler.Var.Location = GPAAllocate(&Compiler.InternalAlloc, sizeof(VarLocation *) * Compiler.Var.Cap);
    for (U32 i = 0; i < Compiler.Var.Cap; i++)
    {
        Compiler.Var.Location[i] = 
            GPAAllocateZero(Compiler.GlobalAlloc, sizeof Compiler.Var.Location[0][0]);
    }


    DefineSystemSubroutines(&Compiler);
    return Compiler;
}

void CompilerDeinit(PVMCompiler *Compiler)
{
    ResolveSubroutineReferences(Compiler);
    PVMSetEntryPoint(&Compiler->Emitter, Compiler->EntryPoint);
    PVMEmitterDeinit(&Compiler->Emitter);
    GPADeinit(&Compiler->InternalAlloc);
}







bool IsAtGlobalScope(const PVMCompiler *Compiler)
{
    return 0 == Compiler->Scope;
}


PascalVartab *CurrentScope(PVMCompiler *Compiler)
{
    if (IsAtGlobalScope(Compiler))
    {
        return Compiler->Global;
    }
    else 
    {
        return Compiler->Locals[Compiler->Scope - 1];
    }
}


void CompilerPushScope(PVMCompiler *Compiler, PascalVartab *Scope)
{
    Compiler->Locals[Compiler->Scope] = Scope;
    Compiler->Scope++;
}

PascalVartab *CompilerPopScope(PVMCompiler *Compiler)
{
    /* when popping a scope, we revert Var.Count back to Var.Count of the previous scope,
     * and in the mean time, we'd also need to refill 
     * the vars that were taken by the scope we're about to pop */

    Compiler->Scope--;
    U32 Last = Compiler->Var.Count;
    Compiler->Var.Count = CurrentScope(Compiler)->Count;

    /* refill the variables that have been taken */
    for (U32 i = Compiler->Var.Count; i < Last; i++)
    {
        Compiler->Var.Location[i] = 
            GPAAllocateZero(Compiler->GlobalAlloc, sizeof(Compiler->Var.Location[0][0]));
    }
    return Compiler->Locals[Compiler->Scope];
}


void CompilerPushSubroutine(PVMCompiler *Compiler, SubroutineData *Subroutine)
{
    Compiler->Subroutine[Compiler->Scope].Current = Subroutine;
    Compiler->Subroutine[Compiler->Scope].VarCount = Compiler->Var.Count;
    CompilerPushScope(Compiler, &Subroutine->Scope);
    PASCAL_ASSERT(Compiler->Scope < (I32)STATIC_ARRAY_SIZE(Compiler->Subroutine), 
            "TODO: dynamic nested scope"
    );
}

void CompilerPopSubroutine(PVMCompiler *Compiler)
{
    CompilerPopScope(Compiler);
}


VarLocation *CompilerAllocateVarLocation(PVMCompiler *Compiler)
{
    if (Compiler->Var.Count >= Compiler->Var.Cap)
    {
        U32 OldCap = Compiler->Var.Cap;
        Compiler->Var.Cap *= 2;
        Compiler->Var.Location = GPAReallocateArray(&Compiler->InternalAlloc, 
                Compiler->Var.Location, VarLocation, 
                Compiler->Var.Cap
        );
        for (U32 i = OldCap; i < Compiler->Var.Cap; i++)
        {
            Compiler->Var.Location[i] = 
                GPAAllocateZero(Compiler->GlobalAlloc, sizeof Compiler->Var.Location[0][0]);
        }
    }
    return Compiler->Var.Location[Compiler->Var.Count++];
}






bool NextTokenIs(const PVMCompiler *Compiler, TokenType Type)
{
    return Type == Compiler->Next.Type;
}

bool ConsumeIfNextIs(PVMCompiler *Compiler, TokenType Type)
{
    if (NextTokenIs(Compiler, Type))
    {
        ConsumeToken(Compiler);
        return true;
    }
    return false;
}


void ConsumeToken(PVMCompiler *Compiler)
{
    Compiler->Curr = Compiler->Next;
    Compiler->Next = TokenizerGetToken(&Compiler->Lexer);
    if (NextTokenIs(Compiler, TOKEN_ERROR))
    {
        Error(Compiler, "%s", Compiler->Next.Literal.Err);
    }
}

bool IsAtEnd(const PVMCompiler *Compiler)
{
    return NextTokenIs(Compiler, TOKEN_EOF);
}

bool IsAtStmtEnd(const PVMCompiler *Compiler)
{
    return IsAtEnd(Compiler) || NextTokenIs(Compiler, TOKEN_SEMICOLON);
}






void PushSubroutineReference(PVMCompiler *Compiler, 
        const VarSubroutine *Subroutine, U32 CallSite, PVMPatchType PatchType)
{
    PASCAL_NONNULL(Compiler);
    PASCAL_NONNULL(Subroutine);

    U32 Count = Compiler->SubroutineReferences.Count;
    if (Count >= Compiler->SubroutineReferences.Cap)
    {
        U32 NewCap = Compiler->SubroutineReferences.Cap * 2 + 8;
        Compiler->SubroutineReferences.Data = GPAReallocateArray(
                &Compiler->InternalAlloc, 
                Compiler->SubroutineReferences.Data, 
                *Compiler->SubroutineReferences.Data, 
                NewCap
        );
        Compiler->SubroutineReferences.Cap = NewCap;
    }
    Compiler->SubroutineReferences.Data[Count].CallSite = CallSite;
    Compiler->SubroutineReferences.Data[Count].Subroutine = Subroutine;
    Compiler->SubroutineReferences.Data[Count].PatchType = PatchType;
    Compiler->SubroutineReferences.Count = Count + 1;
}

void ResolveSubroutineReferences(PVMCompiler *Compiler)
{
    PASCAL_NONNULL(Compiler);
    for (UInt i = 0; i < Compiler->SubroutineReferences.Count; i++)
    {
        PVMPatchBranch(EMITTER(), 
                Compiler->SubroutineReferences.Data[i].CallSite,
                Compiler->SubroutineReferences.Data[i].Subroutine->Location,
                Compiler->SubroutineReferences.Data[i].PatchType
        );
    }
    /* don't need to deallocate the internal allocator, 
     * will get deallocated in batch */
    //GPADeallocate(&Compiler->InternalAlloc, Compiler->SubroutineReferences.Data);
    memset(&Compiler->SubroutineReferences, 0, sizeof(Compiler->SubroutineReferences));
}








void CompilerInitDebugInfo(PVMCompiler *Compiler, const Token *From)
{
    PVMEmitDebugInfo(&Compiler->Emitter, From->Lexeme.Str, From->Lexeme.Len, From->Line);
}

void CompilerEmitDebugInfo(PVMCompiler *Compiler, const Token *From)
{
    U32 LineLen = Compiler->Curr.Lexeme.Str - From->Lexeme.Str + Compiler->Curr.Lexeme.Len;
    if (From->Type == TOKEN_FUNCTION || From->Type == TOKEN_PROCEDURE)
    {
        PVMUpdateDebugInfo(&Compiler->Emitter, LineLen, true);
    }
    else
    {
        PVMUpdateDebugInfo(&Compiler->Emitter, LineLen, false);
    }
}



void CompilerResetTmp(PVMCompiler *Compiler)
{
    PASCAL_NONNULL(Compiler);
    Compiler->Idens.Count = 0;
}

void CompilerPushTmp(PVMCompiler *Compiler, Token Identifier)
{
    PASCAL_NONNULL(Compiler);
    if (Compiler->Idens.Count > Compiler->Idens.Cap)
    {
        PASCAL_UNREACHABLE("TODO: allocator to tmp iden");
    }
    Compiler->Idens.Array[Compiler->Idens.Count] = Identifier;
    Compiler->Idens.Count++;
}

TmpIdentifiers CompilerSaveTmp(PVMCompiler *Compiler)
{
    PASCAL_NONNULL(Compiler);
    TmpIdentifiers Save = Compiler->Idens;
    Compiler->Idens = (TmpIdentifiers) { .Cap = STATIC_ARRAY_SIZE(Save.Array) };
    return Save;
}

void CompilerUnsaveTmp(PVMCompiler *Compiler, const TmpIdentifiers *Save)
{
    PASCAL_NONNULL(Compiler);
    PASCAL_NONNULL(Save);
    Compiler->Idens = *Save;
}

UInt CompilerGetTmpCount(const PVMCompiler *Compiler)
{
    PASCAL_NONNULL(Compiler);
    return Compiler->Idens.Count;
}

Token *CompilerGetTmp(PVMCompiler *Compiler, UInt Idx)
{
    PASCAL_NONNULL(Compiler);
    return &Compiler->Idens.Array[Idx];
}







/*===============================================================================*/
/*
 *                          VARIABLES AND IDENTIFIERS
 */
/*===============================================================================*/


PascalVar *FindIdentifier(PVMCompiler *Compiler, const Token *Identifier)
{
    U32 Hash = VartabHashStr(Identifier->Lexeme.Str, Identifier->Lexeme.Len);
    PascalVar *Info = NULL;

    /* iterate from innermost scope to global - 1 */
    for (int i = Compiler->Scope - 1; i >= 0; i--)
    {
        Info = VartabFindWithHash(Compiler->Locals[i], 
                Identifier->Lexeme.Str, Identifier->Lexeme.Len, Hash
        );
        if (NULL != Info)
            return Info;
    }
    Info = VartabFindWithHash(Compiler->Global,
            Identifier->Lexeme.Str, Identifier->Lexeme.Len, Hash
    );
    return Info;
}


PascalVar *DefineAtScope(PVMCompiler *Compiler, PascalVartab *Scope,
        const Token *Identifier, VarType Type, VarLocation *Location)
{
    PascalVar *AlreadyDefined = VartabFindWithHash(Scope, 
            Identifier->Lexeme.Str, Identifier->Lexeme.Len, 
            VartabHashStr(Identifier->Lexeme.Str, Identifier->Lexeme.Len)
    );
    if (AlreadyDefined)
    {
        if (0 == AlreadyDefined->Line)
        {
            ErrorAt(Compiler, Identifier, "'"STRVIEW_FMT"' is a predefined identifier in this scope.",
                    STRVIEW_FMT_ARG(&Identifier->Lexeme)
            );
        }
        else
        {
            ErrorAt(Compiler, Identifier, "'"STRVIEW_FMT"' is already defined on line %d in this scope.", 
                    STRVIEW_FMT_ARG(&Identifier->Lexeme),
                    AlreadyDefined->Line
            );
        }
        return AlreadyDefined;
    }
    return VartabSet(Scope, 
            Identifier->Lexeme.Str, Identifier->Lexeme.Len, Identifier->Line, 
            Type, Location
    );
}

PascalVar *DefineIdentifier(PVMCompiler *Compiler, 
        const Token *Identifier, VarType Type, VarLocation *Location)
{
    return DefineAtScope(Compiler, CurrentScope(Compiler), Identifier, Type, Location);
}

PascalVar *DefineParameter(PVMCompiler *Compiler,
        const Token *Identifier, VarType Type, VarLocation *Location)
{
    PascalVar *AlreadyDefined = VartabFindWithHash(CurrentScope(Compiler), 
            Identifier->Lexeme.Str, Identifier->Lexeme.Len, 
            VartabHashStr(Identifier->Lexeme.Str, Identifier->Lexeme.Len)
    );
    if (AlreadyDefined)
    {
        ErrorAt(Compiler, Identifier, "Redefinition of parameter with the same name.",
                Identifier->Lexeme.Len, Identifier->Lexeme.Str
        );
        return NULL;
    }
    return VartabSet(CurrentScope(Compiler), 
            Identifier->Lexeme.Str, Identifier->Lexeme.Len, Identifier->Line, 
            Type, Location
    );
}

PascalVar *DefineGlobal(PVMCompiler *Compiler,
        const Token *Identifier, VarType Type, VarLocation *Location)
{
    return DefineAtScope(Compiler, Compiler->Global, Identifier, Type, Location);
}



/* reports error if identifier is not found */
PascalVar *GetIdenInfo(PVMCompiler *Compiler, const Token *Identifier, const char *ErrFmt, ...)
{
    PascalVar *Info = FindIdentifier(Compiler, Identifier);
    if (NULL == Info)
    {
        va_list ArgList;
        va_start(ArgList, ErrFmt);
        VaListError(Compiler, Identifier, ErrFmt, ArgList);
        va_end(ArgList);
    }
    return Info;
}




VarType *CompilerCopyType(PVMCompiler *Compiler, VarType Type)
{
    VarType *Copy = GPAAllocate(Compiler->GlobalAlloc, sizeof Type);
    *Copy = Type;
    return Copy;
}




