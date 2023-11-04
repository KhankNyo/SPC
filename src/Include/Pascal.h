#ifndef PASCAL_H
#define PASCAL_H


#include <stdio.h>
#include "Common.h"

#define PASCAL_EXIT_SUCCESS 0
#define PASCAL_EXIT_FAILURE 1
int PascalMain(int argc, const U8 *const *argv);

void PascalPrintUsage(FILE *f, const U8 *ProgramName);

#endif /* PASCAL_H */

