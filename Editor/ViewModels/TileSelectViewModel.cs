using Ludork.Models;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Text.Json.Nodes;
using Ludork.Services;

namespace Ludork.ViewModels;

public sealed class TileSelectViewModel : ViewModelBase, IDisposable
{
    private TilesetTabViewModel? selectedTileset;
    private AutoTileItemViewModel? selectedAutoTile;
    private TileSelection? selectedTiles;
    private bool isLayerSelected;
    private bool syncingTilesetSelection;
    private bool disposed;

    public TileSelectViewModel(ProjectDataStore gameData)
    {
        GameData = gameData;
        CellSize = gameData.Configs.getCellSize();
        gameData.Documents.ContentChanged += onDataChanged;
        RefreshData();
    }

    public ProjectDataStore GameData { get; }
    public int CellSize { get; }
    public ObservableCollection<TilesetTabViewModel> Tilesets { get; } = [];
    public ObservableCollection<AutoTileItemViewModel> AutoTiles { get; } = [];
    public event EventHandler<string>? TilesetSelected;

    public TilesetTabViewModel? SelectedTileset
    {
        get => selectedTileset;
        set
        {
            if (!SetProperty(ref selectedTileset, value))
                return;
            clearTileSelection();
            if (!syncingTilesetSelection && value is not null)
                TilesetSelected?.Invoke(this, value.Key);
        }
    }

    public AutoTileItemViewModel? SelectedAutoTile
    {
        get => selectedAutoTile;
        set
        {
            if (!IsLayerSelected)
                value = null;
            if (!SetProperty(ref selectedAutoTile, value))
                return;
            if (value is not null)
                SelectedTiles = null;
        }
    }

    public TileSelection? SelectedTiles
    {
        get => selectedTiles;
        private set
        {
            if (!SetProperty(ref selectedTiles, value))
                return;
            if (value is not null)
                SetProperty(ref selectedAutoTile, null, nameof(SelectedAutoTile));
        }
    }

    public bool IsLayerSelected
    {
        get => isLayerSelected;
        set
        {
            if (!SetProperty(ref isLayerSelected, value))
                return;
            OnPropertyChanged(nameof(SelectionOpacity));
            if (!value)
                ClearSelection();
        }
    }

    public double SelectionOpacity => IsLayerSelected ? 1.0 : 0.45;

    public void setCurrentTilesetKey(string? key)
    {
        syncingTilesetSelection = true;
        SelectedTileset = Tilesets.FirstOrDefault(item => item.Key == key) ?? Tilesets.FirstOrDefault();
        syncingTilesetSelection = false;
    }

    public void RefreshData()
    {
        string? tilesetKey = SelectedTileset?.Key;
        string? autoTileKey = SelectedAutoTile?.Key;
        syncingTilesetSelection = true;
        syncTilesets();
        syncAutoTiles();
        SelectedTileset = Tilesets.FirstOrDefault(item => item.Key == tilesetKey) ?? Tilesets.FirstOrDefault();
        syncingTilesetSelection = false;
        SelectedAutoTile = AutoTiles.FirstOrDefault(item => item.Key == autoTileKey);
    }

    public void Dispose()
    {
        if (disposed)
            return;
        disposed = true;
        GameData.Documents.ContentChanged -= onDataChanged;
    }

    public void selectTiles(int originTileNumber, int width, int height)
    {
        if (!IsLayerSelected || SelectedTileset is null)
        {
            ClearSelection();
            return;
        }
        TileSelection next = new TileSelection(originTileNumber, Math.Max(1, width), Math.Max(1, height));
        SelectedTiles = SelectedTiles == next && width == 1 && height == 1 ? null : next;
    }

    public void setTiles(int originTileNumber, int width, int height)
    {
        if (!IsLayerSelected || SelectedTileset is null)
        {
            ClearSelection();
            return;
        }
        SelectedTiles = new TileSelection(originTileNumber, Math.Max(1, width), Math.Max(1, height));
    }

    public void ClearSelection()
    {
        clearTileSelection();
        SelectedAutoTile = null;
    }

    private void clearTileSelection()
    {
        SetProperty(ref selectedTiles, null, nameof(SelectedTiles));
    }

    private void onDataChanged(object? sender, EditorDocumentsChangedEventArgs args)
    {
        if (args.Reset || args.Changes.Any(change => change.Section is "Tilesets" or "AutoTiles"))
            RefreshData();
    }

    private void syncTilesets()
    {
        int index = 0;
        foreach (KeyValuePair<string, TilesetSnapshot> pair in GameData.Assets.TilesetData)
        {
            string name = pair.Value.Name ?? pair.Key;
            string assetPath = pair.Value.FileName;
            int currentIndex = findTilesetIndex(pair.Key, index);
            if (currentIndex < 0)
            {
                Tilesets.Insert(index, new TilesetTabViewModel(pair.Key, name, assetPath));
                index += 1;
                continue;
            }
            if (currentIndex != index)
                Tilesets.Move(currentIndex, index);
            if (!string.Equals(Tilesets[index].Name, name, StringComparison.Ordinal)
                || !string.Equals(Tilesets[index].AssetPath, assetPath, StringComparison.Ordinal))
            {
                Tilesets[index] = new TilesetTabViewModel(pair.Key, name, assetPath);
            }
            index += 1;
        }
        while (Tilesets.Count > index)
            Tilesets.RemoveAt(Tilesets.Count - 1);
    }

    private int findTilesetIndex(string key, int startIndex)
    {
        for (int index = startIndex; index < Tilesets.Count; index++)
        {
            if (string.Equals(Tilesets[index].Key, key, StringComparison.Ordinal))
                return index;
        }
        return -1;
    }

    private void syncAutoTiles()
    {
        int index = 0;
        foreach (KeyValuePair<string, TilesetSnapshot> pair in GameData.Assets.AutoTileData)
        {
            string assetPath = pair.Value.FileName;
            int currentIndex = findAutoTileIndex(pair.Key, index);
            if (currentIndex < 0)
            {
                AutoTiles.Insert(index, new AutoTileItemViewModel(
                    pair.Key,
                    GameData.ProjectPath,
                    assetPath));
                index += 1;
                continue;
            }
            if (currentIndex != index)
                AutoTiles.Move(currentIndex, index);
            if (!string.Equals(AutoTiles[index].AssetPath, assetPath, StringComparison.Ordinal))
            {
                AutoTiles[index] = new AutoTileItemViewModel(
                    pair.Key,
                    GameData.ProjectPath,
                    assetPath);
            }
            index += 1;
        }
        while (AutoTiles.Count > index)
            AutoTiles.RemoveAt(AutoTiles.Count - 1);
    }

    private int findAutoTileIndex(string key, int startIndex)
    {
        for (int index = startIndex; index < AutoTiles.Count; index++)
        {
            if (string.Equals(AutoTiles[index].Key, key, StringComparison.Ordinal))
                return index;
        }
        return -1;
    }
}

public sealed record TilesetTabViewModel(string Key, string Name, string AssetPath);

public sealed record TileSelection(int OriginTileNumber, int Width, int Height);
