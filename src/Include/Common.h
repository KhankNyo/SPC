#ifndef PASCAL_COMMON_H
#define PASCAL_COMMON_H

#include <stdint.h>
#include <stddef.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>


#define _strfy(x) #x
#define STRFY(x) _strfy(x)

#define STATIC_ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))
#define DIE() ((*(volatile char *)NULL) = 0)
#define PASCAL_UNREACHABLE(...) \
do {\
    fprintf(stderr, "Invalid code path reached on line "\
            STRFY(__LINE__)" in "__FILE__":\n    "\
            __VA_ARGS__\
    );\
    DIE();\
}while(0)

typedef uint8_t U8;
typedef uint16_t U16;
typedef uint32_t U32;
typedef uint64_t U64;
typedef size_t USize;
typedef unsigned UInt;

typedef int8_t I8;
typedef int16_t I16;
typedef int32_t I32;
typedef int64_t I64;
typedef intptr_t ISize;



#ifdef DEBUG
#  define PASCAL_ASSERT(expr, ...) do {\
    if (!(expr)) {\
        fprintf(stderr, "Assertion failed on line "\
                STRFY(__LINE__)" in "__FILE__": "\
                STRFY(expr)":\n    "\
                __VA_ARGS__\
        );\
        DIE();\
    }\
}while(0)
#else
#  define PASCAL_ASSERT(expr, ...) (void)(expr)
#endif


#endif /* PASCAL_COMMON_H */

