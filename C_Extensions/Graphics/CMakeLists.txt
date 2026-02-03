cmake_minimum_required(VERSION 3.15)
project(GraphicsExtension)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
set(PROJECT_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set(SFML_INCLUDE_DIR ${PROJECT_ROOT}/C_Extensions/SFML/include)
set(SFML_LIB_DIR ${PROJECT_ROOT}/lib)
set(SFML_BIN_DIR ${PROJECT_ROOT}/Sample/Engine/pysf)

execute_process(COMMAND python -c "import pybind11; print(pybind11.get_cmake_dir())" 
                OUTPUT_VARIABLE pybind11_DIR OUTPUT_STRIP_TRAILING_WHITESPACE)
set(PYBIND11_FINDPYTHON ON)
find_package(pybind11 REQUIRED PATHS ${pybind11_DIR})
include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported OUTPUT ipo_msg)
if(ipo_supported)
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
endif()

pybind11_add_module(GraphicsExtension
    "main.cpp"
    "src/CompressAnimation.cpp"
    "src/RectBase.cpp"
)

target_include_directories(GraphicsExtension PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${SFML_INCLUDE_DIR}
)

target_link_directories(GraphicsExtension PRIVATE
    ${SFML_LIB_DIR}
)

target_link_libraries(GraphicsExtension PRIVATE
    sfml-graphics
    sfml-window
    sfml-system
)
