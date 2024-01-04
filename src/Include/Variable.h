#ifndef PASCAL_VARIABLE_H
#define PASCAL_VARIABLE_H


#include "Common.h"
#include "IntegralTypes.h"
#include "Vartab.h"
#include "PascalString.h"

#include "StringView.h"



struct SubroutineParameterList 
{
    PascalVar *Params;
    U32 Count;
};

#define SUBROUTINE_INVALID_LOCATION ((U32)-1)
struct SubroutineData
{
    /* owns by the compiler */
    const VarType *ReturnType;
    SubroutineParameterList ParameterList;
    PascalVartab Scope;
    U32 StackArgSize;
    U32 HiddenParamCount;
};

struct VarType 
{
    IntegralType Integral;
    U32 Size;
    union {
        struct VarType *Pointee;
        struct {
            StringView Name;
            PascalVartab Field;
        } Record;
        SubroutineData Subroutine;
    } As;
};

struct PascalVar
{
    StringView Str;
    U32 Line;
    U32 Hash;

    VarLocation *Location;
    VarType Type;
};




union VarLiteral 
{
    U64 Int, Chr;
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
    VarType Type;
    VarLocationType LocationType;
    union {
        VarMemory Memory;
        VarRegister Register;
        VarLiteral Literal;
        VarBuiltinRoutine BuiltinSubroutine;
        U32 SubroutineLocation;
    } As;
};


/* Assignment: '.Int = 1' */
#define VAR_LOCATION_LIT(Assignment, LitType) (VarLocation) {\
    .Type = VarTypeInit(LitType, IntegralTypeSize(LitType)), \
    .LocationType = VAR_LIT, \
    .As.Literal Assignment\
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

static inline const char *VarTypeToStr(VarType Type)
{
    if (TYPE_POINTER == Type.Integral && NULL != Type.As.Pointee)
    {
        return VarTypeToStr(*Type.As.Pointee);
    }
    return IntegralTypeToStr(Type.Integral);
}

static inline bool VarTypeEqual(const VarType *A, const VarType *B)
{
    if (!A || !B) 
        return false;
    if (A->Integral != B->Integral)
        return false;

    if (TYPE_POINTER == A->Integral)
    {
        /* opaque pointer, aka 'void *' in C */
        if (A->As.Pointee == NULL || B->As.Pointee == NULL)
            return true;

        /* chase the pointer */
        while (A && B && A->Integral == B->Integral && A->Integral == TYPE_POINTER)
        {
            A = A->As.Pointee;
            B = B->As.Pointee;
        }
        return VarTypeEqual(A, B);
    }
    if (TYPE_RECORD == A->Integral)
    {
        /* has to be the same table that was defined, names don't matter */
        return A->As.Record.Field.Table == B->As.Record.Field.Table;
    }
    return true;
}


static inline VarType VarTypeInit(IntegralType Type, U32 Size)
{
    return (VarType) {
        .Integral = Type, 
        .Size = Size,
        .As.Pointee = NULL,
    };
}

static inline VarType VarTypePtr(VarType *Pointee)
{
    return (VarType) {
        .Integral = TYPE_POINTER, 
        .Size = sizeof(void*),
        .As.Pointee = Pointee,
    };
}

static inline IntegralType PointerTypeOf(VarType Pointer)
{
    if (NULL == Pointer.As.Pointee)
        return TYPE_INVALID;
    return Pointer.As.Pointee->Integral;
}

static inline VarType VarTypeRecord(StringView Name, PascalVartab FieldTable, U32 Size)
{
    return (VarType) {
        .Integral = TYPE_RECORD, 
        .Size = Size,

        .As.Record.Name = Name,
        .As.Record.Field = FieldTable,
    };
}

static inline VarType VarTypeSubroutine(
        SubroutineParameterList ParameterList, PascalVartab Scope, const VarType *ReturnType, U32 StackArgSize)
{
    return (VarType) {
        .Integral = TYPE_FUNCTION,
        .Size = sizeof(void*),

        .As.Subroutine = {
            .ParameterList = ParameterList,
            .Scope = Scope, 
            .StackArgSize = StackArgSize,
            .ReturnType = ReturnType,
            .HiddenParamCount = ReturnType && TYPE_RECORD == ReturnType->Integral,
        },
    };
}


#define ParameterListInit() (SubroutineParameterList) { 0 }
static inline void ParameterListPush(SubroutineParameterList *ParameterList, 
        PascalGPA *Allocator, const PascalVar *Param)
{
    if (ParameterList->Count % 8 == 0)
    {
        ParameterList->Params = GPAReallocateArray(Allocator, ParameterList->Params, 
                *ParameterList->Params, ParameterList->Count + 8
        );
    }
    ParameterList->Params[ParameterList->Count++] = *Param;
}






#endif /* PASCAL_VARIABLE_H */

