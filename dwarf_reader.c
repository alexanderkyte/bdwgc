#include "read_types.h"

int types_init(const char* executableName, Dwarf_Debug* dbg, FILE** dwarfFile, Dwarf_Error* err){  
  if (strlen(executableName) < 2) {
    fprintf(stderr, "Expected a program name as argument\n");
    return -1;
  }

  if ((*dwarfFile = fopen(executableName, "r")) < 0) {
    perror("Error opening file.");
    return -1;
  }

  int fd = fileno(*dwarfFile);

  if (dwarf_init(fd, DW_DLC_READ, 0, 0, dbg, err) != DW_DLV_OK) {
    fprintf(stderr, "Failed DWARF initialization\n");
    return -1;
  }

  return 0;
}

int types_finalize(Dwarf_Debug dbg, FILE* dwarfFile, Dwarf_Error* err){
  if (dwarf_finish(dbg, err) != DW_DLV_OK) {
    fprintf(stderr, "Failed DWARF finalization\n");
    return -1;
  }

  fclose(dwarfFile);
  return 0;
}

int pc_range(Dwarf_Debug dgb, Dwarf_Die* fn_die, Dwarf_Addr* lowPC, Dwarf_Addr* highPC){
  Dwarf_Error err = 0;

  Dwarf_Signed attrcount, i;
  if(dwarf_lowpc(*fn_die, lowPC, &err) != DW_DLV_OK){
    fprintf(stderr, "Error in getting low pc\n");
    return -1;
  }

  Dwarf_Attribute highAttr;
  if(dwarf_attr(*fn_die, DW_AT_high_pc, &highAttr, &err) != DW_DLV_OK){
    fprintf(stderr, "Error in getting high pc\n");
    return -1;
  }

  Dwarf_Half highForm;
  if(dwarf_whatform(highAttr, &highForm, &err) != DW_DLV_OK){
    perror("Error in getting high pc form\n");
    return -1;
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
      return -1;
    }

    Dwarf_Addr high = *lowPC;
    high += offset;
    *highPC = high;

  } else {
    if(dwarf_highpc(*fn_die, highPC, &err) != DW_DLV_OK){
      perror("Error in getting high pc\n");
    }
  }

  return DW_DLV_OK;

}

int dwarf_read(const char* executable, GCContext** context){

  Dwarf_Debug dbg;
  FILE* dwarfFile;
  Dwarf_Error err = 0;

  if(types_init(executable, &dbg, &dwarfFile, &err) != DW_DLV_OK){
    fprintf(stderr, "Error opening dwarf file handle\n");
    return -1;
  }

  Dwarf_Unsigned cu_header_length, abbrev_offset, next_cu_header;
  Dwarf_Half version_stamp, address_size;
  Dwarf_Die no_die = 0, cu_die, child_die;
  
  bool done = false;

  *context = calloc(sizeof(GCContext), 1);
  (*context)->types = newHeapArray(INITIAL_TYPE_LIST_SIZE);
  (*context)->functions = newHeapArray(INITIAL_FUNCTION_LIST_SIZE);

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
      if(dwarf_type_die(dbg, *context, child_die, &err) != DW_DLV_OK){
        fprintf(stderr, "Error while typing die: %s\n", dwarf_errmsg(err));
      }

      int rc = dwarf_siblingof(dbg, child_die, &child_die, &err);
      if (rc == DW_DLV_ERROR){
        perror("Error getting sibling of DIE\n");
        return -1;
      } else if (rc == DW_DLV_NO_ENTRY){
        doneWithCU = true;
      }
    }
  }

  printf("structures created\n");
  
  if(types_finalize(dbg, dwarfFile, &err) != 0){
    fprintf(stderr, "Error closing dwarf file handle\n");
    return -1;
  }
  
  return 0;
}

