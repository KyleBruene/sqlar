
// C-incl
#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <cassert>
#include <cctype>
#ifdef __unix
	#include <unistd.h>
	#define _access access
#else
	#include <io.h>
	#include <process.h> // for getpid() and the exec..() family
	#include <direct.h>  // for _getcwd() and _chdir()
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>

// Cpp-Incl
#include <array>
#include <memory>
#include <iostream>
#include <iterator>

//#if defined(_WIN32)
//#include "dirent.h"
//#else
//#include <dirent.h>
//#endif

#ifdef __unix
	#include <sqlite3.h>
#else
	#include "sqlite3.h"
#endif

#pragma warning( push )
#pragma warning( disable : 4334 )  
	#include "miniz.c" // This looks stooopid, but it's actually how you're supposed to use this single file library...
#pragma warning( pop )

#include "FileArchive.hpp"

Handy::Result decompress_arr(
	int           sz,        // Size of file as stored on disk
	char *        pdest,     // Destination (post decompression), should be sz large
	int           nCompr,    // Size of content (prior to decompression)
	const char *  pCompr);   // Content (usually compressed)

Handy::ResultV<char *> compress_arr(
	unsigned long int nIn,    // Source data num bytes
	const char * dIn,         // Source data.
	unsigned long int * nOut, // Write compressed file size here
	//char **      dOut,        // Compressed data written here. Dealloc using delete[]
	bool noCompress);    

#ifndef __unix
	// Values for the second argument to access. These may be OR'd together.
	static int const R_OK = 4; // Test for read permission.
	static int const W_OK = 2; // Test for write permission.
	static int const F_OK = 0; // Test for existence.
	#if !defined(_WIN32)
		static int const X_OK = 1; // execute permission - unsupported in Windows
	#endif
#endif

void delete_arr_ptr(void * pointer)
{
	delete[] pointer;
}

//// Error out if there are any issues with the given filename
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

namespace SQLarLib
{
struct FileArchive::Private
{
	sqlite3      * m_db                    = nullptr; // Open database connection

	sqlite3_stmt * m_prepStmtREPLACE       = nullptr;
	sqlite3_stmt * m_prepStmtCHECKHAS      = nullptr;
	sqlite3_stmt * m_prepStmtPRINTFILES    = nullptr;
	sqlite3_stmt * m_prepStmtPRINTFILEINFO = nullptr;
	sqlite3_stmt * m_prepStmtEXTRACT       = nullptr;
	sqlite3_stmt * m_prepStmtEXTRACTALL    = nullptr;
	sqlite3_stmt * m_prepStmtDELETE        = nullptr;
	sqlite3_stmt * m_prepStmtDELETEALL     = nullptr;

	bool m_ro = false;

	Handy::Result db_prepare(char const * zSql, sqlite3_stmt ** prepStmt)
	{
		if (sqlite3_prepare_v2(m_db, zSql, -1, prepStmt, 0))
			return Handy::Result(false, std::string("Error: ") + sqlite3_errmsg(m_db) + " while preparing: \n" + zSql);

		return Handy::Result(true);
	}

