cmake_minimum_required(VERSION 3.12.0)

project(Nodel
    VERSION 0.1.0
    DESCRIPTION "Fast Dynamic Object with Optional Data Source"
    LANGUAGES C CXX)

enable_testing()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_PREFIX_PATH deps)
set(CMAKE_EXPORT_COMPILE_COMMANDS OFF)

# WORKAROUND for tsl-ordered-map
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

option(NODEL_DUMP_VARS "Print all CMake variables at configure time" OFF)

include(FetchContent)

#----------------------------------------------------------------------------
# Dependency: cpptrace
#----------------------------------------------------------------------------
if(NOT WIN32)
    set(CPPTRACE_GET_SYMBOLS_WITH_LIBDWARF ON)
else()
    set(CPPTRACE_GET_SYMBOLS_WITH_LIBDWARF OFF)
endif()
set(CPPTRACE_BUILD_SHARED OFF)

FetchContent_Declare(
  cpptrace
  GIT_REPOSITORY https://github.com/jeremy-rifkin/cpptrace.git
  GIT_TAG        v1.0.4
)

#----------------------------------------------------------------------------
# Dependency: libdwarf (only for non-Windows)
#----------------------------------------------------------------------------
if(NOT WIN32)
    FetchContent_Declare(
      libdwarf
      GIT_REPOSITORY https://github.com/davea42/libdwarf-code.git
      GIT_TAG        v2.3.0
    )
endif()

#----------------------------------------------------------------------------
# Dependency: fmtlib
#----------------------------------------------------------------------------
FetchContent_Declare(
  fmtlib
  GIT_REPOSITORY https://github.com/fmtlib/fmt.git
  GIT_TAG        12.1.0
)

#----------------------------------------------------------------------------
# Dependency: Tesseil/ordered-map
#----------------------------------------------------------------------------
FetchContent_Declare(
  tsl-ordered-map
  GIT_REPOSITORY https://github.com/Tessil/ordered-map.git
  GIT_TAG        v1.1.0
)

#----------------------------------------------------------------------------
# Dependency: googletest
#----------------------------------------------------------------------------
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.17.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

if(NOT WIN32)
    FetchContent_MakeAvailable(cpptrace fmtlib tsl-ordered-map googletest libdwarf)
else()
    FetchContent_MakeAvailable(cpptrace fmtlib tsl-ordered-map googletest)
endif()

# googletest builds with warnings-as-errors under MSVC, and clang >= 21 warns about
# char8_t -> char32_t conversions in gtest-printers.h
if (MSVC AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 21)
    target_compile_options(gtest PRIVATE -Wno-character-conversion)
endif()

#----------------------------------------------------------------------------
# Dependency: ZipLib (LOCAL, statically linked)
#----------------------------------------------------------------------------
set(ZIPLIB_DIR ${Nodel_SOURCE_DIR}/deps/ziplib/Source/ZipLib)

