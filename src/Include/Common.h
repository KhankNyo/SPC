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

#if defined(__LITTLE_ENDIAN__)
#  define PASCAL_LITTLE_ENDIAN 1
#elif defined(__BIG_ENDIAN__)
#  define PASCAL_BIG_ENDIAN 1
#elif !defined(__LITTLE_ENDIAN__) && !defined(__BIG_ENDIAN__)
#  if (defined(__BYTE_ORDER__)  && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__) || \
     (defined(__BYTE_ORDER) && __BYTE_ORDER == __BIG_ENDIAN) || \
     (defined(_BYTE_ORDER) && _BYTE_ORDER == _BIG_ENDIAN) || \
     (defined(BYTE_ORDER) && BYTE_ORDER == BIG_ENDIAN) || \
     (defined(__sun) && defined(__SVR4) && defined(_BIG_ENDIAN)) || \
     defined(__ARMEB__) || defined(__THUMBEB__) || defined(__AARCH64EB__) || \
     defined(_MIBSEB) || defined(__MIBSEB) || defined(__MIBSEB__) || \
     defined(_M_PPC)
#    define PASCAL_BIG_ENDIAN 1
#  elif (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__) || /* gcc */\
     (defined(__BYTE_ORDER) && __BYTE_ORDER == __LITTLE_ENDIAN) /* linux header */ || \
     (defined(_BYTE_ORDER) && _BYTE_ORDER == _LITTLE_ENDIAN) || \
     (defined(BYTE_ORDER) && BYTE_ORDER == LITTLE_ENDIAN) /* mingw header */ ||  \
     (defined(__sun) && defined(__SVR4) && defined(_LITTLE_ENDIAN)) || /* solaris */ \
     defined(__ARMEL__) || defined(__THUMBEL__) || defined(__AARCH64EL__) || \
     defined(_MIPSEL) || defined(__MIPSEL) || defined(__MIPSEL__) || \
     defined(_M_IX86) || defined(_M_X64) || defined(_M_IA64) || /* msvc for intel processors */ \
     defined(_M_ARM) /* msvc code on arm executes in little endian mode */
#    define PASCAL_LITTLE_ENDIAN 1
#  endif
#endif

#if PASCAL_LITTLE_ENDIAN
#  define PASCAL_BIG_ENDIAN 0
#else
#  define PASCAL_LITTLE_ENDIAN 0
#endif


#define BIT_MASK32(Size, Index) (((U32)1 << (Size)) - 1)
#define BIT_POS32(Value, Size, Index) (((U32)(Value) & BIT_MASK32(Size, 0)) << (Index))
#define BIT_AT32(U32_Value, Size, Index) (((U32)(U32_Value) >> (Index)) & BIT_MASK32(Size, 0))
/* not a safe macro, but better than a static function in a header file */
#define BIT_SEX32(Value, SignBitIndex) \
    (((Value) & ((U32)1 << (SignBitIndex))) \
        ? (U32)(Value) | ~(((U32)1 << (SignBitIndex)) - 1)\
        : (U32)(Value))



#define STATIC_ARRAY_SIZE(Array) (sizeof(Array) / sizeof((Array)[0]))
#define DIE() ((*(volatile char *)NULL) = 0)
#define PASCAL_UNREACHABLE(...) \
    do {\
        fprintf(stderr, "Invalid code path reached on line "\
                STRFY(__LINE__)" in "__FILE__":\n    "\
                __VA_ARGS__\
                );\
        DIE();\
    }while(0)
#define PASCAL_STATIC_ASSERT(COND, MSG) extern int static_assertion(char foo[(COND)?1:-1])


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



#define IN_I32(x) ((I64)INT32_MIN <= (I64)(x)) && ((I64)(x) <= (I64)INT32_MAX)


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
typedef ptrdiff_t ISize;



PASCAL_STATIC_ASSERT(sizeof(double) == sizeof(U64), "Unsupported double size");
typedef double F64;
typedef float F32;


#define CHR_TO_UPPER(chr) ((U8)(chr) & ~(1u << 5))



static inline U32 IsolateTopBitU32(U32 Value)
{
    U32 Count = 0;
    while (Value >> 1)
    {
        Count++;
        Value >>= 1;
    }
    return (U32)1 << Count;
}



#endif /* PASCAL_COMMON_H */

