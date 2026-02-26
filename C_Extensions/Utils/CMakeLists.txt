cmake_minimum_required(VERSION 3.15)
project(UtilsExtension)
include(../common.cmake)
include(../SFMLcommon.cmake)

pybind11_add_module(UtilsExtension
    "main.cpp"
    "src/AccelerateCalculation.cpp"
)

ConfigureTarget(UtilsExtension)
ConfigureSFMLTarget(UtilsExtension)
