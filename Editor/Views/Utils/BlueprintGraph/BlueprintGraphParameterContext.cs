using Ludork.Services;

namespace Ludork.Views.Utils.BlueprintGraph;

internal sealed record BlueprintGraphParameterContext(
    ProjectDataStore Data,
    BlueprintNodeParameterEditorFactory Editors,
    IGameVariableCatalog Variables,
    string AssetsDirectory,
    int CellSize);
