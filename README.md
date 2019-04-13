# Memory Mapped File
Utility for lazy loading of files into memory, reading them through random access and automatic saving if changes were made. It also contains an utility class for using files containing structs/classes and accessing them in a type-safe way. It also contains a facade for LZMA SDK that hides all its ugly parts in one source file (its source code isn't on GitHub and has to be provided from its website).

## High level usage

```C++
struct Entry {
	uint32_t timestamp;
	int32_t value;
	Entry(uint32_t timestamp, int32_t value) :
			timestamp(timestamp), value(value) {}
}
MemoryMappedFile<Entry, MemoryMappedFileUncompressed> file("stuff");
file.push_back(Entry(time(nullptr), 13));

std::cout << file[0].timestamp << " " << file[0].value << std::endl;
std::cout << "Size is now: " << file.size() << std::endl;
```

The changes are written to disk when the `flush()` method is called or when the object is destroyed.

## Lazy loading and const correctness

The data is lazy loaded, therefore the bytes are not loaded until they are accessed (if it's not available, it always loads all bytes until the intended byte and some reserve behind). This is optimised for scenarios when the most important bytes are at the start of the file.

Changes are not flushed to disk if the file was not modified. `operator[]` counts as modification if the object isn't const-qualified, so make sure you have it const-qualified wherever you can.

## Low level usage

In this example, the file is opened, a few bytes are added, the contents are printed and the changes are flushed.

```C++
MemoryMappedFileUncompressed file("saved_stuff");
file.push_back('a');
file.push_back('h');
file.push_back('o');
file.push_back('y');

for (int i = 0; file.canReadAt(i); i++) {
	std::cout << file[i];
}
std::cout << std::endl;
```
The bytes can be appended also using the `append()` method from a `std::vector<uint8_t>`.

## Compression

To store the data in a compressed file, use `MemoryMappedFileCompressed` that acts as a facade for LZMA SDK's user-hostile headers. LZMA SDK is unfortunately Windows-only, so this is only an option on Windows. It has a common parent class with `MemoryMappedFileUncompressed`, so it is possible to implement other ways to store the data.

## Contributing

Feel free to fork this project and fill a merge request if you want to share any improvements you've made. A platform-independent way to archive files is definitely needed.
