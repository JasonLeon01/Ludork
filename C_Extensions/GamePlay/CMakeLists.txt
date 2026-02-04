cmake_minimum_required(VERSION 3.15)
project(GamePlayExtension)
include(../common.cmake)
include(../SFMLcommon.cmake)

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

ConfigureTarget(GamePlayExtension)
ConfigureSFMLTarget(GamePlayExtension)
