# Nodel
## Fast Dynamic Object with Optional Data Source
### Overview
The Nodel library consists of a dynamic object (the `Object` class), which can represent the data
types (as defined in nodel/support/types.h): 
- `nil`
- `bool`, `int64_t`, `uint64_t`, `double`
- `std::string`
- `std::vector<Object>`
- `std::map<Key, Object>`
- `tsl::ordered_map<Key, Object>`

A Nodel Object may be "bound" to a DataSource sub-class. The DataSource framework provides on-demand, 
lazy loading of data from ... somewhere else.

Nodel data-sources are selected and configured by calling the `nodel::bind` function with a URI that
identifies the scheme of a data-source implementation, with the remaining elements of the URI
interpreted by the data-source implementation.

The following code binds an Object to the current working directory:

```
auto wd = nodel::bind("file://");
```

The Nodel filesystem data-source (see include/nodel/filesystem/Directory.h) represents a directory
with a `tsl::ordered_map<Key, Object>`, where the keys are the file names, and the values are Objects 
bound to a `nodel::filesystem::File` data-source. A data-source load operation is triggered when a
bound object is accessed using one of the `nodel::Object` methods.

Nodel Objects are reference counted. A `nodel::Object` instance is a reference to its underying data.

Nodel is header-only library with *no required dependencies*. This means that your program must
instantiate certain macros and call the `configure` function of the subsystems it uses.  See the
"Quick Start" below for a complete example.

If you link with cpptrace, Nodel exceptions will contain backtraces.

Nodel provides its own *homegrown* JSON and CSV parsers, both of which are stream-based.  These
parsers are simple and eliminate external dependencies.

Nodel includes two example DataSource implementations - one for the filesystem, and one for RocksDB.
The filesystem DataSource collection is extensible and a the RocksDB DataSource additionally provides
integration with the filesystem DataSource.  In this way, you can navigate through data stored in
json files, csv files, text files, binary files, as well as RocksDB databases, from a single tree-like
data-structure.  You can create new directories, files, and databases using the semantics of
semi-structured data.

Nodel interns string keys in a thread-local std::unordered_map.

Nodel provides key and value iterators that transparently use DataSource-specific iterators.  For
example, if you are iterating the keys of an Object backed by a [RocksDB](https://rocksdb.org/) database,
the iteration is actually performed by the native RocksDB Iterator.

Nodel includes a Python 3.x C-API extension.

"Nodel" is a portmanteau of "node" and "model", where "node" refers to a node in a tree. The tree
is similar to other semi-structured data representations: TOML, JSON, XML-DOM, etc....

## Quick Start

```
#include <nodel/core.h>
#include <nodel/filesystem.h>
#include <iostream>

NODEL_CORE_INIT;
NODEL_FILESYSTEM_INIT;

using namespace nodel;

int main(int argc, char** argv) {
    filesystem::configure();                // Register the "file:" URI scheme
    Object dir = bind("file://");           // Bind an object to the current working directory
    for (const auto& f: dir.iter_keys())    // Iterate the names of the files
        std::cout << f << std::endl;
}
```

## C++ Examples
### Find Filenames Matching Regular Expression



Here are a few things you can do with Nodel Objects:
- 
- A Nodel Object can be *bound* to a file-system directory


### Similar Projects
See Niels Lohmann project, [json](https://github.com/nlohmann/json).
