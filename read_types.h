
//pass in file handle, this reads in the dwarf stuff, returns pointer to custom type.
//ask for type of variable, get type
//ask for information about struct layout.

#ifndef DWARF_INCLUDES
#define DWARF_INCLUDES

#define _DEFAULT_SOURCE

#include <dwarf.h>
#include <libdwarf.h>

#include <stdio.h>
#include "read_types.h"
#include <execinfo.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define UNW_LOCAL_ONLY
#include <libunwind.h>

#define INITIAL_ROOT_SIZE 30
#define INITIAL_BACKTRACE_SIZE 50

#include "dwarf_graph.h"

const uint8_t x86_dwarf_to_libunwind_regnum[19];

void pc_range(Dwarf_Debug dgb,
              Dwarf_Die* fn_die,
              Dwarf_Addr* lowPC,
              Dwarf_Addr* highPC);

int dwarf_backtrace(CallStack** callStack);

int type_of(Dwarf_Debug dbg,
           Dwarf_Die die,
           Dwarf_Die* type_die,
           Dwarf_Error* err);

bool is_pointer(Dwarf_Die* die, Dwarf_Error* err);
int type_off(Dwarf_Die* die, Dwarf_Off* ref_off, Dwarf_Error* err);
int type_of(Dwarf_Debug dbg, Dwarf_Die* die, Dwarf_Die* type_die, Dwarf_Error* err);

int var_location(Dwarf_Debug dbg,
                LiveFunction* fun,
                Dwarf_Die* child_die,
                void** location,
                Dwarf_Error* err);

// CallStack and context are in parameters, roots are outparameters
int get_roots(CallStack* callStack, GCContext* context, Roots* roots);

int get_type(TypeKey key, Type* type);
int dwarf_read(const char* executable, GCContext** context);


#endif
