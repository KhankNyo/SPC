#!/bin/sh


SRCDIR="${PWD}/src"
OBJDIR="${PWD}/obj"
BINDIR="${PWD}/bin"
INCPATH="${SRCDIR}/Include"

CC="$1"
CCFLAGS="-DPVM_DEBUGGER \
    -Ofast -flto\
    -Wno-missing-braces -Wno-format\
    -Wall -Wextra -Wpedantic -I${INCPATH}"
LDFLAGS="-flto"
LIBS=""

SRCS="${SRCDIR}/main.c ${SRCDIR}/Pascal.c ${SRCDIR}/PascalFile.c ${SRCDIR}/PascalRepl.c \
    ${SRCDIR}/PascalString.c ${SRCDIR}/Memory.c ${SRCDIR}/Vartab.c \
    ${SRCDIR}/Tokenizer.c \
    ${SRCDIR}/Compiler/Compiler.c ${SRCDIR}/Compiler/Data.c ${SRCDIR}/Compiler/Builtins.c \
    ${SRCDIR}/Compiler/Expr.c ${SRCDIR}/Compiler/Emitter.c ${SRCDIR}/Compiler/VarList.c \
    ${SRCDIR}/Compiler/Error.c \
    ${SRCDIR}/PVM/Chunk.c ${SRCDIR}/PVM/Debugger.c ${SRCDIR}/PVM/Disassembler.c ${SRCDIR}/PVM/PVM.c"
UNITY="${SRCDIR}/UnityBuild.c"
OUTPUT="./bin/pascal"


PrintMessage () {
    echo ""
    echo ---------------------------------------------------
    echo $1
    echo ---------------------------------------------------
}


if [ "$1" = "clean" ];
then
    if [ -d "${BINDIR}" ]; 
    then 
        rm -f ${BINDIR}/*
        rmdir "${BINDIR}"
    fi
    if [ -d "${OBJDIR}" ]; 
    then 
        rm -f ${OBJDIR}/*
        rmdir "${OBJDIR}"
    fi
    PrintMessage "  Removed build artifacts"
elif [ "$2" = "unity" ]; 
then
    echo $CC $LDFLAGS $CCFLAGS -o $OUTPUT $UNITY $LIBS
    $CC $LDFLAGS $CCFLAGS -o $OUTPUT $UNITY $LIBS

    PrintMessage "  Build finished"
else 

    OBJS=""
    if [ ! -d "${BINDIR}" ]; 
    then 
        mkdir $BINDIR 
    fi
    if [ ! -d "${OBJDIR}" ]; 
    then 
        mkdir $OBJDIR 
    fi

    for f in $SRCS; 
    do
        # extract file name only
        FileName="${f##*/}"
        # append '.o'
        FileName="${FileName%.c}.o"

        OBJ="${OBJDIR}/${FileName}"
        OBJS="${OBJS} ${OBJ}"

        echo $CC $CCFLAGS -o $OBJ -c $f
        $CC $CCFLAGS -o $OBJ -c $f
    done

    echo $CC $LDFLAGS -o $OUTPUT $OBJS $LIBS
    $CC $LDFLAGS -o $OUTPUT $OBJS $LIBS

    PrintMessage "  Build finished"
fi

