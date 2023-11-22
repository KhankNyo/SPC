@echo off



set "ARG1=%1"
set "OUTDIRS=bin obj"
set "SRCDIR=%CD%\src"
set "OBJDIR=%CD%\obj"
set "BINDIR=%CD%\bin"
set "INCPATH=%SRCDIR%\Include"

if "%ARG1%"=="cl" (
    set "CC=cl"
    set "CCF=/Zi /DEBUG /FC /I%INCPATH% /DDEBUG /DPVM_DEBUGGER"
    set "LDF=/MTd /DEBUG /Zi"
    set "OBJ_SWITCH=/Fo"
    set "SRC_SWITCH=/c "
    set "EXE_SWITCH=/Fe"
    set "OBJ_EXTENSION=obj"
) else (
    set "CC=gcc"
    set "CCF=-g -O2 -Wall -Wextra -Wpedantic -I%INCPATH% -DDEBUG -DPVM_DEBUGGER"
    set "LDF="
    set "LIBS="
    set "OBJ_SWITCH=-o "
    set "SRC_SWITCH=-c "
    set "EXE_SWITCH=-o "
    set "OBJ_EXTENSION=o"
)

set "MSG="
set "SRCS=%SRCDIR%\PascalString.c %SRCDIR%\main.c %SRCDIR%\Pascal.c %SRCDIR%\Memory.c"

rem set "SRCS=%SRCS% %SRCDIR%\Tokenizer.c %SRCDIR%\Parser.c %SRCDIR%\Ast.c %SRCDIR%\Vartab.c"
rem set "SRCS=%SRCS% %SRCDIR%\PVM\Disassembler.c %SRCDIR%\PVM\PVM.c %SRCDIR%\PVM\CodeChunk.c %SRCDIR%\PVM\CodeGen.c"
set "SRCS=%SRCS% %SRCDIR%\Tokenizer.c %SRCDIR%\Vartab.c"
set "SRCS=%SRCS% %SRCDIR%\PVMCompiler.c %SRCDIR%\PVMEmitter.c %SRCDIR%\IntegralTypes.c"
set "SRCS=%SRCS% %SRCDIR%\PVM\CodeChunk.c %SRCDIR%\PVM\Disassembler.c %SRCDIR%\PVM\PVM.c"

set "SRCS=%SRCS% %SRCDIR%\PVM\Debugger.c"
set "SRCS=%SRCS% %SRCDIR%\PascalFile.c %SRCDIR%\PascalRepl.c"
set "OUTPUT=%BINDIR%\pascal.exe"



if "%ARG1%"=="clean" (
    if exist bin\ rmdir /q /s bin
    if exist obj\ rmdir /q /s obj

    set "MSG=Removed build artifacts."
) else (
    if not exist bin\ mkdir bin
    if not exist obj\ mkdir obj

    setlocal enabledelayedexpansion
    set "OBJS="
    pushd obj\
        for %%F in (%SRCS%) do (
            set CURRENT_OBJ=%OBJDIR%\%%~nF.%OBJ_EXTENSION%

            :: compile each src file 
            echo %CC% %CCF% %SRC_SWITCH%%%F %OBJ_SWITCH%!CURRENT_OBJ!
            %CC% %CCF% %SRC_SWITCH%%%F %OBJ_SWITCH%!CURRENT_OBJ!

            :: add obj file to list 
            set "OBJS=!OBJS! !CURRENT_OBJ!"
        )
    popd

    pushd bin\
        :: linking 
        echo %CC% %LDF% %EXE_SWITCH%%OUTPUT% !OBJS! %LIBS%
        %CC% %LDF% %EXE_SWITCH%%OUTPUT% !OBJS! %LIBS%
    popd

    :: cleanup
    set "OBJS="

    set "MSG=Build finished."
)

echo --------------------------------------
echo %MSG%
echo --------------------------------------


