cmake_minimum_required(VERSION 3.15)
project(GraphicsExtension)
include(../common.cmake)
include(../SFMLcommon.cmake)

pybind11_add_module(GraphicsExtension
    "main.cpp"
    "src/CompressAnimation.cpp"
    "src/RectBase.cpp"
    "src/Color.cpp"
)

ConfigureTarget(GraphicsExtension)
ConfigureSFMLTarget(GraphicsExtension)
