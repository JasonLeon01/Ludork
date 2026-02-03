cmake_minimum_required(VERSION 3.15)
project(GamePlayExtension)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
set(PROJECT_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set(SFML_INCLUDE_DIR ${PROJECT_ROOT}/C_Extensions/SFML/include)
if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set(SFML_LIB_DIR ${PROJECT_ROOT}/lib)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set(SFML_LIB_DIR ${PROJECT_ROOT}/Sample/Engine/pysf)
endif()
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

pybind11_add_module(GamePlayExtension
    "main.cpp"
    "src/Tilemap.cpp"
    "src/GameMap/GetMaterialPropertyMap.cpp"
    "src/GameMap/GetMaterialPropertyTexture.cpp"
    "src/GameMap/FindPath.cpp"
    "src/GameMap/RebuildPassabilityCache.cpp"
    "src/Particles/AddParticle.cpp"
    "src/Particles/UpdateParticlesInfo.cpp"
    "src/Particles/RemoveParticle.cpp"
)

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set_target_properties(GamePlayExtension PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        CXX_VISIBILITY_PRESET default
        VISIBILITY_INLINES_HIDDEN OFF
    )
    set_target_properties(GamePlayExtension PROPERTIES
        INSTALL_RPATH "@loader_path;@loader_path/../pysf"
        BUILD_WITH_INSTALL_RPATH TRUE
    )
endif()

target_include_directories(GamePlayExtension PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${SFML_INCLUDE_DIR}
)

target_link_directories(GamePlayExtension PRIVATE
    ${SFML_LIB_DIR}
)

target_link_libraries(GamePlayExtension PRIVATE
    sfml-graphics
    sfml-window
    sfml-system
)
