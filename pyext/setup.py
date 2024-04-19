from setuptools import Extension, setup
import os

nodel_root = '/Users/bdunnagan/git/Nodel'
print('Nodel root directory: {}', nodel_root)

ordered_map_install = '/Users/bdunnagan/git/ordered-map/include'
print('tsl/ordered_map directory: {}', ordered_map_install)

fmt_install = '/Users/bdunnagan/git/fmt/include'
print('fmt directory: {}', fmt_install)

cpp_trace_install = '/Users/bdunnagan/git/cpptrace/include'
print('cpp_trace directory: {}', cpp_trace_install)

setup(
    ext_modules=[
        Extension(
            name='nodel',
            sources=[
                'module.cpp', 
                'NodelObject.cpp', 
                'NodelKeyIter.cpp',
                'NodelValueIter.cpp',
                'NodelItemIter.cpp',
                'NodelTreeIter.cpp'
            ],
            include_dirs=[
                os.path.join(nodel_root, 'include'),
                ordered_map_install,
                fmt_install,
                cpp_trace_install
            ],
            extra_compile_args=[
                '--std=c++20',
                '-Wno-c99-designator'
            ],
            library_dirs=[
                '/Users/bdunnagan/git/fmt',
                '/usr/local/Cellar/dwarfutils/0.9.1/lib',
                '/Users/bdunnagan/git/cpptrace/build'
            ],
            libraries=[
                'fmt',
                'dwarf',
                'cpptrace'
            ]
        ),
    ]
)