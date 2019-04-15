/*!
* \file memory_mapped_file_uncompressed.hpp
* \date 2019/01/24 9:57
*
* \author Ján Dugáček
*
* \brief Class wrapping contents of a binary file
*
* It is created to share a common interface with MemoryMappedFileCompressed, the wrapper around LZMA archives.
*
* When it flushes its contents into a file, it checks if all the changes were just appends to the end and if it's true, it appends the changes at the end of file on disk.
*/

#ifndef MEMORY_MAPPED_FILE_UNCOMPESSED_H
#define MEMORY_MAPPED_FILE_UNCOMPESSED_H

#include <string>
#include <vector>
#include <cstdint>
#include "memory_mapped_file_base.hpp"

class MemoryMappedFileUncompressed : public MemoryMappedFileBase {
	mutable int appendedFrom_;
	virtual const std::string &fileNameExtension() const override
	{
		const static std::string extension(".dat");
		return extension;
	}

	void reset();

public:
	/*!
	* \brief Constructor: loads file if exists, or starts holding an empty string
	*
	* \param Name of the file, without suffix
	*/
	MemoryMappedFileUncompressed(const std::string &fileName);

	/*!
	* \brief Destructor, flushes changes
	*/
	virtual ~MemoryMappedFileUncompressed() override;

	/*!
	* \brief Gets size of the data
	*
	* \return Size of the data
	*/
	virtual int size() const override;

	/*!
	* \brief Loads the file up to the given byte
	*
	* \param How many bytes have to be loaded, negative number means load all
	*/
	virtual void load(int until = -1) const override;

	/*!
	* \brief Flushes and abandons the old file if necessary, loads a new one if necessary and loads up to the given byte
	*
	* \param Name of the new file to load, initialises to empty string if the file doesn't exist
	* \param How many bytes have to be loaded, negative number means load all
	*/
	virtual void load(const std::string &fileName, int until = -1) override;

	/*!
	* \brief Saves the contents if it was modified into the last file it was loaded from
	*/
	virtual void flush() const override;

	/*!
	* \brief Saves the contents if it was modified into the specified file
	*
	* \param The name of the file to save to
	*/
	virtual void flush(const std::string &fileName) const override;

	/*!
	* \brief Appends data at the end of the file
	*
	* \param Vector of bytes to append
	* \note This overrides parent's method to allow appending to file instead of overwriting if only this method was used to modify it
	*/
	virtual void append(const std::vector<std::uint8_t> &added) override;

	/*!
	* \brief Appends data at the end of the file
	*
	* \param Raw pointer to the data
	* \param Size of the data in bytes
	* \note This overrides parent's method to allow appending to file instead of overwriting if only this method was used to modify it
	*/
	virtual void append(const std::uint8_t* added, int size) override;

	/*!
	* \brief Appends a byte at the end of the file
	*
	* \param The byte to append
	* \note This overrides parent's method to allow appending to file instead of overwriting if only this method was used to modify it
	*/
	virtual void push_back(std::uint8_t added) override;

	/*!
	* \brief Returns the extension typical for this type of archive
	*
	* \return The extension, without point
	*/
	static const std::string &standardExtension();
};

#endif //MEMORY_MAPPED_FILE_UNCOMPESSED_H
