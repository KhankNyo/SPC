

#include <stdarg.h>

#include "StringView.h"

#include "Compiler/Compiler.h"
#include "Compiler/Data.h"
#include "Compiler/Error.h"
#include "Compiler/Builtins.h"








bool IsAtGlobalScope(const PascalCompiler *Compiler)
{
    return 0 == Compiler->Scope;
}


PascalVartab *CurrentScope(PascalCompiler *Compiler)
{
    if (IsAtGlobalScope(Compiler))
    {
        return &Compiler->Global;
    }
    else 
    {
        return Compiler->Locals[Compiler->Scope - 1];
    }
}


void CompilerPushScope(PascalCompiler *Compiler, PascalVartab *Scope)
{
    Compiler->Locals[Compiler->Scope] = Scope;
    Compiler->Scope++;
}

PascalVartab *CompilerPopScope(PascalCompiler *Compiler)
{
    return Compiler->Locals[--Compiler->Scope];
}


void CompilerPushSubroutine(PascalCompiler *Compiler, SubroutineData *Subroutine)
{
    Compiler->Subroutine[Compiler->Scope].Current = Subroutine;
    CompilerPushScope(Compiler, &Subroutine->Scope);
    PASCAL_ASSERT(Compiler->Scope < (I32)STATIC_ARRAY_SIZE(Compiler->Subroutine), 
            "TODO: dynamic nested scope"
    );
}

void CompilerPopSubroutine(PascalCompiler *Compiler)
{
    CompilerPopScope(Compiler);
}


VarLocation *CompilerAllocateVarLocation(PascalCompiler *Compiler)
{
    return ArenaAllocate(&Compiler->InternalArena, sizeof(VarLocation));
}






bool NextTokenIs(const PascalCompiler *Compiler, TokenType Type)
{
    return Type == Compiler->Next.Type;
}

bool ConsumeIfNextIs(PascalCompiler *Compiler, TokenType Type)
{
    if (NextTokenIs(Compiler, Type))
    {
        ConsumeToken(Compiler);
        return true;
    }
    return false;
}


void ConsumeToken(PascalCompiler *Compiler)
{
    Compiler->Curr = Compiler->Next;
    Compiler->Next = TokenizerGetToken(&Compiler->Lexer);
    if (NextTokenIs(Compiler, TOKEN_ERROR))
    {
        Error(Compiler, "%s", Compiler->Next.Literal.Err);
    }
}

bool IsAtEnd(const PascalCompiler *Compiler)
{
    return NextTokenIs(Compiler, TOKEN_EOF);
}

bool IsAtStmtEnd(const PascalCompiler *Compiler)
{
    return IsAtEnd(Compiler) || NextTokenIs(Compiler, TOKEN_SEMICOLON);
}






void PushSubroutineReference(PascalCompiler *Compiler, 
        const U32 *SubroutineLocation, U32 CallSite, PVMPatchType PatchType)
{
    PASCAL_NONNULL(Compiler);
    PASCAL_NONNULL(SubroutineLocation);

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
    Compiler->SubroutineReferences.Data[Count].SubroutineLocation = SubroutineLocation;
    Compiler->SubroutineReferences.Data[Count].PatchType = PatchType;
    Compiler->SubroutineReferences.Count = Count + 1;
}

void ResolveSubroutineReferences(PascalCompiler *Compiler)
{
    PASCAL_NONNULL(Compiler);
    for (UInt i = 0; i < Compiler->SubroutineReferences.Count; i++)
    {
        PVMPatchBranch(EMITTER(), 
                Compiler->SubroutineReferences.Data[i].CallSite,
                *Compiler->SubroutineReferences.Data[i].SubroutineLocation,
                Compiler->SubroutineReferences.Data[i].PatchType
        );
    }
    /* don't need to deallocate the internal allocator, 
     * will get deallocated in batch */
    //GPADeallocate(&Compiler->InternalAlloc, Compiler->SubroutineReferences.Data);
    memset(&Compiler->SubroutineReferences, 0, sizeof(Compiler->SubroutineReferences));
}








void CompilerInitDebugInfo(PascalCompiler *Compiler, const Token *From)
{
    PVMEmitDebugInfo(&Compiler->Emitter, From->Lexeme.Str, From->Lexeme.Len, From->Line);
}

void CompilerEmitDebugInfo(PascalCompiler *Compiler, const Token *From)
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



void CompilerResetTmp(PascalCompiler *Compiler)
{
    PASCAL_NONNULL(Compiler);
    Compiler->Idens.Count = 0;
}

void CompilerPushTmp(PascalCompiler *Compiler, Token Identifier)
{
    PASCAL_NONNULL(Compiler);
    if (Compiler->Idens.Count > Compiler->Idens.Cap)
    {
        PASCAL_UNREACHABLE("TODO: allocator to tmp iden");
    }
    Compiler->Idens.Array[Compiler->Idens.Count] = Identifier;
    Compiler->Idens.Count++;
}

TmpIdentifiers CompilerSaveTmp(PascalCompiler *Compiler)
{
    PASCAL_NONNULL(Compiler);
    TmpIdentifiers Save = Compiler->Idens;
    Compiler->Idens = (TmpIdentifiers) { .Cap = STATIC_ARRAY_SIZE(Save.Array) };
    return Save;
}

void CompilerUnsaveTmp(PascalCompiler *Compiler, const TmpIdentifiers *Save)
{
    PASCAL_NONNULL(Compiler);
    PASCAL_NONNULL(Save);
    Compiler->Idens = *Save;
}

UInt CompilerGetTmpCount(const PascalCompiler *Compiler)
{
    PASCAL_NONNULL(Compiler);
    return Compiler->Idens.Count;
}

Token *CompilerGetTmp(PascalCompiler *Compiler, UInt Idx)
{
    PASCAL_NONNULL(Compiler);
    return &Compiler->Idens.Array[Idx];
}







/*===============================================================================*/
/*
 *                          VARIABLES AND IDENTIFIERS
 */
/*===============================================================================*/


PascalVar *FindIdentifier(PascalCompiler *Compiler, const Token *Identifier)
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
    Info = VartabFindWithHash(&Compiler->Global,
            Identifier->Lexeme.Str, Identifier->Lexeme.Len, Hash
    );
    return Info;
}


PascalVar *DefineAtScope(PascalCompiler *Compiler, PascalVartab *Scope,
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

PascalVar *DefineIdentifier(PascalCompiler *Compiler, 
        const Token *Identifier, VarType Type, VarLocation *Location)
{
    return DefineAtScope(Compiler, CurrentScope(Compiler), Identifier, Type, Location);
}

PascalVar *DefineParameter(PascalCompiler *Compiler,
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

PascalVar *DefineGlobal(PascalCompiler *Compiler,
        const Token *Identifier, VarType Type, VarLocation *Location)
{
    return DefineAtScope(Compiler, &Compiler->Global, Identifier, Type, Location);
}



/* reports error if identifier is not found */
PascalVar *GetIdenInfo(PascalCompiler *Compiler, const Token *Identifier, const char *ErrFmt, ...)
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




VarType *CompilerCopyType(PascalCompiler *Compiler, VarType Type)
{
    VarType *Copy = ArenaAllocate(&Compiler->InternalArena, sizeof Type);
    *Copy = Type;
    return Copy;
}




