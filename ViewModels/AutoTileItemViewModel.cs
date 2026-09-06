
namespace Ludork.ViewModels;

public sealed class AutoTileItemViewModel
{
    public AutoTileItemViewModel(
        string key,
        string projectPath,
        string assetPath)
    {
        Key = key;
        ProjectPath = projectPath;
        AssetPath = assetPath;
    }

    public string Key { get; }
    public string ProjectPath { get; }
    public string AssetPath { get; }
}
