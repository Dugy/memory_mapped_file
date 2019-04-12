// To prevent some MSVC complaints
#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <algorithm>
#include <exception>

#include "lzma_lib/Alloc.h"
#include "lzma_lib/7zFile.h"
#include "lzma_lib/LzmaDec.h"
#include "lzma_lib/LzmaEnc.h"
#include "memory_mapped_file_compressed.hpp"

// Most of this file is quite ugly code written in a heinous C/C++ hybrid.
// It was necessary because of LZMA's ugly interface and similarly ugly C++ wrapper.
// This file should wrap it and separate it from the rest of code, exposing only
// a clean object-oriented RAII-based user-friendly API.

constexpr float LOADED_PART_INCREMENT = 1.5;
constexpr int LOADED_PART_MAX_INCREMENT = (1 << 15);
constexpr int LOADED_PART_MIN_INCREMENT = (1 << 11);


namespace FromLzma {
static void* SzAlloc(void* p, size_t size)
{
	p = p;
	return MyAlloc(size);
}
static void SzFree(void* p, void* address)
{
	p = p;
	MyFree(address);
}
static ISzAlloc g_Alloc = { (void* (__cdecl*)(ISzAllocPtr, std::size_t))SzAlloc, (void (__cdecl*)(ISzAllocPtr, void* ))SzFree };

constexpr uint32_t INPUT_BUFFER_SIZE = (1 << 15);
constexpr uint32_t OUTPUT_BUFFER_SIZE = (1 << 15);

struct CFileSeqOutStream {
	ISeqOutStream funcTable;
	FILE* file;
};

struct ReadingStreamData {
	const std::vector<std::uint8_t> &vector;
	unsigned int position;
};

static SRes readFromMemory(void* p, void* buf, size_t* size)
{
	if (*size == 0)
		return SZ_OK;

	ReadingStreamData* data = (ReadingStreamData*)((CFileSeqInStream*)p)->file.handle;
	int sizeRead = 0;
	for (unsigned int i = data->position; i < data->vector.size() && i < data->position + *size; i++) {
		((std::uint8_t*)buf)[i - data->position] = data->vector[i];
		sizeRead++;
	}
	*size = sizeRead;
	data->position += sizeRead;

	return SZ_OK;
}

static size_t writeToMemory(void* pp, const void* buf, size_t size)
{
	if (size == 0)
		return 0;
	return fwrite(buf, 1, size, ((CFileSeqOutStream*)pp)->file);
}
}

void MemoryMappedFileCompressed::load(int until) const
{
	if ((until >= 0 && loadedUntil_ > until) || fileSize_ == 0) return;

	std::unique_ptr<CLzmaDec> lzmaState;

	int stopAt = (until >= 0) ? std::max<int>(std::min<int>(int(until * LOADED_PART_INCREMENT), until + LOADED_PART_MAX_INCREMENT),
											  until + LOADED_PART_MIN_INCREMENT) : INT_MAX;

	FILE* input = fopen(extendedFileName(fileName_).c_str(), "rb");
	if (input == nullptr) {
		fileSize_ = 0;
		return;
	}

	ELzmaStatus status;
	bool corrupt = false; // bool corrupt = !government.isCorrupt(); // sets variable to false

	//std::cout << "Initialising " << extendedFileName(fileName_) << " ..." << std::endl;

	lzmaState = std::make_unique<CLzmaDec>();

	/* header: 5 bytes of LZMA properties and 8 bytes of uncompressed size */
	std::uint8_t header[LZMA_PROPS_SIZE + 8];

	/* Read and parse header */

	const size_t lengthRead = fread(header, 1, sizeof(header), input);
	if (lengthRead != sizeof(header)) {
		throw(std::runtime_error("Archive header is broken"));
	}

	bool archiveHasSize = false;
	fileSize_ = 0;
	for (int i = 0; i < 8; i++) {
		std::uint8_t b = header[LZMA_PROPS_SIZE + i];
		if (b != 0xFF)
			archiveHasSize = true;
		fileSize_ += (UInt64)b << (i * 8);
	}
	if (!archiveHasSize)
		fileSize_ = -1;

	LzmaDec_Construct(lzmaState.get());
	int result = LzmaDec_Allocate(lzmaState.get(), header, LZMA_PROPS_SIZE, &FromLzma::g_Alloc);

	if (result != SZ_OK) {
		throw(std::runtime_error("LzmaDec_Allocate failed because " + std::to_string(result)));
	}

	LzmaDec_Init(lzmaState.get());
	//std::cout << "Successful allocation for file size " << fileSize_ << std::endl;

	if (result == SZ_OK) {
		const_cast<std::vector<std::uint8_t>&>(data_).clear();
		loadedUntil_ = 0;

		Byte inBuf[FromLzma::INPUT_BUFFER_SIZE];
		Byte outBuf[FromLzma::OUTPUT_BUFFER_SIZE];
		size_t inPos = 0, inSize = 0, outPos = 0;
		LzmaDec_Init(lzmaState.get());
		int left = fileSize_ - data_.size();
		while (true) {
			if (inPos == inSize) {
				inSize = fread(inBuf, 1, FromLzma::INPUT_BUFFER_SIZE, input);
				inPos = 0;
			}
			SizeT inProcessed = inSize - inPos;
			SizeT outProcessed = FromLzma::OUTPUT_BUFFER_SIZE - outPos;
			ELzmaFinishMode finishMode = LZMA_FINISH_ANY;
			if (archiveHasSize && (int)outProcessed > left) {
				outProcessed = (SizeT)left;
				finishMode = LZMA_FINISH_END;
			}

			result = LzmaDec_DecodeToBuf(lzmaState.get(), outBuf + outPos, &outProcessed,
										 inBuf + inPos, &inProcessed, finishMode, &status);
			inPos += (UInt32)inProcessed;
			outPos += outProcessed;
			left -= outProcessed;

			if (input != nullptr) {
				for (unsigned int i = 0; i < outPos; i++) {
					const_cast<std::vector<std::uint8_t>&>(data_).push_back(outBuf[i]);
					loadedUntil_++;
				}
			}
			outPos = 0;

			if (result != SZ_OK || (archiveHasSize && left == 0)) {
				if (result != SZ_OK) std::cout << "Decompression broke" << std::endl;
				break;
			}

			if (inProcessed == 0 && outProcessed == 0) {
				if (archiveHasSize || status != LZMA_STATUS_FINISHED_WITH_MARK)
					corrupt = true;
				break;
			}

			if (data_.size() >= stopAt) break;
		}
	}

	if (!archiveHasSize) fileSize_ = data_.size();

	fclose(input);

	if (result != SZ_OK)
		throw(std::runtime_error("Decompression problem " + std::to_string(result)));

	//   if (m_lastByteRead == size()) {
	LzmaDec_Free(lzmaState.get(), &FromLzma::g_Alloc);
	//   }

	if (corrupt)
		throw(std::runtime_error("Archive seems to be corrupted (has size: " + std::to_string(archiveHasSize) + " status: " +
								 std::to_string(status) + ")"));
}

