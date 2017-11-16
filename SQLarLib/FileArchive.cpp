
// C-incl
#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <cctype>
#include <io.h>
#include <process.h> // for getpid() and the exec..() family
#include <direct.h>  // for _getcwd() and _chdir()
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>

// Cpp-Incl
#include <array>
#include <memory>
#include <iostream>
#include <iterator>

#if defined(_WIN32)
#include "dirent.h"
#else
#include <dirent.h>
#endif

#include "sqlite3.h"

#include "FilesystemOps.hpp"
#include "FileArchive.hpp"

// Values for the second argument to access. These may be OR'd together.
static int const R_OK = 4; // Test for read permission.
static int const W_OK = 2; // Test for write permission.
#if !defined(_WIN32)
static int const X_OK = 1; // execute permission - unsupported in Windows
#endif
static int const F_OK = 0; // Test for existence.

void delete_arr_ptr(void * pointer)
{
	delete[] pointer;
}

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

		//if (m_prepStmt) 
		//{
		//	sqlite3_finalize(m_prepStmt);
		//	m_prepStmt = 0;
		//}

		if (m_db) 
		{
			sqlite3_exec(m_db, commitFlag ? "COMMIT" : "ROLLBACK", 0, 0, 0);
			sqlite3_close(m_db);
			m_db = 0;
		}
	}
};

FileArchive::FileArchive()
	: impl(make_impl_nocopy /*make_impl_nocopy*/<FileArchive::Private>())
{


}

FileArchive::~FileArchive() // Must be above any uses of "std::unique_ptr<FileArchive>"
{
	impl->db_close(1);
}


