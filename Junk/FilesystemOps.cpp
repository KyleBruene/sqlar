
//#include "FilesystemOps.hpp"

//#include <stdio.h>
//#include <string>
//#ifdef __unix
//	#include <unistd.h>
//#else
//	#include <io.h>
//	#include <process.h> // for getpid() and the exec..() family
//	#include <direct.h>  // for _getcwd() and _chdir()
//#endif
//#include <sys/types.h>
//#include <sys/stat.h>
//
//#if defined(_WIN32)
//#include "dirent.h"
//#else
//#include <dirent.h>
//#endif


//#ifdef __unix
//	#define fopen_s(pFile,filename,mode) ((*(pFile))=fopen((filename),(mode)))==NULL
//	#define _chmod chmod
//	#define _access access
//#endif
//
//// Values for the second argument to access. These may be OR'd together.
//#ifndef __unix
//	// Values for the second argument to access. These may be OR'd together.
//	static int const R_OK = 4; // Test for read permission.
//	static int const W_OK = 2; // Test for write permission.
//	static int const F_OK = 0; // Test for existence.
//	#if !defined(_WIN32)
//		static int const X_OK = 1; // execute permission - unsupported in Windows
//	#endif
//#endif
//
//// Make sure the parent directory for zName exists.  Create it if it does not exist.
//Handy::Result make_parent_directory(const char *zName) 
//{
//
//	char *zParent;
//	int i, j, rc;
//
//	for (i = j = 0; zName[i]; i++) 
//		if (zName[i] == '/') 
//			j = i;
//	if (j > 0) 
//	{
//		char const * fmt = "%.*s";
//
//		int sz = std::snprintf(nullptr, 0, fmt, j, zName);
//		zParent = new (std::nothrow) char[sz + 1](); // "+1" for null terminator.
//		std::snprintf(zParent, sz + 1, fmt, j, zName);
//
//		if (zParent == 0)
//			return Handy::Result(false, "mprintf failed\n");
//
//		while (j > 0 && zParent[j] == '/') 
//			j--;
//
//		zParent[j] = 0;
//
//		if (j > 0 && _access(zParent, F_OK) != 0) 
//		{
//			make_parent_directory(zParent);
//
//			#if defined(_WIN32)
//			rc = _mkdir(zParent);
//			#else
//			rc = mkdir(zParent, 0777);
//			#endif
//
//			if (rc)
//			{
//				std::string reason = std::string("cannot create directory: ") + std::string(zParent);
//				delete [] zParent;
//				return Handy::Result(false, reason);
//
//			}
//		}
//
//		delete [] zParent;
//	}
//
//	return Handy::Result(true);
//}

// Error out if there are any issues with the given filename
//Handy::Result check_filename(const char *z) 
//{
//	std::string zs(z);
//
//	if (zs.compare(0, 3, "../") == 0)
//		return Handy::Result(false, std::string("Path begins with '../': ") + std::string(z));
//
//	if (zs.find("/../", 0) !=  std::string::npos)
//		return Handy::Result(false, std::string("Filename contains '/../' in its path: ") + std::string(z));
//
//	if (zs.find("\\", 0) !=  std::string::npos)
//		return Handy::Result(false, std::string("Filename with '\\' in its name: ") + std::string(z));
//
//	return Handy::Result(true);
//}

