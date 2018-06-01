
//#pragma once

//#include <cstdint>
//
//#include "../../HandyCpp/Handy.hpp"

//// Make sure the parent directory for zName exists.  Create it if it does not exist.
//Handy::Result make_parent_directory(const char *zName);

// Error out if there are any issues with the given filename
//Handy::Result check_filename(const char *z);

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
//	int           nCompr);    // Size of content (prior to decompression)



// Read a file from disk into memory obtained from sqlite3_malloc().
// Compress the file as it is read in if doing so reduces the file
// size and if the noCompress flag is false.
//
// Return the original size and the compressed size of the file in
// *pSizeOrig and *pSizeCompr, respectively.  If these two values are
// equal, that means the file was not compressed.
//Handy::ResultV<char *> read_reg_file(
//	const char *zFilename,    // Name of file to read
//	int *pSizeOrig,           // Write original file size here
//	int *pSizeCompr,          // Write compressed file size here
//	bool noCompress);         // Do not compress if true


/// Filesystem-based operations, they should work, but I'm commenting out, since I don't need them right now.
//Handy::Result                           Add       (std::string dstArchivePath, std::string srcOSPath, bool noCompress = false, bool verbose = true);
//Handy::Result                           Extract   (std::string srcArchivePath, std::string dstOSPath, bool verbose = true);
//Handy::Result                           ExtractAll(std::string dstOSDirPath, bool verbose = true);

//Handy::Result FileArchive::Extract(std::string srcArchivePath, std::string dstOSPath, bool verbose)
//{
//	if (sqlite3_reset(impl->m_prepStmtEXTRACT) || sqlite3_clear_bindings(impl->m_prepStmtEXTRACT))
//		return Handy::Result(false, "Unable to reset/clear old state for EXTRACT prepared statment.");
//
//	sqlite3_bind_text(impl->m_prepStmtEXTRACT, 1, srcArchivePath.c_str(), -1, SQLITE_TRANSIENT);
//
//	if (sqlite3_step(impl->m_prepStmtEXTRACT) != SQLITE_ROW)
//		return Handy::Result(false, std::string("File not found in archive: ") + srcArchivePath);
//
//	const char *zFN = (const char*)sqlite3_column_text(impl->m_prepStmtEXTRACT, 0);
//	Handy::Result fRes = check_filename(zFN);
//
//	if (!fRes.Success)
//		return fRes;
//
//	if (zFN[0] == '/')
//		return Handy::Result(false, std::string("absolute pathname: ") + zFN + "\n");
//
//	if (sqlite3_column_type(impl->m_prepStmtEXTRACT, 4) == SQLITE_BLOB && _access(zFN, F_OK) == 0)
//		return Handy::Result(false, std::string("file already exists: ") + zFN + "\n");
//
//	if (verbose) 
//		printf("%s\n", zFN);
//
//	const char *  zFilename = zFN; // Store content in this file
//	int           iMode  = sqlite3_column_int  (impl->m_prepStmtEXTRACT, 1);  // The unix-style access mode
//	sqlite3_int64 mtime  = sqlite3_column_int64(impl->m_prepStmtEXTRACT, 2);  // Modification time
//	int           sz     = sqlite3_column_int  (impl->m_prepStmtEXTRACT, 3);  // Size of file as stored on disk
//	const char *  pCompr = reinterpret_cast<const char *>(
//							sqlite3_column_blob(impl->m_prepStmtEXTRACT, 4)); // Content (usually compressed)
//	int           nCompr = sqlite3_column_bytes(impl->m_prepStmtEXTRACT, 4);  // Size of content (prior to decompression)
//
//	Handy::Result wRes = write_file(
//		dstOSPath.c_str(), 
//		iMode,
//		mtime,
//		sz,
//		pCompr,
//		nCompr);
//
//	if (!wRes.Success)
//		return wRes;
//
//	return Handy::Result(true);
//}
//
//Handy::Result FileArchive::ExtractAll(std::string dstOSDirPath, bool verbose)
//{
//	struct stat x;
//	int rc = stat(dstOSDirPath.c_str(), &x);
//
//	if (rc)
//		return Handy::Result(false, std::string("Cannot stat directory (does it exist?): ") + dstOSDirPath);
//
//	if (!S_ISDIR(x.st_mode))
//		return Handy::Result(false, std::string("Path is not a directory: ") + dstOSDirPath);
//
//	if (sqlite3_reset(impl->m_prepStmtEXTRACTALL) || sqlite3_clear_bindings(impl->m_prepStmtEXTRACTALL))
//		return Handy::Result(false, "Unable to reset/clear old state for EXTRACTALL prepared statment.");
//
//	while (sqlite3_step(impl->m_prepStmtEXTRACTALL) == SQLITE_ROW) 
//	{
//		const char *zFN = (const char *)sqlite3_column_text(impl->m_prepStmtEXTRACTALL, 0);
//		check_filename(zFN);
//
//
//
//
//		//#warning I NEED TO DO MORE/BETTER PATH MANIPULATION!!!
//
//
//
//		if (zFN[0] == '/')
//			return Handy::Result(false, std::string("Absolute pathname: ") + zFN + "\n");
//
//		if (sqlite3_column_type(impl->m_prepStmtEXTRACTALL, 4) == SQLITE_BLOB && _access(zFN, F_OK) == 0)
//			return Handy::Result(false, std::string("file already exists: ") + zFN + "\n");
//
//		if (verbose) 
//			printf("%s\n", zFN);
//
//		write_file(
//			(dstOSDirPath + zFN).c_str(), 
//			sqlite3_column_int  (impl->m_prepStmtEXTRACTALL, 1),
//			sqlite3_column_int64(impl->m_prepStmtEXTRACTALL, 2),
//			sqlite3_column_int  (impl->m_prepStmtEXTRACTALL, 3),
//			reinterpret_cast<const char *>(
//				sqlite3_column_blob(impl->m_prepStmtEXTRACTALL, 4)),
//			sqlite3_column_bytes(impl->m_prepStmtEXTRACTALL, 4));
//	}
//
//	return Handy::Result(true);
//}



