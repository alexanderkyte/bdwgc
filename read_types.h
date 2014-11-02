
//pass in file handle, this reads in the dwarf stuff, returns pointer to custom type.
//ask for type of variable, get type
//ask for information about struct layout.

#ifndef DWARF_INCLUDES
#define DWARF_INCLUDES

#define _POSIX_C_SOURCE 1

#include <dwarf.h>
#include <libdwarf.h>
#include <stdio.h>
#include "read_types.h"
#include <execinfo.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

int types_init(char* executableName);
int types_finalize(void);
int getRoots(void);

#endif
