from setuptools import Extension, setup
import os
import re
import glob

nodel_wflags = re.split(r'\s+', os.environ.get('NODEL_WFLAGS'))
nodel_pyext_include = re.split(r'\s+', os.environ.get('NODEL_PYEXT_INCLUDE'))
nodel_pyext_lib_path = re.split(r'\s+', os.environ.get('NODEL_PYEXT_LIB_PATH'))

print('~' * 100)
print(os.getcwd())
print('~' * 100)

setup(
    ext_modules=[
        Extension(
            name='nodel',
            version='0.1',
            url='https://github.com',
            define_macros=[
                ('PYNODEL_ROCKSDB', 1)
            ],
            sources=[
                'module.cxx', 
                'NodelObject.cxx', 
                'NodelKeyIter.cxx',
                'NodelValueIter.cxx',
                'NodelItemIter.cxx',
                'NodelTreeIter.cxx'
            ],
            include_dirs=[
                *nodel_pyext_include
            ],
            extra_compile_args=[
                '--std=c++20',
                '-Wno-c99-designator',
                '-Wno-delete-non-abstract-non-virtual-dtor',
                *nodel_wflags
            ],
            library_dirs=[
                *nodel_pyext_lib_path
            ],
            libraries=[
                'dwarf',
                'zstd',
                'cpptrace',
                'fmt',
                'speedb',
                'ziplib'
            ],
            extra_link_args=[]
        ),
    ]
)