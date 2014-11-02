#include "read_types.h"

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

void pc_range(Dwarf_Debug dgb, Dwarf_Die fn_die, Dwarf_Addr* lowPC, Dwarf_Addr* highPC, char** dieName){
  const char* tag_name = 0;
  Dwarf_Error err;
  Dwarf_Half tag;
  Dwarf_Attribute* attrs;
  Dwarf_Signed attrcount, i;
  int rc = dwarf_diename(fn_die, dieName, &err);
  
  if (rc == DW_DLV_ERROR)
    perror("Error in dwarf_diename\n");
  else if (rc == DW_DLV_NO_ENTRY)
    return;

  if (dwarf_tag(fn_die, &tag, &err) != DW_DLV_OK)
    perror("Error in dwarf_tag\n");
  /* Only interested in subprogram DIEs here */

  if (tag != DW_TAG_subprogram)
    return;

  if (dwarf_get_TAG_name(tag, &tag_name) != DW_DLV_OK)
    perror("Error in dwarf_get_TAG_name\n");

  if (dwarf_attrlist(fn_die, &attrs, &attrcount, &err) != DW_DLV_OK)
    perror("Error in dwarf_attlist\n");

  for (i = 0; i < attrcount; ++i) {
    Dwarf_Half attrcode;
    if (dwarf_whatattr(attrs[i], &attrcode, &err) != DW_DLV_OK)
      perror("Error in dwarf_whatattr\n");
    /* We only take some of the attributes for display here.
    ** More can be picked with appropriate tag constants.
    */
    if (attrcode == DW_AT_low_pc)
      dwarf_formaddr(attrs[i], lowPC, &err);
    else if (attrcode == DW_AT_high_pc)
      dwarf_formaddr(attrs[i], highPC, &err);
  }

}

void list_funcs_in_stack(Dwarf_Debug dbg, Dwarf_Addr* stack, int stackSize)
{
  Dwarf_Unsigned cu_header_length, abbrev_offset, next_cu_header;
  Dwarf_Half version_stamp, address_size;
  Dwarf_Error err;
  Dwarf_Die no_die = 0, cu_die, child_die;
  /* Find compilation unit header */
  if (dwarf_next_cu_header(
                           dbg,
                           &cu_header_length,
                           &version_stamp,
                           &abbrev_offset,
                           &address_size,
                           &next_cu_header,
                           &err) == DW_DLV_ERROR)
    perror("Error reading DWARF cu header\n");
  /* Expect the CU to have a single sibling - a DIE */
  if (dwarf_siblingof(dbg, no_die, &cu_die, &err) == DW_DLV_ERROR)
    perror("Error getting sibling of CU\n");
  /* Expect the CU DIE to have children */
  if (dwarf_child(cu_die, &child_die, &err) == DW_DLV_ERROR)
    perror("Error getting child of CU DIE\n");
  /* Now go over all children DIEs */
  while (1) {
    int rc;
    Dwarf_Addr lowPC, highPC;
    char* dieName = calloc(sizeof(char), 50);
    
    pc_range(dbg, child_die, &lowPC, &highPC, &dieName);

    for(int i=0; i < stackSize; i++){
      if(stack[i] > lowPC && stack[i] < highPC){
        printf("fn name: %s\n", dieName);
        printf("low pc : 0x%08llx\n", lowPC);
        printf("high pc : 0x%08llx\n", highPC);
      }
    }
    
    rc = dwarf_siblingof(dbg, child_die, &child_die, &err);
    if (rc == DW_DLV_ERROR)
      perror("Error getting sibling of DIE\n");
    else if (rc == DW_DLV_NO_ENTRY)
      break; /* done */
  }
}

#define INITIAL_FRAME_SIZE 50

int getRoots(void){
  int frameCount = INITIAL_FRAME_SIZE;  
  Dwarf_Addr* buffer = malloc(frameCount * sizeof(Dwarf_Addr));
  int frames = backtrace((void **)buffer, frameCount);
  
  // Will always be equal or less.
  // Equal has possibility of missing frames.
  while(frames >= frameCount){
    frameCount += 10;
    buffer = realloc(buffer, frameCount * sizeof(Dwarf_Addr));
    if(buffer == NULL){
      perror("Could not reallocate memory.\n");
    }
    frames = backtrace((void **)buffer, frameCount);
  }
  
  printf("frameCount: %d, frames: %d\n\n\n", frameCount, frames);

  int frames_tmp = frames;
  
  for(int i=0; i < frames_tmp; i++){
    if(buffer[i] == (Dwarf_Addr)0){
      frames--;
    }
    printf("0x%08llx\n", buffer[i]);
  }

  printf("And now the functions held: \n");
  
  if(dwarfHandle == NULL){
    perror("Uninitialized dwarf handle\n");
  } else {
    list_funcs_in_stack(dwarfHandle, buffer, frames);
  }

  free(buffer);
  return 0;
};
