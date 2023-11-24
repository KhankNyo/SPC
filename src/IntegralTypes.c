

#include "Common.h"
#include "IntegralTypes.h"


const char *IntegralTypeToStr(IntegralType Type)
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
    };
    PASCAL_STATIC_ASSERT(TYPE_COUNT == STATIC_ARRAY_SIZE(StrLut), "Missing type for ParserIntegralTypeToStr()");
    if (Type < TYPE_COUNT)
        return StrLut[Type];
    return "invalid";
}

bool IntegralTypeIsSigned(IntegralType Type)
{
    return TYPE_I8 <= Type && Type <= TYPE_I64;
}



