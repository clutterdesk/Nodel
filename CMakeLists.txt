cmake_minimum_required(VERSION 3.12.0)

project(Nodel
    VERSION 0.1.0
    DESCRIPTION "Fast Dynamic Object with Optional Data Source"
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_PREFIX_PATH deps)

find_program(MAKE make)
include(FetchContent)

#----------------------------------------------------------------------------
# Dependency: cpptrace
#----------------------------------------------------------------------------
set(CPPTRACE_GET_SYMBOLS_WITH_LIBDWARF ON)
set(CPPTRACE_BUILD_SHARED OFF)

FetchContent_Declare(
  cpptrace
  GIT_REPOSITORY https://github.com/jeremy-rifkin/cpptrace.git
  GIT_TAG        v0.5.4
)

#----------------------------------------------------------------------------
# Dependency: fmtlib
#----------------------------------------------------------------------------
FetchContent_Declare(
  fmtlib
  GIT_REPOSITORY https://github.com/fmtlib/fmt.git
  GIT_TAG        10.2.1
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
  GIT_TAG        release-1.8.1
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(cpptrace fmtlib tsl-ordered-map googletest)


#----------------------------------------------------------------------------
# DISABLED COMPILER WARNINGS for ZipLib
#----------------------------------------------------------------------------
set(NODEL_WFLAGS 
    -Wno-tautological-undefined-compare # ziplib
    -Wno-deprecated-declarations # ziplib
    -Wno-enum-conversion  # ziplib
)


#----------------------------------------------------------------------------
# Dependency: ZipLib (LOCAL, statically linked)
#----------------------------------------------------------------------------
add_custom_target(nodel_ziplib
                  COMMAND ${MAKE} Bin/libziplib.a -j ${CMAKE_BUILD_PARALLEL_LEVEL}
                  COMMENT "Build ZipLib static library"
                  WORKING_DIRECTORY ${Nodel_SOURCE_DIR}/deps/ziplib)


#----------------------------------------------------------------------------
# Optional Dependency: rocksdb (pre-installed)
#----------------------------------------------------------------------------
find_package(rocksdb 9.2 CONFIG)
if (rocksdb_FOUND)
    get_target_property(ROCKSDB_INCLUDE RocksDB::rocksdb INTERFACE_INCLUDE_DIRECTORIES)
    get_target_property(ROCKSDB_LIBRARY RocksDB::rocksdb LOCATION)
endif()


#----------------------------------------------------------------------------
#-- Nodel Unit Test
#----------------------------------------------------------------------------
file(GLOB NODEL_TEST_SRC 
    nodel/**/*.hxx 
    nodel/**/*.cxx 
    test/*.cxx
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
)

message(WARNING ${NODEL_TEST_SRC})

add_executable(nodel_test ${NODEL_TEST_SRC})
add_dependencies(nodel_test nodel_ziplib)
target_compile_features(nodel_test PRIVATE cxx_std_20)
target_compile_options(nodel_test PRIVATE ${NODEL_WFLAGS})
target_compile_options(nodel_test PRIVATE -pthread)

target_include_directories(nodel_test PRIVATE
    ${Nodel_SOURCE_DIR}
    ${tsl-ordered-map_SOURCE_DIR}/include
    ${Nodel_SOURCE_DIR}/deps/ziplib/Source
    ${gtest_SOURCE_DIR}/include
)

target_link_libraries(nodel_test PRIVATE
    ${Nodel_SOURCE_DIR}/deps/ziplib/Bin/libziplib.a
    cpptrace::cpptrace
    fmt::fmt
    gtest
)

if (rocksdb_FOUND)
    set(NODEL_TEST_SRC ${NODEL_TEST_SRC} test/test_kvdb.cxx)
    target_include_directories(nodel_test PRIVATE ${ROCKSDB_INCLUDE})
    target_link_libraries(nodel_test PRIVATE RocksDB::rocksdb)
endif()


#----------------------------------------------------------------------------
# Copy test_data/ directory
#----------------------------------------------------------------------------
add_custom_command(
    TARGET nodel_test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/test_data/ $<TARGET_FILE_DIR:nodel_test>/test_data/
)                 

                       
#----------------------------------------------------------------------------
#-- Python C-API Extension
#----------------------------------------------------------------------------
find_package (Python3 COMPONENTS Interpreter Development)

if (NOT Python3_FOUND)
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
        ${Nodel_SOURCE_DIR}/deps/ziplib/Bin
        ${libdwarf_BINARY_DIR}/src/lib/libdwarf
        ${zstd_BINARY_DIR}/lib
    )
    
    if (rocksdb_FOUND)
        set(NODEL_PYEXT_INCLUDE ${NODEL_PYEXT_INCLUDE} ${ROCKSDB_INCLUDE})
        get_filename_component(ROCKSDB_LIB_PATH ${ROCKSDB_LIBRARY} DIRECTORY)
        set(NODEL_PYEXT_LIB_PATH ${NODEL_PYEXT_LIB_PATH} ${ROCKSDB_LIB_PATH}) 
    endif()
    
    add_custom_target(
        pyext ALL
        DEPENDS nodel_test
                ${CMAKE_SOURCE_DIR}/pyext/setup.py
                ${CMAKE_SOURCE_DIR}/pyext/setup.cfg
        COMMAND ${CMAKE_COMMAND} -E env 
            NODEL_PYEXT_INCLUDE="${NODEL_PYEXT_INCLUDE}"
            NODEL_PYEXT_LIB_PATH="${NODEL_PYEXT_LIB_PATH}"
            ${Python3_EXECUTABLE} -m build
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/pyext/
        COMMENT "Building Python C-API extension"
    )
endif()


#----------------------------------------------------------------------------
#-- Dump All Vars
#----------------------------------------------------------------------------
if (0)
    get_cmake_property(_variableNames VARIABLES)
    list (SORT _variableNames)
    foreach (_variableName ${_variableNames})
        message(STATUS "${_variableName}=${${_variableName}}")
    endforeach()
endif()