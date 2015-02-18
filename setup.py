from distutils.core import setup, Extension

module1 = Extension("instrument",
        extra_compile_args = ['-std=c99'],
        sources = ["instrument.c"])

setup(name = "Instrument",
        version = "0.1",
        description = "foo",
        ext_modules = [module1],
        )