	void db_close(bool commitFlag) // Close the database
	{
		if (m_prepStmtREPLACE) 
		{
			sqlite3_finalize(m_prepStmtREPLACE);
			m_prepStmtREPLACE = 0;
		}

		if (m_prepStmtCHECKHAS)
		{
			sqlite3_finalize(m_prepStmtCHECKHAS);
			m_prepStmtCHECKHAS = 0;
		}

		if (m_prepStmtPRINTFILES) 
		{
			sqlite3_finalize(m_prepStmtPRINTFILES);
			m_prepStmtPRINTFILES = 0;
		}

		if (m_prepStmtPRINTFILEINFO) 
		{
			sqlite3_finalize(m_prepStmtPRINTFILEINFO);
			m_prepStmtPRINTFILEINFO = 0;
		}

		if (m_prepStmtEXTRACT) 
		{
			sqlite3_finalize(m_prepStmtEXTRACT);
			m_prepStmtEXTRACT = 0;
		}
		
		if (m_prepStmtEXTRACTALL) 
		{
			sqlite3_finalize(m_prepStmtEXTRACTALL);
			m_prepStmtEXTRACTALL = 0;
		}

		if (m_db) 
		{
			sqlite3_exec(m_db, commitFlag ? "COMMIT" : "ROLLBACK", 0, 0, 0);
			sqlite3_close(m_db);
			m_db = 0;
		}
	}
};

FileArchive::FileArchive() : impl(make_impl_nocopy<FileArchive::Private>())
{
	
}

FileArchive::~FileArchive() // Must be above any uses of "std::unique_ptr<FileArchive>"
{
	impl->db_close(1);
}


Handy::ResultV<FileArchive *> 
FileArchive::Open(std::filesystem::path filepath_in, FileArchive::Mode mode) // ALWAYS OpenExistingOnly
{
	using MyResult = Handy::ResultV<FileArchive *>;

	std::filesystem::path filepath = std::filesystem::absolute(filepath_in);
	bool fileExists = std::filesystem::exists(filepath);

	std::unique_ptr<FileArchive> fa(new FileArchive());

	int fg = 0;

	switch (mode)
	{
		case FileArchive::Mode::Create:
			if (fileExists)
				return MyResult(false, std::string("Cannot 'Create' archive, file already exists: ") + filepath.string());

			fg |= SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
			break;

		case FileArchive::Mode::Open_Create:
			fg |= SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
			break;

		case FileArchive::Mode::Open:
			if (!fileExists)
				return MyResult(false, std::string("Cannot 'Open' archive, file not found: ") + filepath.string());

			fg |= SQLITE_OPEN_READWRITE;
			break;

		case FileArchive::Mode::OpenReadOnly:
			if (!fileExists)
				return MyResult(false, std::string("Cannot 'OpenReadOnly' archive, file not found: ") + filepath.string());

			fg |= SQLITE_OPEN_READONLY;

			fa->impl->m_ro = true;
			break;

		case FileArchive::Mode::Create_Replace:
		{
			std::filesystem::path filepathJournal = filepath; filepathJournal += std::string("-journal");
			std::filesystem::path filepathSHM     = filepath; filepathSHM     += std::string("-shm");
			std::filesystem::path filepathWAL     = filepath; filepathWAL     += std::string("-wal");
			
			if (fileExists)                               try { std::filesystem::remove(filepath);        } catch (...) {}
			if (std::filesystem::exists(filepathJournal)) try { std::filesystem::remove(filepathJournal); } catch (...) {}
			if (std::filesystem::exists(filepathSHM))     try { std::filesystem::remove(filepathJournal); } catch (...) {}
			if (std::filesystem::exists(filepathWAL))     try { std::filesystem::remove(filepathJournal); } catch (...) {}

			fg |= SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
		}
		
			break;

		default:
			return MyResult(false, "Unknown/Unimplemented archive FileArchive::Mode::______");
	}

	static char const * zSchema = 
		"CREATE TABLE IF NOT EXISTS sqlar("
		"  name  TEXT PRIMARY KEY,"
		"  mode  INT,"
		"  mtime INT,"
		"  sz    INT,"
		"  data  BLOB);";

	static char const * zSqlREPLACE       = "REPLACE INTO sqlar(name,mode,mtime,sz,data) VALUES(?1,?2,?3,?4,?5)";
	static char const * zSqlCHECKHAS      = "SELECT name FROM sqlar WHERE name == :FILENAMEAGK";
	static char const * zSqlPRINTFILES    = "SELECT name FROM sqlar ORDER BY name";
	static char const * zSqlPRINTFILEINFO = "SELECT name, sz, length(data), mode, datetime(mtime,'unixepoch') FROM sqlar ORDER BY name";
	static char const * zSqlEXTRACT       = "SELECT name, mode, mtime, sz, data FROM sqlar WHERE name == :FILENAMEAGK";
	static char const * zSqlEXTRACTALL    = "SELECT name, mode, mtime, sz, data FROM sqlar";
	static char const * zSqlDELETE        = "DELETE FROM sqlar WHERE name == :FILENAMEAGK";
	static char const * zSqlDELETEALL     = "DELETE FROM sqlar";

	int rc = sqlite3_open_v2(filepath.u8string().c_str(), &fa->impl->m_db, fg, 0);

	if (rc)
		return MyResult(false, std::string("Cannot open archive [") + filepath.string() + std::string("]: ") + sqlite3_errmsg(fa->impl->m_db));

#ifdef USE_SQLITE_WAL
	if (sqlite3_exec(fa->impl->m_db, "PRAGMA journal_mode=WAL;", 0, 0, 0))
		return MyResult(false, "Failed enable WAL on archive.");

	sqlite3_wal_autocheckpoint(fa->impl->m_db, 1);

	//if (sqlite3_exec(fa->impl->m_db, "PRAGMA wal_autocheckpoint=20;", 0, 0, 0))
	//	std::cerr << "Failed to tighten WAL autocheckpointing.";
#else
	if (sqlite3_exec(fa->impl->m_db, "BEGIN", 0, 0, 0))
		return MyResult(false, "Failed BEGIN on archive.");
#endif

	if (sqlite3_exec(fa->impl->m_db, zSchema, 0, 0, 0))
		return MyResult(false, "Failed to execute schema init on archive.");

	Handy::Result resPrepReplace = fa->impl->db_prepare(zSqlREPLACE, &fa->impl->m_prepStmtREPLACE);
	if (!resPrepReplace.Success)
		return MyResult(false, resPrepReplace.Reason);

	Handy::Result resPrepCheckHas = fa->impl->db_prepare(zSqlCHECKHAS, &fa->impl->m_prepStmtCHECKHAS);
	if (!resPrepCheckHas.Success)
		return MyResult(false, resPrepCheckHas.Reason);

	Handy::Result resPrepPrintFiles = fa->impl->db_prepare(zSqlPRINTFILES, &fa->impl->m_prepStmtPRINTFILES);
	if (!resPrepPrintFiles.Success)
		return MyResult(false, resPrepPrintFiles.Reason);

	Handy::Result resPrepPrintFileInfo = fa->impl->db_prepare(zSqlPRINTFILEINFO, &fa->impl->m_prepStmtPRINTFILEINFO);
	if (!resPrepPrintFileInfo.Success)
		return MyResult(false, resPrepPrintFileInfo.Reason);

	Handy::Result resPrepExtract = fa->impl->db_prepare(zSqlEXTRACT, &fa->impl->m_prepStmtEXTRACT);
	if (!resPrepExtract.Success)
		return MyResult(false, resPrepExtract.Reason);

	Handy::Result resPrepExtractAll = fa->impl->db_prepare(zSqlEXTRACTALL, &fa->impl->m_prepStmtEXTRACTALL);
	if (!resPrepExtractAll.Success)
		return MyResult(false, resPrepExtractAll.Reason);

	Handy::Result resPrepDelete = fa->impl->db_prepare(zSqlDELETE, &fa->impl->m_prepStmtDELETE);
	if (!resPrepDelete.Success)
		return MyResult(false, resPrepDelete.Reason);

	Handy::Result resPrepDeleteAll = fa->impl->db_prepare(zSqlDELETEALL, &fa->impl->m_prepStmtDELETEALL);
	if (!resPrepDeleteAll.Success)
		return MyResult(false, resPrepDeleteAll.Reason);

	return MyResult(true, fa.release());
}

// static
void FileArchive::AbortRollback(FileArchive * fa)
{
	if (fa)
		fa->impl->db_close(0);

	Handy::SafeDelete(fa);
}

void FileArchive::PrintKeyNames()
{
	if (sqlite3_reset(impl->m_prepStmtPRINTFILES) || sqlite3_clear_bindings(impl->m_prepStmtPRINTFILES))
		std::cerr << "Unable to reset/clear old state for PRINTFILES prepared statment.";

	Handy::OnScopeExit se([&]
	{
		if (sqlite3_reset(impl->m_prepStmtPRINTFILES) || sqlite3_clear_bindings(impl->m_prepStmtPRINTFILES))
			std::cerr << "Unable to reset/clear old state for PRINTFILES prepared statment." << std::endl;
	});


	while (sqlite3_step(impl->m_prepStmtPRINTFILES) == SQLITE_ROW)
		printf("%s\n", sqlite3_column_text(impl->m_prepStmtPRINTFILES, 0));
}

void FileArchive::PrintKeyInfos()
{
	if (sqlite3_reset(impl->m_prepStmtPRINTFILEINFO) || sqlite3_clear_bindings(impl->m_prepStmtPRINTFILEINFO))
		std::cerr << "Unable to reset/clear old state for PRINTFILEINFO prepared statment.";

	Handy::OnScopeExit se([&]
	{
		if (sqlite3_reset(impl->m_prepStmtPRINTFILEINFO) || sqlite3_clear_bindings(impl->m_prepStmtPRINTFILEINFO))
			std::cerr << "Unable to reset/clear old state for PRINTFILEINFO prepared statment." << std::endl;
	});

	while (sqlite3_step(impl->m_prepStmtPRINTFILEINFO) == SQLITE_ROW) 
	{
		printf("%10d %10d %03o %s %s\n",
			sqlite3_column_int (impl->m_prepStmtPRINTFILEINFO, 1),
			sqlite3_column_int (impl->m_prepStmtPRINTFILEINFO, 2),
			sqlite3_column_int (impl->m_prepStmtPRINTFILEINFO, 3) & 0777,
			sqlite3_column_text(impl->m_prepStmtPRINTFILEINFO, 4),
			sqlite3_column_text(impl->m_prepStmtPRINTFILEINFO, 0));
	}
}

Handy::Result FileArchive::Delete(std::string key)
{
	if (sqlite3_reset(impl->m_prepStmtDELETE) || sqlite3_clear_bindings(impl->m_prepStmtDELETE))
		return Handy::Result(false, "Unable to reset/clear old state for DELETE prepared statment.");

	Handy::OnScopeExit se([&]
	{
		if (sqlite3_reset(impl->m_prepStmtDELETE) || sqlite3_clear_bindings(impl->m_prepStmtDELETE))
			std::cerr << "Unable to reset/clear old state for DELETE prepared statment." << std::endl;
	});

	sqlite3_bind_text(impl->m_prepStmtDELETE, 1, key.c_str(), -1, SQLITE_TRANSIENT);

	if (sqlite3_step(impl->m_prepStmtDELETE) != SQLITE_DONE)
		return Handy::Result(false, std::string("Error removing key from archive: ") + key);

	int recordsChanged = sqlite3_changes(impl->m_db);

	if (recordsChanged == 1)
		return Handy::Result(true);

	if (recordsChanged == 0)
		return Handy::Result(false, "Key not found for deletion.");

	return Handy::Result(false, "Unexpected result encountered on key deletion.");
}

std::vector<std::string> 
FileArchive::KeyNames()
{
	if (sqlite3_reset(impl->m_prepStmtPRINTFILES) || sqlite3_clear_bindings(impl->m_prepStmtPRINTFILES))
		std::cerr << "Unable to reset/clear old state for PRINTFILES prepared statment.";

	Handy::OnScopeExit se([&]
	{
		if (sqlite3_reset(impl->m_prepStmtPRINTFILES) || sqlite3_clear_bindings(impl->m_prepStmtPRINTFILES))
			std::cerr << "Unable to reset/clear old state for PRINTFILES prepared statment." << std::endl;
	});

	std::vector<std::string> ret;

	while (sqlite3_step(impl->m_prepStmtPRINTFILES) == SQLITE_ROW)
		ret.push_back(std::string((const char *)sqlite3_column_text(impl->m_prepStmtPRINTFILES, 0)));

	return std::move(ret);
}

bool FileArchive::Has(std::string key)
{
	if (sqlite3_reset(impl->m_prepStmtCHECKHAS) || sqlite3_clear_bindings(impl->m_prepStmtCHECKHAS))
		std::cerr << "Unable to reset/clear old state for CHECKHAS prepared statment.";

	Handy::OnScopeExit se([&]
	{
		if (sqlite3_reset(impl->m_prepStmtCHECKHAS) || sqlite3_clear_bindings(impl->m_prepStmtCHECKHAS))
			std::cerr << "Unable to reset/clear old state for CHECKHAS prepared statment." << std::endl;
	});

	sqlite3_bind_text(impl->m_prepStmtCHECKHAS, 1, key.c_str(), -1, SQLITE_TRANSIENT);

	bool result = false;

	if (sqlite3_step(impl->m_prepStmtCHECKHAS) == SQLITE_ROW)
		result = key == std::string((const char *)sqlite3_column_text(impl->m_prepStmtCHECKHAS, 0));

	return result;
}

Handy::Result FileArchive::Get(std::string key, std::vector<uint8_t> & buffer)
{
	if (sqlite3_reset(impl->m_prepStmtEXTRACT) || sqlite3_clear_bindings(impl->m_prepStmtEXTRACT))
		return Handy::Result(false, "Unable to reset/clear old state for EXTRACT prepared statment.");

	Handy::OnScopeExit se([&] 
	{
		if (sqlite3_reset(impl->m_prepStmtEXTRACT) || sqlite3_clear_bindings(impl->m_prepStmtEXTRACT))
			std::cerr << "Unable to reset/clear old state for EXTRACT prepared statment." << std::endl;
	});

	sqlite3_bind_text(impl->m_prepStmtEXTRACT, 1, key.c_str(), -1, SQLITE_TRANSIENT);

	if (sqlite3_step(impl->m_prepStmtEXTRACT) != SQLITE_ROW)
		return Handy::Result(false, std::string("Key not found in archive: ") + key);

	const char *  zFN    = (const char*)sqlite3_column_text(impl->m_prepStmtEXTRACT, 0);
	int           iMode  = sqlite3_column_int  (impl->m_prepStmtEXTRACT, 1);  // The unix-style access mode
	sqlite3_int64 mtime  = sqlite3_column_int64(impl->m_prepStmtEXTRACT, 2);  // Modification time
	int           sz     = sqlite3_column_int  (impl->m_prepStmtEXTRACT, 3);  // Size of file as stored on disk
	const char *  pCompr = reinterpret_cast<const char *>(
	                       sqlite3_column_blob (impl->m_prepStmtEXTRACT, 4)); // Content (usually compressed)
	int           nCompr = sqlite3_column_bytes(impl->m_prepStmtEXTRACT, 4);  // Size of content (prior to decompression)

	buffer.clear();
	buffer.resize(sz, 0_u8);

	char * pOut = (char*)&buffer[0];

	if (sz == nCompr)
	{
		memcpy(pOut, pCompr, sz);
	}
	else
	{
		Handy::Result resD = decompress_arr(sz, pOut, nCompr, pCompr);

		if (!resD.Success)
			return resD;
	}

	return Handy::Result(true);
}

Handy::Result FileArchive::Put(std::string key, void * buffer, size_t numBytes, bool compressed)
{
	if (impl->m_ro)
		return Handy::Result(false, "Cannot modify read-only archive.");

	// This number is arbitrary, my vote would be to bump it up to 4 GiB.
	if (numBytes > 1000000000)
		return Handy::Result(false, "Source data is too big: ");

	if (sqlite3_reset(impl->m_prepStmtREPLACE) || sqlite3_clear_bindings(impl->m_prepStmtREPLACE))
		return Handy::Result(false, "Unable to reset/clear old state for REPLACE prepared statment.");

	Handy::OnScopeExit se([&]
	{
		if (sqlite3_reset(impl->m_prepStmtREPLACE) || sqlite3_clear_bindings(impl->m_prepStmtREPLACE))
			std::cerr << "Unable to reset/clear old state for REPLACE prepared statment." << std::endl;
	});
		
	char const * zAName = key.c_str();

	time_t currentTime = time(0);

	sqlite3_bind_text (impl->m_prepStmtREPLACE, 1, zAName, -1, SQLITE_STATIC);
	sqlite3_bind_int  (impl->m_prepStmtREPLACE, 2, (int)0666);
	sqlite3_bind_int64(impl->m_prepStmtREPLACE, 3, currentTime);

	unsigned long int szZ = 0;

	Handy::ResultV<char *> rRes = compress_arr((unsigned long)numBytes, (char const *)buffer, &szZ, !compressed);

	if (!rRes.Success)
		return Handy::Result(false, rRes.Reason);

	if (!rRes.OpValue
		#ifdef IS_WINDOWS
		.has_value()
		#endif
		)
		return Handy::Result(false, "Failed compression data pointer return.");

	char * ptrZ = rRes.OpValue.value();

	sqlite3_bind_int (impl->m_prepStmtREPLACE, 4, (int)numBytes);
	sqlite3_bind_blob(impl->m_prepStmtREPLACE, 5, ptrZ, szZ, delete_arr_ptr);

	if (SQLITE_DONE != sqlite3_step(impl->m_prepStmtREPLACE))
		return Handy::Result(false, std::string("Insert for put: ") + sqlite3_errmsg(impl->m_db));

	return Handy::Result(true);
}

Handy::Result FileArchive::Put(std::string key, std::vector<uint8_t> const & buffer, bool compressed)
//Handy::Result FileArchive::Put(std::string key, char const * ptr, int numBytes, bool noCompress/* = false*/, bool verbose/* = true*/)
{
	if (impl->m_ro)
		return Handy::Result(false, "Cannot modify read-only archive.");

	// This number is arbitrary, my vote would be to bump it up to 4 GiB.
	if (buffer.size() > 1000000000)
		return Handy::Result(false, "Source data is too big: ");

	if (sqlite3_reset(impl->m_prepStmtREPLACE) || sqlite3_clear_bindings(impl->m_prepStmtREPLACE))
		return Handy::Result(false, "Unable to reset/clear old state for REPLACE prepared statment.");

	Handy::OnScopeExit se([&]
	{ 
		if (sqlite3_reset(impl->m_prepStmtREPLACE) || sqlite3_clear_bindings(impl->m_prepStmtREPLACE))
			std::cerr << "Unable to reset/clear old state for REPLACE prepared statment." << std::endl;
	});

	char const * zAName = key.c_str();

	time_t currentTime = time(0);

	sqlite3_bind_text (impl->m_prepStmtREPLACE, 1, zAName, -1, SQLITE_STATIC);
	sqlite3_bind_int  (impl->m_prepStmtREPLACE, 2, (int)0666);
	sqlite3_bind_int64(impl->m_prepStmtREPLACE, 3, currentTime);

	unsigned long int szZ = 0;

	Handy::ResultV<char *> rRes = compress_arr((unsigned long)buffer.size(), (char const *)&buffer[0], &szZ, !compressed);

	if (!rRes.Success)
		return Handy::Result(false, rRes.Reason);
	
	if (!rRes.OpValue
	#ifdef IS_WINDOWS
	.has_value()
	#endif
	)
		return Handy::Result(false, "Failed compression data pointer return.");

	char * ptrZ = rRes.OpValue.value();

	sqlite3_bind_int (impl->m_prepStmtREPLACE, 4, (int)buffer.size());
	sqlite3_bind_blob(impl->m_prepStmtREPLACE, 5, ptrZ, szZ, delete_arr_ptr);

	if (SQLITE_DONE != sqlite3_step(impl->m_prepStmtREPLACE))
		return Handy::Result(false, std::string("Insert for put: ") + sqlite3_errmsg(impl->m_db));

	return Handy::Result(true);
}

} // SQLarLib


