cmake_minimum_required(VERSION 3.15)
project(GamePlayExtension)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
set(SFML_DIR ${CMAKE_SOURCE_DIR}/../SFML)

execute_process(COMMAND python -c "import pybind11; print(pybind11.get_cmake_dir())" 
                OUTPUT_VARIABLE pybind11_DIR OUTPUT_STRIP_TRAILING_WHITESPACE)
set(PYBIND11_FINDPYTHON ON)
find_package(pybind11 REQUIRED PATHS ${pybind11_DIR})
include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported OUTPUT ipo_msg)
if(ipo_supported)
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
endif()

set(SFML_BUILD_GRAPHICS ON)
set(SFML_BUILD_WINDOW ON)
set(SFML_BUILD_SYSTEM ON)
set(BUILD_SHARED_LIBS OFF)
add_subdirectory(${SFML_DIR} ${CMAKE_CURRENT_BINARY_DIR}/SFML)

pybind11_add_module(GamePlayExtension
    "main.cpp"
    "src/Tilemap.cpp"
    "src/GameMap/GetMaterialPropertyMap.cpp"
    "src/GameMap/GetMaterialPropertyTexture.cpp"
    "src/GameMap/FindPath.cpp"
    "src/GameMap/RebuildPassabilityCache.cpp"
    "src/Particles/AddParticle.cpp"
    "src/Particles/UpdateParticlesInfo.cpp"
)

target_include_directories(GamePlayExtension PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${SFML_DIR}/include
)

target_link_libraries(GamePlayExtension PRIVATE
    sfml-graphics
    sfml-system
)
if(MSVC)
  target_compile_options(GamePlayExtension PRIVATE $<$<CONFIG:Release>:/O2 /DNDEBUG /GL>)
  target_link_options(GamePlayExtension PRIVATE $<$<CONFIG:Release>:/LTCG /OPT:REF /OPT:ICF>)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
elseif(APPLE)
  target_compile_options(GamePlayExtension PRIVATE $<$<CONFIG:Release>:-O2 -DNDEBUG -flto>)
  target_link_options(GamePlayExtension PRIVATE $<$<CONFIG:Release>:-flto -Wl,-dead_strip>)
else()
  target_compile_options(GamePlayExtension PRIVATE $<$<CONFIG:Release>:-O2 -DNDEBUG -ffunction-sections -fdata-sections -flto>)
  target_link_options(GamePlayExtension PRIVATE $<$<CONFIG:Release>:-Wl,--gc-sections -flto>)
endif()
if(MSVC)
  target_compile_options(sfml-graphics PRIVATE $<$<CONFIG:Release>:/O2 /DNDEBUG /GL /Gy>)
  target_compile_options(sfml-system PRIVATE $<$<CONFIG:Release>:/O2 /DNDEBUG /GL /Gy>)
elseif(APPLE)
  target_compile_options(sfml-graphics PRIVATE $<$<CONFIG:Release>:-O2 -DNDEBUG -flto>)
  target_compile_options(sfml-system PRIVATE $<$<CONFIG:Release>:-O2 -DNDEBUG -flto>)
else()
  target_compile_options(sfml-graphics PRIVATE $<$<CONFIG:Release>:-O2 -DNDEBUG -ffunction-sections -fdata-sections -flto>)
  target_compile_options(sfml-system PRIVATE $<$<CONFIG:Release>:-O2 -DNDEBUG -ffunction-sections -fdata-sections -flto>)
endif()
