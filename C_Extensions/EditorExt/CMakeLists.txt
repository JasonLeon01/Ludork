project(EditorExt)

pybind11_add_module(EditorExt
    "main.cpp"
    "src/RenderTilemapRGBA.cpp"
    "src/GraphLayout.cpp"
)

ConfigureTarget(EditorExt)
