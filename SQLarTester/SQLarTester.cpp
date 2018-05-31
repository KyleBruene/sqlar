
#include <memory>
#include <iostream>

#include  "FileArchive.hpp"

//This tester hasn't been updated for changes to sqlarlib.

using namespace SQLarLib;

void try_FailPrintExit(Handy::Result const & res)
{
	if (res.Success)
		return;

	std::cerr << res.Reason;
	exit(4);
}

template <typename T>
void try_FailPrintExit(Handy::ResultV<T> const & res)
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

	FileArchive * fa = fao.OpValue.value();

	try_FailPrintExit(fa->Put("testFile1.png", "C:\\testFile.png", 15, false, true));
	try_FailPrintExit(fa->Put("testFile2.png", "C:\\testFile.png", 15, false, true));
	try_FailPrintExit(fa->Put("testFile3.png", "C:\\testFile.png", 15, false, true));

	fa->PrintFilenames();

	try_FailPrintExit(fa->Put("testFile4.png", "C:\\testFile.png", 15, false, true));
	try_FailPrintExit(fa->Put("testFile5.png", "C:\\testFile.png", 15, false, true));

	try_FailPrintExit(fa->Get("testFile1.png"));
	try_FailPrintExit(fa->Get("testFile2.png"));
	try_FailPrintExit(fa->Get("testFile3.png"));

	fa->PrintFileinfos();

	try_FailPrintExit(fa->Get("testFile4.png"));
	try_FailPrintExit(fa->Get("testFile5.png"));

	try_FailPrintExit(fa->Delete("testFile3.png"));

	fa->PrintFilenames();

	auto resG = fa->Get("testFile4.png");

	if (!resG.Success || !resG.OpValue)
	{
		std::cerr << resG.Reason;
		exit(5);
	}

	char * arr = std::get<0>(resG.OpValue.value());



	return 0;
}

