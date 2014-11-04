#include "read_types.h"
#include <stdbool.h>

static Dwarf_Debug dwarfHandle;
static Dwarf_Error err;
static FILE *dwarfFile;

int types_init(char* executableName){
  dwarfHandle = 0;

  if (strlen(executableName) < 2) {
    fprintf(stderr, "Expected a program name as argument\n");
    return -1;
  }

  if ((dwarfFile = fopen(executableName, "r")) < 0) {
    perror("Error opening file.");
    return -1;
  }

  int fd = fileno(dwarfFile);

  if (dwarf_init(fd, DW_DLC_READ, 0, 0, &dwarfHandle, &err) != DW_DLV_OK) {
    fprintf(stderr, "Failed DWARF initialization\n");
    return -1;
  }

  return 0;
}

int types_finalize(void){
  if (dwarf_finish(dwarfHandle, &err) != DW_DLV_OK) {
    fprintf(stderr, "Failed DWARF finalization\n");
    return -1;
  }

  fclose(dwarfFile);
  return 0;
}

void pc_range(Dwarf_Debug dgb, Dwarf_Die fn_die, Dwarf_Addr* lowPC, Dwarf_Addr* highPC){
  Dwarf_Error err;

  Dwarf_Signed attrcount, i;

  /*
    char dieName[50];
    int rc = dwarf_diename(fn_die, dieName, &err);

    if (rc == DW_DLV_ERROR){
    perror("Error in dwarf_diename\n");
    } else if (rc == DW_DLV_NO_ENTRY){
    return;
    }
  */

  if(dwarf_lowpc(fn_die, lowPC, &err) != DW_DLV_OK){
    perror("Error in getting low pc\n");
  }

  Dwarf_Attribute highAttr;
  if(dwarf_attr(fn_die, DW_AT_high_pc, &highAttr, &err) != DW_DLV_OK){
    perror("Error in getting high pc attribute\n");
  }

  Dwarf_Half highForm;

  if(dwarf_whatform(highAttr, &highForm, &err) != DW_DLV_OK){
    perror("Error in getting high pc form\n");
  }


  if(highForm != DW_FORM_addr){

    Dwarf_Half highEncoding;

    dwarf_whatattr(highAttr, &highEncoding, &err);

    Dwarf_Unsigned offset = 0;

    if(dwarf_formudata(highAttr, &offset, &err) != DW_DLV_OK){
      perror("Error in getting high pc value.\n");
      printf("%s\n", dwarf_errmsg(err));
    } else if(offset <= 0){
      // Function will never have negative program count.
      perror("Form data not read");
    }

    Dwarf_Addr high = *lowPC;
    high += offset;
    *highPC = high;

  } else {
    if(dwarf_highpc(fn_die, highPC, &err) != DW_DLV_OK){
      perror("Error in getting high pc\n");
    }
  }

  return;

}


/* - loop through all of them */
/* - if function, add to function lookup */
/* - if struct, add to struct description */



/* int pointers_in_fun */

