<h1 align="center">SQLAR - SQLite Archiver</h1>

This repository contains sources for a proof-of-concept "SQLite Archiver"
program.  This program (named "sqlar") operates much like "zip", except that
the compressed archive it builds is stored in an SQLite database instead
of a ZIP archive.

The motivation for this is to see how much larger an SQLite database file
is compared to a ZIP archive containing the same content.  The answer depends
on the filenames, but 2% seems to be a reasonable guess.  In other words,
storing files as compressed blobs in an SQLite database file results in a 
file that is only about 2% larger than storing those same files in a ZIP
archive using the same compression.

## Compiling

On unix, just type "make".  The SQLite sources are included.  The zlib
compression library is needed to build.

## Usage

To create an archive:

        sqlar ARCHIVE FILES...

All files named in FILES... will be added to the archive.  If another file
with the same name already exists in the archive, it is replaced.  If any
of the named FILES is a directory, that directory is scanned recursively.

To see the contents of an archive:

        sqlar -l ARCHIVE

To extract the contents of an archive:

        sqlar -x ARCHIVE [FILES...]

If a FILES argument is provided, then only the named files are extracted.
Without a FILES argument, all files are extracted.

All commands can be supplemented with -v for verbose output. For example:

        sqlar -v ARCHIVE FILES..
        sqlar -lv ARCHIVE
        sqlar -xv ARCHIVE

File are normally compressed using zlib prior to being stored as BLOBs in
the database.  However, if the file is incompressible or if the -n option
is used on the command-line, then the file is stored in the database exactly
as it appears on disk, without compression.
    
## Storage

The database schema looks like this:

        CREATE TABLE sqlar(
          name TEXT PRIMARY KEY,  -- name of the file
          mode INT,               -- access permissions
          mtime INT,              -- last modification time
          sz INT,                 -- original file size
          data BLOB               -- compressed content
        );
        
Both directories and empty files have sqlar.sz==0.  Directories can be
distinguished from empty files because directories have sqlar.data IS NULL.
The file is compressed if length(sqlar.blob)<sqlar.sz and is stored
as plaintext if length(sqlar.blob)==sqlar.sz.

## Fuse Filesystem

An SQLite Archive file can be mounted as a 
[Fuse Filesystem](http://fuse.sourceforge.net) using the "sqlarfs"
utility, including with this project.

To build the "sqlarfs" utility, run:

        make sqlarfs

Then to mount an SQLite archive as a filesystem, run:

        mkdir ~/fuse
        ./sqlarfs ARCHIVE_NAME -f ~/fuse

Replace ARCHIVE_NAME with the filename of the SQLite archive file to
be mounted, of course.
The -f option keeps sqlarfs running in the foreground, so that you can
unmount the filesystem by simply pressing the interrupt key (usually
Ctrl-C).
