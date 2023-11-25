#ifndef PASCAL_VARIABLE_H
#define PASCAL_VARIABLE_H


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

static inline IntegralType TypeOfLocation(const VarLocation *Location)
{
    switch (Location->LocationType)
    {
    case VAR_REG: return Location->As.Register.Type;
    case VAR_MEM: return Location->As.Memory.Type;
    case VAR_SUBROUTINE: return TYPE_POINTER;
    case VAR_INVALID: 
    {
        PASCAL_UNREACHABLE("TypeOfLocation: Invalid location type.");
    } break;
    }
    return TYPE_INVALID;
}




#endif /* PASCAL_VARIABLE_H */

