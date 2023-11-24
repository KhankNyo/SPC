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




typedef struct VarMemory
{
    U32 Location;
    IntegralType Type;
    bool IsGlobal;
} VarMemory;

typedef struct VarRegister
{
    IntegralType Type;
    UInt ID;
} VarRegister;

typedef struct VarSubroutine
{
    U32 Location;

    U32 ArgCount, Cap;
    PascalVar **Args; /* owned by the function's scope */

    bool HasReturnType;
    IntegralType ReturnType;
} VarSubroutine;

typedef enum VarLocationType
{
    VAR_INVALID = 0,
    VAR_REG,
    VAR_MEM,
    VAR_SUBROUTINE,
} VarLocationType;




struct VarLocation 
{
    VarLocationType LocationType;
    union {
        VarMemory Memory;
        VarRegister Register;
        VarSubroutine Subroutine;
    } As;
};


#endif /* PASCAL_VM2_VARIABLES_H */

