#ifndef PASCAL_H
#define PASCAL_H


#include <stdio.h>
#include "Common.h"

#define PASCAL_EXIT_SUCCESS 0
#define PASCAL_EXIT_FAILURE 1
/* the main Pascal entry point 
 * returns PASCAL_EXIT_SUCCESS or PASCAL_EXIT_FAILURE 
 */
int PascalMain(int argc, const U8 *const *argv);

/* reads in a file and compile it
 * returns PASCAL_EXIT_SUCCESS or PASCAL_EXIT_FAILURE
 */
int PascalRunFile(const U8 *InFileName, const U8 *OutFileName);


/* starts a command line repl session
 * returns PASCAL_EXIT_SUCCESS or PASCAL_EXIT_FAILURE 
 */
int PascalRepl(void);


void PascalPrintUsage(FILE *f, const U8 *ProgramName);

#endif /* PASCAL_H */

