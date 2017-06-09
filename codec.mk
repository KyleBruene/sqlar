#!/bin/make
#
# Use this alternative makefile to build an encrypted version of sqlar
# using the SQLite Encryption Extension (SEE).
#
#     CODEC=../path/to/codec.c make -f codec.mk sqlar sqlarfs
#
CC = gcc -g -I. -D_FILE_OFFSET_BITS=64 -Wall -Werror -static -Os
ZLIB = -lz
FUSELIB = -lfuse -lpthread -ldl
SQLITE_OPT = $(OPT) -DSQLITE_THREADSAFE=0 -DSQLITE_OMIT_LOAD_EXTENSION
SQLITE_OPT += -DSQLITE_OMIT_SHAREDCACHE
CC += -DSQLITE_HAS_CODEC

sqlar:	sqlar.c sqlite3.o
	$(CC) -o sqlar $(OPT) sqlar.c sqlite3.o $(ZLIB)

all: sqlar sqlarfs

sqlarfs:	sqlarfs.c sqlite3.o
	$(CC) -o sqlarfs $(OPT) sqlarfs.c sqlite3.o $(ZLIB) $(FUSELIB)

see-sqlite3.c: sqlite3.c $(CODEC)
	cat sqlite3.c $(CODEC) >see-sqlite3.c

sqlite3.o:	see-sqlite3.c sqlite3.h
	$(CC) $(SQLITE_OPT) -c see-sqlite3.c -o sqlite3.o

clean:	
	rm -f sqlar sqlarfs sqlite3.o see-sqlite3.c
