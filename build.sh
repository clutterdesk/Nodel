#!/bin/sh

CWD=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
JOBS=20

mkdir depends
cd depends

git clone --branch v1.1.0 https://github.com/Tessil/ordered-map.git || exit
git clone --branch 10.2.1 https://github.com/fmtlib/fmt.git || exit
git clone --branch v8.11.4 https://github.com/facebook/rocksdb.git || exit
git clone --branch v0.9.2 https://github.com/davea42/libdwarf-code.git || exit
git clone --branch v0.5.2 https://github.com/jeremy-rifkin/cpptrace.git || exit

# build fmt
cd $CWD/depends/fmt
cmake -B build || exit
cmake --build build --parallel $JOBS || exit

# build rocksdb
cd $CWD/depends/rocksdb
cmake -B build || exit
cmake --build build --parallel $JOBS || exit

// build dwarf
cd $CWD/depends/libdwarf-code
mkdir build
cd build
cmake -G Ninja -DDO_TESTING:BOOL=TRUE $CWD/depends/libdwarf-code || exit
ninja || exit
ninja test || exit

# build cpptrace
cd $CWD/depends/cpptrace
cmake -B build || exit
cmake --build build --parallel $JOBS || exit

cd $CWD/pyext
./build.sh
