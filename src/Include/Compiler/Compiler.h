#ifndef PASCAL_COMPILER_H
#define PASCAL_COMPILER_H



#include "Common.h"
#include "Vartab.h"
#include "PVM/Chunk.h"

#define PVM_MAX_FUNCTION_COUNT 1024
#define PVM_MAX_SCOPE_COUNT 16              /* 15 nested functions */
#define PVM_MAX_VAR_PER_SCOPE (1 << 10)     /* limit on the allocator */
#define PVM_INITIAL_VAR_PER_SCOPE 32

#define VAR_ID_TYPE UINT32_MAX
#define VAR_ID_INVALID (UINT32_MAX - 1)

typedef enum PVMCallConv 
{
    CALLCONV_MSX64,
} PVMCallConv;

typedef enum PascalCompileMode 
{
    COMPMODE_PROGRAM = 0,
    COMPMODE_REPL,
} PascalCompileMode;

typedef struct PascalCompileFlags 
{
    PVMCallConv CallConv;
    PascalCompileMode CompMode;
} PascalCompileFlags;

/* Returns true on success, false on failure and logs error to log file */
bool PascalCompile(const U8 *Source, 
        PascalCompileFlags Flags,
        PascalVartab *PredefinedIdentifiers, 
        PascalGPA *GlobalAlloc, FILE *LogFile,
        PVMChunk *OutChunk
);


#endif /* PASCAL_VM_COMPILER_H */

