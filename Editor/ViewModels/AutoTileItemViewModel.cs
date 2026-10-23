using Ludork.Services;

namespace Ludork.ViewModels;

public sealed class AutoTileItemViewModel
{
    public AutoTileItemViewModel(
        string key,
        ProjectAssetPaths assetPaths,
        string assetPath)
    {
        Key = key;
        AssetPaths = assetPaths;
        AssetPath = assetPath;
    }

    public string Key { get; }
    public ProjectAssetPaths AssetPaths { get; }
    public string AssetPath { get; }
}
