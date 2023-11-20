#ifndef PASCAL_VARIABLE_H
#define PASCAL_VARIABLE_H


#include "Common.h"
#include "IntegralTypes.h"


typedef struct NameAndType 
{
    const U8 *Name;
    UInt Len;
    IntegralType Type;
} NameAndType;



typedef struct LocalVar 
{
    UInt Size;
    U32 FPOffset;
} LocalVar;
typedef struct GlobalVar 
{
    UInt Size;
    U32 Location;
} GlobalVar;
typedef struct FunctionVar
{
    U32 Location;

    U32 ArgCount, Cap;
    NameAndType *Args;

    IntegralType ReturnType;
    bool HasReturnType;
} FunctionVar;
typedef struct RegisterVar 
{
    UInt ID;
} RegisterVar;

typedef enum VarLocationType 
{
    VAR_INVALID = 0,
    VAR_REG,
    VAR_LOCAL,
    VAR_GLOBAL,
    VAR_FUNCTION,

    VAR_TMP_REG,
    VAR_TMP_STK,
} VarLocationType;

typedef struct VarLocation 
{
    VarLocationType LocationType;
    IntegralType Type;

    union {
        RegisterVar Reg;
        LocalVar Local;
        GlobalVar Global;
        FunctionVar Function;
    } As;
} VarLocation;




#endif /* PASCAL_VARIABLE_H */

