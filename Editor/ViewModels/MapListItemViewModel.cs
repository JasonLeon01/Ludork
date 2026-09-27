using Ludork.Models;
using System.Collections.ObjectModel;

namespace Ludork.ViewModels;

public sealed class MapListItemViewModel : ViewModelBase
{
    private bool isExpanded;
    private bool isModified;
    private string displayName;

    public MapListItemViewModel(
        string key,
        string displayName,
        MapCatalogEntryKind kind,
        string? worldKey)
    {
        Key = key;
        this.displayName = displayName;
        Kind = kind;
        WorldKey = worldKey;
    }

    public string Key { get; }
    public string DisplayName
    {
        get => displayName;
        internal set => SetProperty(ref displayName, value);
    }
    public MapCatalogEntryKind Kind { get; }
    public string? WorldKey { get; }
    public bool IsWorld => Kind == MapCatalogEntryKind.WorldMap;
    public bool IsMap => Kind is MapCatalogEntryKind.StandaloneMap or MapCatalogEntryKind.WorldChildMap;
    public bool IsWorldChild => Kind == MapCatalogEntryKind.WorldChildMap;
    public ObservableCollection<MapListItemViewModel> Children { get; } = [];
    public bool IsModified
    {
        get => isModified;
        set => SetProperty(ref isModified, value);
    }
    public bool IsExpanded
    {
        get => isExpanded;
        set => SetProperty(ref isExpanded, value);
    }
}
