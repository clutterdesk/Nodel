#!/bin/bash

# CHANGE THESE PATHS FOR YOUR ENV
export NODEL_INCLUDE=~/git/Nodel/include
export NODEL_OMAP_INCLUDE=~/git/ordered-map/include
export NODEL_FMT_INCLUDE=~/git/fmt/include
export NODEL_CPPTRACE_INCLUDE=~/git/cppstrace/include

export NODEL_FMT_LIB=~/git/fmt
export NODEL_CPPTRACE_LIB=~/git/cpptrace/build
export NODEL_DWARF_LIB=/usr/local/Cellar/dwarfutils/0.9.1/lib/libdwarf.dylib

# UNCOMMENT AND CHANGE PATHS FOR ROCKSDB SUPPORT
#export PYNODEL_ROCKSDB 1
#export NODEL_ROCKSDB_INCLUDE=/usr/local/Cellar/rocksdb/8.11.3/include
#export NODEL_ROCKSDB_LIB=/usr/local/Cellar/rocksdb/8.11.3/lib/librocksdb.dylib

python -m build && (pip install --upgrade --force-reinstall dist/nodel-0.1.0-cp312-cp312-macosx_14_0_x86_64.whl; ipython -c "import nodel" -i)
