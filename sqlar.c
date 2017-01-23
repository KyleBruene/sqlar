/*
** Copyright (c) 2014 D. Richard Hipp
**
** This program is free software; you can redistribute it and/or
** modify it under the terms of the Simplified BSD License (also
** known as the "2-Clause License" or "FreeBSD License".)

** This program is distributed in the hope that it will be useful,
** but without any warranty; without even the implied warranty of
** merchantability or fitness for a particular purpose.
**
** Author contact information:
**   drh@sqlite.org
*/
#include "sqlite3.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <zlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>

/* Maximum length of a pass-phrase */
#define MX_PASSPHRASE  120

/*
** Show a help message and quit.
*/
static void showHelp(const char *argv0){
  fprintf(stderr, "Usage: %s [options] archive [files...]\n", argv0);
  fprintf(stderr,
     "Options:\n"
     "   -d      Delete files from the archive\n"
     "   -e      Prompt for passphrase.  -ee to scramble the prompt\n"
     "   -l      List files in archive\n"
     "   -n      Do not compress files\n"
     "   -x      Extract files from archive\n"
     "   -v      Verbose output\n"
  );
  exit(1);
}

/*
** The database schema:
*/
static const char zSchema[] =
  "CREATE TABLE IF NOT EXISTS sqlar(\n"
  "  name TEXT PRIMARY KEY,\n"
  "  mode INT,\n"
  "  mtime INT,\n"
  "  sz INT,\n"
  "  data BLOB\n"
  ");"
;

/*
** Prepared statement that needs finalizing before sqlite3_close().
*/
static sqlite3_stmt *pStmt = 0;

/*
** Open database connection
*/
static sqlite3 *db = 0;

/*
** Close the database
*/
static void db_close(int commitFlag){
  if( pStmt ){
    sqlite3_finalize(pStmt);
    pStmt = 0;
  }
  if( db ){
    if( commitFlag ){
      sqlite3_exec(db, "COMMIT", 0, 0, 0);
    }else{
      sqlite3_exec(db, "ROLLBACK", 0, 0, 0);
    }
    sqlite3_close(db);
    db = 0;
  }
}


/*
** Panic message
*/
static void errorMsg(const char *zFormat, ...){
  va_list ap;
  va_start(ap, zFormat);
  vfprintf(stderr, zFormat, ap);
  va_end(ap);
  db_close(0);
  exit(1);
}

/*
** Scramble substitution matrix:
*/
static char aSubst[256];

/*
** Descramble the password
*/
static void descramble(char *z){
  int i;
  for(i=0; z[i]; i++) z[i] = aSubst[(unsigned char)z[i]];
}

/* Print a string in 5-letter groups */
static void printFive(const unsigned char *z){
  int i;
  for(i=0; z[i]; i++){
    if( i>0 && (i%5)==0 ) putchar(' ');
    putchar(z[i]);
  }
  putchar('\n');
}

/* Return a pseudo-random integer between 0 and N-1 */
static int randint(int N){
  unsigned char x;
  assert( N<256 );
  sqlite3_randomness(1, &x);
  return x % N;
}

/*
** Generate and print a random scrambling of letters a through z (omitting x)
** and set up the aSubst[] matrix to descramble.
*/
static void generateScrambleCode(void){
  unsigned char zOrig[30];
  unsigned char zA[30];
  unsigned char zB[30];
  int nA = 25;
  int nB = 0;
  int i;
  memcpy(zOrig, "abcdefghijklmnopqrstuvwyz", nA+1);
  memcpy(zA, zOrig, nA+1);
  assert( nA==(int)strlen((char*)zA) );
  for(i=0; i<sizeof(aSubst); i++) aSubst[i] = i;
  printFive(zA);
  while( nA>0 ){
    int x = randint(nA);
    zB[nB++] = zA[x];
    zA[x] = zA[--nA];
  }
  assert( nB==25 );
  zB[nB] = 0;
  printFive(zB);
  for(i=0; i<nB; i++) aSubst[zB[i]] = zOrig[i];
}

