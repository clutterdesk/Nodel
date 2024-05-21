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
find_package(speedb)
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
file(GLOB NODEL_SRC nodel/**/*.hxx nodel/**/*.cxx)
add_library(nodel_lib STATIC ${NODEL_SRC})

target_compile_features(nodel_lib PRIVATE cxx_std_20)
target_link_libraries(nodel_lib PRIVATE cpptrace dwarf fmt speeddb tsl-ordered-map zlib)

#----------------------------------------------------------------------------
#-- Nodel Unit Test
#----------------------------------------------------------------------------
file(GLOB NODEL_TEST_SRC test/*.cxx nodel/**/*.hxx nodel/**/*.cxx)
add_executable(nodel_test ${NODEL_TEST_SRC})

add_dependencies(nodel_test nodel_lib)

target_compile_features(nodel_test PRIVATE cxx_std_20)

target_include_directories(nodel_test PRIVATE
    ${Nodel_SOURCE_DIR}/include
    ${cpptrace_INCLUDE_DIRS}
    ${libdwarf_INCLUDE_DIRS}
    ${fmt_INCLUDE_DIRS}
    ${speedb_INCLUDE_DIRS}
    ${tsl-ordered-map_INCLUDE_DIRS}
    ${zlib_INCLUDE_DIRS}
    ${gtest_INCLUDE_DIRS}    
)

target_link_libraries(nodel_test PRIVATE
    ${cpptrace_LIBRARIES} 
    ${libdwarf_LIBRARIES} 
    ${fmt_LIBRARIES} 
    ${speedb_LIBRARIES} 
    ${tsl-ordered-map_LIBRARIES} 
    ${zlib_LIBRARIES} 
    ${gtest_LIBRARIES} 
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

add_custom_target(
    pyext ALL
    DEPENDS nodel_lib
            ${CMAKE_SOURCE_DIR}/pyext/setup.py
            ${CMAKE_SOURCE_DIR}/pyext/setup.cfg
    COMMAND ${CMAKE_COMMAND} -E env 
        NODEL_INCLUDE="${Nodel_SOURCE_DIR}/include"
        NODEL_OMAP_INCLUDE="${tsl-ordered-map_INCLUDE_DIRS}"
        NODEL_FMT_INCLUDE="${fmt_INCLUDE_DIRS}"
        NODEL_CPPTRACE_INCLUDE="${cpptrace_INCLUDE_DIRS}"
        NODEL_ROCKSDB_INCLUDE="${speedb_INCLUDE_DIRS}"
        NODEL_FMT_LIB="${fmt_INCLUDE_DIRS}" 
        NODEL_CPPTRACE_LIB="${cpptrace_BINARY_DIR}" 
        NODEL_DWARF_LIB="${dwarf}" 
        NODEL_ROCKSDB_LIB="${rocksdb}"
        PYNODEL_ROCKSDB=1
        ${Python3_EXECUTABLE} -m build
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/pyext/
    COMMENT "Building Python C-API extension"
)

endif()

