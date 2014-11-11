
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

typedef struct {
  Dwarf_Die type_die;
  void* location;
} RootPointer;

typedef struct {
  int filled;
  int capacity;
  RootPointer* contents;
} TypedPointers;

typedef struct {
  unw_cursor_t cursor;
  Dwarf_Die fn_die;
  Dwarf_Addr pc;
  Dwarf_Addr sp;
} LiveFunction;

typedef struct {
  int count;
  int capacity;
  LiveFunction *stack;
} CallStack;

const uint8_t x86_dwarf_to_libunwind_regnum[19];

int types_init(char* executableName);
int types_finalize(void);

void pc_range(Dwarf_Debug dgb,
              Dwarf_Die fn_die,
              Dwarf_Addr* lowPC,
              Dwarf_Addr* highPC);


int dwarf_backtrace(CallStack** callStack);

int type_of(Dwarf_Debug dbg,
           Dwarf_Die die,
           Dwarf_Die* type_die,
           Dwarf_Error* err);

int var_location(Dwarf_Debug dbg,
                LiveFunction* fun,
                Dwarf_Die child_die,
                void** location,
                Dwarf_Error* err);

void type_fun(Dwarf_Debug dbg,
             LiveFunction* fun,
             TypedPointers* roots,
             Dwarf_Error* err);

// Structure has byte_size
// Stack array has subrange_type in array_type.
// This has an upper bound of the largest valid index.
int pointers_in_struct(Dwarf_Die structure, TypedPointers* pointers);

int type_roots(TypedPointers* out);

#endif
