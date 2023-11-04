@echo off



set "ARG1=%1"
set "OUTDIRS=bin obj"

set "CC=gcc"
set "CCF=-g -Og -DDEBUG -Wall -Wextra -Wpedantic"
set "LDF="
set "LIBS="

set "MSG="
set "SRCS=src\main.c src\Pascal.c src\Memory.c src\Tokenizer.c, src\PascalString.c"
set "OUTPUT=bin\pascal.exe"



if "%ARG1%"=="clean" (
    if exist bin\ rmdir /q /s bin
    if exist obj\ rmdir /q /s obj

    set "MSG=Removed build artifacts."
) else (
    if not exist bin\ mkdir bin
    if not exist obj\ mkdir obj

    setlocal enabledelayedexpansion
    set "OBJS="
    for %%F in (%SRCS%) do (
        set CURRENT_OBJ=obj/%%~nF.o

        :: compile each src file 
        echo %CC% %CCF% -c %%F -o !CURRENT_OBJ!
        %CC% %CCF% -c %%F -o !CURRENT_OBJ!

        :: add obj file to list 
        set "OBJS=!OBJS! !CURRENT_OBJ!"
    )
    :: linking 
    echo %CC% %LDF% -o %OUTPUT% !OBJS! %LIBS%
    %CC% %LDF% -o %OUTPUT% !OBJS! %LIBS%

    :: cleanup
    set "OBJS="

    set "MSG=Build finished."
)

echo --------------------------------------
echo %MSG%
echo --------------------------------------


