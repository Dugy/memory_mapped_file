#include <iostream>
#include <functional>
#include <memory>
#include "memory_mapped_file_base.hpp"
#include "memory_mapped_file_uncompressed.hpp"
#include "memory_mapped_file_compressed.hpp"
#include "memory_mapped_file.hpp"

bool flawless = true;

template<typename retType>
void makeTest(retType expected, std::function<retType()> action, const std::string &failureComment)
{
	retType returned;
	try {
		returned = action();
	}
	catch(std::exception &e) {
		std::cout << failureComment << ": " << e.what() << std::endl;
		flawless = false;
	}
	if (returned != expected) {
		std::cout << failureComment << ": '" << returned << "' vs '" << expected << "'" << std::endl;
		flawless = false;
	}
}

inline std::string vec2string(const std::vector<unsigned char> &str)
{
	std::string retVal;
	for (unsigned char c : str)
		retVal.push_back(c);
	return retVal;
}


int main()
{

	for (int i = 0; i < 2; i++) {
		if (i) std::cout << "Starting tests of archivation" << std::endl;
		else std::cout << "Starting tests of plaintext storage" << std::endl;

		std::string sample = "This string contains highly interesting text that can pick people's attention at first sight";
		std::vector<unsigned char> shortData;
		for (unsigned int i = 0; i < sample.size() && i < 5; i++)
			shortData.push_back(sample[i]);
		std::string shortDataAsString = vec2string(shortData);
		std::vector<unsigned char> longData;
		for (unsigned int i = 0; i < sample.size(); i++)
			longData.push_back(sample[i]);

		auto getTheRightArchive = [&](const std::string& name) -> std::unique_ptr<MemoryMappedFileBase> {
			if (i) return std::make_unique<MemoryMappedFileCompressed>(name);
			else return std::make_unique<MemoryMappedFileUncompressed>(name);
		};
		try {
			{
				std::unique_ptr<MemoryMappedFileBase> archive = getTheRightArchive("test1");
				archive->clear();
				makeTest<int>(0, [&] { return archive->size(); }, "Basic test of archive clear and size checking failed");

				archive->append(shortData);
				makeTest<std::string>(shortDataAsString, [&] { return vec2string(archive->data()); }, "Basic test of archive append and access failed");

				(*archive)[0] = 's';
				shortDataAsString[0] = 's';
				makeTest<unsigned char>('s', [&] { return archive->data()[0]; }, "Test of archive overwrite failed");
				makeTest<unsigned char>('h', [&] { return archive->data()[1]; }, "Second test of archive overwrite failed");
			}

			{
				std::unique_ptr<MemoryMappedFileBase> archive = getTheRightArchive("test1");
				makeTest<std::string>(shortDataAsString, [&] { return vec2string(archive->data()); }, "Test of archive re-read failed");

				archive->append(longData);
				archive->flush("test2");
				makeTest<int>(true, [&] { return (archive->size() > 30); }, "Later test of archive append failed");
			}

			{
				std::unique_ptr<MemoryMappedFileBase> archive = getTheRightArchive("test2");
				makeTest<bool>(true, [&] { return (archive->size() > 30); }, "Test of archive flush failed");

				archive->clear();
				std::vector<unsigned char> appended;
				appended.push_back('E');
				archive->append(appended);
			}

			{
				std::unique_ptr<MemoryMappedFileBase> archive = getTheRightArchive("test1");
				archive->load("test2");
				makeTest<std::string>("E", [&] { return vec2string(archive->data()); }, "Test of archive load failed");
			}

			{
				std::unique_ptr<MemoryMappedFileBase> archive = getTheRightArchive("test_nonexistent");
				makeTest<int>(0, [&] { return archive->size(); }, "Test of empty archive failed");
			}

			{
				{
					std::unique_ptr<MemoryMappedFileBase> archive = getTheRightArchive("test1");
					archive->clear();
					archive->append(longData);
				}
				makeTest<std::string>(sample, [&] {
					std::string obtained;
					std::unique_ptr<const MemoryMappedFileBase> archive = getTheRightArchive("test1");
					for (int i = 0; archive->canReadAt(i); i++)
					{
						obtained.push_back((*archive)[i]);
					}
					return obtained;
				}, "Test of lazy loading clearly failed"); // Try also to test with smaller cache
			}

			{
				std::vector<std::uint8_t> testData;
				for (int i = 0; i < 10000; i++) {
					union {
						std::uint8_t bytes[2];
						std::uint16_t num;
					} block;
					block.num = i;
					testData.push_back(block.bytes[0]);
					testData.push_back(block.bytes[1]);
				}

				{
					std::unique_ptr<MemoryMappedFileBase> archive = getTheRightArchive("test2");
					archive->clear();
					archive->append(testData);
				}

				std::unique_ptr<MemoryMappedFileBase> archive = getTheRightArchive("test2");
				for (unsigned int i = 0; i < testData.size(); i++)
					makeTest<int>(testData[i], [&] { return (*archive)[i]; }, "Test with large archive failed");
			}


		}
		catch(std::exception &e) {
			std::cout << "A test failed with exception: " << e.what() << std::endl;
			flawless = false;
		}
	}

	try {
		std::cout << "Starting tests of more complex access" << std::endl;
		struct entry {
			uint64_t number;
			char name[8];
			entry(uint64_t _number, std::string _name) : number(_number) {
				for (int i = 0; i < 8; i++) {
					name[i] = _name[i];
					if (!name[i]) break;
				}
			}
		};

		std::vector<entry> entries{{1, "Gary"},{ 2, "Johnny" },{ 4, "Tim" },{ 6, "Mark" },{ 7, "Tony" }};
		{
			MemoryMappedFile<entry> file = MemoryMappedFile<entry, MemoryMappedFileCompressed>("struct_test");
			file.clear();
		}
		{
			MemoryMappedFile<entry> file = MemoryMappedFile<entry, MemoryMappedFileCompressed>("struct_test");
			file.push_back(entries[3]);
			makeTest<uint64_t>(entries[3].number, [&] { return file[0].number; }, "Test of append failed");
			file.clear();
			makeTest<uint64_t>(0, [&] { return file.size(); }, "Test of clear failed");
		}
		{
			MemoryMappedFile<entry> file = MemoryMappedFile<entry, MemoryMappedFileCompressed>("struct_test");
			file.push_back(entries[0]);
			file.push_back(entries[1]);
			file.push_back(entries[1]);
			file[2] = entries[2];
		}
		{
			MemoryMappedFile<entry> file = MemoryMappedFile<entry, MemoryMappedFileCompressed>("struct_test");
			const entry* ptr = file.data();

			for (int i = 0; i < 3; i++) {
				makeTest<uint64_t>(entries[i].number, [&] { return ptr[i].number; }, "Larger test failed");
			}

			file.clear();
			for (const entry &it: entries)
				file.push_back(it);

			for (unsigned int i = 0; i < entries.size(); i++)
				makeTest<uint64_t>(entries[i].number, [&] { return file[i].number; }, "Iteration test failed");
		}
	}
	catch(std::exception &e) {
		std::cout << "A test failed with exception: " << e.what() << std::endl;
		flawless = false;
	}

	if (flawless) {
		std::cout << "All tests finished successfully." << std::endl;
	}
	else {
		std::cout << "There were errors." << std::endl;
	}

	return 0;
}

