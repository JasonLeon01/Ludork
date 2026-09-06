using Ludork.Models;
using System.Collections.ObjectModel;

namespace Ludork.ViewModels;

public sealed class MapListItemViewModel : ViewModelBase
{
    private bool isExpanded;

    public MapListItemViewModel(
        string key,
        string displayName,
        MapCatalogEntryKind kind,
        string? worldKey)
    {
        Key = key;
        DisplayName = displayName;
        Kind = kind;
        WorldKey = worldKey;
    }

    public string Key { get; }
    public string DisplayName { get; }
    public MapCatalogEntryKind Kind { get; }
    public string? WorldKey { get; }
    public bool IsWorld => Kind == MapCatalogEntryKind.WorldMap;
    public bool IsMap => Kind is MapCatalogEntryKind.StandaloneMap or MapCatalogEntryKind.WorldChildMap;
    public bool IsWorldChild => Kind == MapCatalogEntryKind.WorldChildMap;
    public ObservableCollection<MapListItemViewModel> Children { get; } = [];
    public bool IsExpanded
    {
        get => isExpanded;
        set => SetProperty(ref isExpanded, value);
    }
}
