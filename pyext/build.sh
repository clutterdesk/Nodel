#!/bin/bash

CWD=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
pushd $CWD

NODEL_DIR=`realpath ..`
NODEL_DEPENDS_DIR=$NODEL_DIR/depends

export NODEL_WFLAGS="-Wno-tautological-undefined-compare -Wno-deprecated-declarations"
export NODEL_PYEXT_INCLUDE="/Users/bdunnagan/git/Nodel /Users/bdunnagan/.conan2/p/tsl-o32e7a3bc45605/p/include /Users/bdunnagan/.conan2/p/b/fmteb9ad2f6cc0f0/p/include /Users/bdunnagan/.conan2/p/b/cpptr44e4e1c9e6c43/p/include /Users/bdunnagan/.conan2/p/b/fmteb9ad2f6cc0f0/p/include"
export NODEL_PYEXT_LIB_PATH="/Users/bdunnagan/.conan2/p/b/cpptr44e4e1c9e6c43/p/lib;/Users/bdunnagan/.conan2/p/b/libdw83da55fdeb655/p/lib;/Users/bdunnagan/.conan2/p/b/zstd70d105971e2fb/p/lib;/Users/bdunnagan/.conan2/p/b/fmteb9ad2f6cc0f0/p/lib;/Users/bdunnagan/.conan2/p/b/zlib3251832c00845/p/lib;/Users/bdunnagan/.conan2/p/b/gtestcbb9d1b936575/p/lib"

python -m build && (pip install --upgrade --force-reinstall dist/nodel-0.1.0-cp312-cp312-macosx_14_0_x86_64.whl; ipython -c "import nodel" -i)

popd