void MemoryMappedFileCompressed::load(const std::string &fileName, int until)
{
	if (fileName != fileName_) {
		flush();
		reset();
		fileName_ = fileName;
	}

	load(until);
}

void MemoryMappedFileCompressed::flush() const
{
	flush(fileName_);
	modified_ = false;
}

void MemoryMappedFileCompressed::flush(const std::string &fileName) const
{
	if (fileName == fileName_) {
		if (!modified_)
			return;
	}

	//std::cout << "Flushing into " << extendedFileName(fileName) << std::endl;
	FILE* output = fopen(extendedFileName(fileName).c_str(), "wb");
	if (!output) {
		std::cerr << "Cannot save the file" << std::endl; // Better shouldn't throw here
		throw(std::runtime_error("Cannot save file " + extendedFileName(fileName)));
		return;
	}

	CLzmaEncHandle enc = LzmaEnc_Create(&g_Alloc);
	if (enc == 0) {
		std::cerr << "Cannot create encoder to save the file" << std::endl; // Better shouldn't throw here
		return;
	}

	CFileSeqInStream inStream;
	inStream.vt.Read = (SRes(*)(const ISeqInStream*, void* , size_t*))FromLzma::readFromMemory;
	FromLzma::ReadingStreamData data{ data_, 0 };
	inStream.file.handle = (FILE*)&data;

	FromLzma::CFileSeqOutStream outStream;
	outStream.funcTable.Write = (size_t(*)(const ISeqOutStream*, const void* , size_t))FromLzma::writeToMemory;
	outStream.file = output;


	CLzmaEncProps props;
	LzmaEncProps_Init(&props);
	SRes result = LzmaEnc_SetProps(enc, &props);

	if (result == SZ_OK) {
		Byte header[LZMA_PROPS_SIZE + 8];
		size_t headerSize = LZMA_PROPS_SIZE;
		result = LzmaEnc_WriteProperties(enc, header, &headerSize);
		const UInt64 fileSize = data_.size();

		for (int i = 0; i < 8; i++)
			header[headerSize++] = Byte(fileSize >> (8 * i));

		if (headerSize == 0 || fwrite(header, 1, headerSize, output) != headerSize)
			result = SZ_ERROR_WRITE;

		if (result == SZ_OK)
			result = LzmaEnc_Encode(enc, &outStream.funcTable, &inStream.vt,
									nullptr, &g_Alloc, &g_Alloc);
	}
	LzmaEnc_Destroy(enc, &g_Alloc, &g_Alloc);
	fclose(output);

	if (result != SZ_OK)
		throw(std::runtime_error("Could not save compressed file"));
}

//inline std::string vec2string(const std::vector<std::uint8_t> &str)
//{
//    std::string retVal;
//    for (std::uint8_t c : str)
//        retVal.push_back(c);
//    return retVal;
//}

void MemoryMappedFileCompressed::reset()
{
	data_.clear();
	modified_ = false;
	loadedUntil_ = 0;
	fileSize_ = -1;
}

MemoryMappedFileCompressed::MemoryMappedFileCompressed(const std::string &fileName) :
	MemoryMappedFileBase(fileName)
{
	reset();
}


MemoryMappedFileCompressed::~MemoryMappedFileCompressed()
{
	try {
		flush();
	}
	catch(std::exception &exception) {
		std::cout << "Failed to flush: " << exception.what();
	}
}

int MemoryMappedFileCompressed::size() const
{
	if (fullyLoaded()) {
		if (modified_) return data_.size();
		else return fileSize_;
	}
	if (fileSize_ >= 0) return fileSize_;

	load(100);
	if (fileSize_ >= 0)
		return fileSize_;
	load();
	return fileSize_;
}

const std::string &MemoryMappedFileCompressed::standardExtension()
{
	static std::string retval = "lzma";
	return retval;
}
