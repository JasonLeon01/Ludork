cmake_minimum_required(VERSION 3.15)
project(EditorExtension)
include(../common.cmake)

pybind11_add_module(EditorExtension
    "main.cpp"
    "src/RenderTilemapRGBA.cpp"
)

ConfigureTarget(EditorExtension)