int dwarf_type_die(Dwarf_Debug dbg, GCContext* context, Dwarf_Die child_die, Dwarf_Error* err){
  Dwarf_Half tag;

  if (dwarf_tag(child_die, &tag, err) != DW_DLV_OK){
    perror("Error in dwarf_tag\n");
  }

  if(tag == DW_TAG_subprogram){
    Function* fun = calloc(sizeof(Function), 1);

#ifdef DEBUG
    {
      int status = dwarf_diename(child_die, &(fun->dieName), err);
      if(status == DW_DLV_NO_ENTRY){
        fprintf(stderr, "Error no given function name\n");
        return -1;
      } else if(status == DW_DLV_ERROR){
        fprintf(stderr, "Error given argument was null\n");
        return -1;
      }
    }
#endif

    dwarf_read_function(dbg, &child_die, &fun, err);

#ifdef DEBUG

#endif

    arrayAppend(context->functions, fun);

  } else {

    Dwarf_Off typeKey;

    if(dwarf_dieoffset(child_die, &typeKey, err) != DW_DLV_OK){
      fprintf(stderr, "Error getting die offset.\n");
      return -1;
    }

    Type* type = calloc(sizeof(Type), 1);
    type->key.offset = typeKey;
        
    void* info;

    if(tag == DW_TAG_structure_type) {
      dwarf_read_struct(dbg, &child_die, (StructInfo**)&info, err);
      type->category = STRUCTURE_TYPE;
      type->info.structInfo = info;

    } else  if(tag == DW_TAG_union_type) {
      dwarf_read_union(dbg, &child_die, (UnionInfo**)&info, err);
      type->category = UNION_TYPE;
      type->info.unionInfo = info;
          
    } else  if(tag == DW_TAG_pointer_type) {
      dwarf_read_pointer(dbg, &child_die, (PointerInfo**)&info, err);
      type->category = POINTER_TYPE;
      type->info.pointerInfo = info;

    } else if(tag == DW_TAG_array_type) {
      dwarf_read_array(dbg, &child_die, (ArrayInfo**)&info, err);
      type->category = ARRAY_TYPE;
      type->info.pointerArrayInfo = info;
    } else if(tag == DW_TAG_base_type ||
              tag == DW_TAG_enumeration_type ||
              tag == DW_TAG_typedef ||
              tag == DW_TAG_const_type){
      // NOP
    } else if(tag == DW_TAG_variable){
      // TODO: Track global pointers
    } else {
      fprintf(stderr, "Missed die type: %x\n", tag);
      return -1;
    }

    arrayAppend(context->types, type);
        
    // What to do if someone defines a type inside of a function?
    // We'll handle that later. Globally scope the type, since the
    // dwarf type offset is unique.        
  }

  return DW_DLV_OK;
}

int dwarf_read_function(Dwarf_Debug dbg, Dwarf_Die* fn_die, Function** fun, Dwarf_Error* err){
  Scope* top_scope;

  if(dwarf_read_scope(dbg, fn_die, &top_scope, err) != DW_DLV_OK){
    fprintf(stderr, "Problem reading scope in function reader.\n");
    return -1;
  }

  *fun = calloc(sizeof(Function), 1);
  (*fun)->topScope = top_scope;

  return DW_DLV_OK;
}


