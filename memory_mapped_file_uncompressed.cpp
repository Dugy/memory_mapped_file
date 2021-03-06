#include "memory_mapped_file_uncompressed.hpp"
#include <fstream>
#include <iostream>
#include <algorithm>

constexpr float LOADED_PART_INCREMENT = 1.5;
constexpr int LOADED_PART_MAX_INCREMENT = (1 << 15);
constexpr int LOADED_PART_MIN_INCREMENT = (1 << 11);

inline std::string vec2string(const std::vector<unsigned char> &str)
{
	std::string retVal;
	for (unsigned char c : str)
		retVal.push_back(char(c));
	return retVal;
}

void MemoryMappedFileUncompressed::reset()
{
	data_.clear();
	modified_ = false;
	appendedFrom_ = 0;
	loadedUntil_ = 0;
	fileSize_ = -1;
}

MemoryMappedFileUncompressed::MemoryMappedFileUncompressed(const std::string &fileName) :
	MemoryMappedFileBase(fileName)
{
	reset();
}

MemoryMappedFileUncompressed::~MemoryMappedFileUncompressed()
{
	try {
		flush();
	}
	catch(std::exception &exception) {
		std::cout << "Failed to flush: " << exception.what();
	}
}

int MemoryMappedFileUncompressed::size() const
{
	if (modified_) return int(data_.size());
	
	if (fileSize_ >= 0) {
		return std::max<int>(int(data_.size()), fileSize_);
	}
	
	std::ifstream file(extendedFileName(fileName_), std::ifstream::ate | std::ifstream::binary);
	fileSize_ = file.good() ? int(file.tellg()) : 0;
	return fileSize_;
}

void MemoryMappedFileUncompressed::load(int until) const
{
	if (fullyLoaded() || (until >= 0 && loadedUntil_ > until)) return;
	
	const int stopAt = (until >= 0) ? std::max<int>(std::min<int>(int(until * LOADED_PART_INCREMENT), until + LOADED_PART_MAX_INCREMENT),
			until + LOADED_PART_MIN_INCREMENT) : INT_MAX;

	std::ifstream file(extendedFileName(fileName_), std::fstream::binary);
	file.seekg(loadedUntil_);
	
	auto formerLoadedUntil = loadedUntil_;
	while (file.good() && loadedUntil_ < stopAt) {
		const_cast<std::vector<std::uint8_t>&>(data_).push_back(uint8_t(file.get()));
		loadedUntil_++;
	}
	if (loadedUntil_ != formerLoadedUntil) {
		const_cast<std::vector<std::uint8_t>&>(data_).pop_back();  // The previous cycle reads an extra symbol, we need to remove it
		loadedUntil_--;
	}
	if (!file.good()) {
		fileSize_ = loadedUntil_;
	}
	
	if (loadedUntil_ == fileSize_) appendedFrom_ = int(data_.size());
}

void MemoryMappedFileUncompressed::load(const std::string &fileName, int until)
{
	if (fileName != fileName_) {
		flush();
		reset();
		fileName_ = fileName;
	}
	load(until);
}

void MemoryMappedFileUncompressed::flush() const
{
	flush(fileName_);
}

void MemoryMappedFileUncompressed::flush(const std::string &fileName) const
{
	// If modified, it must be fully loaded
	auto updateSizes = [this] {
		appendedFrom_ = int(data_.size());
		loadedUntil_ = int(data_.size());
		fileSize_ = int(data_.size());
	};
	if (modified_) {
		std::ofstream file(extendedFileName(fileName), std::fstream::trunc | std::fstream::binary);
		if (!file.good()) throw(std::runtime_error("Could not open file " + extendedFileName(fileName)));
		for (uint8_t byte : data_)
			file << byte;
		if (!file.good()) throw(std::runtime_error("Could not write to file " + extendedFileName(fileName)));
		updateSizes();
	}
	else if (loadedUntil_ == fileSize_ && appendedFrom_ < int(data_.size())) {
		std::ofstream file(extendedFileName(fileName), std::fstream::app | std::fstream::ate | std::fstream::binary);
		if (!file.good()) throw(std::runtime_error("Could not open file " + extendedFileName(fileName)));
		for (unsigned int i = static_cast<unsigned int>(appendedFrom_); i < data_.size(); i++)
			file << data_[i];
		if (!file.good()) throw(std::runtime_error("Could not write to file " + extendedFileName(fileName)));
		updateSizes();
	} // else don't need to save
}

void MemoryMappedFileUncompressed::append(const std::vector<std::uint8_t>& added)
{
	load();
	data_.insert(data_.end(), added.begin(), added.end());
}

void MemoryMappedFileUncompressed::append(const std::uint8_t *added, int size)
{
	load();
	for (int i = 0; i < size; i++)
		data_.push_back(added[i]);
}

void MemoryMappedFileUncompressed::push_back(std::uint8_t added)
{
	load();
	data_.push_back(added);
}

const std::string &MemoryMappedFileUncompressed::standardExtension()
{
	static std::string retval = "dat";
	return retval;
}
