from setuptools import Extension, setup
import os

nodel_include = os.environ.get('NODEL_INCLUDE')
omap_include = os.environ.get('NODEL_OMAP_INCLUDE')
fmt_include = os.environ.get('NODEL_FMT_INCLUDE')
cpptrace_include = os.environ.get('NODEL_CPPTRACE_INCLUDE')
rocksdb_include = os.environ.get('NODEL_ROCKSDB_INCLUDE')

fmt_lib = os.environ.get('NODEL_FMT_LIB')
cpptrace_lib = os.environ.get('NODEL_CPPTRACE_LIB')
dwarf_lib = os.environ.get('NODEL_DWARF_LIB')
rocksdb_lib = os.environ.get('NODEL_ROCKSDB_LIB')

# nodel_root = os.path.realpath('..')
# print('Nodel root directory: {}', nodel_root)
#
# ordered_map_install = '/Users/bdunnagan/git/ordered-map/include'
# print('tsl/ordered_map directory: {}', ordered_map_install)
#
# fmt_install = '/Users/bdunnagan/git/fmt/include'
# print('fmt directory: {}', fmt_install)
#
# cpp_trace_install = '/Users/bdunnagan/git/cpptrace/include'
# print('cpp_trace directory: {}', cpp_trace_install)

setup(
    ext_modules=[
        Extension(
            name='nodel',
            define_macros=[
                ('PYNODEL_ROCKSDB', 1)
            ],
            sources=[
                'module.cpp', 
                'NodelObject.cpp', 
                'NodelKeyIter.cpp',
                'NodelValueIter.cpp',
                'NodelItemIter.cpp',
                'NodelTreeIter.cpp'
            ],
            include_dirs=[
                nodel_include,
                omap_include,
                rocksdb_include,
                fmt_include,
                cpptrace_include
            ],
            extra_compile_args=[
                '--std=c++20',
                '-Wno-c99-designator'
            ],
            library_dirs=[
                fmt_lib,
                dwarf_lib,
                cpptrace_lib
            ],
            libraries=[
                'fmt',
                'dwarf',
                'cpptrace'
            ],
            extra_link_args=[
                rocksdb_lib
            ]
        ),
    ]
)