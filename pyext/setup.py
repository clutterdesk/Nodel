from setuptools import Extension, setup
import os

nodel_pyext_include = os.environ.get('NODEL_PYEXT_INCLUDE')
nodel_pyext_lib = os.environ.get('NODEL_PYEXT_LIB')

print('~' * 100)
print(nodel_pyext_include);
print('~' * 100)
print(nodel_pyext_lib);
print('~' * 100)

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
                '-Wno-tautological-undefined-compare'
                '-Wdeprecated-declarations'

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