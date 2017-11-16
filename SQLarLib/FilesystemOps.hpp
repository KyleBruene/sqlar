
#pragma once

#include <cstdint>

#include "../../HandyCpp/Handy.hpp"

// Make sure the parent directory for zName exists.  Create it if it does not exist.
Handy::Result make_parent_directory(const char *zName);

// Error out if there are any issues with the given filename
Handy::Result check_filename(const char *z);

// Write a file or a directory.
//
// Create any missing directories leading up to the given file or directory.
// Also set the access mode and the modification time.
//
// If sz>nCompr that means that the content is compressed and needs to be
// decompressed before writing.
Handy::Result write_file(
	const char *  zFilename, // Store content in this file
	int           iMode,     // The unix-style access mode
	int64_t       mtime,     // Modification time
	int           sz,        // Size of file as stored on disk
	const char *  pCompr,    // Content (usually compressed)
	int           nCompr);    // Size of content (prior to decompression)

Handy::Result decompress_arr(
	int           sz,        // Size of file as stored on disk
	char *        pdest,     // Destination (post decompression), should be sz large
	int           nCompr,    // Size of content (prior to decompression)
	const char *  pCompr);   // Content (usually compressed)

// Read a file from disk into memory obtained from sqlite3_malloc().
// Compress the file as it is read in if doing so reduces the file
// size and if the noCompress flag is false.
//
// Return the original size and the compressed size of the file in
// *pSizeOrig and *pSizeCompr, respectively.  If these two values are
// equal, that means the file was not compressed.
Handy::ResultV<char *> read_reg_file(
	const char *zFilename,    // Name of file to read
	int *pSizeOrig,           // Write original file size here
	int *pSizeCompr,          // Write compressed file size here
	bool noCompress);         // Do not compress if true

Handy::ResultV<char *> compress_arr(
	unsigned long int nIn,    // Source data num bytes
	const char * dIn,         // Source data.
	unsigned long int * nOut, // Write compressed file size here
	//char **      dOut,        // Compressed data written here. Dealloc using delete[]
	bool noCompress);       

