
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

	std::string testString = "C:\\testFile.png";
	std::vector<uint8_t> testData;
	testData.resize(testString.size() + 1, 0);
	memcpy(&testData[0], testString.c_str(), testString.size() + 1);

	auto fao = FileArchive::Open("test.archive", FileArchive::Mode::Create_Replace);

	if (!fao.Success ||!fao.OpValue)
	{
		std::cerr << "Archive open/create failed: " << fao.Reason << std::endl;

		return 3;
	}

	std::cout << "Successful!" << std::endl;

	FileArchive * fa = fao.OpValue.value();

	try_FailPrintExit(fa->Put("testFile1.png", testData, false));
	try_FailPrintExit(fa->Put("testFile2.png", testData, false));
	try_FailPrintExit(fa->Put("testFile3.png", testData, false));

	fa->PrintKeyNames();

	try_FailPrintExit(fa->Put("testFile4.png", testData, false));
	try_FailPrintExit(fa->Put("testFile5.png", testData, false));

	std::vector<uint8_t> buffer;

	try_FailPrintExit(fa->Get("testFile1.png", buffer));
	try_FailPrintExit(fa->Get("testFile2.png", buffer));
	try_FailPrintExit(fa->Get("testFile3.png", buffer));

	fa->PrintKeyInfos();

	try_FailPrintExit(fa->Get("testFile4.png", buffer));
	try_FailPrintExit(fa->Get("testFile5.png", buffer));

	try_FailPrintExit(fa->Delete("testFile3.png"));

	fa->PrintKeyNames();

	Handy::Result resG = fa->Get("testFile4.png", buffer);

	if (!resG.Success)
	{
		std::cerr << resG.Reason;
		exit(5);
	}

	return 0;
}

