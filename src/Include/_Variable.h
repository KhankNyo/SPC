#ifndef PASCAL_VM2_VARIABLES_H
#define PASCAL_VM2_VARIABLES_H


#include "Common.h"
#include "IntegralTypes.h"

typedef struct VarLocation VarLocation;

typedef struct PascalVar 
{
    const U8 *Str;
    UInt Len;
    U32 Hash;

    IntegralType Type;
    VarLocation *Location;
} PascalVar;




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
    PascalVar **Args;

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

struct VarLocation 
{
    VarLocationType LocationType;
    IntegralType Type;

    union {
        RegisterVar Reg;
        LocalVar Local;
        GlobalVar Global;
        FunctionVar Function;
    } As;
};




#endif /* PASCAL_VM2_VARIABLES_H */

