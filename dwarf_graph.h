#ifndef DWARF_GRAPH
#define DWARF_GRAPH

typedef struct HeapArray {
  void* contents;
  int capacity;
  int count;
} *Array;


void arrayAppend(Array array, void* item){
  if(array->count >= array->capacity){
    array->capacity *= 2;
    array->contents = realloc(array->contents, array->capacity);
  }
  array->contents[array->count] = item;
}

Array newHeapArray(int capacity);
void resizeArray(Array array);

typedef struct Scope {
  Array contents; // Contains variables
  Array children; //
  void* lowPC;
  void* highPC;
} Scope;

typedef struct Function {
  Scope* topScope;
#ifdef DEBUG
  char* dieName;
#endif
} Function;

int findFunction // binary search on lower / upper pc

enum {
  POINTER_TYPE,
  STRUCTURE_TYPE,
  UNION_TYPE,
  ARRAY_TYPE,
} TypeCategory;

// handle typedefs in variable traversal
  /* TYPEDEF_TYPE, */
// Both non-pointer types?
/* DW_TAG_base_type */
/* DW_TAG_const_type */

// Remove pointers like typedefs?

typedef struct {
  int layersOfIndirection;
  Type* targetType;
} PointerInfo;

typedef struct {
  Dwarf_Locdesc** location;
  int expression_count;
  Dwarf_Off type;
} Root;

typedef struct {
  // These two are parallel arrays
  HeapArray* pointerTypes; // Contains type pointers for elements
  HeapArray* pointerOffsets; // The offsets from struct base
} StructInfo;

typedef struct {
  Type* alternatives;
} UnionInfo;

typedef struct {
  Type* elementTypes;
  int count;
} ArrayInfo;

typedef struct Type {
  TypeCategory type;
  Dwarf_Offset offset;
#ifdef DEBUG
  char* dieName;
#endif
  union {
    PointerInfo* pointerInfo;
    StructInfo* structInfo;
    UnionInfo* unionInfo;
    ArrayInfo* pointerArrayInfo;
  }
} Type;

int insertIntoTypeTree(Type* root, Type* item);

int dwarf_read_function(Dwarf_Debug dbg, Dwarf_Die* fn_die, Array functions, Dwarf_Err* err);
int dwarf_read_scope(Dwarf_Debug dbg, Dwarf_Die* fn_die, Array functions, Dwarf_Err* err);
int dwarf_read_struct(Dwarf_Debug dbg, Dwarf_Die* type_die, Array types, Dwarf_Err* err);
int dwarf_read_pointer(Dwarf_Debug dbg, Dwarf_Die* type_die, Array types, Dwarf_Err* err);
int dwarf_read_array(Dwarf_Debug dbg, Dwarf_Die* type_die, Array types, Dwarf_Err* err);

#endif