int dwarf_read_scope(Dwarf_Debug dbg, Dwarf_Die* top_die, Scope** top_scope, Dwarf_Error* err){
  Dwarf_Attribute location_check;

  Dwarf_Addr lowPC = 0;
  Dwarf_Addr highPC = 0;
  
  if(dwarf_attr(*top_die, DW_AT_ranges, &location_check, err) == DW_DLV_OK){
     /* Dwarf_Unsigned original_off = 0; */
                  
     /* int fres = dwarf_global_formref(location_check, &original_off, err); */
     /* if (fres == DW_DLV_OK) { */
     /*     Dwarf_Ranges *rangeset = 0; */
     /*     Dwarf_Signed rangecount = 0; */
     /*     Dwarf_Unsigned bytecount = 0; */
     /*     int rres = dwarf_get_ranges_a(dbg,original_off, */
     /*         die,  */
     /*         &rangeset, */
     /*         &rangecount,&bytecount,&err); */
     /* } else { */
     /*   fprintf(stderr, "An error occured reading ranges in scope\n"); */
     /* } */

    fprintf(stderr, "I need to figure out how to parse ranges\n");
  } else {
    if(pc_range(dbg, top_die, &lowPC, &highPC) != DW_DLV_OK){
      fprintf(stderr, "Error getting high/lowpc for scope\n");
      return -1;
    }
  }

  void* castedLowPC = (void *)lowPC;
  void* castedHighPC = (void *)highPC;
  
  *top_scope = calloc(sizeof(Scope), 1);

  Dwarf_Die child_die;

  if(dwarf_child(*top_die, &child_die, err) == DW_DLV_ERROR){
    perror("Error getting child of CU DIE\n");
    return -1;
  }

  bool done = false;

  while(!done){

    Dwarf_Half tag;

    if(dwarf_tag(child_die, &tag, err) != DW_DLV_OK){
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

      if(type_of(dbg, &child_die, &type_die, err) != DW_DLV_OK){
        fprintf(stderr, "Error when getting scope variable's type tag.\n");
        return -1;
      }

      if(dwarf_tag(child_die, &type_tag, err) != DW_DLV_OK){
        perror("Error in dwarf_tag\n");
        return -1;
      }

      if(type_tag == DW_TAG_pointer_type){

        if( (*top_scope)->contents == NULL ){
          (*top_scope)->contents = newHeapArray(DEFAULT_SCOPE_CONTENTS_SIZE);
          // 5 elements maybe?
        }

        RootInfo* info;
        
        if(dwarf_read_root(dbg, &child_die, &info, err) != DW_DLV_OK){
          fprintf(stderr, "Error reading pointer\n");
          return -1;
        }

        arrayAppend((*top_scope)->contents, info);
        
      }
      // type has tag pointer type?

    } else if(tag == DW_TAG_lexical_block){
        if( (*top_scope)->children == NULL ){
          (*top_scope)->children = newHeapArray(DEFAULT_SCOPE_CHILDREN_SIZE);
          // 5 elements maybe?
          Scope* child_scope;
          if(dwarf_read_scope(dbg, &child_die, &child_scope, err) != DW_DLV_OK){
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
  
  return DW_DLV_OK;

}


int dwarf_read_root(Dwarf_Debug dbg, Dwarf_Die* root_die, RootInfo** info, Dwarf_Error* err){
  Dwarf_Attribute die_location;

  if(dwarf_attr(*root_die, DW_AT_location, &die_location, err) != DW_DLV_OK){
    perror("Error in getting location attribute\n");
    return -1;
  }

  Dwarf_Locdesc** llbufarray;

  Dwarf_Signed number_of_expressions;

  if(dwarf_loclist_n(die_location, &llbufarray, &number_of_expressions, err) != DW_DLV_OK){
    perror("Error in getting location attribute\n");
    return -1;
  }

  Dwarf_Off ref_off = 0;

  if(type_off(root_die, &ref_off, err) != DW_DLV_OK){
    fprintf(stderr, "Error getting offset\n");
    return -1;
  }

  *info = calloc(sizeof(RootInfo), 1);
  (*info)->location = llbufarray;
  (*info)->expression_count = number_of_expressions;
  (*info)->type.offset = ref_off;

  return DW_DLV_OK;
}


int dwarf_read_struct(Dwarf_Debug dbg, Dwarf_Die* type_die, StructInfo** structInfo, Dwarf_Error* err){
  #ifdef DEBUG
  Dwarf_Half type_tag;
  if(dwarf_tag(*type_die, &type_tag, err) != DW_DLV_OK){
    perror("Error in dwarf_tag\n");
    return -1;
  }
  assert(type_tag == DW_TAG_structure_type);
  #endif

  *structInfo = calloc(sizeof(StructInfo), 1);
  (*structInfo)->members = newHeapArray(DEFAULT_STRUCT_MEMBER_LIST_SIZE);

  Dwarf_Die child_die;

  int status = dwarf_child(*type_die, &child_die, err);
  bool done = false;
  
  if(status == DW_DLV_ERROR){
    perror("Error getting child of CU DIE\n");
    return -1;
  } else if(status == DW_DLV_NO_ENTRY){
    done = true;
  }

  while(!done){

    #ifdef DEBUG
    Dwarf_Half child_tag;
    if(dwarf_tag(child_die, &child_tag, err) != DW_DLV_OK){
      perror("Error in dwarf_tag\n");
      return -1;
    }
    assert(DW_TAG_member == child_tag);
    #endif

    Dwarf_Off type_key;
    
    if(type_off(&child_die, &type_key, err) != 0){
      fprintf(stderr, "Error getting type offset of struct member\n");
      return -1;
    }
    
    if(is_pointer(dbg, &child_die, err)){
      
      int field_offset;
      // Casting from unsigned long long, assuming offsets aren't more than an int can hold.
      if(dwarf_read_member_offset(dbg, &child_die, &field_offset, err) != DW_DLV_OK){
        fprintf(stderr, "Error in reading member offset\n");
        return -1;
      }

      StructMember* member = calloc(sizeof(StructMember), 1);
      member->type.offset = type_key;
      member->offset = field_offset;
      
      arrayAppend((*structInfo)->members, (void *)member);
    }

    int rc = dwarf_siblingof(dbg, child_die, &child_die, err);
    if (rc == DW_DLV_ERROR){
      perror("Error getting sibling of DIE\n");
      return -1;
    } else if (rc == DW_DLV_NO_ENTRY){
      done = true;
    }
  }

  return DW_DLV_OK;

}

int dwarf_read_array(Dwarf_Debug dbg, Dwarf_Die* type_die, ArrayInfo** info, Dwarf_Error* err){
  #ifdef DEBUG
  Dwarf_Half type_tag;
  if(dwarf_tag(*type_die, &type_tag, err) != DW_DLV_OK){
    perror("Error in dwarf_tag\n");
    return -1;
  }
  assert(type_tag == DW_TAG_array_type);
  #endif

  Dwarf_Off content_type_off;
  if(type_off(type_die, &content_type_off, err) != DW_DLV_OK){
    fprintf(stderr, "error getting offset\n");
    return -1;
  }

  Dwarf_Die subrange_die;

  // Expect the CU DIE to have children 
  if(dwarf_child(*type_die, &subrange_die, err) == DW_DLV_ERROR){
    perror("Error getting child of function DIE\n");
    return -1;
  }

  #ifdef DEBUG
  Dwarf_Half subrange_type_tag;
  if(dwarf_tag(subrange_die, &type_tag, err) != DW_DLV_OK){
    perror("Error in dwarf_tag\n");
    return -1;
  }
  assert(type_tag == DW_TAG_subrange_type);
  #endif

  Dwarf_Attribute upper_bound;

  if(dwarf_attr(subrange_die, DW_AT_upper_bound, &upper_bound, err) != DW_DLV_OK){
    fprintf(stderr, "Error in getting type attribute: %s\n", dwarf_errmsg(*err));
    return -1;
  }

  Dwarf_Unsigned count;
  if(dwarf_formudata(upper_bound, &count, err) != DW_DLV_OK){
    fprintf(stderr, "Error getting upper bound of array\n");
    return -1;
  }

  *info = calloc(sizeof(ArrayInfo), 1);
  (*info)->elementTypes = (TypeKey)content_type_off;
  (*info)->count = (int)count;

  return DW_DLV_OK;
}


int dwarf_read_pointer(Dwarf_Debug dbg, Dwarf_Die* type_die, PointerInfo** info, Dwarf_Error* err){
  #ifdef DEBUG
  Dwarf_Half type_tag;
  if(dwarf_tag(*type_die, &type_tag, err) != DW_DLV_OK){
    perror("Error in dwarf_tag\n");
    return -1;
  }
  assert(type_tag == DW_TAG_pointer_type);
  #endif

  Dwarf_Die* target_die = type_die;
  Dwarf_Off target_off = 0;
  Dwarf_Half target_type = DW_TAG_pointer_type;

  int indirectionCount = 0;

  Dwarf_Attribute type_check;

  bool void_star = false;
  
  while(target_type == DW_TAG_pointer_type){

    if(dwarf_attr(*target_die, DW_AT_type, &type_check, err) == DW_DLV_NO_ENTRY){
      void_star = true;
      break;
    } else {
      indirectionCount++;
      
      if(type_off(target_die, &target_off, err) != DW_DLV_OK){
        fprintf(stderr, "Error getting offset of target die: %s\n", dwarf_errmsg(*err));
        return -1;
      }

      if(dwarf_offdie(dbg, target_off, target_die, err) != DW_DLV_OK){
        fprintf(stderr, "Error in getting die at offset: %s\n", dwarf_errmsg(*err));
        return -1;
      }

      if(dwarf_tag(*target_die, &target_type, err) != DW_DLV_OK){
        perror("Error in dwarf_tag\n");
        return -1;
      }
    }
  }

  *info = calloc(sizeof(PointerInfo), 1);
  (*info)->layersOfIndirection = indirectionCount;
  (*info)->targetType.offset = target_off;
  (*info)->void_star = void_star;

  return DW_DLV_OK;
}

int dwarf_read_member_offset(Dwarf_Debug dbg, Dwarf_Die* member_die, int* offset, Dwarf_Error* err){
  Dwarf_Attribute member_location;
  if(dwarf_attr(*member_die, DW_AT_data_member_location, &member_location, err) != DW_DLV_OK){
    perror("Error in getting member location attribute\n");
  }

  Dwarf_Unsigned dwarf_offset;
    Dwarf_Half form;
    dwarf_whatform(member_location, &form, err);

    if (form == DW_FORM_data1 || form == DW_FORM_data2 || 
        form == DW_FORM_data4 || form == DW_FORM_data8 ||
        form == DW_FORM_udata) {
      
        dwarf_formudata(member_location, &dwarf_offset, 0);

    } else if (form == DW_FORM_sdata) {
      
        Dwarf_Signed soffset;
        dwarf_formsdata(member_location, &soffset, 0);
        if (soffset < 0) {
             printf("unsupported negative offset\n");
             /* FAIL */
        }
        dwarf_offset = (Dwarf_Unsigned)soffset;

    } else {
      Dwarf_Locdesc **locdescs;
      Dwarf_Signed len;

      if (dwarf_loclist_n(member_location, &locdescs, &len, err) == DW_DLV_ERROR) {
        printf("unsupported member offset\n");
      } else if(len != 1
                || locdescs[0]->ld_cents != 1
                || (locdescs[0]->ld_s[0]).lr_atom != DW_OP_plus_uconst) {
        printf("unsupported location expression\n");
      }
      dwarf_offset = (locdescs[0]->ld_s[0]).lr_number;
    }
    
    *offset = (int)dwarf_offset;

    return DW_DLV_OK;  
}

int dwarf_read_union(Dwarf_Debug dbg, Dwarf_Die* type_die, UnionInfo** unionInfo, Dwarf_Error* err){
  Dwarf_Die child_die;
  
  if(dwarf_child(*type_die, &child_die, err) == DW_DLV_ERROR){
    perror("Error getting child of union DIE\n");
    return -1;
  }

  bool done = false;

  *unionInfo = calloc(sizeof(UnionInfo), 1);
  (*unionInfo)->alternatives = newHeapArray(DEFAULT_UNION_TYPE_LIST_SIZE);

  while(!done){
    Dwarf_Half child_tag;
    
    if(dwarf_tag(child_die, &child_tag, err) != DW_DLV_OK){
      perror("Error in dwarf_tag\n");
      return -1;
    }

    if(child_tag == DW_TAG_member &&
       is_pointer(dbg, &child_die, err)){
      
      Dwarf_Off key;
      if(type_off(&child_die, &key, err) != DW_DLV_OK){
        fprintf(stderr, "Error getting type offset for union member\n");
        return -1; 
      }

      arrayAppend((*unionInfo)->alternatives, (void *)key);      
    }
    
    int rc = dwarf_siblingof(dbg, child_die, &child_die, err);
    if (rc == DW_DLV_ERROR){
      perror("Error getting sibling of DIE\n");
      return -1;
    } else if (rc == DW_DLV_NO_ENTRY){
      done = true;
    }
  }

  return DW_DLV_OK;  
}


void update_index(Array types, TypeKey* offset, int limit){
  #ifdef DEBUG
  assert(limit <= types->count);
  #endif

  for(int i=0; i < limit; i++){
    if( ((Type*)types->contents[i])->key.offset == offset->offset){
      offset->index = i;
      return;
    }
  }
  
  fprintf(stderr, "Could not find the type offset in the type list");
  exit(1);
}

void compress_type_table(GCContext* context){

  // Update all type indices
  for(int i=0; i < context->types->count; i++){
    Type* type = (Type *)context->types->contents[i];
    
    switch(type->category){
    case POINTER_TYPE:
      update_index(context->types, &(type->info.pointerInfo->targetType), i);
      break;
    case STRUCTURE_TYPE:
      for(int i=0; i < type->info.structInfo->members->count; i++){
        update_index(context->types, &( ((StructMember*)type->info.structInfo->members->contents[i])->type ), i);
      }
      break;
    case UNION_TYPE:
      for(int i=0; i < type->info.unionInfo->alternatives->count; i++){
        update_index(context->types, &( ((TypeKey*)(type->info.unionInfo->alternatives->contents))[i] ), i);
      }
      break;
    case ARRAY_TYPE:
      update_index(context->types, &(type->info.pointerArrayInfo->elementTypes), i);
      break;
    }
  }

  // Update all stack map types.
  for(int i=0; i < context->functions->count; i++){
    Function* this = context->functions->contents[i];
    for(int k=0; k < this->topScope->contents->count; k++){
      update_index(context->types, &( ((RootInfo*)(this->topScope->contents->contents))[i].type), context->types->count);
    }
  }
}

int cmpFunctions(const void* firstArg, const void* secondArg){
  Function* first = (Function*)firstArg;
  Function* second = (Function*)secondArg;
  int lowDiff = first->topScope->lowPC - second->topScope->lowPC;

  if(lowDiff == 0){
    int highDiff = first->topScope->highPC - second->topScope->highPC;
    return highDiff;
  } else {
    return lowDiff;
  }
}

void sort_functions(GCContext* context){
  void* base = context->functions->contents;
  size_t nitems = context->functions->count;
  size_t size = sizeof(Function *);
  qsort(base, nitems, size, cmpFunctions);
}

int finalizeContext(GCContext* context){
  compress_type_table(context);
  sort_functions(context);
}

int findFunction(void* PC, GCContext* context, Function** outValue){
  int left = 0;
  int right = context->functions->count;
  bool found = false;
  Function* candidate = NULL;
  
  while(!found && left < right){
    int mid = (left+right) / 2;
    candidate = context->functions->contents[mid];
    
    if(candidate->topScope->lowPC <= PC && candidate->topScope->highPC >= PC){
      found == true;
    } else if(candidate->topScope->lowPC <= PC && candidate->topScope->highPC <= PC){
      left = mid + 1;
    } else if(candidate->topScope->lowPC >= PC){
      right = mid;
    } else {
      fprintf(stderr, "Something has gone very wrong in the binary search for a function.\n");
      exit(1);
    }
  }

  if(found || (candidate && candidate->topScope->lowPC <= PC && candidate->topScope->highPC >= PC)){
    *outValue = candidate;
    return 0;
  } else {
    *outValue = NULL;
    return -1;
  }
  
}

void get_scope_roots(LiveFunction* fun, Scope* scope, GCContext* context, Roots* roots){

  Scope** children = (Scope **)scope->children->contents;
  RootInfo** rootInfo = (RootInfo**)scope->contents->contents;
  void* pc = (void *)fun->pc;
  
  for(int i=0; i < scope->children->count; i++){
    if(pc >= children[i]->lowPC && pc <= children[i]->highPC){
      get_scope_roots(fun, children[i], context, roots);
    }
  }
  
  for(int i=0; i < scope->contents->count; i++){
    Root* root = calloc(sizeof(Root), 1);
    root->typeIndex = ((RootInfo**)scope->contents->contents)[i]->type.index;
    void* location;
    if(var_location(fun, rootInfo[i]->location, rootInfo[i]->expression_count, root->location) != 0){
      fprintf(stderr, "Error reading root\n");
      exit(1);
    }
    arrayAppend(roots->roots, root);
  }
}

// CallStack and context are in parameters, roots are outparameters
int get_roots(CallStack* callStack, GCContext* context, Roots** roots){
  *roots = calloc(sizeof(Roots), 1);
  (*roots)->roots = newHeapArray(DEFAULT_ROOT_COUNT);
  
  for(int i=0; i < callStack->count; i++){
    LiveFunction* fun = &(callStack->stack[i]);

    Function* outValue = NULL;
    if(findFunction((void *)fun->pc, context, &outValue) != 0){
      fprintf(stderr, "Could not find function\n");
      break;
    }
    
    fun->function = outValue;
    
    get_scope_roots(fun, fun->function->topScope, context, *roots);
  }

  return 0;
}
