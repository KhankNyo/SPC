#ifndef PASCAL_VM_COMPILER_H
#define PASCAL_VM_COMPILER_H



#include "Common.h"
#include "PVM/CodeChunk.h"
#include "Vartab.h"

#define PVM_MAX_FUNCTION_COUNT 1024
#define PVM_MAX_SCOPE_COUNT 8
#define PVM_MAX_VAR_PER_SCOPE (1 << 10)

#define VAR_ID_TYPE UINT32_MAX
#define VAR_ID_INVALID (UINT32_MAX - 1)

typedef U32 VarID;

/* Returns true on success, false on failure and logs error to log file */
bool PVMCompile(const U8 *Source, 
        PascalVartab *PredefinedIdentifiers, CodeChunk *Chunk, FILE *LogFile
);


#endif /* PASCAL_VM_COMPILER_H */