/*
** Do a single prompt for a passphrase.  Store the results in the blob.
**
** If the FOSSIL_PWREADER environment variable is set, then it will
** be the name of a program that prompts the user for their password/
** passphrase in a secure manner.  The program should take one or more
** arguments which are the prompts and should output the acquired
** passphrase as a single line on stdout.  This function will read the
** output using popen().
**
** If FOSSIL_PWREADER is not set, or if it is not the name of an
** executable, then use the C-library getpass() routine.
**
** The return value is a pointer to a static buffer that is overwritten
** on subsequent calls to this same routine.
*/
static void prompt_for_passphrase(
  const char *zPrompt,    /* Passphrase prompt */
  int doScramble,         /* Scramble the input if true */
  char *zPassphrase       /* Write result here */
){
  char *z;
  int i;
  if( doScramble ){
    generateScrambleCode();
    z = getpass(zPrompt);
    if( z ) descramble(z);
    printf("\033[3A\033[J");  /* Erase previous three lines */
    fflush(stdout);
  }else{
    z = getpass(zPrompt);
  }
  while( isspace(z[0]) ) z++;
  for(i=0; i<MX_PASSPHRASE-1; i++){
    zPassphrase[i] = z[i];
  }
  while( i>0 && isspace(z[i-1]) ){ i--; }
  zPassphrase[i] = 0;
}

/*
** List of command-line arguments
*/
typedef struct NameList NameList;
struct NameList {
  const char **azName;   /* List of names */
  int nName;             /* Number of names on the list */
};

/*
** Inplementation of SQL function "name_on_list(X)".  Return
** true if X is on the list of GLOB patterns given on the command-line.
*/
static void name_on_list(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  NameList *pList = (NameList*)sqlite3_user_data(context);
  int i;
  int rc = 0;
  const char *z = (const char*)sqlite3_value_text(argv[0]);
  if( z!=0 ){
    for(i=0; i<pList->nName; i++){
      if( sqlite3_strglob(pList->azName[i], z)==0 ){
        rc = 1;
        break;
      }
    }
  }
  sqlite3_result_int(context, rc);
}

/*
** SQL functions that always true.  This is used in place of
** name_on_list() when no command-line arguments are given.
*/
static void alwaysTrue(sqlite3_context *ctx, int n, sqlite3_value **v){
  sqlite3_result_int(ctx, 1);
}


/*
** Open the database.
*/
static void db_open(
  const char *zArchive,
  int writeFlag,
  int seeFlag,
  const char **azFiles,
  int nFiles
){
  int rc;
  int fg;
  NameList *x = 0;
  if( writeFlag ){
    fg = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
  }else{
    fg = SQLITE_OPEN_READONLY;
  }
  rc = sqlite3_open_v2(zArchive, &db, fg, 0);
  if( rc ) errorMsg("Cannot open archive [%s]: %s\n", zArchive,
                    sqlite3_errmsg(db));
  if( azFiles!=0 && nFiles>0 ){
    x = sqlite3_malloc( sizeof(NameList) );
    if( x==0 ) errorMsg("Out of memory");
    x->azName = azFiles;
    x->nName = nFiles;
    sqlite3_create_function(db, "name_on_list", 1, SQLITE_UTF8,
                           (char*)x, name_on_list, 0, 0);
  }else{
    sqlite3_create_function(db, "name_on_list", 1, SQLITE_UTF8,
                            0, alwaysTrue, 0, 0);
  }
  if( seeFlag ){
    char zPassPhrase[MX_PASSPHRASE+1];
#ifndef SQLITE_HAS_CODEC
    printf("WARNING:  The passphrase is a no-op because this build of\n"
           "sqlar is compiled without encryption capabilities.\n");
#endif
    memset(zPassPhrase, 0, sizeof(zPassPhrase));
    prompt_for_passphrase("passphrase: ", seeFlag>1, zPassPhrase);
#ifdef SQLITE_HAS_CODEC
    sqlite3_key_v2(db, "main", zPassPhrase, -1);
#endif
  }
  sqlite3_exec(db, "BEGIN", 0, 0, 0);
  sqlite3_exec(db, zSchema, 0, 0, 0);
  rc = sqlite3_exec(db, "SELECT 1 FROM sqlar LIMIT 1", 0, 0, 0);
  if( rc!=SQLITE_OK ){
    fprintf(stderr, "File [%s] is not an SQLite archive\n", zArchive);
    exit(1);
  }
}

