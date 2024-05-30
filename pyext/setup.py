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


# lookup <module-path>/..
mod_path = os.path.dirname(os.path.abspath(os.path.dirname(__file__)))

def find_includes(dirs):
    paths = []
    for dir in dirs:
        path = next((entry[0] for entry in os.walk(mod_path) 
                     if dir in os.path.join(entry[0], entry[1])[-len(dir):]), None)
        if path is None:
            sys.stderr.write(f'Include file not found: {dir}')
            sys.exit(-1)
        paths.append()
    return paths

def find_libs(names):
    paths = []
    for name in names:
        path = next((entry[0] for entry in os.walk(mod_path) if name in entry[2]), None)
        if path is None:
            sys.stderr.write(f'Library not found: {name}')
            sys.exit(-1)
        paths.append()
    return paths


includes = [
    'Source/ZipLib',
    'include/cpptrace',
    'include/fmt',
    'include/tsl',
    'include/rocksdb'
]

libs = [
    'dwarf',
    'zstd',
    'cpptrace',
    'fmt',
    'speedb',
    'ziplib'
]


include_paths = find_includes(includes)
lib_paths = find_libs([f'lib{lib}.a' for lib in libs])


setup(
    ext_modules=[
        Extension(
            name='nodel',
            version='0.1',
            url='https://github.com/clutterdesk/Nodel',
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
            include_dirs=include_paths,
            extra_compile_args=[
                '--std=c++20',
                '-Wno-c99-designator',
                '-Wno-delete-non-abstract-non-virtual-dtor',
                *nodel_wflags
            ],
            library_dirs=lib_paths,
            libraries=libs,
            extra_link_args=[]
        ),
    ]
)