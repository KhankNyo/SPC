#ifndef PASCAL_VM_COMPILER_H
#define PASCAL_VM_COMPILER_H



#include "Common.h"
#include "PVM/_Chunk.h"
#include "Vartab.h"

#define PVM_MAX_FUNCTION_COUNT 1024
#define PVM_MAX_SCOPE_COUNT 16              /* 15 nested functions */
#define PVM_MAX_VAR_PER_SCOPE (1 << 10)     /* limit on the allocator */
#define PVM_INITIAL_VAR_PER_SCOPE 32

#define VAR_ID_TYPE UINT32_MAX
#define VAR_ID_INVALID (UINT32_MAX - 1)

/* Returns true on success, false on failure and logs error to log file */
bool PVMCompile(const U8 *Source, 
        PascalVartab *PredefinedIdentifiers, PVMChunk *Chunk, 
        PascalGPA *GlobalAlloc, FILE *LogFile
);


#endif /* PASCAL_VM_COMPILER_H */

