try:
  from setuptools import setup, Extension
except ImportError:
  from distutils.core import setup, Extension
import distutils.sysconfig
from distutils.sysconfig import customize_compiler
from distutils.command.build_clib import build_clib
from distutils.command.build_ext import build_ext
import os.path
import re
import sys

libdconv = (
    'dconv',
    dict(
        sources = ["src/dconv.cc"],
        include_dirs = ["./src"],
        language = "c++"
    )
)

class build_clib_without_warnings(build_clib):
    def build_libraries(self, libraries):
        customize_compiler(self.compiler)

        try:
            self.compiler.compiler_so.remove("-Wstrict-prototypes")
        except (AttributeError, ValueError):
            pass

        build_clib.build_libraries(self, libraries)

with open('README.md', encoding='utf-8') as f:
    README = f.read()

with open('version.txt', encoding='utf-8') as f:
    VERSION = f.read()

module1 = Extension(
    'mrjson',
     sources = [
         './src/mrjson.c',
         './src/dec.c',
         './src/enc.c'
     ],
     include_dirs = ['./src'],
     extra_compile_args = ['-D_GNU_SOURCE','-mavx2'],
     extra_link_args = ['-lstdc++', '-lm'],
     define_macros = [('MRJSON_VERSION', VERSION)]
)



setup(
    name = 'mrjson',
    version = VERSION,
    license="MIT License",
    description = "JSON encoder and decoder for Python",
    keywords='json mrjson',
    long_description = README,
    libraries = [libdconv],
    ext_modules = [module1],
    author="Mark Reed",
    author_email="mark@untilfluent.com",
    download_url="http://github.com/esnme/ultrajson",
    platforms=['any'],
    url="http://www.esn.me",
    cmdclass = {'build_ext': build_ext, 'build_clib': build_clib_without_warnings},
    classifiers=[
        'Development Status :: 5 - Production/Stable'
        'Intended Audience :: Developers',
        'License :: OSI Approved :: MIT License',
        'Programming Language :: C',
        'Programming Language :: C++',
        'Programming Language :: Python :: 3 :: Only',
        'Programming Language :: Python :: 3',
        'Programming Language :: Python :: 3.4',
        'Programming Language :: Python :: 3.5',
        'Programming Language :: Python :: 3.6',
        'Programming Language :: Python',
    ],
)
