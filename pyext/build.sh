#!/bin/bash

CWD=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
pushd $CWD

NODEL_DIR=`realpath ..`
NODEL_DEPENDS_DIR=$NODEL_DIR/depends

# CHANGE THESE PATHS FOR YOUR ENV
export NODEL_INCLUDE=$NODEL_DIR/include
export NODEL_OMAP_INCLUDE=$NODEL_DEPENDS_DIR/ordered-map/include
export NODEL_FMT_INCLUDE=$NODEL_DEPENDS_DIR/fmt/include
export NODEL_CPPTRACE_INCLUDE=$NODEL_DEPENDS_DIR/cppstrace/include

export NODEL_FMT_LIB=$NODEL_DEPENDS_DIR/fmt/build
export NODEL_CPPTRACE_LIB=$NODEL_DEPENDS_DIR/cpptrace/build
export NODEL_DWARF_LIB=$NODEL_DEPENDS_DIR/libdwarf-code/build/src/lib/libdwarf

export PYNODEL_ROCKSDB=1
export NODEL_ROCKSDB_INCLUDE=$NODEL_DEPENDS_DIR/rocksdb/include
export NODEL_ROCKSDB_LIB=$NODEL_DEPENDS_DIR/rocksdb/build/librocksdb.8.11.4.dylib

python -m build && (pip install --upgrade --force-reinstall dist/nodel-0.1.0-cp312-cp312-macosx_14_0_x86_64.whl; ipython -c "import nodel" -i)

popd