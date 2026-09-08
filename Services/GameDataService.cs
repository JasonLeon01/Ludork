using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;
using System.Threading.Tasks;
using Ludork.Models;

namespace Ludork.Services;

public sealed partial class GameDataService : IDisposable
{
    private const int MaximumHistoryEntries = 100;
    private const string WorldHistoryScope = "__WorldHistoryScope";
    private const string WorldFileMovesScope = "__WorldFileMovesScope";
    private static readonly JsonSerializerOptions WriteOptions = new()
    {
        WriteIndented = true,
    };
    private readonly Dictionary<string, DataSection> sections = new(StringComparer.Ordinal)
    {
        ["Configs"] = new("system", true),
        ["Tilesets"] = new("tileset", true),
        ["AutoTiles"] = new("autoTile", true),
        ["Maps"] = new("map", true),
        ["WorldMaps"] = new(["worldMap"]),
        ["MapCatalog"] = new(null, false, false),
        ["CommonFunctions"] = new("commonFunction", true),
        ["Blueprints"] = new("blueprint", true),
        ["Animations"] = new("animation", true),
        ["Curves"] = new(["curve", "vector2Curve", "vector3Curve", "vector4Curve"]),
        ["TextConfigs"] = new(["plainTextConfig", "richTextConfig"]),
        ["UI"] = new([UiAssetSchema.UiAssetType]),
        ["General"] = new(null, false),
    };

    private Dictionary<string, Dictionary<string, JsonObject>> originData = new(StringComparer.Ordinal);
    private readonly Stack<Dictionary<string, Dictionary<string, JsonObject>>> undoStack = new();
    private readonly Stack<Dictionary<string, Dictionary<string, JsonObject>>> redoStack = new();
    private readonly List<string> invalidLoadPaths = [];
    private readonly GeneralEnumService generalEnums;
    private readonly LazyMapDataDictionary mapData;
    private readonly WorldMapValidationService worldMapValidation = new();
    private readonly Dictionary<string, long> mapAccessOrder = new(StringComparer.Ordinal);
    private readonly Dictionary<string, long> mapLoadedBytes = new(StringComparer.Ordinal);
    private readonly Dictionary<string, Dictionary<string, List<MapActorTagLocation>>> mapActorTagIndexes = new(StringComparer.Ordinal);
    private readonly Dictionary<string, JsonObject> loadedMapCatalogCache = new(StringComparer.Ordinal);
    private readonly Dictionary<string, JsonObject> nextMapCatalogCache = new(StringComparer.Ordinal);
    private readonly Dictionary<string, string> pendingWorldDirectoryMoves = new(StringComparer.Ordinal);
    private readonly Dictionary<string, string> archivedOriginWorldDirectories = new(StringComparer.Ordinal);
    private readonly string worldHistorySessionRoot;
    private long nextMapAccessOrder;
    private long nextHistoryGestureId;
    private long activeHistoryGestureId;
    private bool activeHistoryGestureHasSnapshot;
    private bool isModified;
    private bool generalDataGenerationPending;
    private bool disposed;

    public GameDataService(string projectPath)
    {
        ProjectPath = Path.GetFullPath(projectPath);
        worldHistorySessionRoot = Path.Combine(
            ProjectPath,
            "Temp",
            "EditorMapHistory",
            Guid.NewGuid().ToString("N"));
        generalEnums = new GeneralEnumService(ProjectPath);
        MapPathPolicy = new WorldMapPathPolicy(ProjectPath);
        mapData = new LazyMapDataDictionary(this);
        loadAll();
    }

    public event EventHandler? ModifiedChanged;
    public event EventHandler? DataChanged;
    public event EventHandler? DataReloaded;
    public event EventHandler? DataRestored;
    public event EventHandler? DataSaved;
    public event EventHandler? UiAssetsChanged;
    public event EventHandler? UndoRedoStateChanged;
    public event EventHandler<MapPreviewChangedEventArgs>? MapPreviewChanged;

    public string ProjectPath { get; }
    public bool IsModified => isModified;
    public bool CanUndo => undoStack.Count != 0;
    public bool CanRedo => redoStack.Count != 0;
    public IReadOnlyList<string> InvalidLoadPaths => invalidLoadPaths;
    public IReadOnlyDictionary<string, JsonObject> SystemConfigData => sections["Configs"].Snapshots;
    public IReadOnlyDictionary<string, JsonObject> TilesetData => sections["Tilesets"].Snapshots;
    public IReadOnlyDictionary<string, JsonObject> AutoTileData => sections["AutoTiles"].Snapshots;
    public IReadOnlyDictionary<string, JsonObject> MapData => mapData;
    public IReadOnlyDictionary<string, JsonObject> LoadedMapData => sections["Maps"].Snapshots;
    public IReadOnlyDictionary<string, JsonObject> WorldMapData => sections["WorldMaps"].Snapshots;
    public IReadOnlyList<MapCatalogEntry> MapCatalog => getMapCatalogEntries();
    public WorldMapPathPolicy MapPathPolicy { get; }
    public IReadOnlyDictionary<string, JsonObject> CommonFunctionsData => sections["CommonFunctions"].Snapshots;
    public IReadOnlyDictionary<string, JsonObject> BlueprintsData => sections["Blueprints"].Snapshots;
    public IReadOnlyDictionary<string, JsonObject> AnimationsData => sections["Animations"].Snapshots;
    public IReadOnlyDictionary<string, JsonObject> CurvesData => sections["Curves"].Snapshots;
    public IReadOnlyDictionary<string, JsonObject> TextConfigsData => sections["TextConfigs"].Snapshots;
    public IReadOnlyDictionary<string, JsonObject> UiAssetsData => new JsonSnapshotDictionary(sections["UI"].Data
        .Where(pair => isUiDataType(pair.Value, UiAssetSchema.UiAssetType))
        .ToDictionary(pair => pair.Key, pair => pair.Value, StringComparer.Ordinal));
    public IReadOnlyDictionary<string, JsonObject> GeneralData => sections["General"].Snapshots;

}

public sealed record DataFileInfo(string Type, string? Key);
public sealed record SaveResult(bool Success, string Details);
