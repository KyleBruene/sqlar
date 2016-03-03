#!/bin/make

CC = gcc -g -I. -D_FILE_OFFSET_BITS=64 -Wall -Werror -march=native -O3
ZLIB = -lz
FUSELIB = -lfuse
SQLITE_OPT = -DSQLITE_THREADSAFE=0 -DSQLITE_OMIT_LOAD_EXTENSION

all: sqlar
	

sqlar:	sqlar.c sqlite3.o
	$(CC) -o sqlar sqlar.c sqlite3.o $(ZLIB)

sqlarfs:	sqlarfs.c sqlite3.o
	$(CC) -o sqlarfs sqlarfs.c sqlite3.o $(ZLIB) $(FUSELIB)

sqlite3.o:	sqlite3.c sqlite3.h
	$(CC) -c sqlite3.c $(SQLITE_OPT) sqlite3.c

clean:	
	rm -f sqlar sqlarfs sqlite3.o
