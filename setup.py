try:
  from setuptools import setup, Extension
except ImportError:
  from distutils.core import setup, Extension
import distutils.sysconfig
from distutils.sysconfig import customize_compiler
from distutils.command.build_clib import build_clib
from distutils.command.build_ext import build_ext
import os.path
import platform
import re
import sys, codecs

libdconv = (
    'dconv',
    dict(
        sources = ["src/dconv.cc"],
        include_dirs = ["./src"],
        language = "c++"
        # build_clib doesn't allow modifying the compile args
        #extra_compile_args = ['-O3'], 
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

with codecs.open('README.md', encoding='utf-8') as f:
    README = f.read()

with codecs.open('version.txt', encoding='utf-8') as f:
    VERSION = f.read().strip()


link_args = ['-lstdc++', '-lm']
compile_args = ['-D_GNU_SOURCE','-mavx2','-O3']
if platform.system() == 'Windows':
  compile_args = ['-D_GNU_SOURCE']
  link_args = []
  #compile_args = ['-D_GNU_SOURCE','/arch:AVX2']

  

module1 = Extension(
    'mrjson',
     sources = [
         './src/mrjson.c',
         './src/dec.c',
         './src/enc.c'
     ],
     include_dirs = ['./src'],
     extra_compile_args = compile_args,
     extra_link_args = link_args,
     define_macros = [('MRJSON_VERSION', VERSION),
                      ('_CRT_SECURE_NO_WARNINGS', 1)]
)

setup(
    name = 'mrjson',
    version = VERSION,
    license="MIT License",
    description = "JSON encoder and decoder for Python",
    keywords='json mrjson',
    long_description = README,
    long_description_content_type='text/markdown',
    libraries = [libdconv],
    ext_modules = [module1],
    author="Mark Reed",
    author_email="mark@untilfluent.com",
    download_url="https://github.com/MarkReedZ/mrjson/archive/v1.4.tar.gz",
    platforms=['any'],
    url="https://github.com/MarkReedZ/mrjson",
    cmdclass = {'build_ext': build_ext, 'build_clib': build_clib_without_warnings},
    test_suite = 'nose.collector',
    classifiers=[
        'Development Status :: 5 - Production/Stable',
        'Intended Audience :: Developers',
        'License :: OSI Approved :: MIT License',
        'Programming Language :: C',
        'Programming Language :: C++',
        'Programming Language :: Python :: 2.6',
        'Programming Language :: Python :: 2.7',
        'Programming Language :: Python :: 3',
        'Programming Language :: Python',
    ],
)