/*
** Prepare the pStmt statement.
*/
static void db_prepare(const char *zSql){
  int rc;
  sqlite3_finalize(pStmt);
  rc = sqlite3_prepare_v2(db, zSql, -1, &pStmt, 0);
  if( rc ){
    errorMsg("Error: %s\nwhile preparing: %s\n",
             sqlite3_errmsg(db), zSql);
  }
}

/*
** Read a file from disk into memory obtained from sqlite3_malloc().
** Compress the file as it is read in if doing so reduces the file
** size and if the noCompress flag is false.
**
** Return the original size and the compressed size of the file in
** *pSizeOrig and *pSizeCompr, respectively.  If these two values are
** equal, that means the file was not compressed.
*/
static char *read_file(
  const char *zFilename,    /* Name of file to read */
  int *pSizeOrig,           /* Write original file size here */
  int *pSizeCompr,          /* Write compressed file size here */
  int noCompress            /* Do not compress if true */
){
  FILE *in;
  char *zIn;
  long int nIn;
  char *zCompr;
  unsigned long int nCompr;
  int rc;

  in = fopen(zFilename, "rb");
  if( in==0 ) errorMsg("cannot open \"%s\" for reading\n", zFilename);
  fseek(in, 0, SEEK_END);
  nIn = ftell(in);
  rewind(in);
  zIn = sqlite3_malloc( nIn+1 );
  if( zIn==0 ) errorMsg("cannot malloc for %d bytes\n", nIn+1);
  if( nIn>0 && fread(zIn, nIn, 1, in)!=1 ){
    errorMsg("unable to read %d bytes of file %s\n", nIn, zFilename);
  }
  fclose(in);
  if( noCompress ){
    *pSizeOrig = *pSizeCompr = nIn;
    return zIn;
  }
  nCompr = 13 + nIn + (nIn+999)/1000;
  zCompr = sqlite3_malloc( nCompr+1 );
  if( zCompr==0 ) errorMsg("cannot malloc for %d bytes\n", nCompr+1);
  rc = compress((Bytef*)zCompr, &nCompr, (const Bytef*)zIn, nIn);
  if( rc!=Z_OK ) errorMsg("Cannot compress %s\n", zFilename);
  if( nIn>nCompr ){
    sqlite3_free(zIn);
    *pSizeOrig = nIn;
    *pSizeCompr = (int)nCompr;
    return zCompr;
  }else{
    sqlite3_free(zCompr);
    *pSizeOrig = *pSizeCompr = nIn;
    return zIn;
  }
}

/*
** Make sure the parent directory for zName exists.  Create it if it does
** not exist.
*/
static void make_parent_directory(const char *zName){
  char *zParent;
  int i, j, rc;
  for(i=j=0; zName[i]; i++) if( zName[i]=='/' ) j = i;
  if( j>0 ){
    zParent = sqlite3_mprintf("%.*s", j, zName);
    if( zParent==0 ) errorMsg("mprintf failed\n");
    while( j>0 && zParent[j]=='/' ) j--;
    zParent[j] = 0;
    if( j>0 && access(zParent,F_OK)!=0 ){
      make_parent_directory(zParent);
      rc = mkdir(zParent, 0777);
      if( rc ) errorMsg("cannot create directory: %s\n", zParent);
    }
    sqlite3_free(zParent);
  }
}

