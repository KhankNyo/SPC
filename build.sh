#!/bin/sh


SRCDIR="${PWD}/src"
OBJDIR="${PWD}/obj"
BINDIR="${PWD}/bin"
INCPATH="${SRCDIR}/Include"

CC="gcc"
CCFLAGS="-DPVM_DEBUGGER \
    -Ofast -flto\
    -Wno-missing-braces -Wno-format\
    -Wall -Wextra -Wpedantic -I${INCPATH}"
LDFLAGS="-flto"
LIBS=""

ARG="$1"

SRCS="${SRCDIR}/PVMCompiler.c ${SRCDIR}/PVMEmitter.c ${SRCDIR}/Vartab.c \
    ${SRCDIR}/PascalFile.c ${SRCDIR}/PascalRepl.c ${SRCDIR}/Pascal.c \
    ${SRCDIR}/main.c ${SRCDIR}/Tokenizer.c ${SRCDIR}/Memory.c \
    ${SRCDIR}/PascalString.c"
SRCS="${SRCS} ${SRCDIR}/PVM/PVM.c ${SRCDIR}/PVM/Chunk.c ${SRCDIR}/PVM/Disassembler.c \
    ${SRCDIR}/PVM/Debugger.c"
OUTPUT="./bin/pascal"


PrintMessage () {
    echo ""
    echo ---------------------------------------------------
    echo $1
    echo ---------------------------------------------------
}


if [ "$ARG" = "clean" ];
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

