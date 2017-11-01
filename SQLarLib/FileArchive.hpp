#pragma once

#include <string>
#include <memory>
#include <array>
#include <vector>
#include <cstdint>

#include "../../HandyCpp/Handy.hpp"
#include "../../HandyCpp/HandyExtended.hpp"

namespace SQLarLib
{
	// Unlike the official "sqlar" utility, this API does not support encryption extentions for SQLite (aka "see").
	struct FileArchive
	{
		ADDPRIVATE_STRUCT_NOCOPY

		enum class Mode : uint32_t
		{
			Create,         // Create a new archive and then open. Fail is file already exists.
			Create_Replace, // Create a new archive and then open. Delete and replace if the file already exists.
			Open_Create,    // Open existing archive, or if the file doesn't already exist--creates a new one and opens it.
			Open,           // Open existing archive, or fail if the file doesn't already exist.
			OpenReadOnly    // Open existing archive, or fail if the file doesn't already exist. DO NOT ALLOW WRITE OPERATIONS.
		};

		static ResultV<FileArchive *>    Open(std::string filepath, Mode mode);
		static void                      AbortRollback(std::unique_ptr<FileArchive> fa);

		Result                           Add       (std::string dstArchivePath, std::string srcOSPath, bool noCompress = false, bool verbose = true);
		Result                           Delete    (std::string archivePath);
	
		Result                           Extract   (std::string srcArchivePath, std::string dstOSPath, bool verbose = true);
		Result                           ExtractAll(std::string dstOSDirPath, bool verbose = true);

		ResultV<std::tuple<char *, size_t>>   
										 Get(std::string archivePath);
		Result                           Put       (std::string archivePath, char const * ptr, int numBytes, bool noCompress = false, bool verbose = true);

		std::vector<std::string>         Filenames ();
		bool                             HasFile   (std::string filename);

		void                             PrintFilenames();
		void                             PrintFileinfos();

		virtual                         ~FileArchive();

	private: 
		                                 FileArchive();
	};

}
