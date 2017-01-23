/*
** This file implements a FuseFS userspace client to mount an SQLAR 
** file archive (read-only).
**
** Usage:
**
**    sqlarfs ARCHIVE-FILE MOUNT-POINT
*/
#define FUSE_USE_VERSION 26
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <zlib.h>
#include "sqlite3.h"
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <assert.h>
#include <ctype.h>

/*
** Global state information about the archive
*/
struct sGlobal {
  sqlite3 *db;           /* Open database connection */
  sqlite3_stmt *pStat;   /* Prepared statement to read stat info */
  sqlite3_stmt *pFList;  /* Prepared statement to list all files */
  sqlite3_stmt *pExists; /* Prepared statement to check if a file exists */
  sqlite3_stmt *pRead;   /* Prepared statement to get file content */
  char *zCacheName;      /* Cached file */
  unsigned long int szCache; /* Size of the cached file */
  char *zCacheData;      /* Content of the cached files */
  pid_t uid;             /* User ID for all content files */
  gid_t gid;             /* Group ID for all content files */
} g;

/*
** Implementation of stat()
*/
static int sqlarfs_getattr(const char *path, struct stat *stbuf){
  int rc = 0;
  memset(stbuf, 0, sizeof(*stbuf));
  if( strcmp(path, "/")==0 ){
    stbuf->st_mode = S_IFDIR | 0755;
    stbuf->st_nlink = 2;
    return 0;
  }
  if( g.pStat==0 ){
    rc = sqlite3_prepare_v2(g.db,
               "SELECT mode, mtime, sz FROM sqlar WHERE name=?1",
               -1, &g.pStat, 0);
    if( rc!=SQLITE_OK ){
      return -ENOENT;
    }
  }
  sqlite3_bind_text(g.pStat, 1, &path[1], -1, SQLITE_STATIC);
  if( sqlite3_step(g.pStat)==SQLITE_ROW ){
    stbuf->st_mode = sqlite3_column_int(g.pStat, 0) & ~0222;
    stbuf->st_nlink = 1;
    stbuf->st_mtime = sqlite3_column_int(g.pStat, 1);
    stbuf->st_atime = stbuf->st_ctime = stbuf->st_mtime;
    stbuf->st_size = sqlite3_column_int64(g.pStat, 2);
    stbuf->st_uid = g.uid;
    stbuf->st_gid = g.gid;
    rc = 0;
  }else{
    rc = -ENOENT;
  }
  sqlite3_reset(g.pStat);
  return rc;
}


/*
** Implementation of readdir()
*/
static int sqlarfs_readdir(
  const char *path,
  void *buf,
  fuse_fill_dir_t filler,
  off_t offset,
  struct fuse_file_info *fi
){
  int rc;
  char *zGlob;
  int nGlob;
  if( g.pFList==0 ){
    rc = sqlite3_prepare_v2(g.db,
               "SELECT substr(name,?2) FROM sqlar"
               " WHERE name GLOB ?1"
               "   AND substr(name,?2) NOT GLOB '*/*'",
               -1, &g.pFList, 0);
    if( rc!=SQLITE_OK ){
      return -ENOENT;
    }
  }
  filler(buf, ".", NULL, 0);
  filler(buf, "..", NULL, 0);
  if( strcmp(path, "/")==0 ){
    zGlob = sqlite3_mprintf("*");
    nGlob = 1;
  }else{
    zGlob = sqlite3_mprintf("%s/*", &path[1]);
    nGlob = (int)strlen(zGlob);
  }
  if( zGlob==0 ) return -EIO;
  sqlite3_bind_text(g.pFList, 1, zGlob, -1, sqlite3_free);
  sqlite3_bind_int(g.pFList, 2, nGlob);
  while( sqlite3_step(g.pFList)==SQLITE_ROW ){
    filler(buf, (const char*)sqlite3_column_text(g.pFList, 0), NULL, 0);
  }
  sqlite3_reset(g.pFList);
  return 0;
}

/*
** Implementation of open()
*/
static int sqlarfs_open(const char *path, struct fuse_file_info *fi){
  int rc;
  if( (fi->flags & 3) != O_RDONLY ) return -EACCES;
  if( g.pExists==0 ){
    rc = sqlite3_prepare_v2(g.db,
               "SELECT 1 FROM sqlar WHERE name=?1",
               -1, &g.pExists, 0);
    if( rc!=SQLITE_OK ){
      return -ENOENT;
    }
  }
  sqlite3_bind_text(g.pExists, 1, &path[1], -1, SQLITE_STATIC);
  rc = sqlite3_step(g.pExists);
  sqlite3_reset(g.pExists);
  if( rc==SQLITE_DONE ) return -ENOENT;
  return 0;
}


