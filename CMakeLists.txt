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
# Dependency: lz4
#----------------------------------------------------------------------------
FetchContent_Declare(
  lz4
  GIT_REPOSITORY https://github.com/lz4/lz4.git
  GIT_TAG v1.9.4        
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

FetchContent_MakeAvailable(cpptrace fmtlib tsl-ordered-map lz4 googletest)


#----------------------------------------------------------------------------
# DISABLED COMPILER WARNINGS for ZipLib
#----------------------------------------------------------------------------
set(NODEL_WFLAGS 
    -Wno-tautological-undefined-compare # ziplib
    -Wno-deprecated-declarations # ziplib
    -Wno-enum-conversion  # ziplib
)


#----------------------------------------------------------------------------
# Dependency: speedb (LOCAL, statically linked)
#----------------------------------------------------------------------------
add_custom_target(nodel_speedb
                  COMMAND ${MAKE} static_lib -j ${CMAKE_BUILD_PARALLEL_LEVEL}
                  COMMENT "Build Speedb static library"
                  WORKING_DIRECTORY ${Nodel_SOURCE_DIR}/deps/speedb)


#----------------------------------------------------------------------------
# Dependency: ZipLib (LOCAL, statically linked)
#----------------------------------------------------------------------------
add_custom_target(nodel_ziplib
                  COMMAND ${MAKE} Bin/libziplib.a -j ${CMAKE_BUILD_PARALLEL_LEVEL}
                  COMMENT "Build ZipLib static library"
                  WORKING_DIRECTORY ${Nodel_SOURCE_DIR}/deps/ziplib)


#----------------------------------------------------------------------------
#-- Nodel Unit Test
#----------------------------------------------------------------------------
file(GLOB NODEL_TEST_SRC 
    nodel/**/*.hxx 
    nodel/**/*.cxx 
    test/*.cxx
)

add_executable(nodel_test ${NODEL_TEST_SRC})
add_dependencies(nodel_test nodel_speedb nodel_ziplib)
target_compile_features(nodel_test PRIVATE cxx_std_20)
target_compile_options(nodel_test PRIVATE ${NODEL_WFLAGS})
target_compile_options(nodel_test PRIVATE -pthread)

target_include_directories(nodel_test PRIVATE
    ${Nodel_SOURCE_DIR}
    ${Nodel_SOURCE_DIR}/deps/speedb/include
    ${tsl-ordered-map_SOURCE_DIR}/include
    ${Nodel_SOURCE_DIR}/deps/ziplib/Source
    ${gtest_SOURCE_DIR}/include
)

target_link_libraries(nodel_test PRIVATE
    ${Nodel_SOURCE_DIR}/deps/ziplib/Bin/libziplib.a
    ${Nodel_SOURCE_DIR}/deps/speedb/libspeedb.a
    cpptrace::cpptrace
    fmt::fmt
    gtest
)

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
        ${Nodel_SOURCE_DIR}/deps/speedb/include
    )
    
    set(NODEL_PYEXT_LIB_PATH
        ${Nodel_SOURCE_DIR}/build/_deps/cpptrace-build
        ${Nodel_SOURCE_DIR}/build/_deps/fmtlib-build
        ${Nodel_SOURCE_DIR}/deps/ziplib/Bin
        ${Nodel_SOURCE_DIR}/deps/speedb/build
        ${libdwarf_BINARY_DIR}/src/lib/libdwarf
        ${zstd_BINARY_DIR}/lib
    )
    
    add_custom_target(
        pyext ALL
        DEPENDS nodel_test
                ${CMAKE_SOURCE_DIR}/pyext/setup.py
                ${CMAKE_SOURCE_DIR}/pyext/setup.cfg
        COMMAND ${CMAKE_COMMAND} -E env
            NODEL_WFLAGS="${NODEL_WFLAGS}" 
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
