from setuptools import Extension, setup
import os
import re

nodel_wflags = re.split(r'\s+', os.environ.get('NODEL_WFLAGS'))
nodel_pyext_include = re.split(r'\s+', os.environ.get('NODEL_PYEXT_INCLUDE'))
nodel_pyext_lib_path = re.split(r'\s+', os.environ.get('NODEL_PYEXT_LIB_PATH'))

print('~' * 100)
print(os.getcwd())
print('~' * 100)
print(nodel_pyext_include);
print('~' * 100)
print(nodel_pyext_lib_path);
print('~' * 100)

setup(
    ext_modules=[
        Extension(
            name='nodel',
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
                *nodel_wflags
            ],
            library_dirs=[
                *nodel_pyext_lib_path
            ],
            libraries=[
                'fmt',
                'dwarf',
                'cpptrace',
                'speedb'
            ],
            extra_link_args=[]
        ),
    ]
)