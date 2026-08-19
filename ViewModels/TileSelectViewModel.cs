using System;
using System.Collections.ObjectModel;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;
using Ludork.Services;

namespace Ludork.ViewModels;

public sealed class TileSelectViewModel : ViewModelBase
{
    private TilesetTabViewModel? selectedTileset;
    private AutoTileItemViewModel? selectedAutoTile;
    private TileSelection? selectedTiles;
    private bool isLayerSelected;
    private bool syncingTilesetSelection;

    public TileSelectViewModel(GameDataService gameData)
    {
        GameData = gameData;
        CellSize = gameData.getCellSize();
        foreach (string key in gameData.TilesetData.Keys)
        {
            string fileName = gameData.TilesetData[key]["fileName"]?.GetValue<string>() ?? string.Empty;
            Tilesets.Add(new TilesetTabViewModel(key, Path.Combine(gameData.ProjectPath, "Assets", "Tilesets", fileName)));
        }
        foreach (string key in gameData.AutoTileData.Keys)
        {
            string fileName = gameData.AutoTileData[key]["fileName"]?.GetValue<string>() ?? string.Empty;
            AutoTiles.Add(new AutoTileItemViewModel(key, Path.Combine(gameData.ProjectPath, "Assets", "Autotiles", fileName)));
        }
        SelectedTileset = Tilesets.FirstOrDefault();
    }

    public GameDataService GameData { get; }
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
        Tilesets.Clear();
        AutoTiles.Clear();
        foreach (string key in GameData.TilesetData.Keys)
        {
            string fileName = GameData.TilesetData[key]["fileName"]?.GetValue<string>() ?? string.Empty;
            Tilesets.Add(new TilesetTabViewModel(key, Path.Combine(GameData.ProjectPath, "Assets", "Tilesets", fileName)));
        }
        foreach (string key in GameData.AutoTileData.Keys)
        {
            string fileName = GameData.AutoTileData[key]["fileName"]?.GetValue<string>() ?? string.Empty;
            AutoTiles.Add(new AutoTileItemViewModel(key, Path.Combine(GameData.ProjectPath, "Assets", "Autotiles", fileName)));
        }
        SelectedTileset = Tilesets.FirstOrDefault(item => item.Key == tilesetKey) ?? Tilesets.FirstOrDefault();
        syncingTilesetSelection = false;
        SelectedAutoTile = AutoTiles.FirstOrDefault(item => item.Key == autoTileKey);
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
}

public sealed record TilesetTabViewModel(string Key, string ImagePath);

public sealed class AutoTileItemViewModel
{
    public AutoTileItemViewModel(string key, string imagePath)
    {
        Key = key;
        ImagePath = imagePath;
    }

    public string Key { get; }
    public string ImagePath { get; }
}

public sealed record TileSelection(int OriginTileNumber, int Width, int Height);
