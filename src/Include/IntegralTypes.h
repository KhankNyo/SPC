#ifndef PASCAL_INTEGRAL_TYPE_H
#define PASCAL_INTEGRAL_TYPE_H


#include "Common.h"

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
    TYPE_COUNT,
} IntegralType;

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
        [TYPE_POINTER] = "pointer",
    };
    PASCAL_STATIC_ASSERT(TYPE_COUNT == STATIC_ARRAY_SIZE(StrLut), "Missing type for ParserIntegralTypeToStr()");
    if (Type < TYPE_COUNT)
        return StrLut[Type];
    return "invalid";
}

static inline bool IntegralTypeIsSigned(IntegralType Type)
{
    return TYPE_I8 <= Type && Type <= TYPE_I64;
}





#endif /* PASCAL_INTEGRAL_TYPE_H */

