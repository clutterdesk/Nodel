cmake_minimum_required(VERSION 3.28.0)

project(Nodel
    VERSION 0.1.0
    DESCRIPTION "Fast Dynamic Object with Optional Data Source"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


#----------------------------------------------------------------------------
#-- Fetch Dependencies
#----------------------------------------------------------------------------
include(FetchContent)

FetchContent_Declare(
  tsl-ordered-map
  GIT_REPOSITORY https://github.com/Tessil/ordered-map.git
  GIT_TAG        v1.1.0        
)

FetchContent_Declare(
  fmtlib
  GIT_REPOSITORY https://github.com/fmtlib/fmt.git
  GIT_TAG        10.2.1
)

FetchContent_Declare(
  cpptrace
  GIT_REPOSITORY https://github.com/jeremy-rifkin/cpptrace.git
  GIT_TAG        v0.5.2
)

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        release-1.8.1
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(tsl-ordered-map fmtlib cpptrace googletest)


#----------------------------------------------------------------------------
#-- Other Dependencies
#----------------------------------------------------------------------------
find_library(dwarf dwarf)
find_library(rocksdb rocksdb)
find_path(ROCKSDB_INCLUDE_DIR rocksdb/db.h)

#----------------------------------------------------------------------------
#-- Dump All Vars
#----------------------------------------------------------------------------
get_cmake_property(_variableNames VARIABLES)
list (SORT _variableNames)
foreach (_variableName ${_variableNames})
    message(STATUS "${_variableName}=${${_variableName}}")
endforeach()


#----------------------------------------------------------------------------
#-- Nodel Unit Test
#----------------------------------------------------------------------------
file(GLOB NODEL_TEST_SRC test/*.cpp include/**/*.h)
add_executable(nodel_test ${NODEL_TEST_SRC})

target_compile_features(nodel_test PRIVATE cxx_std_20)
target_include_directories(nodel_test PUBLIC 
    ${Nodel_SOURCE_DIR}/include
    ${tsl-ordered-map_SOURCE_DIR}/include
    ${ROCKSDB_INCLUDE_DIR}
    ${cpptrace_SOURCE_DIR}/include
    ${fmtlib_SOURCE_DIR}/include
    ${gtest_SOURCE_DIR}/include
)

target_link_directories(nodel_test PUBLIC 
    ${cpptrace_BINARY_DIR}
)
    
target_link_libraries(nodel_test PUBLIC ${rocksdb} fmt dwarf cpptrace::cpptrace gtest)

add_custom_command(
    TARGET nodel_test PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/test_data/ $<TARGET_FILE_DIR:nodel_test>/test_data/
)
                       