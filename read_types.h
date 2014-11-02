
//pass in file handle, this reads in the dwarf stuff, returns pointer to custom type.
//ask for type of variable, get type
//ask for information about struct layout.

#include <dwarf.h>
#include <libdwarf.h>
#include <stdio.h>
#include "read_types.h"
#include <execinfo.h>
#include <stdlib.h>
#include <unistd.h>

int types_init(char* executableName);
int types_finalize(void);
int getRoots(int frameCount);
