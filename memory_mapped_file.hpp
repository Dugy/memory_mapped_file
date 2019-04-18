/*!
* \file memory_mapped_file.hpp
* \date 2019/01/24 14:43
*
* \author Ján Dugáček
*
* \brief Class for accessing an array of structs stored in a file
*
* This class allows storing an array of structs in a binary file or archive. The struct must not contain pointers, variables dependent on any other
* values or addresses the program or whatever else that could be damaged when memory-copied into another instance of the program. If you really
* need pointers, you must use array indexes instead.
*
* \note RAII guarantees flushing the changes into files
*/

#ifndef MEMORY_MAPPED_FILE_H
#define MEMORY_MAPPED_FILE_H

#include "memory_mapped_file_base.hpp"

template<typename...>
class MemoryMappedFile;

template<typename T>
class MemoryMappedFile<T> {
	std::unique_ptr<MemoryMappedFileBase> archiver_;

	union Converter {
		std::uint8_t byte[sizeof(T)];
		T contents;
	};

protected:

	/*!
	* \brief Constructor, use a derived class' constructor to specify the way the data is stored
	*/
	MemoryMappedFile(std::unique_ptr<MemoryMappedFileBase> archiver) : archiver_(std::move(archiver)) {}

public:

	/*!
	* \brief Move constructor
	*/
	MemoryMappedFile(MemoryMappedFile<T>&& other) = default;

	/*!
	* \brief Returns the file name without extension
	*
	* \return The name of the file
	*/
	const std::string &fileName() const
	{
		return archiver_->fileName();
	}

	/*!
	* \brief Returns the file name with extension
	*
	* \return The name of the file
	*/
	std::string extendedFileName() const
	{
		return archiver_->extendedFileName(archiver_->fileName());
	}

	/*!
	* \brief Saves the contents if it was modified into the file it was loaded from
	*/
	void flush() const
	{
		archiver_->flush();
	}

	/*!
	* \brief Byte acccess, allows modification
	*
	* \param Index of the byte
	* \return Reference to the byte
	*/
	T &operator[](int at)
	{
		if (!archiver_->canReadAt(at * (sizeof(T)) + 1))
			throw(std::logic_error("Reading behind the end of an archive"));
		return reinterpret_cast<T &>((*archiver_)[at * sizeof(T)]);
	}

	/*!
	* \brief Byte acccess, modification not possible
	*
	* \param Index of the byte
	* \return Const reference to the byte
	*/
	const T &operator[](int at) const
	{
		if (!archiver_->canReadAt(at * (sizeof(T)) + 1))
			throw(std::logic_error("Reading behind the end of an archive"));
		return reinterpret_cast<const T &>(const_cast<const MemoryMappedFileBase &>(*archiver_)[at * sizeof(T)]);
	}

	/*!
	* \brief Appends data at the end of the file
	*
	* \param Vector of bytes to append
	* \note It's virtual to allow optimising flushing after this probably frequent operation
	*/

	void push_back(const T &added)
	{
		archiver_->append(reinterpret_cast<const std::uint8_t*>(&added), sizeof(T));
	}

	/*!
	* \brief Clears the contents
	*/
	void clear()
	{
		archiver_->clear();
	}

	/*!
	* \brief Access to constant data
	*
	* \return Const reference to array containing the data
	*/
	const T *data() const
	{
		return reinterpret_cast<const T*>((archiver_->data().data()));
	}

	/*!
	* \brief Gets size of the data
	*
	* \return Size of the data
	*/
	int size() const
	{
		return archiver_->size() / sizeof(T);
	}

	/*!
	* \brief Swaps loaded contents with another file
	*
	* \param Another file to swap
	*/

	void swap(MemoryMappedFile<T> &other)
	{
		archiver_.swap(other.archiver_);
	}

	/*!
	* \brief Swaps contents with other data
	*
	* \param Another file to swap
	*/

	void swap(const T* data, int size)
	{
		archiver_->clear();
		archiver_->append(reinterpret_cast<const std::uint8_t*>(data), size * sizeof(T));
	}

	/*!
	* \brief Destructor
	*/

	virtual ~MemoryMappedFile() = default;
};

template<typename T, typename archiverType>
class MemoryMappedFile<T, archiverType> : public MemoryMappedFile<T> {
public:

	/*!
	* \brief Constructor, specify the second argument to set the way the data is stored
	*
	* \param The name of the file
	*/
	MemoryMappedFile(const std::string &fileName) : MemoryMappedFile<T>(std::make_unique<archiverType>(fileName)) {}

	/*!
	* \brief Move constructor
	*/
	MemoryMappedFile(MemoryMappedFile<T, archiverType>&& other) = default;
};


#endif // !memory_mapped_file_H
