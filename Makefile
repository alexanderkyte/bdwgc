CC = gcc
FLAGS = -ggdb
LIBS = /usr/include/libdwarf/
FILES = test.c read_types.c

# -gsplit-dwarf

all: test

test: test.c
	$(CC) test.c read_types.c $(FLAGS) -I $(LIBS) -std=c99 -o test

clean:
	rm test
