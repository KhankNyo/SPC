#ifndef PASCAL_H
#define PASCAL_H


#include <stdio.h>
#include "Common.h"

#define PASCAL_EXIT_SUCCESS 0
#define PASCAL_EXIT_FAILURE 1
/* 
 * returns PASCAL_EXIT_SUCCESS or PASCAL_EXIT_FAILURE 
 */
int PascalMain(int argc, const U8 *const *argv);

/* 
 * returns PASCAL_EXIT_SUCCESS or PASCAL_EXIT_FAILURE
 */
int PascalRunFile(const U8 *InFileName, const U8 *OutFileName);


void PascalPrintUsage(FILE *f, const U8 *ProgramName);

#endif /* PASCAL_H */

