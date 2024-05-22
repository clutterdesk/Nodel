cmake_minimum_required(VERSION 3.28.0)

project(Nodel
    VERSION 0.1.0
    DESCRIPTION "Fast Dynamic Object with Optional Data Source"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


#----------------------------------------------------------------------------
#-- Dependencies
#----------------------------------------------------------------------------
find_package(cpptrace)
find_package(dwarf)
find_package(fmt REQUIRED)
find_package(rocksdb)
find_package(tsl-ordered-map REQUIRED)
find_package(zlib REQUIRED)
find_package(gtest REQUIRED)

#----------------------------------------------------------------------------
#-- Dump All Vars
#----------------------------------------------------------------------------
get_cmake_property(_variableNames VARIABLES)
list (SORT _variableNames)
foreach (_variableName ${_variableNames})
    message(STATUS "${_variableName}=${${_variableName}}")
endforeach()


#----------------------------------------------------------------------------
#-- Nodel Library Target
#----------------------------------------------------------------------------
#file(GLOB NODEL_SRC nodel/**/*.hxx nodel/**/*.cxx)
#add_library(nodel_lib STATIC ${NODEL_SRC})

#target_compile_features(nodel_lib PRIVATE cxx_std_20)
#target_link_libraries(nodel_lib PRIVATE cpptrace dwarf fmt speeddb tsl-ordered-map zlib)

#----------------------------------------------------------------------------
#-- Nodel Unit Test
#----------------------------------------------------------------------------
file(GLOB NODEL_TEST_SRC test/*.cxx nodel/**/*.hxx nodel/**/*.cxx)
add_executable(nodel_test ${NODEL_TEST_SRC})

#add_dependencies(nodel_test nodel_lib)

target_compile_features(nodel_test PRIVATE cxx_std_20)

target_compile_options(nodel_test PRIVATE
    -Wno-tautological-undefined-compare
    -Wdeprecated-declarations
)

target_include_directories(nodel_test PRIVATE
    ${Nodel_SOURCE_DIR}
    ${Nodel_SOURCE_DIR}/deps/ziplib/Source
    ${cpptrace_INCLUDE_DIRS}
    ${libdwarf_INCLUDE_DIRS}
    ${fmt_INCLUDE_DIRS}
#    ${speedb_INCLUDE_DIRS_RELEASE}
    ${rocksdb_INCLUDE_DIRS_RELEASE}
    ${tsl-ordered-map_INCLUDE_DIRS}
    ${zlib_INCLUDE_DIRS}
    ${gtest_INCLUDE_DIRS_RELEASE}    
)

target_link_libraries(nodel_test PRIVATE
    ${cpptrace_LIBRARIES} 
    ${libdwarf_LIBRARIES} 
    ${fmt_LIBRARIES} 
#    ${speedb_LIBRARIES_RELEASE}
    ${rocksdb_LIBRARIES_RELEASE} 
    ${tsl-ordered-map_LIBRARIES} 
    ${zlib_LIBRARIES} 
    ${Nodel_SOURCE_DIR}/deps/ziplib/Bin/libziplib.dylib
    ${gtest_LIBRARIES_RELEASE} 
)

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
    ${Nodel_SOURCE_DIR/deps/ziplib/Source}
    ${tsl-ordered-map_INCLUDE_DIRS}
    ${fmt_INCLUDE_DIRS}
    ${cpptrace_INCLUDE_DIRS}
#    ${speedb_INCLUDE_DIRS_RELEASE}
    ${rocksdb_INCLUDE_DIRS_RELEASE}
    ${fmt_INCLUDE_DIRS}
    ${cpptrace_BINARY_DIR} 
    ${dwarf} 
    ${rocksdb}
)

set(NODEL_PYEXT_LIB
    ${cpptrace_LIBRARIES} 
    ${libdwarf_LIBRARIES} 
    ${fmt_LIBRARIES} 
#    ${speedb_LIBRARIES}
    ${rocksdb_LIBRARIES_RELEASE} 
    ${tsl-ordered-map_LIBRARIES} 
    ${zlib_LIBRARIES} 
    ${Nodel_SOURCE_DIR/deps/ziplib/Bin/libziplib.dylib}
)

add_custom_target(
    pyext ALL
    #DEPENDS nodel_lib
            ${CMAKE_SOURCE_DIR}/pyext/setup.py
            ${CMAKE_SOURCE_DIR}/pyext/setup.cfg
    COMMAND ${CMAKE_COMMAND} -E env 
        NODEL_PYEXT_INCLUDE="${NODEL_PYEXT_INCLUDE}"
        NODEL_PYEXT_LIB="${NODEL_PYEXT_LIB}"
        PYNODEL_ROCKSDB=1
        ${Python3_EXECUTABLE} -m build
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/pyext/
    COMMENT "Building Python C-API extension"
)

endif()

