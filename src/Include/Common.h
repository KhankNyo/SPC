#ifndef PASCAL_COMMON_H
#define PASCAL_COMMON_H

#include <stdint.h>
#include <stddef.h>
#include <limits.h>

#define STATIC_ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))


typedef uint8_t U8;
typedef uint16_t U16;
typedef uint32_t U32;
typedef uint64_t U64;
typedef size_t USize;

typedef int8_t I8;
typedef int16_t I16;
typedef int32_t I32;
typedef int64_t I64;
typedef intptr_t ISize;


#endif /* PASCAL_COMMON_H */

