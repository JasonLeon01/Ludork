# -*- encoding: utf-8 -*-

from setuptools import setup, Extension
import sys

cppArgs = ["-std=c++17"]
if sys.platform == "win32":
    cppArgs = ["/std:c++17"]

module = Extension(
    "GamePlayExtension",
    sources=[
        "../src/utils.cpp",
        "src/Tilemap.cpp",
        "src/GameMap/GetMaterialPropertyMap.cpp",
        "src/GameMap/GetMaterialPropertyTexture.cpp",
        "src/GameMap/FindPath.cpp",
        "src/Particles/AddParticle.cpp",
        "src/Particles/UpdateParticlesInfo.cpp",
        "main.cpp",
    ],
    include_dirs=["./include", "../include"],
    language="c++",
    extra_compile_args=cppArgs,
)

setup(
    name="GamePlayExtension",
    version="1.0",
    ext_modules=[module],
)
