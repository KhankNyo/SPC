#ifndef PASCAL_TYPEDEFS_H
#define PASCAL_TYPEDEFS_H


#include <stdint.h>

typedef struct PVMCompiler PVMCompiler;
typedef struct PVMEmitter PVMEmitter;
typedef struct PascalTokenizer PascalTokenizer;
typedef struct CompilerFrame CompilerFrame;
typedef struct TmpIdentifiers TmpIdentifiers;
typedef struct Token Token;

typedef struct VarLocation VarLocation;
typedef union VarLiteral VarLiteral;
typedef struct VarRegister VarRegister;
typedef struct VarMemory VarMemory;
typedef struct VarSubroutine VarSubroutine;

typedef struct OptionalReturnValue OptionalReturnValue;
typedef OptionalReturnValue (*VarBuiltinRoutine)(PVMCompiler *, const Token *);


typedef union PascalStr PascalStr;
typedef struct PascalVartab PascalVartab;
typedef struct PascalVar PascalVar;



typedef uint8_t U8;
typedef uint16_t U16;
typedef uint32_t U32;
typedef uint64_t U64;
typedef size_t USize;
typedef unsigned UInt;

typedef int8_t I8;
typedef int16_t I16;
typedef int32_t I32;
typedef int64_t I64;
typedef ptrdiff_t ISize;



typedef double F64;
typedef float F32;
typedef long double LargeType;





#endif /* PASCAL_TYPEDEFS_H */

