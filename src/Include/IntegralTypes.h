#ifndef PASCAL_INTEGRAL_TYPE_H
#define PASCAL_INTEGRAL_TYPE_H


#include "Common.h"
#include "PascalString.h"

typedef enum IntegralType 
{
    TYPE_INVALID = 0,

    TYPE_I8, TYPE_I16, TYPE_I32, TYPE_I64,
    TYPE_U8, TYPE_U16, TYPE_U32, TYPE_U64,
    TYPE_F32, TYPE_F64, 

    TYPE_FUNCTION,
    TYPE_BOOLEAN,
    TYPE_POINTER,
    TYPE_STRING,
    TYPE_RECORD,
    TYPE_CHAR,

    TYPE_COUNT,
} IntegralType;

#if UINTPTR_MAX == UINT32_MAX
#  define TYPE_SIZE TYPE_U32
#else
#  define TYPE_SIZE TYPE_U64
#endif /* TYPE_SIZE */

static inline const char *IntegralTypeToStr(IntegralType Type)
{
    static const char *StrLut[] = {
        [TYPE_INVALID] = "invalid",
        [TYPE_I8] = "int8",
        [TYPE_I16] = "int16",
        [TYPE_I32] = "int32",
        [TYPE_I64] = "int64",
        [TYPE_U8] = "uint8",
        [TYPE_U16] = "uint16",
        [TYPE_U32] = "uint32",
        [TYPE_U64] = "uint64",
        [TYPE_F32] = "float",
        [TYPE_F64] = "double",
        [TYPE_FUNCTION] = "function",
        [TYPE_BOOLEAN] = "boolean",
        [TYPE_STRING] = "string",
        [TYPE_RECORD] = "record",
        [TYPE_POINTER] = "pointer",
        [TYPE_CHAR] = "char",
    };
    PASCAL_STATIC_ASSERT(TYPE_COUNT == STATIC_ARRAY_SIZE(StrLut), "Missing type");
    if (Type < TYPE_COUNT)
        return StrLut[Type];
    return "invalid";
}

static inline U32 IntegralTypeSize(IntegralType Type)
{
    switch (Type)
    {
    case TYPE_BOOLEAN:
    case TYPE_I8:
    case TYPE_U8:
    case TYPE_CHAR:
        return 1;
    case TYPE_I16:
    case TYPE_U16:
        return 2;
    case TYPE_I32:
    case TYPE_U32:
    case TYPE_F32:
        return 4;
    case TYPE_I64:
    case TYPE_U64:
    case TYPE_F64:
        return 8;
    case TYPE_FUNCTION:
    case TYPE_POINTER:
        return sizeof(void*);
    case TYPE_STRING:
        return sizeof(PascalStr);

    default: 
    {
        PASCAL_UNREACHABLE("%s is not an integral type.", IntegralTypeToStr(Type));
    } break;
    }
    return 0;
}


static inline bool IntegralTypeIsSigned(IntegralType Type)
{
    return TYPE_I8 <= Type && Type <= TYPE_I64;
}


static inline bool IntegralTypeIsInteger(IntegralType Type)
{
    switch (Type)
    {
    case TYPE_I8:
    case TYPE_I16:
    case TYPE_I32:
    case TYPE_I64:
    case TYPE_U8:
    case TYPE_U16:
    case TYPE_U32:
    case TYPE_U64:
        return true;
    default: return false;
    }
}

static inline bool IntegralTypeIsFloat(IntegralType Type)
{
    return TYPE_F64 == Type || TYPE_F32 == Type;
}

static inline bool IntegralTypeIsCompatibleWithF64(IntegralType Type)
{
    return IntegralTypeIsInteger(Type) || IntegralTypeIsFloat(Type);
}

static inline bool IntegralTypeIsOrdinal(IntegralType Type)
{
    return IntegralTypeIsInteger(Type) || TYPE_BOOLEAN == Type || TYPE_CHAR == Type;
}





#endif /* PASCAL_INTEGRAL_TYPE_H */

