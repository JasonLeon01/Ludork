project(EditorExt)

pybind11_add_module(EditorExt
    "main.cpp"
    "src/MapImageBuffer.cpp"
    "src/RenderTilemapRGBA.cpp"
    "src/AutoTileCore.cpp"
    "src/AutoTile.cpp"
    "src/MapLayer.cpp"
    "src/GraphLayout.cpp"
)

ConfigureTarget(EditorExt)
