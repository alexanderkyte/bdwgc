#include "read_types.h"
#include <stdbool.h>


static Dwarf_Debug dwarfHandle;
static Dwarf_Error global_err;
static FILE *dwarfFile;

static const uint8_t x86_dwarf_to_libunwind_regnum[19] = {                   
  UNW_X86_EAX, UNW_X86_ECX, UNW_X86_EDX, UNW_X86_EBX,                                                                                                                                                 
  UNW_X86_ESP, UNW_X86_EBP, UNW_X86_ESI, UNW_X86_EDI,
  UNW_X86_EIP, UNW_X86_EFLAGS, UNW_X86_TRAPNO,
  UNW_X86_ST0, UNW_X86_ST1, UNW_X86_ST2, UNW_X86_ST3,
  UNW_X86_ST4, UNW_X86_ST5, UNW_X86_ST6, UNW_X86_ST7
};

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

  if (dwarf_init(fd, DW_DLC_READ, 0, 0, &dwarfHandle, &global_err) != DW_DLV_OK) {
    fprintf(stderr, "Failed DWARF initialization\n");
    return -1;
  }

  return 0;
}

int types_finalize(void){
  if (dwarf_finish(dwarfHandle, &global_err) != DW_DLV_OK) {
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

int read_dwarf(Dwarf_Debug dbg){
  Dwarf_Unsigned cu_header_length, abbrev_offset, next_cu_header;
  Dwarf_Half version_stamp, address_size;
  Dwarf_Error err;
  Dwarf_Die no_die = 0, cu_die, child_die;

  Array types = newHeapArray(INITIAL_TYPE_LIST_SIZE);
  Array functions = newHeapArray(INITIAL_FUNCTION_LIST_SIZE);

  // Type list
  // Function list
  

  bool done = false;
  
  while(!done){
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
      return -1;
    } else if(status == DW_DLV_NO_ENTRY){
      done = true;
      continue;
    } else if(status != DW_DLV_OK){
      perror("An undiagnosed error occurred.\n");
      return -1;
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
    bool doneWithCU = false;
    
    while(!doneWithCU){
      Dwarf_Half tag;

      if (dwarf_tag(child_die, &tag, &err) != DW_DLV_OK){
        perror("Error in dwarf_tag\n");
      }

      if(tag == DW_TAG_subprogram){
        dwarf_read_function(dbg, &child_die, functions);
      } else if(tag == DW_TAG_structure_type) {
        dwarf_read_struct(dbg, &child_die, types);
      } else if(tag == DW_TAG_pointer_type) {
        dwarf_read_pointer(dbg, &child_die, types);
      } else if(tag == DW_TAG_array_type) {
        dwarf_read_array(dbg, &child_die, types);
      } else {
        fprintf(stderr, "Missed die type: %x\n", tag);
        return -1
      }


      // What to do if someone defines a type inside of a function?
      // We'll handle that later. Globally scope the type, since the
      // dwarf type offset is unique.

      int rc = dwarf_siblingof(dbg, child_die, &child_die, &err);
      if (rc == DW_DLV_ERROR){
        perror("Error getting sibling of DIE\n");
        return -1;
      } else if (rc == DW_DLV_NO_ENTRY){
        doneWithCU = true;
      }
      
    }      
  }
  
  return functionCount;
}

int dwarf_read_function(Dwarf_Debug dbg, Dwarf_Die* fn_die, Array functions, Dwarf_Err* err){

  #ifdef DEBUG
  char* dieName;
  if(dwarf_diename(fn_die, &dieName, err) != DW_DLV_OK){
    fprintf(stderr, "Error reading function name\n");
    return -1;
  }
  #endif

  Scope* top_scope;

  if(dwarf_read_scope(dbg, fn_die, &top_scope, err) != DW_DLV_SUCCESS){
    fprintf(stderr, "Problem reading scope in function reader.\n");
    return -1;
  }

  Function* fun = calloc(sizeof(Function), 1);
  fun->topScope = top_scope;
  
  #ifdef DEBUG
  fn_type->dieName = dieName;
  #endif

  arrayAppend(functions, top_scope);

  return DW_DLV_OK;
}

int dwarf_read_scope(Dwarf_Debug dbg, Dwarf_Die* top_die, Scope** top_scope, Dwarf_Err* err){
  Dwarf_Addr lowPC = 0;
  Dwarf_Addr highPC = 0;

  pc_range(dbg, top_die, &lowPC, &highPC);

  void* castedLowPC = lowPC;
  void* castedHighPC = highPC;

  *top_scope = calloc(sizeof(Scope), 1);

  
  Dwarf_Die child_die;
  
  if(dwarf_child(top_die, &child_die, &err) == DW_DLV_ERROR){
    perror("Error getting child of CU DIE\n");
    return -1;
  }

  bool done = false;
  
  while(!done){

    Dwarf_Half tag;

    if(dwarf_tag(child_die, &tag, &err) != DW_DLV_OK){
      perror("Error in dwarf_tag\n");
      return -1;
    }
    
    // loop through children
    // if variable, find out if pointer type, add to contents
    // if scope, add to children

    if(tag == DW_TAG_formal_parameter || 
       tag == DW_TAG_variable){

      Dwarf_Die type_die;
      Dwarf_Half type_tag;
      
      if(type_of(dbg, child_die, &type_die, err) != DW_DLV_OK){
        fprintf(stderr, "Error when getting scope variable's type tag.\n");
        return -1;
      }

      if(dwarf_tag(child_die, &type_tag, &err) != DW_DLV_OK){
        perror("Error in dwarf_tag\n");
        return -1;
      }

      if(type_tag == DW_TAG_pointer_type){

        if( (*top_scope)->contents == NULL ){
          (*top_scope)->contents = newHeapArray(DEFAULT_CAPACITY);
          // 5 elements maybe?
        }
        
        if(dwarf_read_root(dbg, child_die, (*top_scope)->contents, err) != DW_DLV_OK){
          fprintf(stderr, "Error reading pointer\n");
          return -1;
        }
      }
      // type has tag pointer type?

    } else if(tag == DW_TAG_lexical_block){
        if( (*top_scope)->children == NULL ){
          (*top_scope)->children = newHeapArray(DEFAULT_CAPACITY);
          // 5 elements maybe?
          Scope* child_scope;
          if(dwarf_read_scope(dbg, child_die, &child_scope, err) != DW_DLV_OK){
            fprintf(stderr, "error recursing on scope.\n");
            return -1;
          }
          arrayAppend((*top_scope)->children, child_scope);
        }
    }
              
    int rc = dwarf_siblingof(dbg, child_die, &child_die, err);
    if (rc == DW_DLV_ERROR){
      perror("Error getting sibling of DIE\n");
      return -1;
    } else if (rc == DW_DLV_NO_ENTRY){
      done = true;
    }
  }
  
}

int dwarf_read_root(Dwarf_Debug dbg, Dwarf_Die* root_die, Array roots, Dwarf_Err* err){
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

  Dwarf_Attribute type;
  Dwarf_Off ref_off = 0;
  int status;

  if((status = dwarf_attr(die, DW_AT_type, &type, err)) != DW_DLV_OK){
    if(status == DW_DLV_NO_ENTRY){
      fprintf(stderr, "No type information associated with die\n");
    } else {
      fprintf(stderr, "Error %d in getting type attribute: %s\n", status, dwarf_errmsg(*err));
    }
    return status;
  }

  if((status = dwarf_global_formref(type, &ref_off, err)) != DW_DLV_OK){
    fprintf(stderr, "Error %d in getting type offset: %s\n", status, dwarf_errmsg(*err));
    return status;
  }

  Root* root = calloc(sizeof(Root), 1);
  root->locdesc = llbufarray;
  root->expression_count = number_of_expressions;
  root->type = ref_off;
}

int dwarf_read_struct(Dwarf_Debug dbg, Dwarf_Die* type_die, Array types, Dwarf_Err* err){
  Dwarf_Off offset;
  
  if(dwarf_global_die_offset(fn_die, &offset,err) != DW_DLV_SUCCESS){
    fprintf(stderr, "Error getting die offset.\n");
    return -1;
  }
  
}

int dwarf_read_array(Dwarf_Debug dbg, Dwarf_Die* type_die, Array types, Dwarf_Err* err);

int dwarf_read_pointer(Dwarf_Debug dbg, Dwarf_Die* type_die, Array types, Dwarf_Err* err);




  Dwarf_Half tag;
  
  int rc = dwarf_siblingof(dbg, child_die, &child_die, &err);
  if (rc == DW_DLV_ERROR){
    perror("Error getting sibling of DIE\n");
  } else if (rc == DW_DLV_NO_ENTRY){
    doneWithCU = true;
  }
}

int func_dies_in_stack(Dwarf_Debug dbg, CallStack* callstack, int stackSize){
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
        callStack->stack = realloc(callStack->stack, callStack->capacity * sizeof(LiveFunction *));
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
      printf("pc: %p sp: %p\n", (void *)callStack->stack[i].pc, (void *)callStack->stack[i].sp);
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
          type_fun(dwarfHandle, &(callStack->stack[i]), roots, &global_err);

          for(int i=0; i < roots->filled; i++){
            RootPointer root = roots->contents[i];
            
            Dwarf_Die type_die = root.type_die;
            Dwarf_Half type_tag;
            
            if (dwarf_tag(type_die, &type_tag, &global_err) != DW_DLV_OK){
              perror("Error in dwarf_tag\n");
            }

            printf("pointer: %p, tag:", root.location);

            bool done = false;
            
            while(!done && type_tag == DW_TAG_pointer_type){
            
              switch(type_of(dwarfHandle, type_die, &type_die, &global_err)){
              case DW_DLV_OK:
                break;
              case DW_DLV_NO_ENTRY:
                printf(" void * ");
                done = true;
                break;
              default:
                fprintf(stderr, "error getting pointed-to type\n");
                return -1;
              }

              if (dwarf_tag(type_die, &type_tag, &global_err) != DW_DLV_OK){
                perror("Error in dwarf_tag\n");
                return -1;
              }

              printf("*");
              
            }

            printf("0x%04x\n", type_tag);
          }

          if (rc == DW_DLV_ERROR){
            fprintf(stderr, "Error in dwarf_diename: %s\n", dwarf_errmsg(global_err));
          } else if (rc == DW_DLV_NO_ENTRY){
            break;
          } else {
            printf("name: %s pc: %llu sp: %llu\n", dieName, callStack->stack[i].pc, callStack->stack[i].sp);
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
  int status;

  if((status = dwarf_attr(die, DW_AT_type, &type, err)) != DW_DLV_OK){
    if(status == DW_DLV_NO_ENTRY){
      fprintf(stderr, "No type information associated with die\n");
    } else {
      fprintf(stderr, "Error %d in getting type attribute: %s\n", status, dwarf_errmsg(*err));
    }
    return status;
  }

  if((status = dwarf_global_formref(type, &ref_off, err)) != DW_DLV_OK){
    fprintf(stderr, "Error %d in getting type offset: %s\n", status, dwarf_errmsg(*err));
    return status;
  }

  if((status = dwarf_offdie(dbg, ref_off, type_die, err)) != DW_DLV_OK){
    fprintf(stderr, "Error %d in getting die at offset: %s\n", status, dwarf_errmsg(*err));
    return status;
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

    } else if(tag == DW_TAG_formal_parameter){
      //
    } else if(tag == DW_TAG_variable){

      Dwarf_Die type_die;
      if(type_of(dbg, child_die, &type_die, err) != DW_DLV_OK){
        perror("Error in typing variable\n");
      }
      // TODO: Handle typedefs
      // TODO: Handle const indirection

      Dwarf_Half type_tag;

      if (dwarf_tag(type_die, &type_tag, err) != DW_DLV_OK){
        perror("Error in dwarf_tag\n");
      }

      if(type_tag == DW_TAG_pointer_type){

        void* pointer_location = NULL;

        printf("printing var start\n");
        if(var_location(dbg, fun, child_die, &pointer_location, err) != DW_DLV_OK){
          perror("Error deriving the location of a variable\n");
        }

        if(roots->filled == roots->capacity - 1){
          roots->contents = realloc(roots->contents, (roots->capacity) * 2 * sizeof(RootPointer *));
          roots->capacity = (roots->capacity) * 2;
       }

        if(roots->filled >= roots->capacity){
          roots->contents = realloc(roots->contents, roots->capacity * 2);
          roots->capacity = roots->capacity * 2;
        }

        (roots->contents)[roots->filled].type_die = type_die;
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

    Dwarf_Small op = llbuf->ld_s[i].lr_atom;
    
    if(op == DW_OP_fbreg){

      /* printf("fbreg: \n"); */

      if(llbuf->ld_lopc != 0 &&
         llbuf->ld_lopc > fun->pc){
        continue;
      }

      if(llbuf->ld_hipc != 0 &&
         llbuf->ld_hipc < fun->pc){
        continue;
      }

      Dwarf_Unsigned offset = llbuf->ld_s[i].lr_number;

      /* printf("setting to %llu\n", fun->sp + (unsigned long long)offset); */

      *location = (void *)fun->sp + offset;
      
      return DW_DLV_OK;

    } else {

      Dwarf_Signed offset;

      unw_regnum_t reg;
      unw_word_t reg_value;
      
      switch(op){
      case DW_OP_breg0:
      case DW_OP_breg1:
      case DW_OP_breg2:
      case DW_OP_breg3:
      case DW_OP_breg4:
      case DW_OP_breg5:
      case DW_OP_breg6:
      case DW_OP_breg7:
      case DW_OP_breg8:
      case DW_OP_breg9:
      case DW_OP_breg10:
      case DW_OP_breg11:
      case DW_OP_breg12:
      case DW_OP_breg13:
      case DW_OP_breg14:
      case DW_OP_breg15:
      case DW_OP_breg16:
      case DW_OP_breg17:
      case DW_OP_breg18: {
        reg = x86_dwarf_to_libunwind_regnum[op-DW_OP_breg0];
          
        offset = llbuf->ld_s[i].lr_number;

        if(unw_get_reg(&(fun->cursor), reg, &reg_value) != 0){
          fprintf(stderr, "Error occurred reading register\n");
        }

        /* printf("breg %d register: %s register value: %p offset: %ld\n", reg, unw_regname(reg), (void *)reg_value, offset); */

        *location = (void *)reg_value + offset;

        return DW_DLV_OK;
        
        break;
      }

      default:
        printf("other: %x\n", op);

      }

    }

  }

  return 0;
}
