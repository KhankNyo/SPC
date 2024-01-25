#ifndef PASCAL_COMMON_H
#define PASCAL_COMMON_H

#include <stddef.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include "Typedefs.h"


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
#define _VANARGS(...) _COUNT(__VA_ARGS__, 5, 4, 3, 2, 1, 0)
#define _UNUSED_N(N, ...) GLUE(_UNUSED, N) (__VA_ARGS__)
#define UNUSED(...) _UNUSED_N(_VANARGS(__VA_ARGS__), __VA_ARGS__) 


#if (defined(__GNUC__))
#  if __has_attribute(__fallthrough__)
#    define FALLTHROUGH __attribute__((__fallthrough__))
#  elif __has_attribute(fallthrough)
#    define FALLTHROUGH __attribute__((fallthrough))
#  else
#    define FALLTHROUGH do {} while (0) /* fallthrough */
#  endif /* __has_attribute */
#elif defined(__clang__)
#  if __has_attribute(__fallthrough__)
#    define FALLTHROUGH __attribute__((__fallthrough__))
#  elif __has_attribute(fallthrough)
#    define FALLTHROUGH __attribute__((fallthrough))
#  endif /* __has_attribute */
#elif defined(_MSC_VER)
#  define FALLTHROUGH __fallthrough
#endif /* _MSC_VER */

#ifndef FALLTHROUGH 
#  define FALLTHROUGH /* fallthrough */
#endif /* FALLTHROUGH */


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
/* not a safe macro */
#define BIT_SEX32(Value, SignBitIndex) \
    (((Value) & ((U32)1 << (SignBitIndex))) \
        ? (U32)(Value) | ~(((U32)1 << (SignBitIndex)) - 1)\
        : (U32)(Value))
static inline I32 BitSex32Safe(U32 Value, UInt SignIndex)
{
    return BIT_SEX32(Value, SignIndex);
}

static inline I64 BitSex64(U64 Value, UInt SignIndex)
{
    if ((Value >> SignIndex) & 1)
    {
        Value |= ~(((U64)1 << SignIndex) - 1);
    }
    return Value;
}



#define STATIC_ARRAY_SIZE(Array) (sizeof(Array) / sizeof((Array)[0]))
#define DIE() ((*(char *)0) = 0)
#define PASCAL_UNREACHABLE(...) \
    do {\
        fprintf(stderr, "Invalid code path reached on line "\
                STRFY(__LINE__)" in "__FILE__":\n    "\
                __VA_ARGS__\
        );\
        fputc('\n', stderr);\
        DIE();\
    }while(0)
#define PASCAL_STATIC_ASSERT(COND, MSG) extern int static_assertion(char foo[(COND)?1:-1])


#ifdef DEBUG
#  define PASCAL_ASSERT(expr, ...) do {\
    if (!(expr)) {\
        fprintf(stderr, "Assertion failed on line %d in %s in %s:"\
                "\n    '"STRFY(expr)"': ", \
                __LINE__, __func__, __FILE__\
        );\
        fprintf(stderr, __VA_ARGS__);\
        fputc('\n', stderr);\
        DIE();\
    }\
}while(0)
#define PASCAL_NONNULL(Ptr) do {\
    if (NULL == (Ptr)) {\
        fprintf(stderr, "Invalid Pointer on line %d in %s in %s\n",\
                __LINE__, __func__, __FILE__\
        );\
        DIE();\
    }\
} while (0)
#  define DBG_PRINT(...) fprintf(stderr, __VA_ARGS__)
#else
#  define PASCAL_ASSERT(expr, ...) ((void)(expr), (void)UNUSED(__VA_ARGS__))
#  define PASCAL_NONNULL(Ptr) ((void)(Ptr))
#  define DBG_PRINT(...) ((void)UNUSED(__VA_ARGS__))
#endif


#define INT21_MAX (I32)(((U32)1 << 20) - 1)
#define INT21_MIN (I32)(BIT_SEX32(((U32)1 << 20), 20))
#define INT48_MIN (I64)~(((U64)1 << 47) - 1)
#define INT48_MAX (I64)(((U64)1 << 47) - 1)

