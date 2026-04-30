project(GlobalExt)

pybind11_add_module(GlobalExt
    "main.cpp"
    "src/AccelerateCalculation.cpp"
    "src/GameMapExt.cpp"
    "src/GameMap.cpp"
)

ConfigureTarget(GlobalExt)
ConfigureSFMLTarget(GlobalExt)
