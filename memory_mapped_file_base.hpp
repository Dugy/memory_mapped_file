/*!
* \file memory_mapped_file_base.hpp
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

#ifndef MEMORY_MAPPED_FILE_BASE_H
#define MEMORY_MAPPED_FILE_BASE_H

#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include <iostream>

class MemoryMappedFileBase {
protected:
	mutable bool modified_;
	std::string fileName_;
	std::vector<std::uint8_t> data_;
	mutable int loadedUntil_;
	mutable int fileSize_;

	virtual const std::string &fileNameExtension() const = 0;
public:
	/*!
	* \brief Constructor: should load file if exists, or start holding an empty string
	*
	* \param Name of the file, without suffix
	*/
	MemoryMappedFileBase(const std::string &fileName);

	/*!
	* \brief Destructor, should flush changes
	*/
	virtual ~MemoryMappedFileBase();

	/*!
	* \brief Should load the file up to the given byte
	*
	* \param How many bytes have to be loaded, negative number means load all
	*/
	virtual void load(int until = -1) const = 0;

	/*!
	* \brief Should flush and abandon the old file if necessary, load a new one if necessary and load until given byte
	*
	* \param Name of the new file to load, becomes empty if the file doesn't exist
	* \param How many bytes have to be loaded, negative number means load all
	*/
	virtual void load(const std::string &fileName, int until = -1) = 0;

	/*!
	* \brief Saves the contents if it was modified into the last file it was loaded from
	*/
	virtual void flush() const = 0;

	/*!
	* \brief Saves the contents if it was modified into the specified file
	*
	* \param The name of the file to save to
	*/
	virtual void flush(const std::string &fileName) const = 0;

	/*!
	* \brief Checks if a byte is accessible, to allow boundary checks without checking size(), which may be inefficient
	*
	* \param Index of the byte
	* \return Whether the byte can be loaded
	*/
	inline bool canReadAt(int at) const
	{
		if (at < loadedUntil_ || at < int(data_.size())) return true;
		if (fullyLoaded()) return false;
		load(at);
		return (at < loadedUntil_ || at < int(data_.size()));
	}

	/*!
	* \brief Byte acccess, allows modification
	*
	* \param Index of the byte
	* \return Reference to the byte
	*/
	inline std::uint8_t &operator[](int at)
	{
		if (!fullyLoaded()) load();
		modified_ = true;
		return data_[at];
	}

	/*!
	* \brief Byte acccess, modification not possible
	*
	* \param Index of the byte
	* \return Const reference to the byte
	*/
	inline const std::uint8_t &operator[](int at) const
	{
		if (at >= loadedUntil_) load(at);
		return data_[at];
	}

	/*!
	* \brief Gets size of the data
	*
	* \return Size of the data
	* \note May have to read the whole file
	*/
	virtual int size() const = 0;

	/*!
	* \brief Gets size of the data
	*
	* \return Size of the data
	*/
	const std::string &fileName() const;

	/*!
	* \brief Gets size of the data
	*
	* \return Size of the data
	*/
	const std::string extendedFileName(const std::string &from) const;

	/*!
	* \brief Appends data at the end of the file
	*
	* \param Vector of bytes to append
	* \note It's virtual to allow optimising flushing after this probably frequent operation
	*/
	virtual void append(const std::vector<std::uint8_t> &added);

	/*!
	* \brief Appends data at the end of the file
	*
	* \param Raw pointer to the data
	* \param Size of the data in bytes
	* \note It's virtual to allow optimising flushing after this probably frequent operation
	*/
	virtual void append(const std::uint8_t* added, int size);

	/*!
	* \brief Appends a byte at the end of the file
	*
	* \param The byte to append
	* \note It's virtual to allow optimising flushing after this probably frequent operation
	*/
	virtual void push_back(std::uint8_t added);

	/*!
	* \brief Clears the contents
	*/
	void clear();

	/*!
	* \brief Access to constant data
	*
	* \return Const reference to vector containing the data
	*/
	const std::vector<std::uint8_t> &data() const;

	/*!
	* \brief Swaps contents for another vector of std::uint8_ts
	*
	* \param The other vector
	*/
	void swapContents(std::vector<std::uint8_t> &other);

	/*!
	* \brief Returns if the archive is fully loaded
	*
	* \return Bool whether it's fully loaded
	*/
	inline bool fullyLoaded() const
	{
		return ((fileSize_ >= 0 && fileSize_ <= loadedUntil_) || fileSize_ == 0);
	}
};

#endif //MEMORY_MAPPED_FILE_BASE_H
