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
    TYPE_PROCEDURE = TYPE_FUNCTION,
    TYPE_BOOLEAN,
    TYPE_COUNT,
} IntegralType;

const char *IntegralTypeToStr(IntegralType Type);

bool IntegralTypeIsSigned(IntegralType Type);




#endif /* PASCAL_INTEGRAL_TYPE_H */

