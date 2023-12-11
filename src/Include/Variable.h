#ifndef PASCAL_VARIABLE_H
#define PASCAL_VARIABLE_H


#include "Common.h"
#include "IntegralTypes.h"
#include "Vartab.h"
#include "PascalString.h"



typedef struct VarLocation VarLocation;
typedef struct PascalVar
{
    const U8 *Str;
    UInt Len;
    U32 Line;
    U32 Hash;

    IntegralType Type;
    VarLocation *Location;
} PascalVar;



typedef struct VarLiteral 
{
    union {
        U64 Int;
        F64 Flt;
        bool Bool;
        GenericPtr Ptr;
        PascalStr Str;
    };
} VarLiteral;

typedef struct VarMemory
{
    U32 Location;
    UInt RegPtr;
} VarMemory;

typedef struct VarRegister
{
    UInt ID;
} VarRegister;

typedef struct VarSubroutine
{
    U32 Location;

	bool Defined;
	U32 Line;
	U32 *References;
	U32 RefCount, RefCap;

    U32 ArgCount, Cap;
	U32 StackArgSize;
	PascalVar *Args;

    bool HasReturnType;
    IntegralType ReturnType;
    PascalVartab Scope;
} VarSubroutine;

typedef enum VarLocationType
{
    VAR_INVALID = 0,
    VAR_REG,
    VAR_MEM,
    VAR_LIT,
    VAR_SUBROUTINE,
} VarLocationType;




struct VarLocation 
{
    VarLocationType LocationType;
    IntegralType Type;
    U32 Size;
    union {
        PascalVar PointsAt;
        PascalVartab Record;
    };
    union {
        VarMemory Memory;
        VarRegister Register;
        VarSubroutine Subroutine;
        VarLiteral Literal;
    } As;
};



static inline F64 VarLiteralToF64(VarLiteral Literal, IntegralType Type)
{
    if (IntegralTypeIsInteger(Type))
    {
        return Literal.Int;
    }
    else if (IntegralTypeIsFloat(Type))
    {
        return Literal.Flt;
    }
    PASCAL_UNREACHABLE("cannot convert %s into f64", IntegralTypeToStr(Type));
    return 0;
}



#endif /* PASCAL_VARIABLE_H */