//Handy::Result FileArchive::Add(std::string dstArchivePath, std::string srcOSPath, bool noCompress, bool verbose/* = true */)
//{
//	if (impl->m_ro)
//		return Handy::Result(false, "Cannot modify read-only archive.");
//
//	const char *zFilenameDstArchive = dstArchivePath.c_str();
//	const char *zFilenameSrcOSPath = srcOSPath.c_str();
//
//	Handy::Result cfaRes = check_filename(zFilenameDstArchive);
//
//	if (!cfaRes.Success)
//		return cfaRes;
//
//	struct stat x;
//	int rc = stat(zFilenameSrcOSPath, &x);
//
//	if (rc)
//		return Handy::Result(false, std::string("Cannot stat file (does it exist?):: ") + zFilenameSrcOSPath);
//
//	// This number seems arbitrary to me, my vote would be to bump it up to 4 GiB.
//	if (x.st_size > 1000000000)
//		return Handy::Result(false, std::string("Source file is too big: ") + zFilenameSrcOSPath);
//
//	if (!S_ISREG(x.st_mode))
//		return Handy::Result(false, std::string("Source file is non-regular: ") + zFilenameSrcOSPath);
//
//	if (sqlite3_reset(impl->m_prepStmtREPLACE) || sqlite3_clear_bindings(impl->m_prepStmtREPLACE))
//		return Handy::Result(false, "Unable to reset/clear old state for REPLACE prepared statment.");
//
//	const char * zAName = zFilenameDstArchive;
//
//	while (zAName[0] == '/') 
//		zAName++;
//
//	sqlite3_bind_text (impl->m_prepStmtREPLACE, 1, zAName, -1, SQLITE_STATIC);
//	sqlite3_bind_int  (impl->m_prepStmtREPLACE, 2, x.st_mode);
//	sqlite3_bind_int64(impl->m_prepStmtREPLACE, 3, x.st_mtime);
//
//	int szOrig;
//	int szCompr;
//
//	Handy::ResultV<char *> rRes = read_reg_file(zFilenameSrcOSPath, &szOrig, &szCompr, noCompress);
//
//	if (!rRes.Success || !rRes.OpValue.has_value())
//		return Handy::Result(false, rRes.Reason);
//
//	char * zContent = rRes.OpValue.value();
//
//	sqlite3_bind_int (impl->m_prepStmtREPLACE, 4, szOrig);
//	sqlite3_bind_blob(impl->m_prepStmtREPLACE, 5, zContent, szCompr, delete_arr_ptr);
//
//	if (verbose) 
//	{
//		if (szCompr < szOrig) 
//		{
//			int pct = szOrig ? (100 * (sqlite3_int64)szCompr) / szOrig : 0;
//			printf("  added: %s -> %s (deflate %d%%)\n", zFilenameSrcOSPath, zFilenameDstArchive, 100 - pct);
//		}
//		else 
//		{
//			printf("  added: %s -> %s\n", zFilenameSrcOSPath, zFilenameDstArchive);
//		}
//	}
//
//	if (SQLITE_DONE != sqlite3_step(impl->m_prepStmtREPLACE))
//		return Handy::Result(false, std::string("Insert failed for ") + zFilenameSrcOSPath + " -> " + zFilenameDstArchive + " : " + sqlite3_errmsg(impl->m_db));
//
//	return Handy::Result(true);
//}