file(GLOB ZIPLIB_SRC
    ${ZIPLIB_DIR}/*.cpp
    ${ZIPLIB_DIR}/detail/*.cpp
    ${ZIPLIB_DIR}/extlibs/zlib/*.c
    ${ZIPLIB_DIR}/extlibs/bzip2/*.c
)

if (WIN32)
    # Same lzma sources as extlibs/lzma/lzma.vcxproj (Win32 threads)
    foreach(src 7zAlloc 7zBuf 7zBuf2 7zCrc 7zCrcOpt 7zDec 7zFile 7zIn 7zStream Alloc Bcj2 Bra Bra86
                BraIA64 CpuArch Delta LzFind LzFindMt Lzma2Dec Lzma2Enc Lzma86Dec Lzma86Enc LzmaDec
                LzmaEnc LzmaLib MtCoder Ppmd7 Ppmd7Dec Ppmd7Enc Sha256 Threads Xz XzCrc64 XzDec XzEnc XzIn)
        list(APPEND ZIPLIB_SRC ${ZIPLIB_DIR}/extlibs/lzma/${src}.c)
    endforeach()
else()
    file(GLOB ZIPLIB_SRC_LZMA ${ZIPLIB_DIR}/extlibs/lzma/unix/*.c)
    list(APPEND ZIPLIB_SRC ${ZIPLIB_SRC_LZMA})
endif()

add_library(nodel_ziplib STATIC ${ZIPLIB_SRC})
set_target_properties(nodel_ziplib PROPERTIES
    OUTPUT_NAME ziplib
    CXX_STANDARD 11
    POSITION_INDEPENDENT_CODE ON)
target_include_directories(nodel_ziplib PUBLIC ${Nodel_SOURCE_DIR}/deps/ziplib/Source)
if (MSVC)
    target_compile_definitions(nodel_ziplib PRIVATE _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_DEPRECATE)
    target_compile_options(nodel_ziplib PRIVATE /w)
    # CpuArch.c relies on MSVC's built-in __cpuid; clang-cl needs the declaration
    set_source_files_properties(${ZIPLIB_DIR}/extlibs/lzma/CpuArch.c PROPERTIES COMPILE_OPTIONS "/FIintrin.h")
else()
    target_compile_options(nodel_ziplib PRIVATE $<$<COMPILE_LANGUAGE:C>:-Wno-enum-conversion>)
endif()

#----------------------------------------------------------------------------
# Optional Dependency: rocksdb (pre-installed)
#----------------------------------------------------------------------------
if (NOT CMAKE_CROSSCOMPILING)
    find_package(rocksdb CONFIG)
endif()
if (rocksdb_FOUND)
    get_target_property(ROCKSDB_INCLUDE RocksDB::rocksdb INTERFACE_INCLUDE_DIRECTORIES)
    get_target_property(ROCKSDB_LIBRARY RocksDB::rocksdb LOCATION)

    # Prebuilt RocksDB packages (e.g. Homebrew) can export absolute paths into the SDK they were
    # built with, such as .../MacOSX14.sdk/usr/lib/libbz2.tbd. Link missing ones by name instead,
    # so the linker resolves them from the current SDK.
    get_target_property(ROCKSDB_LINK_LIBS RocksDB::rocksdb INTERFACE_LINK_LIBRARIES)
    if (ROCKSDB_LINK_LIBS)
        set(ROCKSDB_FIXED_LINK_LIBS)
        foreach(lib IN LISTS ROCKSDB_LINK_LIBS)
            if (IS_ABSOLUTE "${lib}" AND NOT EXISTS "${lib}")
                get_filename_component(lib_name "${lib}" NAME_WE)
                string(REGEX REPLACE "^lib" "" lib_name "${lib_name}")
                message(STATUS "RocksDB: ${lib} does not exist, linking '${lib_name}' instead")
                set(lib "${lib_name}")
            endif()
            list(APPEND ROCKSDB_FIXED_LINK_LIBS "${lib}")
        endforeach()
        set_target_properties(RocksDB::rocksdb PROPERTIES INTERFACE_LINK_LIBRARIES "${ROCKSDB_FIXED_LINK_LIBS}")
    endif()
endif()


#----------------------------------------------------------------------------
#-- Nodel Unit Test
#----------------------------------------------------------------------------
file(GLOB NODEL_TEST_SRC
    nodel/**/*.hxx
    nodel/**/*.cxx
    test/main.cxx
    test/test_csv.cxx
    test/test_filesystem.cxx
    test/test_intern.cxx
    test/test_json.cxx
    test/test_key.cxx
    test/test_lcs.cxx
    test/test_object.cxx
    test/test_query.cxx
    test/test_ref.cxx
    test/test_uri.cxx
    test/test_zip.cxx
)

if (rocksdb_FOUND)
    file(GLOB NODEL_TEST_SRC_ROCKSDB test/test_kvdb.cxx)
    set(NODEL_TEST_SRC ${NODEL_TEST_SRC} ${NODEL_TEST_SRC_ROCKSDB})
endif()

find_package(Threads REQUIRED)

add_executable(nodel_test ${NODEL_TEST_SRC})
target_compile_features(nodel_test PRIVATE cxx_std_20)
if (NOT MSVC)
    target_compile_options(nodel_test PRIVATE ${NODEL_WFLAGS})
endif()

if (rocksdb_FOUND)
    target_include_directories(nodel_test PRIVATE ${ROCKSDB_INCLUDE})
    target_link_libraries(nodel_test PRIVATE RocksDB::rocksdb)
endif()

target_include_directories(nodel_test PRIVATE
    ${Nodel_SOURCE_DIR}
    ${tsl-ordered-map_SOURCE_DIR}/include
    ${gtest_SOURCE_DIR}/include
)

target_link_libraries(nodel_test PRIVATE
    nodel_ziplib
    cpptrace::cpptrace
    fmt::fmt
    gtest
    Threads::Threads
)

#----------------------------------------------------------------------------
# Copy test_data/ directory
#----------------------------------------------------------------------------
add_custom_command(
    TARGET nodel_test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/test_data/ $<TARGET_FILE_DIR:nodel_test>/test_data/
)

# ObjectBug.* holds the regression tests for open issues, which fail until each is fixed
add_test(
    NAME nodel_test
    COMMAND nodel_test --gtest_filter=-ObjectBug.*
    WORKING_DIRECTORY $<TARGET_FILE_DIR:nodel_test>
)


