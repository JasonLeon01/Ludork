from setuptools import setup, Extension

module = Extension(
    "TilemapExtension",
    sources=["main.c"],
)

setup(
    name="TilemapExtension",
    version="1.0",
    ext_modules=[module],
)
