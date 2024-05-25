cmake_minimum_required(VERSION 3.12.0)

project(Nodel
    VERSION 0.1.0
    DESCRIPTION "Fast Dynamic Object with Optional Data Source"
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
# Dependency: speedb (LOCAL, statically linked)
#----------------------------------------------------------------------------
message(STATUS "Building Speedb ------------------------------------------------------------")
execute_process(COMMAND ${CMAKE_COMMAND} -B build -G Ninja 
                WORKING_DIRECTORY deps/speedb)
execute_process(COMMAND ${CMAKE_COMMAND} --build build --target speedb 
                WORKING_DIRECTORY deps/speedb)
message(STATUS "----------------------------------------------------------------------------")


#----------------------------------------------------------------------------
# Dependency: ZipLib (LOCAL, statically linked)
#----------------------------------------------------------------------------
set(CMAKE_PREFIX_PATH deps)
find_program(MAKE make)
execute_process(COMMAND ${MAKE} WORKING_DIRECTORY deps/ziplib)


#----------------------------------------------------------------------------
#-- Dump All Vars
#----------------------------------------------------------------------------
get_cmake_property(_variableNames VARIABLES)
list (SORT _variableNames)
foreach (_variableName ${_variableNames})
    message(STATUS "${_variableName}=${${_variableName}}")
endforeach()


#----------------------------------------------------------------------------
# DISABLED COMPILER WARNINGS for ZipLib
#----------------------------------------------------------------------------
set(NODEL_WFLAGS 
    -Wno-tautological-undefined-compare # ziplib
    -Wno-deprecated-declarations # ziplib
    -Wno-enum-conversion  # ziplib
)


#----------------------------------------------------------------------------
#-- Nodel Unit Test
#----------------------------------------------------------------------------
file(GLOB NODEL_TEST_SRC 
    deps/ziplib/Source/ZipLib/extlibs/zlib/*.c
    deps/ziplib/Source/ZipLib/extlibs/lzma/unix/*.c
    deps/ziplib/Source/ZipLib/extlibs/bzip2/*.h
    deps/ziplib/Source/ZipLib/extlibs/bzip2/*.c
    deps/ziplib/Source/ZipLib/*.h
    deps/ziplib/Source/ZipLib/*.cpp
    deps/ziplib/Source/ZipLib/detail/h.cpp
    deps/ziplib/Source/ZipLib/detail/*.cpp
    nodel/**/*.hxx 
    nodel/**/*.cxx 
    test/*.cxx
)

add_executable(nodel_test ${NODEL_TEST_SRC})
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
    cpptrace::cpptrace
    fmt::fmt
    gtest
    ${Nodel_SOURCE_DIR}/deps/speedb/build/libspeedb.a
)

add_custom_command(
    TARGET nodel_test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/test_data/ $<TARGET_FILE_DIR:nodel_test>/test_data/
)
                       
                       
if (0)                       
#----------------------------------------------------------------------------
#-- Python C-API Extension
#----------------------------------------------------------------------------
find_package (Python3 COMPONENTS Interpreter Development)

if (NOT Python3_FOUND)
message(WARNING "Python3 not found. Unable to build Python C-API extension.")
else()

set(NODEL_PYEXT_INCLUDE
    ${Nodel_SOURCE_DIR}
    ${cpptrace_INCLUDE_DIRS}
    ${libdwarf_INCLUDE_DIRS}
    ${fmt_INCLUDE_DIRS}
    ${Nodel_SOURCE_DIR}/deps/speedb/include
    ${tsl-ordered-map_INCLUDE_DIRS}
    ${Nodel_SOURCE_DIR}/deps/ziplib/Source
    ${zlib_INCLUDE_DIRS}
)

set(NODEL_PYEXT_LIB_PATH
    ${cpptrace_LIB_DIRS_RELEASE}
    ${libdwarf_LIB_DIRS_RELEASE}    
    ${fmt_LIB_DIRS_RELEASE}
    ${Nodel_SOURCE_DIR}/deps/speedb/build
    ${zlib_LIB_DIRS_RELEASE}
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
endif()

