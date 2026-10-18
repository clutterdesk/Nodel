from setuptools import Extension, setup
import os
import re
import glob
import sys

nodel_rocksdb = int(os.environ.get('NODEL_PYEXT_ROCKSDB_FOUND', False))
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
            version='0.2',
            url='https://github.com/clutterdesk/Nodel',
            define_macros=[
                ('PYNODEL_ROCKSDB', nodel_rocksdb)
            ],
            sources=[
                'module.cxx', 
                'NodelObject.cxx', 
                'NodelKeyIter.cxx',
                'NodelValueIter.cxx',
                'NodelItemIter.cxx',
                'NodelTreeIter.cxx',
                'NodelQuery.cxx'
            ],
            include_dirs=nodel_pyext_include,
            extra_compile_args=[
                '--std=c++20'
            ],
            library_dirs=nodel_pyext_lib_path,
            libraries=[lib for (lib, cond) in cond_libs if cond],
            extra_link_args=[]
        ),
    ]
)