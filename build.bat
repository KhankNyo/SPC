@echo off



set "SRCDIR=%CD%\src"
set "OBJDIR=%CD%\obj"
set "BINDIR=%CD%\bin"
set "INCPATH=%SRCDIR%\Include"

if "%1"=="cl" (
    if "%VisualStudioVersion%"=="" call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64

    set "CC=cl"
    set "CCF=/Zi /DEBUG /FC /I%INCPATH% /DDEBUG /DPVM_DEBUGGER"
    set "LDF=/MTd /DEBUG /Zi"
    set "OBJ_SWITCH=/Fo"
    set "SRC_SWITCH=/c "
    set "EXE_SWITCH=/Fe"
    set "OBJ_EXTENSION=obj"
) else (
    set "CC=%1"

    :: STFU gcc you piece of shit 
    set "CCF=-DDEBUG -Ofast -flto -std=c99 -Wall -Wextra -Wpedantic -Wno-missing-braces -Wno-format-zero-length -I%INCPATH%"
    set "LDF= -flto"
    set "LIBS= "
    set "OBJ_SWITCH=-o "
    set "SRC_SWITCH=-c "
    set "EXE_SWITCH=-o "
    set "OBJ_EXTENSION=o"
)

set "SRCS=%SRCDIR%\PascalString.c %SRCDIR%\main.c %SRCDIR%\Pascal.c %SRCDIR%\Memory.c"
set "SRCS=%SRCS% %SRCDIR%\PascalFile.c %SRCDIR%\PascalRepl.c"

set "SRCS=%SRCS% %SRCDIR%\Tokenizer.c %SRCDIR%\Vartab.c"
set "SRCS=%SRCS% %SRCDIR%\Compiler\Compiler.c %SRCDIR%\Compiler\Emitter.c "
set "SRCS=%SRCS% %SRCDIR%\Compiler\Data.c %SRCDIR%\Compiler\Error.c %SRCDIR%\Compiler\Builtins.c"
set "SRCS=%SRCS% %SRCDIR%\Compiler\Expr.c %SRCDIR%\Compiler\VarList.c"

set "SRCS=%SRCS% %SRCDIR%\PVM\Chunk.c %SRCDIR%\PVM\Disassembler.c %SRCDIR%\PVM\PVM.c"
set "SRCS=%SRCS% %SRCDIR%\PVM\Debugger.c"


set "UNITY=%SRCDIR%\UnityBuild.c"
set "OUTPUT=%BINDIR%\pascal.exe"



if "%1"=="clean" (
    if exist %OBJDIR%\ rmdir /q /s %OBJDIR%
    if exist %BINDIR%\ rmdir /q /s %BINDIR%

    echo --------------------------------------
    echo        Removed build artifacts.
    echo --------------------------------------
) else if "%2"=="unity" (
    if not exist %BINDIR%\ mkdir %BINDIR%

    pushd %BINDIR%
        echo %CC% %CCF% %LDF% %EXE_SWITCH%%OUTPUT% %UNITY% %LIBS%
        %CC% %CCF% %LDF% %EXE_SWITCH%%OUTPUT% %UNITY% %LIBS%
    popd %BINDIR%

    echo --------------------------------------
    echo            Build finished.
    echo --------------------------------------
) else (
    if not exist %BINDIR%\ mkdir %BINDIR%
    if not exist %OBJDIR%\ mkdir %OBJDIR%

    setlocal enabledelayedexpansion
    set "OBJS="
    pushd %OBJDIR%\
        for %%F in (%SRCS%) do (
            set CURRENT_OBJ=%OBJDIR%\%%~nF.%OBJ_EXTENSION%

            :: compile each src file 
            echo %CC% %CCF% %SRC_SWITCH%%%F %OBJ_SWITCH%!CURRENT_OBJ!
            %CC% %CCF% %SRC_SWITCH%%%F %OBJ_SWITCH%!CURRENT_OBJ!

            :: add obj file to list 
            set "OBJS=!OBJS! !CURRENT_OBJ!"
        )
    popd

    pushd %BINDIR%\
        :: linking 
        echo %CC% %LDF% %EXE_SWITCH%%OUTPUT% !OBJS! %LIBS%
        %CC% %LDF% %EXE_SWITCH%%OUTPUT% !OBJS! %LIBS%
    popd

    :: cleanup
    set "OBJS="

    echo --------------------------------------
    echo            Build finished.
    echo --------------------------------------
)