// Write a file or a directory.
//
// Create any missing directories leading up to the given file or directory.
// Also set the access mode and the modification time.
//
// If sz>nCompr that means that the content is compressed and needs to be
// decompressed before writing.
//Handy::Result write_file(
//	const char *  zFilename, // Store content in this file
//	int           iMode,     // The unix-style access mode
//	int64_t       mtime,     // Modification time
//	int           sz,        // Size of file as stored on disk
//	const char *  pCompr,    // Content (usually compressed)
//	int           nCompr)    // Size of content (prior to decompression)
//{
//	Handy::Result mpdRes = make_parent_directory(zFilename);
//
//	if (!mpdRes.Success)
//		return mpdRes;
//
//	if (pCompr == 0) 
//	{
//		#if defined(_WIN32)
//		int rc = _mkdir(zFilename);
//		#else
//		int rc = mkdir(zFilename, iMode);
//		#endif
//
//		if (rc) 
//			return Handy::Result(false, std::string("cannot make directory: ") + "zFilename");
//	}
//
//	FILE * out = NULL;
//
//	auto err = fopen_s(&out, zFilename, "wb");
//
//	if (out == 0 || err != 0) 
//		return Handy::Result(false, std::string("cannot open for writing: ") + zFilename);
//
//	if (sz == nCompr) 
//	{
//		if (sz > 0 && fwrite(pCompr, sz, 1, out) != 1)
//		{
//			fclose(out);
//			return Handy::Result(false, std::string("failed to write: ") + zFilename);
//		}
//	}
//	else 
//	{
//		char * pOut = new (std::nothrow) char[sz + 1]();
//
//		if (pOut == 0)
//		{
//			fclose(out);
//			delete[] pOut;
//			return Handy::Result(false, std::string("cannot allocate ") + std::to_string(sz) + "bytes");
//		}
//
//		unsigned long int nOut = sz;
//
//		int rc = uncompress((Bytef *)pOut, &nOut, (const Bytef*)pCompr, nCompr);
//
//		if (rc != Z_OK)
//		{
//			fclose(out);
//			delete[] pOut;
//			return Handy::Result(false, std::string("uncompress failed for ") + zFilename);
//		}
//
//		if (nOut > 0 && fwrite(pOut, nOut, 1, out) != 1)
//		{
//			fclose(out);
//			delete[] pOut;
//			return Handy::Result(false, std::string("failed to write: ") + zFilename);
//		}
//
//		delete [] pOut;
//	}
//
//	fclose(out);
//
//	int rc = _chmod(zFilename, iMode & 0777);
//
//	if (rc) 
//		return Handy::Result(false, std::string("cannot change mode to ") + std::to_string(iMode) + "File: " + zFilename);
//
//	return Handy::Result(true);
//}
//

// Read a file from disk into memory obtained from new[]().
// Compress the file as it is read in if doing so reduces the file
// size and if the noCompress flag is false.
//
// THIS FUNCTION ASSUMES THE FILE IS REGULAR!
//
// Return the original size and the compressed size of the file in
// *pSizeOrig and *pSizeCompr, respectively.  If these two values are
// equal, that means the file was not compressed.
//Handy::ResultV<char *> read_reg_file(
//	const char *zFilename,    // Name of file to read
//	int *pSizeOrig,           // Write original file size here
//	int *pSizeCompr,          // Write compressed file size here
//	bool noCompress)           // Do not compress if true
//{
//	FILE * in = NULL;
//
//	auto err = fopen_s(&in, zFilename, "rb");
//
//	if (in == 0 || err != 0) 
//		return Handy::ResultV<char *>(false, std::string("Could not open file for reading: ") + zFilename);
//
//	fseek(in, 0, SEEK_END);
//
//	long int nIn = ftell(in);
//
//	rewind(in);
//
//	char * zIn = new (std::nothrow) char[nIn + 1](); // reinterpret_cast<char *>(sqlite3_malloc(nIn + 1));
//	if (zIn == 0)
//	{
//		fclose(in);
//		return Handy::ResultV<char *>(false, std::string("Could not new[] alloc for ") + std::to_string(nIn+1) + "bytes");
//	}
//
//	if (nIn > 0 && fread(zIn, nIn, 1, in) != 1)
//	{
//		delete[] zIn;
//		fclose(in);
//		return Handy::ResultV<char *>(false, std::string("unable to read ") + std::to_string(nIn) + " bytes of file " + zFilename);
//	}
//		
//	fclose(in);
//
//	if (noCompress)
//	{
//		*pSizeOrig = *pSizeCompr = nIn;
//		return Handy::ResultV<char *>(true, zIn);
//	}
//
//	unsigned long int nCompr = 13 + nIn + (nIn + 999) / 1000;
//	char * zCompr = new (std::nothrow) char[nCompr + 1](); // reinterpret_cast<char *>(sqlite3_malloc(nCompr + 1));
//	if (zCompr == 0)
//	{
//		delete[] zIn;
//		return Handy::ResultV<char *>(false, std::string("Could not new[] alloc for ") + std::to_string(nCompr + 1) + "bytes");
//	}
//
//	int rc = compress((Bytef*)zCompr, &nCompr, (const Bytef*)zIn, nIn);
//
//	if (rc != Z_OK)
//	{
//		delete[] zIn;
//		delete[] zCompr;
//		return Handy::ResultV<char *>(false, std::string("Cannot compress ") + zFilename);
//	}
//
//	if (nIn > (long int)nCompr) 
//	{
//		delete[] zIn;
//		*pSizeOrig = nIn;
//		*pSizeCompr = (int)nCompr;
//		return Handy::ResultV<char *>(true, zCompr);
//	}
//	else 
//	{
//		delete[] zCompr;
//		*pSizeOrig = *pSizeCompr = nIn;
//		return Handy::ResultV<char *>(true, zIn);
//	}
//
//	return Handy::ResultV<char *>(false, "Failed to free temporary buffer");
//}


