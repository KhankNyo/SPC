#ifndef PASCAL_VARIABLE_H
#define PASCAL_VARIABLE_H


#include "Common.h"
#include "IntegralTypes.h"
#include "Vartab.h"
#include "PascalString.h"


struct PascalVar
{
    const U8 *Str;
    UInt Len;
    U32 Line;
    U32 Hash;

    IntegralType Type;
    VarLocation *Location;
};



union VarLiteral 
{
    U64 Int;
    F64 Flt;
    bool Bool;
    GenericPtr Ptr;
    PascalStr Str;
};

struct VarRegister 
{
    UInt ID;
    bool Persistent;
};

struct VarMemory 
{
    U32 Location;
    VarRegister RegPtr;
};

struct VarSubroutine
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
};

typedef enum VarLocationType
{
    VAR_INVALID = 0,
    VAR_FLAG,
    VAR_REG,
    VAR_MEM,
    VAR_LIT,
    VAR_BUILTIN,
    VAR_SUBROUTINE,
} VarLocationType;




struct VarLocation 
{
    VarLocationType LocationType;
    IntegralType Type;
    U32 Size;
    union {
        PascalVar Var;
        PascalVartab Record;
    } PointerTo;
    union {
        VarMemory Memory;
        VarRegister Register;
        VarSubroutine Subroutine;
        VarLiteral Literal;
        VarBuiltinRoutine BuiltinSubroutine;
    } As;
};


#define VAR_LOCATION_LIT(Assignment, LitType) (VarLocation) {\
    .As.Literal Assignment, .Type = LitType, .LocationType = VAR_LIT,\
}



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

