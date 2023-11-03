#ifndef PASCAL_COMMON_H
#define PASCAL_COMMON_H

#include <stdint.h>
#include <stddef.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>


#define _strfy(x) #x
#define STRFY(x) _strfy(x)

/* this is a cry for help */
#define _UNUSED1(a) (void)(a)
#define _UNUSED2(a, b) ((void)(a), (void)(b))
#define _UNUSED3(a, b, c) (_UNUSED2(a, b), (void)(c))
#define _UNUSED4(a, b, c, d) (_UNUSED3(a, b, c), (void)(d))
#define _UNUSED5(a, b, c, d, e) (_UNUSED4(a, b, c, d), (void)(e))

#define _COUNT(_1, _2, _3, _4, _5, N, ...) N
#define _VANARGS(...) _COUNT(__VA_ARGS__, 5, 4, 3, 2, 1)
#define _UNUSED_N(N, ...) _UNUSED ## N (__VA_ARGS__)
#define UNUSED(...) _UNUSED_N(_VANARGS(__VA_ARGS__), __VA_ARGS__) 





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
#  define DBG_PRINT(...) fprintf(stderr, __VA_ARGS__)
#else
#  define PASCAL_ASSERT(expr, ...) (void)(expr), UNUSED(__VA_ARGS__)
#  define DBG_PRINT(...) (void)UNUSED(__VA_ARGS__)
#endif



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




#endif /* PASCAL_COMMON_H */