int funcs_in_stack(Dwarf_Debug dbg, Dwarf_Addr* stack, int stackSize, Dwarf_Die** functions)
{
  Dwarf_Unsigned cu_header_length, abbrev_offset, next_cu_header;
  Dwarf_Half version_stamp, address_size;
  Dwarf_Error err;
  Dwarf_Die no_die = 0, cu_die, child_die;

  int status = DW_DLV_ERROR;

  *functions = calloc(stackSize, sizeof(Dwarf_Die*));
  int functionCount = 0;

  while(true){

    int status = dwarf_next_cu_header(dbg,
                                      &cu_header_length,
                                      &version_stamp,
                                      &abbrev_offset,
                                      &address_size,
                                      &next_cu_header,
                                      &err);

    /* Find compilation unit header */
    if (status == DW_DLV_ERROR){
      perror("Error reading DWARF cu header\n");
    } else if(status == DW_DLV_NO_ENTRY){
      return functionCount;
    } else if(status != DW_DLV_OK){
      perror("An undiagnosed error occurred.\n");
    }

    /* Expect the CU to have a single sibling - a DIE */
    if(dwarf_siblingof(dbg, no_die, &cu_die, &err) == DW_DLV_ERROR){
      perror("Error getting sibling of CU\n");
      return -1;
    }
    /* Expect the CU DIE to have children */
    if(dwarf_child(cu_die, &child_die, &err) == DW_DLV_ERROR){
      perror("Error getting child of CU DIE\n");
      return -1;
    }

    /* Now go over all children DIEs */
    while(true) {
      Dwarf_Half tag;

      if (dwarf_tag(child_die, &tag, &err) != DW_DLV_OK){
        perror("Error in dwarf_tag\n");
      }
      /* Only interested in subprogram DIEs here */

      if (tag == DW_TAG_subprogram){
        Dwarf_Addr lowPC = 0;
        Dwarf_Addr highPC = 0;

        pc_range(dbg, child_die, &lowPC, &highPC);

        for(int i=0; i < stackSize; i++){

          if(stack[i] > lowPC && stack[i] < highPC){
            (*functions)[functionCount] = child_die;
            functionCount++;

          }

        }
      }

      int rc;
      rc = dwarf_siblingof(dbg, child_die, &child_die, &err);
      if (rc == DW_DLV_ERROR){
        perror("Error getting sibling of DIE\n");
      } else if (rc == DW_DLV_NO_ENTRY){
        break; /* done */
      }
    }
  }

  return -1;
}

#define INITIAL_FRAME_SIZE 50

int getRoots(void){
  int frameCount = INITIAL_FRAME_SIZE;

  void** buffer = calloc(frameCount, sizeof(void*));
  int frames = backtrace((void **)buffer, frameCount);

  // Will always be equal or less.
  // Equal has possibility of missing frames.
  while(frames >= frameCount){
    frameCount += 10;
    buffer = realloc(buffer, frameCount * sizeof(void *));
    if(buffer == NULL){
      perror("Could not reallocate memory.\n");
    }
    frames = backtrace((void**)buffer, frameCount);
  }

  // To safely convert between the 32/64 bit pointers and the long long unsigned dwarf addrs.
  Dwarf_Addr* encoded_addrs = calloc(frameCount, sizeof(Dwarf_Addr));

  printf("frameCount: %d, frames: %d\n\n\n", frameCount, frames);

  int frames_tmp = frames;

  for(int i=0; i < frames_tmp; i++){
    printf("%p\n", buffer[i]);
    if(buffer[i] == NULL){
      frames--;
    }
  }

  printf("end of stack printing\n\n");

  for(int i=0; i < frames; i++){
    encoded_addrs[i] = (Dwarf_Addr)buffer[i];
  }

  free(buffer);

  for(int i=0; i < frames; i++){
    printf("0x%08llx\n", encoded_addrs[i]);
  }

  printf("\n\nAnd now the functions held: \n");

  if(dwarfHandle == NULL){
    perror("Uninitialized dwarf handle\n");
  } else {

    Dwarf_Die *functions;
    int functionCount = funcs_in_stack(dwarfHandle, encoded_addrs, frames, &functions);

    if(functionCount < 0){
      return -1;
    }

    printf("Results: \n");

    for(int i=0; i < functionCount; i++){
      char *dieName;
      int rc = dwarf_diename(functions[i], &dieName, &err);

      if (rc == DW_DLV_ERROR){
        perror("Error in dwarf_diename\n");
      } else if (rc == DW_DLV_NO_ENTRY){
        return -1;
      } else {
        printf("%s\n", dieName);
      }

    }

    free(functions);
  }

  free(encoded_addrs);

  return 0;
};

int typeOf(Dwarf_Debug dbg, Dwarf_Die die, Dwarf_Die* type_die, Dwarf_Error* err){
  Dwarf_Attribute type;
  Dwarf_Off ref_off = 0;

  if(dwarf_attr(die, DW_AT_type, &type, err) != DW_DLV_OK){
    perror("Error in getting type attribute\n");
    return -1;
  }

  if(dwarf_global_formref(type, &ref_off, err) != DW_DLV_OK){
    perror("Error in getting type offset\n");
    return -1;
  }

  if(dwarf_offdie(dbg, ref_off, type_die, err) != DW_DLV_OK){
    perror("Error in getting die at offset\n");
    return -1;
  }

  return DW_DLV_OK;
}


