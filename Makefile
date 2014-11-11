CC = gcc
FLAGS = -ggdb -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast -fno-dwarf2-cfi-asm -I/usr/include/libdwarf/ -L/usr/lib -l elf -l dwarf -std=c99 -lunwind
FILES = test.c read_types.c

# -gsplit-dwarf

all: test

test: test.c read_types.c
	$(CC) test.c read_types.c $(FLAGS) -o test


clean:
	rm test