// Decompress a file or blob.
//
// If sz>nCompr that means that the content is compressed and needs to be
// decompressed.
Handy::Result decompress_arr(
	int           sz,        // Size of file as stored on disk
	char *        pOut,      // Destination (post decompression), should be sz large
	int           nCompr,    // Size of content (prior to decompression)
	const char *  pCompr)    // Content (usually compressed)
{
	if (sz == nCompr)
		return Handy::Result(false, "Pre and Post decompress sizes are the same.");

	unsigned long int nOut = sz;

	int rc = uncompress((Bytef *)pOut, &nOut, (const Bytef*)pCompr, nCompr);

	if (rc != Z_OK)
		return Handy::Result(false, "Decompression failed.");

	std::cout << "Decompressed: " << nCompr << " to " << sz << std::endl;

	return Handy::Result(true);
}

Handy::ResultV<char *> compress_arr(
	unsigned long int   nIn,  // Source data num bytes
	const char *        dIn,  // Source data.
	unsigned long int * nOut, // Write compressed file size here
							  //	char *              dOut, // Compressed data written here. Dealloc using delete[]
	bool                noCompress)
{
	if (noCompress)
	{
		*nOut = nIn;
		char * dOut = new (std::nothrow) char[nIn]();
		memcpy(dOut, dIn, nIn);

		return Handy::ResultV<char *>(true, dOut);
	}

	*nOut = 13 + nIn + (nIn + 999) / 1000;
	char * dOut = new (std::nothrow) char[*nOut]();

	if (dOut == 0)
		return Handy::ResultV<char *>(false, std::string("Could not new[] alloc for ") + std::to_string(*nOut) + "bytes");

	int rc = compress((Bytef*)dOut, nOut, (const Bytef*)dIn, nIn);

	if (rc != Z_OK)
	{
		delete[] dOut;
		return Handy::ResultV<char *>(false, "Cannot compress.");
	}

	if (nIn > *nOut)
	{
		std::cout << "Compressed: " << nIn << " to " << *nOut << std::endl;
		return Handy::ResultV<char *>(true, dOut);
	}
	else 
	{
		delete[] dOut;
		return compress_arr(nIn, dIn, nOut, true);
	}

	return Handy::ResultV<char *>(false, "Unlikely error encountered (threading or memory currupt?)");
}

