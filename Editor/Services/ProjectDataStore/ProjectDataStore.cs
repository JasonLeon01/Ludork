using Ludork.Models;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Encodings.Web;
using System.Text.Json;
using System.Text.Json.Nodes;
using System.Text.Unicode;
using System.Threading;

namespace Ludork.Services;

public sealed partial class ProjectDataStore : IDisposable
{
    public MapDataService Maps { get; }
    public WorldDataService Worlds { get; }
    public BlueprintDataService Blueprints { get; }
    public GeneralDataService General { get; }
    public ConfigDataService Configs { get; }
    public AssetDataService Assets { get; }
    public UiAssetDataService UiAssets { get; }
    internal static readonly JsonSerializerOptions WriteOptions = new()
    {
        WriteIndented = true,
        Encoder = JavaScriptEncoder.Create(UnicodeRanges.All),
    };

    private readonly Dictionary<string, EditorDocumentCollection> sections = new(StringComparer.Ordinal)
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
        ["Particles"] = new("particle", true),
        ["Curves"] = new(["curve", "vector2Curve", "vector3Curve", "vector4Curve"]),
        ["TextConfigs"] = new(["plainTextConfig", "richTextConfig"]),
        ["UI"] = new([UiAssetSchema.UiAssetType]),
        ["General"] = new(null, false),
    };

    private Dictionary<string, Dictionary<string, JsonObject>> originData = new(StringComparer.Ordinal);

    private readonly List<string> invalidLoadPaths = [];

    private readonly GeneralEnumService generalEnums;

    private readonly bool cacheMapCatalog;
    internal bool CacheMapCatalog => cacheMapCatalog;

    private readonly CancellationToken loadCancellationToken;

    private readonly Action<string>? loadProgress;

    private long activeHistoryGestureId;

    private bool isModified;

    private bool generalDataGenerationPending;

    private bool disposed;
    internal bool IsDisposed => disposed;

    public ProjectDataStore(string projectPath, bool cacheMapCatalog = true,
        CancellationToken loadCancellationToken = default, Action<string>? loadProgress = null)
    {
        this.cacheMapCatalog = cacheMapCatalog;
        this.loadCancellationToken = loadCancellationToken;
        this.loadProgress = loadProgress;
        ProjectPath = Path.GetFullPath(projectPath);
        generalEnums = new GeneralEnumService(ProjectPath);

        foreach ((string sectionName, EditorDocumentCollection collection) in sections)
            collection.BindHistory((key, description, marker) => RecordDocumentSnapshot(sectionName, key, description, marker));
        Maps = new MapDataService(this, sections["Maps"], sections["MapCatalog"]);
        Worlds = new WorldDataService(this, sections["WorldMaps"]);
        Blueprints = new BlueprintDataService(this, sections["Blueprints"], sections["CommonFunctions"]);
        General = new GeneralDataService(this, sections["General"]);
        Configs = new ConfigDataService(this, sections["Configs"]);
        Assets = new AssetDataService(this, sections["Tilesets"], sections["AutoTiles"], sections["Animations"], sections["Particles"], sections["Curves"], sections["TextConfigs"]);
        UiAssets = new UiAssetDataService(this, sections["UI"]);
        loadAll();
    }

    internal void reportDataRead(string path)
    {
        loadCancellationToken.ThrowIfCancellationRequested();
        loadProgress?.Invoke(path);
    }

    public event EventHandler? ModifiedChanged;

    public event EventHandler? DataReloaded;

    public event EventHandler? DataRestored;

    public event EventHandler? DataSaved;

    public event EventHandler? UndoRedoStateChanged;

    public string ProjectPath { get; }

    public bool IsModified => isModified;

    public IReadOnlyList<string> InvalidLoadPaths => invalidLoadPaths;

}
