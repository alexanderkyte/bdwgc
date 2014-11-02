#include "read_types.h"

static Dwarf_Debug dbg;
static Dwarf_Error err;
static FILE *dwarfFile;

int types_init(char* executableName){
  dbg = 0;

  if (strlen(executableName) < 2) {
    fprintf(stderr, "Expected a program name as argument\n");
    return -1;
  }

  if ((dwarfFile = fopen(executableName, O_RDONLY)) < 0) {
    perror("Error opening file.");
    return -1;
  }
  
  if (dwarf_init(fd, DW_DLC_READ, 0, 0, &dbg, &err) != DW_DLV_OK) {
    fprintf(stderr, "Failed DWARF initialization\n");
    return -1;
  }

  return 0;
}

int types_finalize(void){
  if (dwarf_finish(dbg, &err) != DW_DLV_OK) {
    fprintf(stderr, "Failed DWARF finalization\n");
    return -1;
  }
  
  fclose(dwarfFile);
  return 0;
}


int getRoots(int frameCount){
  int* buffer = malloc(frameCount+1 * sizeOf(void *));
  int frames = backtrace(buffer, frameCount + 1);

  for(int i=0; i < frames; i++){
    printf("%p\n", frames[i]);
  }

  
  return 0;
};
