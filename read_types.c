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

int func_dies_in_stack(Dwarf_Debug dbg, CallStack* callstack, int stackSize){
  Dwarf_Unsigned cu_header_length, abbrev_offset, next_cu_header;
  Dwarf_Half version_stamp, address_size;
  Dwarf_Error err;
  Dwarf_Die no_die = 0, cu_die, child_die;

  int status = DW_DLV_ERROR;

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

          if(callstack->stack[i].pc > lowPC && callstack->stack[i].pc < highPC){
            if(callstack->stack[i].fn_die != NULL){
                fprintf(stderr, "Overlapping functions, err!");
                exit(1);
            }
            printf("assigning: %llu < %llu < %llu\n", lowPC, callstack->stack[i].pc, highPC);
            callstack->stack[i].fn_die = child_die;
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

#define INITIAL_LIVE_FUNCTION_SIZE 20

int dwarf_backtrace(CallStack** returnStack){
  *returnStack = calloc(sizeof(CallStack), 1);

  CallStack *callStack = *returnStack;
  callStack->stack = calloc(sizeof(LiveFunction), INITIAL_LIVE_FUNCTION_SIZE);
  callStack->count = 0;
  callStack->capacity = INITIAL_LIVE_FUNCTION_SIZE;

  unw_cursor_t cursor;

  unw_context_t uc;
  unw_word_t ip, sp;

  unw_getcontext(&uc);
  unw_init_local(&cursor, &uc);
  while (unw_step(&cursor) > 0) {
    unw_get_reg(&cursor, UNW_REG_IP, &ip);
    unw_get_reg(&cursor, UNW_REG_SP, &sp);
    if(callStack -> count >= callStack->capacity){
        callStack->capacity *= 2;
        callStack->stack = realloc(callStack->stack, callStack->capacity);
    }
    callStack->stack[callStack->count].cursor = cursor;
    callStack->stack[callStack->count].pc = (Dwarf_Addr) ip;
    callStack->stack[callStack->count].sp = (Dwarf_Addr) sp;
    callStack->count++;
  }

  return callStack->count;
}


int type_roots(TypedPointers* out){
  if(dwarfHandle == NULL){
    perror("Uninitialized dwarf handle\n");
  } else {

    CallStack* callStack;

    int frames = dwarf_backtrace(&callStack);

    printf("%d %d\n", callStack->count, frames);

    func_dies_in_stack(dwarfHandle, callStack, frames);

    for(int i=0; i < callStack->count; i++){
      printf("pc: %llu sp: %llu\n", callStack->stack[i].pc, callStack->stack[i].pc);
    }

    if(frames > 0){
      TypedPointers* roots = calloc(1, sizeof(TypedPointers));
      RootPointer* pointers = calloc(INITIAL_ROOT_SIZE, sizeof(RootPointer));
      roots->filled = 0;
      roots->capacity = INITIAL_ROOT_SIZE;
      roots->contents = pointers;


      printf("Results: \n");

      for(int i=0; i < callStack->count; i++){
        if(callStack->stack[i].fn_die != NULL){
          char* dieName;
          int rc = dwarf_diename(callStack->stack[i].fn_die, &dieName, &err);

          // type_fun(dwarfHandle, &(functions[i]), roots, &err);

          if (rc == DW_DLV_ERROR){
            fprintf(stderr, "Error in dwarf_diename: %s\n", dwarf_errmsg(err));
          } else if (rc == DW_DLV_NO_ENTRY){
            break;
          } else {
            printf("name: %s pc: %llu sp: %llu\n", dieName, callStack->stack[i].pc, callStack->stack[i].pc);
          }
        }

      }
      free(roots);
    }


    free(callStack);
  }

  return 0;
};

int type_of(Dwarf_Debug dbg, Dwarf_Die die, Dwarf_Die* type_die, Dwarf_Error* err){
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

// TODO: lexical blocks don't have their own frame bases.
// But otherwise share everything with this.
void type_fun(Dwarf_Debug dbg, LiveFunction* fun, TypedPointers* roots, Dwarf_Error* err){
  Dwarf_Die child_die;

  /* Expect the CU DIE to have children */
  if(dwarf_child(fun->fn_die, &child_die, err) == DW_DLV_ERROR){
    perror("Error getting child of function DIE\n");
    return;
  }

  Dwarf_Half tag;
  Dwarf_Addr lowPC, highPC;

  while(1){
    if (dwarf_tag(child_die, &tag, err) != DW_DLV_OK){
      perror("Error in dwarf_tag\n");
    }
    if (tag == DW_TAG_lexical_block){

      pc_range(dbg, child_die, &lowPC, &highPC);
      if(fun->pc > lowPC && fun->pc < highPC){
        /* typeScope(pc, dbg, child_die, pointer_store_size, pointers); */
      }

    } else if(tag == DW_TAG_variable){

      Dwarf_Die type_die;
      if(type_of(dbg, child_die, &type_die, err) != DW_DLV_OK){
        perror("Error in typing variable\n");
      }

      Dwarf_Half type_tag;

      if (dwarf_tag(type_die, &type_tag, err) != DW_DLV_OK){
        perror("Error in dwarf_tag\n");
      }

      if(type_tag == DW_TAG_pointer_type){

        void* pointer_location;

        printf("printing var start\n");
        if(var_location(dbg, fun, child_die, &pointer_location, err) != DW_DLV_OK){
          perror("Error deriving the location of a variable\n");
        }
        printf("\\ printing var start\n");

        if(roots->filled == roots->capacity - 1){
          roots->contents = realloc(roots->contents, (roots->capacity) * 2 * sizeof(RootPointer *));
          roots->capacity = (roots->capacity) * 2;
       }

        if(roots->filled >= roots->capacity){
          roots->contents = realloc(roots->contents, roots->capacity * 2);
          roots->capacity = roots->capacity * 2;
        }

        (roots->contents)[roots->filled].type_die;
        (roots->contents)[roots->filled].location = pointer_location;

        roots->filled++;
      }
    }


    int rc = dwarf_siblingof(dbg, child_die, &child_die, err);

    if(rc == DW_DLV_ERROR){
      perror("Error getting sibling of DIE\n");
    } else if(rc == DW_DLV_NO_ENTRY){
      return;
    }
  }
};

int var_location(Dwarf_Debug dbg,
                LiveFunction* fun,
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

  Dwarf_Locdesc** llbufarray;

  Dwarf_Signed number_of_expressions;

  if(dwarf_loclist_n(die_location, &llbufarray, &number_of_expressions, err) != DW_DLV_OK){
    perror("Error in getting location attribute\n");
    return -1;
  }

  for(int i = 0; i < number_of_expressions; ++i) {
    Dwarf_Locdesc* llbuf = llbufarray[i];

    if(llbuf->ld_s[i].lr_atom == DW_OP_fbreg){

      if(llbuf->ld_lopc != 0 &&
         llbuf->ld_lopc > fun->pc){
        continue;
      }

      if(llbuf->ld_hipc != 0 &&
         llbuf->ld_hipc < fun->pc){
        continue;
      }

      int offset = llbuf->ld_s[i].lr_number;

      /* Dwarf_Cie* cie_list; */
      /* Dwarf_Signed cie_count; */
      /* Dwarf_Fde* fde_list; */
      /* Dwarf_Signed fde_count; */

      /* if(dwarf_get_fde_list(dbg, &cie_list, &cie_count, &fde_list, &fde_count, err) != DW_DLV_OK){ */
      /*   fprintf(stderr, "Error getting fde list: %s\n", dwarf_errmsg(*err)); */
      /*   return -1; */
      /* } */


      /* Dwarf_Fde fde; */
      /* Dwarf_Addr lopc; */
      /* Dwarf_Addr hipc; */
      
      /* if(dwarf_get_fde_at_pc(fde_list, fun->pc, &fde, &lopc, &hipc, err) != DW_DLV_OK){ */
      /*   perror("Error getting fde for pc\n"); */
      /*   return -1; */
      /* } */

      
      printf("%d\n", offset);


    } else {
      printf("ir:");
      for(int j=0; j < llbuf->ld_cents; j++){
        printf("%x\n", llbuf->ld_s[j].lr_atom);
      }
      printf("\n\n");
    }

  }

   /* for (i = 0; i < no_of_elements; ++i) { */
   /*   dwarf_dealloc(dbg, llbufarray[i]->ld_s, DW_DLA_LOC_BLOCK); */
   /*   dwarf_dealloc(dbg, llbufarray[i], DW_DLA_LOCDESC); */
   /* } */
   /* dwarf_dealloc(dbg, llbufarray, DW_DLA_LIST); */

  // get the location type
  // if fbreg, get function call base, otherwise raise unimplemented

  return 0;
}