#define INITIAL_ROOT_SIZE 30
typedef struct {
  Dwarf_Die* type_die;
  void* location;
} RootPointer;

typedef struct {
  int filled;
  int capacity;
  RootPointer** contents;
} FunctionRoots;

void newFunctionRoots(FunctionRoots** roots){
  RootPointer* pointers = calloc(INITIAL_ROOT_SIZE, sizeof(RootPointer *));
  *roots = calloc(1, sizeof(FunctionRoots));
  (*roots)->filled = 0;
  (*roots)->capacity = INITIAL_ROOT_SIZE;
  (*roots)->contents = &pointers;
  return;
}

int varLocation(Dwarf_Debug dbg,
                Dwarf_Addr program_counter,
                Dwarf_Die fun_die,
                Dwarf_Die child_die,
                void** location,
                Dwarf_Error* err){

  // Program counter is return address, so can be used to find
  // saved registers

  Dwarf_Attribute die_location;

  if(dwarf_attr(child_die, DW_AT_location, &die_location, err) != DW_DLV_OK){
    perror("Error in getting location attribute\n");
    return -1;
  }

  Dwarf_Signed numberOfExpressions;
  Dwarf_Locdesc*** llbuf;
  
  if(dwarf_loclist_n(die_location, llbuf, &numberOfExpressions, err) != DW_DLV_OK){
    perror("Error in getting location attribute\n");
    return -1;
  }

  return 0;
  
}


// TODO: lexical blocks don't have their own frame bases.
// But otherwise share everything with this.
void typeFun(Dwarf_Addr pc, Dwarf_Debug dbg, Dwarf_Die fn_die, FunctionRoots* roots){
  Dwarf_Error err;
  Dwarf_Die child_die;

  /* Expect the CU DIE to have children */
  if(dwarf_child(fn_die, &child_die, &err) == DW_DLV_ERROR){
    perror("Error getting child of function DIE\n");
    return;
  }

  Dwarf_Half tag;
  Dwarf_Addr lowPC, highPC;

  while(1){
    if (dwarf_tag(child_die, &tag, &err) != DW_DLV_OK){
      perror("Error in dwarf_tag\n");
    }
    if (tag == DW_TAG_lexical_block){

      pc_range(dbg, child_die, &lowPC, &highPC);
      if(pc > lowPC && pc < highPC){
        continue;
        /* typeScope(pc, dbg, child_die, pointer_store_size, pointers); */
      }

    } else if(tag == DW_TAG_variable){

      Dwarf_Die type_die;
      if(typeOf(dbg, child_die, &type_die, &err) != DW_DLV_OK){
        perror("Error in typing variable\n");
      }

      Dwarf_Half type_tag;
      
      if (dwarf_tag(type_die, &type_tag, &err) != DW_DLV_OK){
        perror("Error in dwarf_tag\n");
      }

      if(type_tag == DW_TAG_pointer_type){

        void* pointer_location;

        if(varLocation(dbg, pc, fn_die, child_die, &pointer_location, &err) != DW_DLV_OK){
          perror("Error deriving the location of a variable\n");
        }
        
        if(roots->filled == roots->capacity - 1){
          roots->contents = realloc(roots->contents, (roots->capacity) * 2 * sizeof(RootPointer *));
          roots->capacity = (roots->capacity) * 2;
        }

        RootPointer* root = calloc(1, sizeof(RootPointer));
        root->type_die = &type_die;
        root->location = &pointer_location;
        
        roots->contents[roots->filled] = root;
        
        (roots->filled)++;
      }
    }


    int rc = dwarf_siblingof(dbg, child_die, &child_die, &err);

    if (rc == DW_DLV_ERROR){
      perror("Error getting sibling of DIE\n");
    } else if (rc == DW_DLV_NO_ENTRY){
      return;
    }
  }
};
