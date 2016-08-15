
#include <memory>
#include <iostream>

#include "FileArchive.hpp"

//This tester hasn't been updated for changes to sqlarlib.

void try_FailPrintExit(Result const & res)
{
	if (res.Success)
		return;

	std::cerr << res.Reason;
	exit(4);
}

int main()
{
	std::cout << "Beginning tests... " << std::endl;

	auto fao = FileArchive::Open("test.archive", FileArchive::Mode::Create_Replace);

	if (!fao.Success ||!fao.OpValue)
	{
		std::cerr << "Archive open/create failed: " << fao.Reason << std::endl;

		return 3;
	}

	std::cout << "Successful!" << std::endl;

	std::unique_ptr<FileArchive> fa = std::move(fao.OpValue.value());

	try_FailPrintExit(fa->Add("testFile1.png", "C:\\testFile.png", false, true));
	try_FailPrintExit(fa->Add("testFile2.png", "C:\\testFile.png", false, true));
	try_FailPrintExit(fa->Add("testFile3.png", "C:\\testFile.png", false, true));

	fa->PrintFilenames();

	try_FailPrintExit(fa->Add("testFile4.png", "C:\\testFile.png", false, true));
	try_FailPrintExit(fa->Add("testFile5.png", "C:\\testFile.png", false, true));

	try_FailPrintExit(fa->Extract("testFile1.png", "C:\\resultFile1.png", true));
	try_FailPrintExit(fa->Extract("testFile2.png", "C:\\resultFile2.png", true));
	try_FailPrintExit(fa->Extract("testFile3.png", "C:\\resultFile3.png", true));

	fa->PrintFileinfos();

	try_FailPrintExit(fa->Extract("testFile4.png", "C:\\resultFile4.png", true));
	try_FailPrintExit(fa->Extract("testFile5.png", "C:\\resultFile5.png", true));

	try_FailPrintExit(fa->Delete("testFile3.png"));

	fa->PrintFilenames();

	auto resG = fa->Get("testFile4.png");

	if (!resG.Success || !resG.OpValue)
	{
		std::cerr << resG.Reason;
		exit(5);
	}

	std::unique_ptr<char[]> arr(std::move(resG.OpValue.value()));



	return 0;
}