#----------------------------------------------------------------------------
#-- Dump All Vars
#----------------------------------------------------------------------------
if (NODEL_DUMP_VARS)
    get_cmake_property(_variableNames VARIABLES)
    list (SORT _variableNames)
    foreach (_variableName ${_variableNames})
        message(STATUS "${_variableName}=${${_variableName}}")
    endforeach()
endif()

#----------------------------------------------------------------------------
#-- Python C-API Extension
#----------------------------------------------------------------------------
if (NOT CMAKE_CROSSCOMPILING)
    find_package (Python3 COMPONENTS Interpreter Development)
endif()

if (CMAKE_CROSSCOMPILING)
    message(STATUS "Cross-compiling: skipping Python C-API extension.")

elseif (NOT Python3_FOUND)
    message(WARNING "Python3 not found. Unable to build Python C-API extension.")

else()

    set(NODEL_PYEXT_INCLUDE
        ${Nodel_SOURCE_DIR}
        ${Nodel_SOURCE_DIR}/deps/ziplib/Source
        ${cpptrace_SOURCE_DIR}/include
        ${fmtlib_SOURCE_DIR}/include
        ${tsl-ordered-map_SOURCE_DIR}/include
    )

    set(NODEL_PYEXT_LIB_PATH
        ${Nodel_SOURCE_DIR}/build/_deps/cpptrace-build
        ${Nodel_SOURCE_DIR}/build/_deps/fmtlib-build
        $<TARGET_FILE_DIR:nodel_ziplib>
    )

    if(NOT WIN32)
        list(APPEND NODEL_PYEXT_LIB_PATH
            ${libdwarf_BINARY_DIR}/src/lib/libdwarf
            ${zstd_BINARY_DIR}/lib
        )
    endif()

    if (rocksdb_FOUND)
        set(NODEL_PYEXT_INCLUDE ${NODEL_PYEXT_INCLUDE} ${ROCKSDB_INCLUDE})
        get_filename_component(ROCKSDB_LIB_PATH ${ROCKSDB_LIBRARY} DIRECTORY)
        set(NODEL_PYEXT_LIB_PATH ${NODEL_PYEXT_LIB_PATH} ${ROCKSDB_LIB_PATH})
    endif()

    if (WIN32)
        set(NODEL_VENV_PYTHON ${CMAKE_BINARY_DIR}/venv/Scripts/python.exe)
    else()
        set(NODEL_VENV_PYTHON ${CMAKE_BINARY_DIR}/venv/bin/python)
    endif()

    add_custom_target(
        pyext ALL
        DEPENDS nodel_test nodel_ziplib
                ${CMAKE_SOURCE_DIR}/pyext/setup.py
                ${CMAKE_SOURCE_DIR}/pyext/setup.cfg
        COMMAND ${Python3_EXECUTABLE} -m venv ${CMAKE_BINARY_DIR}/venv
        COMMAND ${NODEL_VENV_PYTHON} -m pip install setuptools build
        COMMAND ${CMAKE_COMMAND} -E env
            NODEL_PYEXT_INCLUDE="${NODEL_PYEXT_INCLUDE}"
            NODEL_PYEXT_LIB_PATH="${NODEL_PYEXT_LIB_PATH}"
            NODEL_PYEXT_ROCKSDB_FOUND="${rocksdb_FOUND}"
            ${NODEL_VENV_PYTHON} -m build
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/pyext/
        COMMENT "Building Python C-API extension"
    )

    # install the wheel built by the pyext target into the venv, then run each test module
    add_test(
        NAME pyext_install
        COMMAND ${NODEL_VENV_PYTHON} -m pip install --force-reinstall --no-deps
                --no-index --only-binary :all: --find-links ${CMAKE_SOURCE_DIR}/pyext/dist nodel
    )
    set_tests_properties(pyext_install PROPERTIES FIXTURES_SETUP pyext)

    file(GLOB NODEL_PYEXT_TESTS ${CMAKE_SOURCE_DIR}/pyext/test/test_*.py)
    foreach(test_file ${NODEL_PYEXT_TESTS})
        get_filename_component(test_module ${test_file} NAME_WE)
        add_test(
            NAME pyext_${test_module}
            COMMAND ${NODEL_VENV_PYTHON} -m unittest -v ${test_module}
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/pyext/test
        )
        set_tests_properties(pyext_${test_module} PROPERTIES FIXTURES_REQUIRED pyext)
    endforeach()
endif()


#----------------------------------------------------------------------------
#-- Documentation with Doxygen
#----------------------------------------------------------------------------
find_package(Doxygen)
if(NOT DOXYGEN_FOUND)
    message(STATUS "Doxygen not found: 'docs' target unavailable.")
else()
    set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in)
    set(DOXYGEN_OUT ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile)
    configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)

    add_custom_target(docs
        COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Build Documentation"
        VERBATIM)
endif()
