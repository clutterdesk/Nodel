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

A Nodel Object may be "bound" to a class derived from `nodel::DataSource`, which loads data on-demand
from an external storage location into memory.  

Nodel data sources are selected and configured by calling the `nodel::bind` function with a URI that
identifies the scheme of a data-source implementation, with the remaining elements of the URI
interpreted by the data-source implementation.  URI schemes are extensible.

The following code binds an Object to the current working directory:

```
auto wd = nodel::bind("file://?path=.");
```

The Nodel filesystem data-source (see include/nodel/filesystem/Directory.h) represents a directory
with a `tsl::ordered_map<Key, Object>`, where the keys are the file names, and the values are Objects 
bound to a `nodel::filesystem::File` data-source. A data-source load operation is triggered when a
bound object is accessed using one of the `nodel::Object` methods.

Nodel Objects are reference counted. A `nodel::Object` instance is a reference to its underying data.

If you link with cpptrace, Nodel exceptions will contain backtraces.

Nodel provides its own homegrown JSON and CSV parsers, both of which are stream-based and reduce
external dependencies.

Nodel includes two example DataSource implementations - one for the filesystem, and one for RocksDB.
The filesystem DataSource collection is extensible and a the RocksDB DataSource additionally provides
integration with the filesystem DataSource.  In this way, you can navigate through data stored in
json files, csv files, text files, binary files, as well as RocksDB databases, with a unified
data model.

Nodel interns string keys in a thread-local hash map.

Nodel provides key and value iterators that transparently use DataSource-specific iterators.  For
example, if you are iterating the keys of an Object backed by a [RocksDB](https://rocksdb.org/) database,
the iteration is actually performed by the native RocksDB Iterator.

Nodel includes a Python 3.x C-API extension, which supports URI binding to take advantage of whatever
URI schemes have been registered in the Nodel build.

"Nodel" is a portmanteau of "node" and "model", where "node" refers to a node in a tree.


## How To Build
The CMakeLists.txt has limited testing, so if you have trouble building with it, the core features can be 
built without external dependencies using the following compiler flags:

```
--std=c++20 -I<NODEL_DIRECTORY>/include -Wall -O2
```

If you compile and link with the excellent [cpptrace](https://github.com/jeremy-rifkin/cpptrace) library, 
all Nodel exceptions will have backtraces.

To build with [RocksDB](https://rocksdb.org/) support, just add the compiler and linker flags required by RocksDB.

To build and install the Python C-API, run the following commands:

```
cd <NODEL_DIRECTORY>/pyext
python3 -m build
pip install dist/nodel*.whl
```

## Quick Start

```
#include <nodel/core.h>
#include <nodel/filesystem.h>
#include <iostream>

NODEL_CORE_INIT;       // initialize thread-local string intern maps
NODEL_FILESYSTEM_INIT; // initialize thread-local extension registry (the default registry)

using namespace nodel;

int main(int argc, char** argv) {
    filesystem::configure();                // Register the "file:" URI scheme
    Object dir = bind("file://?path=.");    // Bind an object to a relative path
    for (const auto& f: dir.iter_keys())    // Iterate the names of the files
        std::cout << f << std::endl;
}
```

## Python Quick Start
```
import nodel as nd
wd = nd.bind("file://?path=.")
print(list(wd))
```

## API Documentation and Examples
[Nodel API documentation](https://clutterdesk.github.io/Nodel)

### Similar Projects
See Niels Lohmann project, [json](https://github.com/nlohmann/json).