/*
** Load the file named path[] into the cache, if it is not there already.
**
** Return 0 on success.  Return an error code if the file could not be loaded.
*/
static int loadCache(const char *path){
  unsigned long int nIn;
  const char *zIn;
  int rc;
  if( g.zCacheName ){
    if( strcmp(path, g.zCacheName)==0 ) return 0;
    sqlite3_free(g.zCacheName); g.zCacheName = 0;
    sqlite3_free(g.zCacheData); g.zCacheData = 0;
  }
  if( g.pRead==0 ){
    rc = sqlite3_prepare_v2(g.db,
               "SELECT sz, data FROM sqlar WHERE name=?1",
               -1, &g.pRead, 0);
    if( rc!=SQLITE_OK ){
      return -EIO;
    }
  }
  sqlite3_bind_text(g.pRead, 1, path, -1, SQLITE_STATIC);
  if( sqlite3_step(g.pRead)==SQLITE_ROW ){
    g.szCache = sqlite3_column_int64(g.pRead, 0);
    zIn = (const char*)sqlite3_column_blob(g.pRead, 1);
    nIn = (unsigned long int)sqlite3_column_bytes(g.pRead, 1);
    g.zCacheData = sqlite3_malloc( g.szCache );
    if( g.zCacheData==0 ){
      rc = -EIO;
    }else{
      rc = uncompress((Bytef*)g.zCacheData, &g.szCache, (const Bytef*)zIn, nIn);
      if( rc!=Z_OK ){
        sqlite3_free(g.zCacheData);
        g.zCacheData = 0;
        rc = -EIO;
      }
    }
    if( g.zCacheData ){
      g.zCacheName = sqlite3_mprintf("%s", path);
      if( g.zCacheName==0 ){
        rc = -EIO;
        sqlite3_free(g.zCacheData);
        g.zCacheData = 0;
      }
    }
  }
  sqlite3_reset(g.pRead);
  return rc;
}

/*
** Implementation of read()
*/
static int sqlarfs_read(
  const char *path,
  char *buf,
  size_t size,
  off_t offset,
  struct fuse_file_info *fi
){
  int rc;

  rc = loadCache(&path[1]);
  if( rc==0 ){
    if( offset>=g.szCache ){
      return 0;
    }
    if( offset+size>g.szCache ) size = g.szCache - offset;
    memcpy(buf, g.zCacheData + offset, size);
    return size;
  }else{
    return rc;
  }
}  

static struct fuse_operations sqlarfs_methods = {
  .getattr = sqlarfs_getattr,
  .readdir = sqlarfs_readdir,
  .open   	= sqlarfs_open,
  .read    = sqlarfs_read,
};

/*
** Show a help message and quit.
*/
static void showHelp(const char *argv0){
  fprintf(stderr, "Usage: %s [options] archive mount-point\n", argv0);
  fprintf(stderr,
     "Options:\n"
     "   -e      Prompt for passphrase.  -ee to scramble the prompt\n"
  );
  exit(1);
}

/* Maximum length of a pass-phrase */
#define MX_PASSPHRASE  120

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


int main(int argc, char **argv){
  int rc;
  int i, j;
  int seeFlag = 0;
  char *zArchive = 0;
  char *zMountPoint = 0;
  char *azNewArg[5];
  for(i=1; i<argc; i++){
    if( argv[i][0]=='-' ){
      for(j=1; argv[i][j]; j++){
        switch( argv[i][j] ){
          case 'e':   seeFlag++;       break;
          case '-':   break;
          default:    showHelp(argv[0]);
        }
      }
    }else if( zArchive==0 ){
      zArchive = argv[i];
    }else if( zMountPoint==0 ){
      zMountPoint = argv[i];
    }else{
      showHelp(argv[0]);
    }
  }
  if( zMountPoint==0 ) showHelp(argv[0]);
  rc = sqlite3_open(zArchive, &g.db);
  if( rc!=SQLITE_OK ){
    fprintf(stderr, "Cannot open sqlar file [%s]\n", argv[1]);
    exit(1);
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
    sqlite3_key_v2(g.db, "main", zPassPhrase, -1);
#endif
  }
  rc = sqlite3_exec(g.db, "SELECT 1 FROM sqlar LIMIT 1", 0, 0, 0);
  if( rc!=SQLITE_OK ){
    fprintf(stderr, "File [%s] is not an SQLite archive\n", argv[1]);
    exit(1);
  }
  g.uid = getuid();
  g.gid = getgid();
  azNewArg[0] = argv[0];
  azNewArg[1] = "-f";
  azNewArg[2] = "-s";
  azNewArg[3] = zMountPoint;
  azNewArg[4] = 0;
  rc = fuse_main(4, azNewArg, &sqlarfs_methods, NULL);
  sqlite3_finalize(g.pStat);
  sqlite3_finalize(g.pFList);
  sqlite3_finalize(g.pExists);
  sqlite3_finalize(g.pRead);
  sqlite3_free(g.zCacheName);
  sqlite3_free(g.zCacheData);
  sqlite3_close(g.db);
  return rc;
}