/*
** Write a file or a directory.
**
** Create any missing directories leading up to the given file or directory.
** Also set the access mode and the modification time.
**
** If sz>nCompr that means that the content is compressed and needs to be
** decompressed before writing.
*/
static void write_file(
  const char *zFilename,   /* Store content in this file */
  int iMode,               /* The unix-style access mode */
  sqlite3_int64 mtime,     /* Modification time */
  int sz,                  /* Size of file as stored on disk */
  const char *pCompr,      /* Content (usually compressed) */
  int nCompr               /* Size of content (prior to decompression) */
){
  char *pOut;
  unsigned long int nOut;
  int rc;
  FILE *out;
  make_parent_directory(zFilename);
  if( pCompr==0 ){
    rc = mkdir(zFilename, iMode);
    if( rc ) errorMsg("cannot make directory: %s\n", zFilename);
    return;
  }
  out = fopen(zFilename, "wb");
  if( out==0 ) errorMsg("cannot open for writing: %s\n", zFilename);
  if( sz==nCompr ){
    if( sz>0 && fwrite(pCompr, sz, 1, out)!=1 ){
      errorMsg("failed to write: %s\n", zFilename);
    }
  }else{
    pOut = sqlite3_malloc( sz+1 );
    if( pOut==0 ) errorMsg("cannot allocate %d bytes\n", sz+1);
    nOut = sz;
    rc = uncompress((Bytef*)pOut, &nOut, (const Bytef*)pCompr, nCompr);
    if( rc!=Z_OK ) errorMsg("uncompress failed for %s\n", zFilename);
    if( nOut>0 && fwrite(pOut, nOut, 1, out)!=1 ){
      errorMsg("failed to write: %s\n", zFilename);
    }
    sqlite3_free(pOut);
  }
  fclose(out);
  rc = chmod(zFilename, iMode&0777);
  if( rc ) errorMsg("cannot change mode to %03o: %s\n", iMode, zFilename);
}

/*
** Error out if there are any issues with the given filename
*/
static void check_filename(const char *z){
  if( strncmp(z, "../", 3)==0 || sqlite3_strglob("*/../*", z)==0 ){
    errorMsg("Filename with '..' in its path: %s\n", z);
  }
  if( sqlite3_strglob("*\\*", z)==0 ){
    errorMsg("Filename with '\\' in its name: %s\n", z);
  }
}

/*
** Add a file to the database.
*/
static void add_file(
  const char *zFilename,     /* Name of file to add */
  int verboseFlag,           /* If true, show each file added */
  int noCompress             /* If true, always omit compression */
){
  int rc;
  struct stat x;
  int szOrig;
  int szCompr;
  const char *zName;

  check_filename(zFilename);
  rc = stat(zFilename, &x);
  if( rc ) errorMsg("no such file or directory: %s\n", zFilename);
  if( x.st_size>1000000000 ){
    errorMsg("file too big: %s\n", zFilename);
  }
  if( pStmt==0 ){
    db_prepare("REPLACE INTO sqlar(name,mode,mtime,sz,data)"
               " VALUES(?1,?2,?3,?4,?5)");
  }
  zName = zFilename;
  while( zName[0]=='/' ) zName++;
  sqlite3_bind_text(pStmt, 1, zName, -1, SQLITE_STATIC);
  sqlite3_bind_int(pStmt, 2, x.st_mode);
  sqlite3_bind_int64(pStmt, 3, x.st_mtime);
  if( S_ISREG(x.st_mode) ){
    char *zContent = read_file(zFilename, &szOrig, &szCompr, noCompress);
    sqlite3_bind_int(pStmt, 4, szOrig);
    sqlite3_bind_blob(pStmt, 5, zContent, szCompr, sqlite3_free);
    if( verboseFlag ){
      if( szCompr<szOrig ){
        int pct = szOrig ? (100*(sqlite3_int64)szCompr)/szOrig : 0;
        printf("  added: %s (deflate %d%%)\n", zFilename, 100-pct);
      }else{
        printf("  added: %s\n", zFilename);
      }
    }
  }else{
    sqlite3_bind_int(pStmt, 4, 0);
    sqlite3_bind_null(pStmt, 5);
    if( verboseFlag ) printf("  added: %s\n", zFilename);
  }
  rc = sqlite3_step(pStmt);
  if( rc!=SQLITE_DONE ){
    errorMsg("Insert failed for %s: %s\n", zFilename, sqlite3_errmsg(db));
  }
  sqlite3_reset(pStmt);
  if( S_ISDIR(x.st_mode) ){
    DIR *d;
    struct dirent *pEntry;
    char *zSubpath;
    d = opendir(zFilename);
    if( d ){
      while( (pEntry = readdir(d))!=0 ){
        if( strcmp(pEntry->d_name,".")==0 || strcmp(pEntry->d_name,"..")==0 ){
          continue;
        }
        zSubpath = sqlite3_mprintf("%s/%s", zFilename, pEntry->d_name);
        add_file(zSubpath, verboseFlag, noCompress);
        sqlite3_free(zSubpath);
      }
      closedir(d);
    }
  }
}

