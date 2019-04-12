#include "memory_mapped_file_base.hpp"

MemoryMappedFileBase::MemoryMappedFileBase(const std::string &fileName) :
	modified_(false),
	fileName_(fileName),
	loadedUntil_(0),
	fileSize_(-1)
{
}

MemoryMappedFileBase::~MemoryMappedFileBase()
{
}

const std::string &MemoryMappedFileBase::fileName() const
{
	return fileName_;
}

const std::string MemoryMappedFileBase::extendedFileName(const std::string &from) const
{
	return from + fileNameExtension();
}

void MemoryMappedFileBase::append(const std::vector<std::uint8_t> &added)
{
	if (!fullyLoaded()) load();
	modified_ = true;
	data_.insert(data_.end(), added.begin(), added.end());
}

void MemoryMappedFileBase::append(const std::uint8_t* added, int size)
{
	if (!fullyLoaded()) load();
	modified_ = true;
	for (int i = 0; i < size; i++)
		data_.push_back(added[i]);
}

void MemoryMappedFileBase::push_back(uint8_t added)
{
	if (!fullyLoaded()) load();
	modified_ = true;
	data_.push_back(added);
}

void MemoryMappedFileBase::clear()
{
	if (!data_.empty() || !fullyLoaded()) {
		modified_ = true;
		data_.clear();
		loadedUntil_ = 0;
		fileSize_ = 0;
	}
}

const std::vector<std::uint8_t> &MemoryMappedFileBase::data() const
{
	if (!fullyLoaded()) load();
	return data_;
}

void MemoryMappedFileBase::swapContents(std::vector<std::uint8_t> &other)
{
	if (!fullyLoaded()) load();
	modified_ = true;
	swap(data_, other);
}