Handy::ResultV<FileArchive *> 
FileArchive::Open(std::string filepath, FileArchive::Mode mode) // ALWAYS OpenExistingOnly
{
	using MyHandy::Result = Handy::ResultV<FileArchive *>;

	std::unique_ptr<FileArchive> fa(new FileArchive());
	
	int fg = 0;

	bool fileExists = _access(filepath.c_str(), F_OK) == 0;

	switch (mode)
	{
		case FileArchive::Mode::Create:
			if (fileExists)
			{
				return MyResult(false, std::string("Cannot 'Create' archive, file already exists: ") + filepath);
			}

			fg |= SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
			break;

		case FileArchive::Mode::Open_Create:
			fg |= SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
			break;

		case FileArchive::Mode::Open:
			if (!fileExists)
			{
				return MyResult(false, std::string("Cannot 'Open' archive, file not found: ") + filepath);
			}

			fg |= SQLITE_OPEN_READWRITE;
			break;

		case FileArchive::Mode::OpenReadOnly:
			if (!fileExists)
			{
				return MyResult(false, std::string("Cannot 'OpenReadOnly' archive, file not found: ") + filepath);
			}
						
			fg |= SQLITE_OPEN_READONLY;

			fa->impl->m_ro = true;
			break;

		case FileArchive::Mode::Create_Replace:
			
			if (fileExists)
			{
				remove(filepath.c_str());
				remove((filepath + "-journal").c_str());
			}

			fg |= SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
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

	int rc = sqlite3_open_v2(filepath.c_str(), &fa->impl->m_db, fg, 0);

	if (rc)
		return MyResult(false, std::string("Cannot open archive [") + filepath + "]: " + sqlite3_errmsg(fa->impl->m_db));

	if (sqlite3_exec(fa->impl->m_db, "BEGIN", 0, 0, 0))
		return MyResult(false, "Failed BEGIN on archive.");

	if (sqlite3_exec(fa->impl->m_db, zSchema, 0, 0, 0))
		return MyResult(false, "Failed to execute scema init on archive.");

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
void FileArchive::AbortRollback(std::unique_ptr<FileArchive> fa)
{
	fa->impl->db_close(0);
}

Handy::Result FileArchive::Add(std::string dstArchivePath, std::string srcOSPath, bool noCompress, bool verbose/* = true */)
{
	if (impl->m_ro)
		return Handy::Result(false, "Cannot modify read-only archive.");

	const char *zFilenameDstArchive = dstArchivePath.c_str();
	const char *zFilenameSrcOSPath = srcOSPath.c_str();

	Handy::Result cfaRes = check_filename(zFilenameDstArchive);

	if (!cfaRes.Success)
		return cfaRes;

	struct stat x;
	int rc = stat(zFilenameSrcOSPath, &x);

	if (rc)
		return Handy::Result(false, std::string("Cannot stat file (does it exist?):: ") + zFilenameSrcOSPath);

	// This number seems arbitrary to me, my vote would be to bump it up to 4 GiB.
	if (x.st_size > 1000000000)
		return Handy::Result(false, std::string("Source file is too big: ") + zFilenameSrcOSPath);

	if (!S_ISREG(x.st_mode))
		return Handy::Result(false, std::string("Source file is non-regular: ") + zFilenameSrcOSPath);

	if (sqlite3_reset(impl->m_prepStmtREPLACE) || sqlite3_clear_bindings(impl->m_prepStmtREPLACE))
		return Handy::Result(false, "Unable to reset/clear old state for REPLACE prepared statment.");

	const char * zAName = zFilenameDstArchive;

	while (zAName[0] == '/') 
		zAName++;

	sqlite3_bind_text (impl->m_prepStmtREPLACE, 1, zAName, -1, SQLITE_STATIC);
	sqlite3_bind_int  (impl->m_prepStmtREPLACE, 2, x.st_mode);
	sqlite3_bind_int64(impl->m_prepStmtREPLACE, 3, x.st_mtime);

	int szOrig;
	int szCompr;

	Handy::ResultV<char *> rRes = read_reg_file(zFilenameSrcOSPath, &szOrig, &szCompr, noCompress);

	if (!rRes.Success || !rRes.OpValue)
	{
		return Handy::Result(false, rRes.Reason);
	}

	char * zContent = *rRes.OpValue;

	sqlite3_bind_int (impl->m_prepStmtREPLACE, 4, szOrig);
	sqlite3_bind_blob(impl->m_prepStmtREPLACE, 5, zContent, szCompr, delete_arr_ptr);

	if (verbose) 
	{
		if (szCompr < szOrig) 
		{
			int pct = szOrig ? (100 * (sqlite3_int64)szCompr) / szOrig : 0;
			printf("  added: %s -> %s (deflate %d%%)\n", zFilenameSrcOSPath, zFilenameDstArchive, 100 - pct);
		}
		else 
		{
			printf("  added: %s -> %s\n", zFilenameSrcOSPath, zFilenameDstArchive);
		}
	}

	if (SQLITE_DONE != sqlite3_step(impl->m_prepStmtREPLACE))
		return Handy::Result(false, std::string("Insert failed for ") + zFilenameSrcOSPath + " -> " + zFilenameDstArchive + " : " + sqlite3_errmsg(impl->m_db));

	return Handy::Result(true);
}

void FileArchive::PrintFilenames()
{
	if (sqlite3_reset(impl->m_prepStmtPRINTFILES) || sqlite3_clear_bindings(impl->m_prepStmtPRINTFILES))
		std::cerr << "Unable to reset/clear old state for PRINTFILES prepared statment.";

	while (sqlite3_step(impl->m_prepStmtPRINTFILES) == SQLITE_ROW)
		printf("%s\n", sqlite3_column_text(impl->m_prepStmtPRINTFILES, 0));
}

void FileArchive::PrintFileinfos()
{
	if (sqlite3_reset(impl->m_prepStmtPRINTFILEINFO) || sqlite3_clear_bindings(impl->m_prepStmtPRINTFILEINFO))
		std::cerr << "Unable to reset/clear old state for PRINTFILEINFO prepared statment.";

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

Handy::Result FileArchive::Extract(std::string srcArchivePath, std::string dstOSPath, bool verbose)
{
	if (sqlite3_reset(impl->m_prepStmtEXTRACT) || sqlite3_clear_bindings(impl->m_prepStmtEXTRACT))
		return Handy::Result(false, "Unable to reset/clear old state for EXTRACT prepared statment.");

	sqlite3_bind_text(impl->m_prepStmtEXTRACT, 1, srcArchivePath.c_str(), -1, SQLITE_TRANSIENT);

	if (sqlite3_step(impl->m_prepStmtEXTRACT) != SQLITE_ROW)
		return Handy::Result(false, std::string("File not found in archive: ") + srcArchivePath);

	const char *zFN = (const char*)sqlite3_column_text(impl->m_prepStmtEXTRACT, 0);
	Handy::Result fRes = check_filename(zFN);

	if (!fRes.Success)
		return fRes;

	if (zFN[0] == '/')
		return Handy::Result(false, std::string("absolute pathname: ") + zFN + "\n");

	if (sqlite3_column_type(impl->m_prepStmtEXTRACT, 4) == SQLITE_BLOB && _access(zFN, F_OK) == 0)
		return Handy::Result(false, std::string("file already exists: ") + zFN + "\n");

	if (verbose) 
		printf("%s\n", zFN);

	const char *  zFilename = zFN; // Store content in this file
	int           iMode  = sqlite3_column_int  (impl->m_prepStmtEXTRACT, 1);  // The unix-style access mode
	sqlite3_int64 mtime  = sqlite3_column_int64(impl->m_prepStmtEXTRACT, 2);  // Modification time
	int           sz     = sqlite3_column_int  (impl->m_prepStmtEXTRACT, 3);  // Size of file as stored on disk
	const char *  pCompr = reinterpret_cast<const char *>(
							sqlite3_column_blob(impl->m_prepStmtEXTRACT, 4)); // Content (usually compressed)
	int           nCompr = sqlite3_column_bytes(impl->m_prepStmtEXTRACT, 4);  // Size of content (prior to decompression)

	Handy::Result wRes = write_file(
		dstOSPath.c_str(), 
		iMode,
		mtime,
		sz,
		pCompr,
		nCompr);

	if (!wRes.Success)
		return wRes;

	return Handy::Result(true);
}

Handy::Result FileArchive::ExtractAll(std::string dstOSDirPath, bool verbose)
{
	struct stat x;
	int rc = stat(dstOSDirPath.c_str(), &x);

	if (rc)
		return Handy::Result(false, std::string("Cannot stat directory (does it exist?): ") + dstOSDirPath);

	if (!S_ISDIR(x.st_mode))
		return Handy::Result(false, std::string("Path is not a directory: ") + dstOSDirPath);

	if (sqlite3_reset(impl->m_prepStmtEXTRACTALL) || sqlite3_clear_bindings(impl->m_prepStmtEXTRACTALL))
		return Handy::Result(false, "Unable to reset/clear old state for EXTRACTALL prepared statment.");

	while (sqlite3_step(impl->m_prepStmtEXTRACTALL) == SQLITE_ROW) 
	{
		const char *zFN = (const char *)sqlite3_column_text(impl->m_prepStmtEXTRACTALL, 0);
		check_filename(zFN);




		//#warning I NEED TO DO MORE/BETTER PATH MANIPULATION!!!



		if (zFN[0] == '/')
			return Handy::Result(false, std::string("Absolute pathname: ") + zFN + "\n");

		if (sqlite3_column_type(impl->m_prepStmtEXTRACTALL, 4) == SQLITE_BLOB && _access(zFN, F_OK) == 0)
			return Handy::Result(false, std::string("file already exists: ") + zFN + "\n");

		if (verbose) 
			printf("%s\n", zFN);

		write_file(
			(dstOSDirPath + zFN).c_str(), 
			sqlite3_column_int  (impl->m_prepStmtEXTRACTALL, 1),
			sqlite3_column_int64(impl->m_prepStmtEXTRACTALL, 2),
			sqlite3_column_int  (impl->m_prepStmtEXTRACTALL, 3),
			reinterpret_cast<const char *>(
				sqlite3_column_blob(impl->m_prepStmtEXTRACTALL, 4)),
			sqlite3_column_bytes(impl->m_prepStmtEXTRACTALL, 4));
	}

	return Handy::Result(true);
}

Handy::Result FileArchive::Delete(std::string archivePath)
{
	if (sqlite3_reset(impl->m_prepStmtDELETE) || sqlite3_clear_bindings(impl->m_prepStmtDELETE))
		return Handy::Result(false, "Unable to reset/clear old state for DELETE prepared statment.");

	sqlite3_bind_text(impl->m_prepStmtDELETE, 1, archivePath.c_str(), -1, SQLITE_TRANSIENT);

	if (sqlite3_step(impl->m_prepStmtDELETE) != SQLITE_DONE)
		return Handy::Result(false, std::string("Error removing file from archive: ") + archivePath);

	int recordsChanged = sqlite3_changes(impl->m_db);

	if (recordsChanged == 1)
		return Handy::Result(true);

	if (recordsChanged == 0)
		return Handy::Result(false, "File not found for deletion.");

	return Handy::Result(false, "Unexpected result encountered on file deletion.");
}

std::vector<std::string> 
FileArchive::Filenames()
{
	if (sqlite3_reset(impl->m_prepStmtPRINTFILES) || sqlite3_clear_bindings(impl->m_prepStmtPRINTFILES))
		std::cerr << "Unable to reset/clear old state for PRINTFILES prepared statment.";

	std::vector<std::string> ret;

	while (sqlite3_step(impl->m_prepStmtPRINTFILES) == SQLITE_ROW)
		ret.push_back(std::string((const char *)sqlite3_column_text(impl->m_prepStmtPRINTFILES, 0)));

	return std::move(ret);
}

bool FileArchive::HasFile(std::string filename)
{
	if (sqlite3_reset(impl->m_prepStmtCHECKHAS) || sqlite3_clear_bindings(impl->m_prepStmtCHECKHAS))
		std::cerr << "Unable to reset/clear old state for CHECKHAS prepared statment.";
	
	sqlite3_bind_text(impl->m_prepStmtCHECKHAS, 1, filename.c_str(), -1, SQLITE_TRANSIENT);

	if (sqlite3_step(impl->m_prepStmtCHECKHAS) == SQLITE_ROW)
		return filename == std::string((const char *)sqlite3_column_text(impl->m_prepStmtCHECKHAS, 0));

	return false;
}


Handy::ResultV<std::tuple<char *, size_t>>   
FileArchive::Get(std::string archivePath)
{				
	using MyResult = Handy::ResultV<std::tuple<char *, size_t>>;

	if (sqlite3_reset(impl->m_prepStmtEXTRACT) || sqlite3_clear_bindings(impl->m_prepStmtEXTRACT))
		return MyResult(false, "Unable to reset/clear old state for EXTRACT prepared statment.");

	sqlite3_bind_text(impl->m_prepStmtEXTRACT, 1, archivePath.c_str(), -1, SQLITE_TRANSIENT);

	if (sqlite3_step(impl->m_prepStmtEXTRACT) != SQLITE_ROW)
		return MyResult(false, std::string("File not found in archive: ") + archivePath);

	const char *zFN = (const char*)sqlite3_column_text(impl->m_prepStmtEXTRACT, 0);
	Handy::Result fRes = check_filename(zFN);

	if (!fRes.Success)
		return MyResult(false, fRes.Reason);

	if (zFN[0] == '/')
		return MyResult(false, std::string("absolute pathname: ") + zFN + "\n");

	int           iMode  = sqlite3_column_int  (impl->m_prepStmtEXTRACT, 1);  // The unix-style access mode
	sqlite3_int64 mtime  = sqlite3_column_int64(impl->m_prepStmtEXTRACT, 2);  // Modification time
	int           sz     = sqlite3_column_int  (impl->m_prepStmtEXTRACT, 3);  // Size of file as stored on disk
	const char *  pCompr = reinterpret_cast<const char *>(
							sqlite3_column_blob (impl->m_prepStmtEXTRACT, 4)); // Content (usually compressed)
	int           nCompr = sqlite3_column_bytes(impl->m_prepStmtEXTRACT, 4);  // Size of content (prior to decompression)

	char * pOut = new (std::nothrow) char[sz]();

	if (sz == nCompr)
	{
		memcpy(pOut, pCompr, sz);
	}
	else
	{
		Handy::Result resD = decompress_arr(sz, pOut, nCompr, pCompr);

		if (!resD.Success)
			return MyResult(false, resD.Reason);
	}

	//std::cout << "GET: " << sz << " " << archivePath << " {" 
	//	<< (int)pOut[0]  << " " 
	//	<< (int)pOut[1]  << " " 
	//	<< (int)pOut[2]  << " " 
	//	<< (int)pOut[3]  << " " 
	//	<< (int)pOut[4]  << "}" << std::endl;
								
	return MyResult(true, std::tuple<char *, size_t>(pOut, sz));
}

Handy::Result FileArchive::Put(std::string archivePath, char const * ptr, int numBytes, bool noCompress/* = false*/, bool verbose/* = true*/)
{
	//std::cout << "PUT: " << numBytes << " " << archivePath << " {" 
	//	<< (int)ptr[0]  << " " 
	//	<< (int)ptr[1]  << " " 
	//	<< (int)ptr[2]  << " " 
	//	<< (int)ptr[3]  << " " 
	//	<< (int)ptr[4]  << "}" << std::endl;

	if (impl->m_ro)
		return Handy::Result(false, "Cannot modify read-only archive.");

	Handy::Result cfaRes = check_filename(archivePath.c_str());

	if (!cfaRes.Success)
		return cfaRes;

	// This number seems arbitrary to me, my vote would be to bump it up to 4 GiB.
	if (numBytes > 1000000000)
		return Handy::Result(false, "Source data is too big: ");

	if (sqlite3_reset(impl->m_prepStmtREPLACE) || sqlite3_clear_bindings(impl->m_prepStmtREPLACE))
		return Handy::Result(false, "Unable to reset/clear old state for REPLACE prepared statment.");

	char const * zAName = archivePath.c_str();

	while (zAName[0] == '/') 
		zAName++;

	time_t currentTime = time(0);

	sqlite3_bind_text (impl->m_prepStmtREPLACE, 1, zAName, -1, SQLITE_STATIC);
	sqlite3_bind_int  (impl->m_prepStmtREPLACE, 2, 0666);
	sqlite3_bind_int64(impl->m_prepStmtREPLACE, 3, currentTime);

	unsigned long int szZ = 0;

	Handy::ResultV<char *> rRes = compress_arr(numBytes, ptr, &szZ, noCompress);// read_reg_file(zFilenameSrcOSPath, &szOrig, &szCompr, noCompress);

	if (!rRes.Success)
		return Handy::Result(false, rRes.Reason);

	char * ptrZ = *rRes.OpValue;
								
	sqlite3_bind_int (impl->m_prepStmtREPLACE, 4, numBytes);
	sqlite3_bind_blob(impl->m_prepStmtREPLACE, 5, ptrZ, szZ, delete_arr_ptr);

	if (SQLITE_DONE != sqlite3_step(impl->m_prepStmtREPLACE))
		return Handy::Result(false, std::string("Insert for put: ") + sqlite3_errmsg(impl->m_db));

	return Handy::Result(true);
}



//std::unique_ptr<std::vector<uint8_t>> 
//			   FileArchive::Get(std::string archivePath);
//
//void                        Put(std::string archivePath, std::vector<uint8_t> const & data,   bool noCompress = false, bool verbose = true);
//void                        Put(std::string archivePath, char const * ptr, uint64_t numBytes, bool noCompress = false, bool verbose = true);
//

}