int main(int argc, char **argv){
  const char *zArchive = 0;
  const char **azFiles = 0;
  int nFiles = 0;
  int listFlag = 0;
  int extractFlag = 0;
  int verboseFlag = 0;
  int noCompress = 0;
  int seeFlag = 0;
  int deleteFlag = 0;
  int i, j;

  if( sqlite3_strglob("*/unsqlar", argv[0])==0 ){
    extractFlag = 1;
  }
  for(i=1; i<argc; i++){
    if( argv[i][0]=='-' ){
      for(j=1; argv[i][j]; j++){
        switch( argv[i][j] ){
          case 'l':   listFlag = 1;    break;
          case 'n':   noCompress = 1;  break;
          case 'v':   verboseFlag = 1; break;
          case 'x':   extractFlag = 1; break;
          case 'e':   seeFlag++;       break;
          case 'd':   deleteFlag = 1;  break;
          case '-':   break;
          default:    showHelp(argv[0]);
        }
      }
    }else if( zArchive==0 ){
      zArchive = argv[i];
    }else{
      azFiles = (const char**)&argv[i];
      nFiles = argc - i;
      break;
    }
  }
  if( zArchive==0 ) showHelp(argv[0]);
  if( listFlag || deleteFlag ){
    if( deleteFlag && nFiles==0 ){
      errorMsg("Specify one or more files to delete on the command-line");
    }
    db_open(zArchive, deleteFlag, seeFlag, azFiles, nFiles);
    if( verboseFlag ){
      db_prepare(
          "SELECT name, sz, length(data), mode, datetime(mtime,'unixepoch')"
          " FROM sqlar WHERE name_on_list(name) ORDER BY name"
      );
      while( sqlite3_step(pStmt)==SQLITE_ROW ){
        if( deleteFlag ) printf("DELETE ");
        printf("%10d %10d %03o %s %s\n", 
               sqlite3_column_int(pStmt, 1),
               sqlite3_column_int(pStmt, 2),
               sqlite3_column_int(pStmt, 3)&0777,
               sqlite3_column_text(pStmt, 4),
               sqlite3_column_text(pStmt, 0));
      }
    }else{
      db_prepare(
          "SELECT name FROM sqlar WHERE name_on_list(name) ORDER BY name"
      );
      while( sqlite3_step(pStmt)==SQLITE_ROW ){
        if( deleteFlag ) printf("DELETE ");
        printf("%s\n", sqlite3_column_text(pStmt,0));
      }
    }
    if( deleteFlag ){
      sqlite3_exec(db, "DELETE FROM sqlar WHERE name_on_list(name)", 0, 0, 0);
    }
    db_close(1);
  }else if( extractFlag ){
    const char *zSql;
    db_open(zArchive, 0, seeFlag, azFiles, nFiles);
    zSql = "SELECT name, mode, mtime, sz, data FROM sqlar"
           " WHERE name_on_list(name)";
    db_prepare(zSql);
    while( sqlite3_step(pStmt)==SQLITE_ROW ){
      const char *zFN = (const char*)sqlite3_column_text(pStmt, 0);
      check_filename(zFN);
      if( zFN[0]=='/' ){
        errorMsg("absolute pathname: %s\n", zFN);
      }
      if( sqlite3_column_type(pStmt,4)==SQLITE_BLOB && access(zFN, F_OK)==0 ){
        errorMsg("file already exists: %s\n", zFN);
      }
      if( verboseFlag ) printf("%s\n", zFN);
      write_file(zFN, sqlite3_column_int(pStmt,1),
                 sqlite3_column_int64(pStmt,2),
                 sqlite3_column_int(pStmt,3),
                 sqlite3_column_blob(pStmt,4),
                 sqlite3_column_bytes(pStmt,4));
    }
    db_close(1);
  }else{
    if( azFiles==0 ){
      errorMsg("Specify one or more files to add on the command-line");
    }
    db_open(zArchive, 1, seeFlag, 0, 0);
    for(i=0; i<nFiles; i++){
      add_file(azFiles[i], verboseFlag, noCompress);
    }
    db_close(1);
  }
  return 0;
}
