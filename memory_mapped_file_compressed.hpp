/*!
* \file memory_mapped_file_compressed.hpp
* \date 2019/01/23 14:06
*
* \author Ján Dugáček
*
* \brief Class wrapping contents of an archive
*
* Encapsulates access to a LZMA archive and allows modifying it as a vector of bytes and flushing the changes afterwards
*
* \note The main reason to create this class is to abstract from the user-hostile API of the otherwise very efficient implementation of LZMA
*/

#ifndef MEMORY_MAPPED_FILE_COMPESSED_H
#define MEMORY_MAPPED_FILE_COMPESSED_H

#include <string>
#include <vector>
#include <cstdint>
#include "memory_mapped_file_base.hpp"

class MemoryMappedFileCompressed : public MemoryMappedFileBase {
	virtual std::string fileNameExtension() const override
	{
		return ".lzma";
	}
	void reset();
public:
	/*!
	* \brief Constructor: loads file if exists, or starts holding an empty string
	*
	* \param Name of the file, without suffix
	*/
	MemoryMappedFileCompressed(const std::string &fileName);

	/*!
	* \brief Destructor, flushes changes
	*/
	virtual ~MemoryMappedFileCompressed() override;

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
	* \brief Returns the extension typical for this type of archive
	*
	* \return The extension, without point
	*/
	static const std::string &standardExtension();
};

#endif //MEMORY_MAPPED_FILE_COMPESSED_H
