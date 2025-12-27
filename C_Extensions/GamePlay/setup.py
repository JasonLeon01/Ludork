from setuptools import setup, Extension

module = Extension(
    "GamePlayExtension",
    sources=["src/Tilemap.c", "src/GameMap.c", "main.c"],
    include_dirs=["./include"],
)

setup(
    name="GamePlayExtension",
    version="1.0",
    ext_modules=[module],
)
