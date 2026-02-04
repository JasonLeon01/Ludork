cmake_minimum_required(VERSION 3.15)
project(GamePlayExtension)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
if (CMAKE_SYSTEM_NAME STREQUAL "Windows" AND MSVC)
  set(CMAKE_CXX_FLAGS_RELEASE "/O2 /DNDEBUG")
else()
  set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -flto")
endif()
set(PROJECT_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set(SFML_INCLUDE_DIR ${PROJECT_ROOT}/C_Extensions/SFML/include)
set(SFML_LIB_DIR ${PROJECT_ROOT}/lib)
set(SFML_BIN_DIR ${PROJECT_ROOT}/Sample/Engine/pysf)

execute_process(COMMAND python -c "import pybind11; print(pybind11.get_cmake_dir())" 
                OUTPUT_VARIABLE pybind11_DIR OUTPUT_STRIP_TRAILING_WHITESPACE)
set(PYBIND11_FINDPYTHON ON)
find_package(pybind11 REQUIRED PATHS ${pybind11_DIR})

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