#define IN_RANGE(Low, n, High) ((Low) <= (n) && (n) <= (High))
#define IN_I8(x) IN_RANGE(INT8_MIN, (I64)(x), INT8_MAX)
#define IN_I16(x) IN_RANGE(INT16_MIN, (I64)(x), INT16_MAX)
#define IN_I32(x) IN_RANGE(INT32_MIN, (I64)(x), INT32_MAX)
#define IN_I48(x) IN_RANGE(INT48_MIN, (I64)(x), INT48_MAX)

#define IN_U8(x) ((U64)(x) <= 0xFF)
#define IN_U16(x) ((U64)(x) <= 0xFFFF)
#define IN_U32(x) ((U64)(x) <= 0xFFFFFFFF)
#define IN_U48(x) ((U64)(x) <= (U64)0xFFFFFFFFFFFF)

#define CHR_TO_UPPER(chr) ((U8)(chr) & ~(1u << 5))
#define IS_POW2(UnsignedConstexpr) (0 == ((UnsignedConstexpr) & ((UnsignedConstexpr) - 1)))

typedef struct GenericPtr
{
    union {
        void *Raw;
        U8 *u8;
        U16 *u16;
        U32 *u32;
        U64 *u64;
        I8 *i8;
        I16 *i16;
        I32 *i32;
        I64 *i64;
        F32 *f32;
        F64 *f64;
        long double *ldbl;
        uintptr_t UInt;
    } As;
} GenericPtr;



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

static inline U64 RoundUpToPow2(U64 n)
{
    U64 Ret = n;
    U64 i = n;
    while (i)
    {
        Ret = i;
        i &= i - 1;
    }
    return n % 2 == 0 
        ? Ret : 2 * Ret;
}

static inline U64 uRoundUpToMultipleOfPow2(U64 n, U64 Multiple)
{
    U64 Mask = Multiple - 1;
    if (n & Mask)
        return (n + Multiple) & ~Mask;
    else return n;
}

static inline I64 iRoundUpToMultipleOfPow2(I64 n, U64 Multiple)
{
    if (n < 0)
    {
        U64 Unsigned = n;
        U64 Mask = Multiple - 1;
        if (Unsigned & Mask)
            return n & ~Mask;
        return (I64)n;
    }
    else return (I64)uRoundUpToMultipleOfPow2(n, Multiple);
}

static inline U64 uMin(U64 a, U64 b) { return a < b ? a : b; }
static inline I64 iMin(I64 a, I64 b) { return a < b ? a : b; }
static inline U64 uMax(U64 a, U64 b) { return a > b ? a : b; }
static inline I64 iMax(I64 a, I64 b) { return a > b ? a : b; }

static inline U64 AbsoluteValue(I64 i)
{
    U64 Sign = ((U64)i >> 63);
    return (i ^ Sign) - Sign;
}

static inline U64 ArithmeticShiftRight(U64 n, U64 ShiftAmount)
{
    if (n & (U64)1 << 63)
        return (n >> (ShiftAmount & 0x3F)) | ~(((U64)1 << ShiftAmount) - 1);
    return n >> ShiftAmount;
}

static inline UInt BitCount(U64 n)
{
    UInt i = 0;
    while (n)
    {
        i++;
        n &= n - 1;
    }
    return i;
}

/* return 0 if no bit is set */
/* TODO: compiler does not optimize this out to use bsr on x86.
 *      At least on Compiler Explorer with gcc 13.2 and clang 17.0.1
 * but it shouldn't matter since this function is not used in any critical path yet. */
static inline UInt IndexOfTopSetBit(U64 n)
{
    if (0 == n) 
        return 0;

    UInt Count = 64;
    if (0 == (n & 0xFFFFFFFF00000000))
    {
        Count -= 32;
        n <<= 32;
    }
    if (0 == (n & 0xFFFF000000000000))
    {
        Count -= 16;
        n <<= 16;
    }
    if (0 == (n & 0xFF00000000000000))
    {
        Count -= 8;
        n <<= 8;
    }
    if (0 == (n & 0xF000000000000000))
    {
        Count -= 4;
        n <<= 4;
    }
    if (0 == (n & 0xC000000000000000))
    {
        Count -= 2;
        n <<= 2;
    }
    if (0 == (n & 0x8000000000000000))
    {
        Count -= 1;
    }
    return Count;
}



PASCAL_STATIC_ASSERT(sizeof(F64) == sizeof(U64), "Unsupported double size");


#endif /* PASCAL_COMMON_H */

