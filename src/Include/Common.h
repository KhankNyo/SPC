#ifndef PASCAL_COMMON_H
#define PASCAL_COMMON_H

#include <stdint.h>
#include <stddef.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>


/* this is a cry for help */
#define _strfy(x) #x
#define STRFY(x) _strfy(x)

#define _glue1(x, y) x ## y
#define _glue2(x, y) _glue1(x, y)
#define _glue3(x, y) _glue2(x, y)
#define _glue4(x, y) _glue3(x, y)
#define _glue5(x, y) _glue4(x, y)
#define _glue6(x, y) _glue5(x, y)
#define _glue7(x, y) _glue6(x, y)
#define GLUE(x, y) _glue7(x, y)

#define _UNUSED1(a) (void)(a)
#define _UNUSED2(a, b) ((void)(a), (void)(b))
#define _UNUSED3(a, b, c) (_UNUSED2(a, b), (void)(c))
#define _UNUSED4(a, b, c, d) (_UNUSED3(a, b, c), (void)(d))
#define _UNUSED5(a, b, c, d, e) (_UNUSED4(a, b, c, d), (void)(e))

#define _COUNT(_1, _2, _3, _4, _5, N, ...) N
#define _VANARGS(...) _COUNT(__VA_ARGS__, 5, 4, 3, 2, 1)
#define _UNUSED_N(N, ...) GLUE(_UNUSED, N) (__VA_ARGS__)
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

#define PASCAL_STATIC_ASSERT(COND, MSG) extern int static_assertion[(COND)?1:-1]


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



PASCAL_STATIC_ASSERT(sizeof(double) == sizeof(U64), "Unsupported double size");
typedef double F64;


#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__) || \
    defined(__BIG_ENDIAN__) || \
    defined(__ARMEB__) || \
    defined(__THUMBEB__) || \
    defined(__AARCH64EB__) || \
    defined(_MIBSEB) || defined(__MIBSEB) || defined(__MIBSEB__)
#  define PASCAL_BIG_ENDIAN 1
#  define PASCAL_LITTLE_ENDIAN 0

#elif defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__) || \
    defined(__LITTLE_ENDIAN__) || \
    defined(__ARMEL__) || \
    defined(__THUMBEL__) || \
    defined(__AARCH64EL__) || \
    defined(_MIPSEL) || defined(__MIPSEL) || defined(__MIPSEL__)
#  define PASCAL_BIG_ENDIAN 0
#  define PASCAL_LITTLE_ENDIAN 1

#else
static bool IsLittleEndian(void)
{
    U64 Test = 1;
    return *(U8*)&Test == 1;
}
#define PASCAL_BIG_ENDIAN IsLittleEndian()
#define PASCAL_LITTLE_ENDIAN IsLittleEndian()

#endif



#endif /* PASCAL_COMMON_H */

