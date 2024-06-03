from setuptools import Extension, setup
import os
import re
import glob
import sys

nodel_rocksdb = os.environ.get('NODEL_PYEXT_INCLUDE') is not None
nodel_pyext_include = re.split(r'\s+', os.environ.get('NODEL_PYEXT_INCLUDE'))
nodel_pyext_lib_path = re.split(r'\s+', os.environ.get('NODEL_PYEXT_LIB_PATH'))

cond_libs = [
    ('dwarf', 1),
    ('zstd', 1),
    ('cpptrace', 1),
    ('fmt', 1),
    ('ziplib', 1),
    ('rocksdb', nodel_rocksdb)
]

setup(
    ext_modules=[
        Extension(
            name='nodel',
            version='0.1',
            url='https://github.com/clutterdesk/Nodel',
            define_macros=[
                ('PYNODEL_ROCKSDB', 1 if nodel_rocksdb else 0)
            ],
            sources=[
                'module.cxx', 
                'NodelObject.cxx', 
                'NodelKeyIter.cxx',
                'NodelValueIter.cxx',
                'NodelItemIter.cxx',
                'NodelTreeIter.cxx'
            ],
            include_dirs=nodel_pyext_include,
            extra_compile_args=[
                '--std=c++20',
                '-Wno-c99-designator',
                '-Wno-delete-non-abstract-non-virtual-dtor',
                '-Wno-tautological-undefined-compare', 
                '-Wno-deprecated-declarations',
                '-Wno-enum-conversion'
            ],
            library_dirs=nodel_pyext_lib_path,
            libraries=[lib for (lib, cond) in cond_libs if cond],
            extra_link_args=[]
        ),
    ]
)