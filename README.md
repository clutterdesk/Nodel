# Nodel
## A high-performance data-source parameterized dynamic object in C++ with Python C-API extension
### Overview
"Nodel" is a portmanteau of "node" and "model", where "node" refers to a node in a tree data-structure.

Nodel consists of an Object class and a DataSource base class, which can be sub-classed to provide 
on-demand/lazy loading.  The Nodel Object class is dynamic type implemented in C++ as a union. Due 
to the stringent performance requirements entailed, std::variant is avoided. Each dynamic method is 
implemented as a switch statement.  

Nodel Objects are reference counted, so you don't have to worry about deallocating the objects in
a tree.

Nodel is header-only library with *no required dependencies*. However, if you link with cpptrace,
any Nodel exceptions will contain backtraces.

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

Here are a few things you can do with Nodel Objects:
- 
- A Nodel Object can be *bound* to a file-system directory


### Similar Projects
See Niels Lohmann project, [json](https://github.com/nlohmann/json).
