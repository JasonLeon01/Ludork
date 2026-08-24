using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;
using Ludork.Models;

namespace Ludork.Services;

public sealed class GameDataService
{
    private const int MaximumHistoryEntries = 100;
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
    private long nextHistoryGestureId;
    private long activeHistoryGestureId;
    private bool activeHistoryGestureHasSnapshot;
    private bool isModified;
    private bool generalEnumSavePending;

    public GameDataService(string projectPath)
    {
        ProjectPath = Path.GetFullPath(projectPath);
        generalEnums = new GeneralEnumService(ProjectPath);
        loadAll();
    }

    public event EventHandler? ModifiedChanged;
    public event EventHandler? DataChanged;
    public event EventHandler? DataReloaded;
    public event EventHandler? DataRestored;
    public event EventHandler? DataSaved;
    public event EventHandler? UiAssetsChanged;
    public event EventHandler? UndoRedoStateChanged;

    public string ProjectPath { get; }
    public bool IsModified => isModified;
    public bool CanUndo => undoStack.Count != 0;
    public bool CanRedo => redoStack.Count != 0;
    public IReadOnlyList<string> InvalidLoadPaths => invalidLoadPaths;
    public IReadOnlyDictionary<string, JsonObject> SystemConfigData => sections["Configs"].Data;
    public IReadOnlyDictionary<string, JsonObject> TilesetData => sections["Tilesets"].Data;
    public IReadOnlyDictionary<string, JsonObject> AutoTileData => sections["AutoTiles"].Data;
    public IReadOnlyDictionary<string, JsonObject> MapData => sections["Maps"].Data;
    public IReadOnlyDictionary<string, JsonObject> CommonFunctionsData => sections["CommonFunctions"].Data;
    public IReadOnlyDictionary<string, JsonObject> BlueprintsData => sections["Blueprints"].Data;
    public IReadOnlyDictionary<string, JsonObject> AnimationsData => sections["Animations"].Data;
    public IReadOnlyDictionary<string, JsonObject> CurvesData => sections["Curves"].Data;
    public IReadOnlyDictionary<string, JsonObject> TextConfigsData => sections["TextConfigs"].Data;
    public IReadOnlyDictionary<string, JsonObject> UiAssetsData => sections["UI"].Data
        .Where(pair => isUiDataType(pair.Value, UiAssetSchema.UiAssetType))
        .ToDictionary(pair => pair.Key, pair => pair.Value, StringComparer.Ordinal);
    public IReadOnlyDictionary<string, JsonObject> GeneralData => sections["General"].Data;

    public void Reload()
    {
        loadAll();
        DataChanged?.Invoke(this, EventArgs.Empty);
        UiAssetsChanged?.Invoke(this, EventArgs.Empty);
        DataReloaded?.Invoke(this, EventArgs.Empty);
    }

    public IReadOnlyList<string> GetModifiedBlueprintKeys()
    {
        IReadOnlyDictionary<string, JsonObject> current = sections["Blueprints"].Data;
        IReadOnlyDictionary<string, JsonObject> origin = originData["Blueprints"];
        return current.Keys
            .Where(key => !origin.TryGetValue(key, out JsonObject? originValue)
                || !current.TryGetValue(key, out JsonObject? currentValue)
                || !nodesEqual(currentValue, originValue))
            .OrderBy(key => key, StringComparer.Ordinal)
            .ToArray();
    }

    public IReadOnlyList<string> GetModifiedUiKeys()
    {
        IReadOnlyDictionary<string, JsonObject> current = sections["UI"].Data;
        IReadOnlyDictionary<string, JsonObject> origin = originData["UI"];
        return current.Keys
            .Union(origin.Keys, StringComparer.Ordinal)
            .Where(key => !origin.TryGetValue(key, out JsonObject? originValue)
                || !current.TryGetValue(key, out JsonObject? currentValue)
                || !nodesEqual(currentValue, originValue))
            .OrderBy(key => key, StringComparer.Ordinal)
            .ToArray();
    }

    public IReadOnlyList<string> GetUiAssetKeysForMove()
    {
        return collectUiAssetKeysAcrossHistory()
            .OrderBy(key => key, StringComparer.Ordinal)
            .ToArray();
    }

    public bool CreateUiAsset(string key, JsonObject? asset = null)
    {
        string normalizedKey = UiAssetSchema.NormalizeAssetKey(key);
        string dataKey = UiAssetSchema.ToAssetDataKey(normalizedKey);
        Dictionary<string, JsonObject> data = sections["UI"].Data;
        if (dataKey.Length == 0 || data.ContainsKey(dataKey))
            return false;
        JsonObject value;
        if (asset is null)
        {
            (int width, int height) = getGameSize();
            value = UiAssetSchema.CreateDefaultAsset(normalizedKey, width, height);
        }
        else
        {
            value = (JsonObject)asset.DeepClone();
            value["type"] = UiAssetSchema.UiAssetType;
        }
        RecordSnapshot();
        data[dataKey] = value;
        refreshModifiedState();
        UiAssetsChanged?.Invoke(this, EventArgs.Empty);
        return true;
    }

    public bool UpdateUiAsset(string key, JsonObject asset)
    {
        string normalizedKey = UiAssetSchema.NormalizeAssetKey(key);
        string dataKey = UiAssetSchema.ToAssetDataKey(normalizedKey);
        Dictionary<string, JsonObject> data = sections["UI"].Data;
        if (!data.TryGetValue(dataKey, out JsonObject? current)
            || !isUiDataType(current, UiAssetSchema.UiAssetType))
        {
            return false;
        }
        JsonObject value = (JsonObject)asset.DeepClone();
        value["type"] = UiAssetSchema.UiAssetType;
        if (JsonNode.DeepEquals(current, value))
            return false;
        RecordSnapshot();
        data[dataKey] = value;
        refreshModifiedState();
        UiAssetsChanged?.Invoke(this, EventArgs.Empty);
        return true;
    }

    public bool RenameUiAsset(string oldKey, string newKey)
    {
        string normalizedOldKey = UiAssetSchema.NormalizeAssetKey(oldKey);
        string normalizedNewKey = UiAssetSchema.NormalizeAssetKey(newKey);
        string oldDataKey = UiAssetSchema.ToAssetDataKey(normalizedOldKey);
        string newDataKey = UiAssetSchema.ToAssetDataKey(normalizedNewKey);
        if (normalizedOldKey.Length == 0
            || normalizedNewKey.Length == 0
            || !sections["UI"].Data.TryGetValue(oldDataKey, out JsonObject? source)
            || !isUiDataType(source, UiAssetSchema.UiAssetType))
        {
            return false;
        }
        return renameDataEntry("UI", oldDataKey, newDataKey);
    }

    public bool DeleteUiAsset(string key)
    {
        string normalizedKey = UiAssetSchema.NormalizeAssetKey(key);
        string dataKey = UiAssetSchema.ToAssetDataKey(normalizedKey);
        return normalizedKey.Length != 0
            && sections["UI"].Data.TryGetValue(dataKey, out JsonObject? source)
            && isUiDataType(source, UiAssetSchema.UiAssetType)
            && deleteDataEntry("UI", dataKey);
    }

    public string? CopyUiAsset(string key)
    {
        string normalizedKey = UiAssetSchema.NormalizeAssetKey(key);
        string dataKey = UiAssetSchema.ToAssetDataKey(normalizedKey);
        Dictionary<string, JsonObject> data = sections["UI"].Data;
        if (!data.TryGetValue(dataKey, out JsonObject? source)
            || !isUiDataType(source, UiAssetSchema.UiAssetType))
        {
            return null;
        }
        string copyKey = normalizedKey + " (copy)";
        if (data.ContainsKey(UiAssetSchema.ToAssetDataKey(copyKey)))
        {
            int index = 1;
            while (data.ContainsKey(UiAssetSchema.ToAssetDataKey($"{copyKey}_{index}")))
                index += 1;
            copyKey = $"{copyKey}_{index}";
        }
        return CopyUiAsset(normalizedKey, copyKey) ? copyKey : null;
    }

    public bool CopyUiAsset(string sourceKey, string destinationKey)
    {
        string normalizedSourceKey = UiAssetSchema.NormalizeAssetKey(sourceKey);
        string normalizedDestinationKey = UiAssetSchema.NormalizeAssetKey(destinationKey);
        string sourceDataKey = UiAssetSchema.ToAssetDataKey(normalizedSourceKey);
        string destinationDataKey = UiAssetSchema.ToAssetDataKey(normalizedDestinationKey);
        Dictionary<string, JsonObject> data = sections["UI"].Data;
        if (normalizedSourceKey.Length == 0
            || normalizedDestinationKey.Length == 0
            || data.ContainsKey(destinationDataKey)
            || !data.TryGetValue(sourceDataKey, out JsonObject? source)
            || !isUiDataType(source, UiAssetSchema.UiAssetType))
        {
            return false;
        }
        JsonObject copy = UiAssetSchema.CloneForCopy(
            source,
            Path.GetFileName(normalizedDestinationKey));
        return CreateUiAsset(normalizedDestinationKey, copy);
    }

    public bool CreateBlueprint(string key, JsonObject blueprint)
    {
        string normalizedKey = normalizeJsonKey(key);
        Dictionary<string, JsonObject> data = sections["Blueprints"].Data;
        if (normalizedKey.Length == 0 || data.ContainsKey(normalizedKey))
            return false;
        RecordSnapshot();
        data[normalizedKey] = (JsonObject)blueprint.DeepClone();
        refreshModifiedState();
        return true;
    }

    public bool RenameBlueprint(string oldKey, string newKey)
    {
        return renameDataEntry("Blueprints", oldKey, newKey);
    }

    public bool UpdateBlueprint(string key, JsonObject blueprint)
    {
        string normalizedKey = normalizeJsonKey(key);
        Dictionary<string, JsonObject> data = sections["Blueprints"].Data;
        if (!data.TryGetValue(normalizedKey, out JsonObject? current))
            return false;
        JsonObject value = (JsonObject)blueprint.DeepClone();
        value.Remove("type");
        if (JsonNode.DeepEquals(current, value))
            return false;
        RecordSnapshot();
        data[normalizedKey] = value;
        refreshModifiedState();
        return true;
    }

    public bool DeleteBlueprint(string key)
    {
        return deleteDataEntry("Blueprints", key);
    }

    public bool CreateCommonFunction(string name, JsonObject? commonFunction = null)
    {
        string key = normalizeJsonKey(name);
        Dictionary<string, JsonObject> data = sections["CommonFunctions"].Data;
        if (key.Length == 0 || data.ContainsKey(key))
            return false;
        JsonObject value = commonFunction is null
            ? new JsonObject
            {
                ["parent"] = null,
                ["nodeGraph"] = new JsonObject
                {
                    ["common"] = new JsonObject
                    {
                        ["nodes"] = new JsonArray(),
                        ["links"] = new JsonArray(),
                    },
                },
                ["startNodes"] = new JsonObject(),
            }
            : (JsonObject)commonFunction.DeepClone();
        RecordSnapshot();
        data[key] = value;
        refreshModifiedState();
        return true;
    }

    public bool UpdateCommonFunction(string name, JsonObject commonFunction)
    {
        string key = normalizeJsonKey(name);
        Dictionary<string, JsonObject> data = sections["CommonFunctions"].Data;
        if (!data.TryGetValue(key, out JsonObject? current)
            || JsonNode.DeepEquals(current, commonFunction))
        {
            return false;
        }
        RecordSnapshot();
        data[key] = (JsonObject)commonFunction.DeepClone();
        refreshModifiedState();
        return true;
    }

    public bool RenameCommonFunction(string oldName, string newName)
    {
        return renameDataEntry("CommonFunctions", oldName, newName);
    }

    public bool DeleteCommonFunction(string name)
    {
        return deleteDataEntry("CommonFunctions", name);
    }

    public string? CopyCommonFunction(string name)
    {
        string key = normalizeJsonKey(name);
        if (!sections["CommonFunctions"].Data.TryGetValue(key, out JsonObject? source))
            return null;
        string copyName = key + " (copy)";
        if (sections["CommonFunctions"].Data.ContainsKey(copyName))
        {
            int index = 1;
            while (sections["CommonFunctions"].Data.ContainsKey($"{copyName}_{index}"))
                index += 1;
            copyName = $"{copyName}_{index}";
        }
        return CreateCommonFunction(copyName, source) ? copyName : null;
    }

    public bool RemoveDataPaths(IEnumerable<string> paths)
    {
        string[] deletedPaths = paths.ToArray();
        bool changed = deletedPaths.Any(isDataPath);
        ApplyExternalFileChanges([], [], deletedPaths);
        return changed;
    }

    public bool RenameDataPath(string oldPath, string newPath)
    {
        if (!isDataPath(oldPath) || !isDataPath(newPath))
            return false;
        ApplyExternalFileChanges([], [(oldPath, newPath)], []);
        return true;
    }

    public void ApplyExternalFileChanges(
        IReadOnlyList<string> addedPaths,
        IReadOnlyList<(string OldPath, string NewPath)> movedPaths,
        IReadOnlyList<string> deletedPaths)
    {
        Dictionary<string, string> uiAssetMoves = createUiAssetMoveMap(movedPaths);
        Dictionary<string, Dictionary<string, JsonObject>> currentBefore = cloneAllData();
        Dictionary<string, Dictionary<string, JsonObject>> originBefore = cloneData(originData);
        IReadOnlyList<Dictionary<string, Dictionary<string, JsonObject>>> undoBefore = cloneHistory(undoStack);
        IReadOnlyList<Dictionary<string, Dictionary<string, JsonObject>>> redoBefore = cloneHistory(redoStack);
        Dictionary<string, string> uiFilesBefore =
            new Dictionary<string, string>(StringComparer.Ordinal);
        try
        {
            bool changed = false;
            foreach ((string oldPath, string newPath) in movedPaths)
                changed |= applyExternalMove(oldPath, newPath);
            remapUiAssetReferences(uiAssetMoves);
            if (uiAssetMoves.Count != 0)
            {
                writeUiDiskAssetReferences(uiAssetMoves, uiFilesBefore);
                changed = true;
            }
            foreach (string deletedPath in deletedPaths)
                changed |= applyExternalDelete(deletedPath);
            foreach (string addedPath in addedPaths)
                changed |= applyExternalAdd(addedPath);
            if (!changed)
                return;
            refreshModifiedState();
            if (!sectionEqual(currentBefore["UI"], sections["UI"].Data))
                UiAssetsChanged?.Invoke(this, EventArgs.Empty);
            UndoRedoStateChanged?.Invoke(this, EventArgs.Empty);
        }
        catch (Exception exception)
        {
            originData = cloneData(originBefore);
            restoreHistory(undoStack, undoBefore);
            restoreHistory(redoStack, redoBefore);
            restoreSnapshot(currentBefore, false);
            if (uiFilesBefore.Count != 0)
            {
                try
                {
                    restoreUiOriginAssets(uiFilesBefore);
                }
                catch (IOException restoreException)
                {
                    throw new IOException(
                        exception.Message + Environment.NewLine + restoreException.Message,
                        new AggregateException(exception, restoreException));
                }
            }
            throw;
        }
    }

    public bool ContainsDataPath(string absolutePath, bool directory)
    {
        if (!tryGetDataLocation(absolutePath, out string sectionName, out string relativePath))
            return false;
        if (!directory)
        {
            if (!hasDataFileExtension(sectionName, absolutePath))
                return false;
            string key = Path.ChangeExtension(relativePath, null)!.Replace('\\', '/');
            return sections[sectionName].Data.ContainsKey(key);
        }
        string prefix = normalizeDataKey(relativePath);
        return sections[sectionName].Data.Keys.Any(key => keyMatchesPrefix(key, prefix));
    }

    public DataFileInfo? TryLoadDataFile(string absolutePath)
    {
        if (!File.Exists(absolutePath)
            || DataConfig.isAnimationCache(absolutePath)
            || !string.Equals(Path.GetExtension(absolutePath), DataConfig.DataFileExtension, StringComparison.OrdinalIgnoreCase))
            return null;
        bool dataFile = tryGetDataLocation(
            absolutePath,
            out string sectionName,
            out _);
        DataSection? section = dataFile ? sections[sectionName] : null;
        bool textConfigFile = dataFile && sectionName == "TextConfigs";
        bool uiFile = dataFile && sectionName == "UI";
        if (uiFile && !hasDataFileExtension(sectionName, absolutePath))
            return new DataFileInfo("invalidUiData", getDataKey(absolutePath));
        try
        {
            if (JsonNode.Parse(File.ReadAllText(absolutePath)) is not JsonObject file)
            {
                return textConfigFile
                    ? new DataFileInfo("invalidTextConfig", getDataKey(absolutePath))
                    : uiFile
                        ? new DataFileInfo("invalidUiData", getDataKey(absolutePath))
                    : null;
            }
            string? type = file["type"] is JsonValue typeValue
                && typeValue.TryGetValue<string>(out string? parsedType)
                    ? parsedType
                    : null;
            if (section is not null && !section.AcceptsType(type))
            {
                return textConfigFile
                    ? new DataFileInfo("invalidTextConfig", getDataKey(absolutePath))
                    : uiFile
                        ? new DataFileInfo("invalidUiData", getDataKey(absolutePath))
                    : null;
            }
            string? resolvedType = string.IsNullOrWhiteSpace(type)
                ? section?.ExpectedType
                : type;
            if (string.IsNullOrWhiteSpace(resolvedType)
                && sectionName != "General")
                return null;
            return new DataFileInfo(resolvedType ?? "general", getDataKey(absolutePath));
        }
        catch (JsonException)
        {
            return textConfigFile
                ? new DataFileInfo("invalidTextConfig", getDataKey(absolutePath))
                : uiFile
                    ? new DataFileInfo("invalidUiData", getDataKey(absolutePath))
                : null;
        }
    }

    public JsonObject? getMap(string key)
    {
        return sections["Maps"].Data.TryGetValue(key, out JsonObject? value) ? value : null;
    }

    public MapInfo? getMapInfo(string key)
    {
        if (getMap(key) is not JsonObject map)
            return null;
        JsonArray ambientLight = map["ambientLight"] is JsonArray values && values.Count >= 4
            ? (JsonArray)values.DeepClone()
            : new JsonArray(255, 255, 255, 255);
        return new MapInfo
        {
            FileName = key + ".json",
            MapName = map["mapName"]?.GetValue<string>() ?? key,
            Width = Math.Max(1, map["width"]?.GetValue<int?>() ?? 13),
            Height = Math.Max(1, map["height"]?.GetValue<int?>() ?? 13),
            AmbientLight = ambientLight,
            Bgm = map["bgm"]?.GetValue<string>() ?? string.Empty,
            BgmFilter = cloneObject(map["bgmFilter"]),
            Bgs = map["bgs"]?.GetValue<string>() ?? string.Empty,
            BgsFilter = cloneObject(map["bgsFilter"]),
            Fog = map["fog"]?.GetValue<string>() ?? string.Empty,
            FogPower = map["fogPower"]?.GetValue<int?>() ?? 0,
            FogOx = map["fogOx"]?.GetValue<double?>() ?? 0.0,
            FogOy = map["fogOy"]?.GetValue<double?>() ?? 0.0,
            FogDistort = map["fogDistort"]?.GetValue<int?>() ?? 0,
        };
    }

    public string getMapDisplayName(string key)
    {
        string? mapName = getMap(key)?["mapName"]?.GetValue<string>();
        return string.IsNullOrWhiteSpace(mapName) ? key : mapName;
    }

    public string getNewMapFileName()
    {
        for (int index = 1; ; index += 1)
        {
            string key = $"Map_{index:D2}";
            if (!MapData.ContainsKey(key))
                return key + ".json";
        }
    }

    public bool CreateMap(string key, string mapName, int width, int height)
    {
        return CreateMap(new MapInfo
        {
            FileName = key,
            MapName = mapName,
            Width = width,
            Height = height,
        });
    }

    public bool CreateMap(MapInfo info)
    {
        if (info is null)
            return false;
        string key = normaliseMapKey(info.FileName);
        if (string.IsNullOrWhiteSpace(key) || MapData.ContainsKey(key) || !isValidMapSize(info.Width, info.Height)
            || TilesetData.Keys.FirstOrDefault() is not { } tilesetKey)
            return false;
        RecordSnapshot();
        JsonObject map = new JsonObject
        {
            ["mapName"] = string.IsNullOrWhiteSpace(info.MapName) ? LocaleService.Get("NEW_MAP_DEFAULT_NAME") : info.MapName.Trim(),
            ["width"] = info.Width,
            ["height"] = info.Height,
            ["ambientLight"] = normaliseAmbientLight(info.AmbientLight),
            ["bgm"] = info.Bgm.Trim(),
            ["bgmFilter"] = cloneObject(info.BgmFilter),
            ["bgs"] = info.Bgs.Trim(),
            ["bgsFilter"] = cloneObject(info.BgsFilter),
            ["fog"] = info.Fog.Trim(),
            ["fogPower"] = string.IsNullOrWhiteSpace(info.Fog) ? 0 : info.FogPower,
            ["fogOx"] = string.IsNullOrWhiteSpace(info.Fog) ? 0.0 : info.FogOx,
            ["fogOy"] = string.IsNullOrWhiteSpace(info.Fog) ? 0.0 : info.FogOy,
            ["fogDistort"] = string.IsNullOrWhiteSpace(info.Fog) ? 0 : info.FogDistort,
            ["layerOrder"] = new JsonArray("floor", "default"),
            ["layers"] = new JsonObject
            {
                ["floor"] = createEmptyLayer("floor", tilesetKey, info.Width, info.Height),
                ["default"] = createEmptyLayer("default", tilesetKey, info.Width, info.Height),
            },
            ["actors"] = new JsonObject
            {
                ["floor"] = new JsonArray(),
                ["default"] = new JsonArray(),
            },
        };
        sections["Maps"].Data[key] = map;
        refreshModifiedState();
        return true;
    }

    public bool UpdateMap(string currentKey, MapInfo info)
    {
        if (info is null || getMap(currentKey) is not JsonObject map)
            return false;
        string newKey = normaliseMapKey(info.FileName);
        if (string.IsNullOrWhiteSpace(newKey) || !isValidMapSize(info.Width, info.Height)
            || (newKey != currentKey && MapData.ContainsKey(newKey)))
            return false;

        RecordSnapshot();
        int oldWidth = map["width"]?.GetValue<int?>() ?? 0;
        int oldHeight = map["height"]?.GetValue<int?>() ?? 0;
        if (oldWidth != info.Width || oldHeight != info.Height)
            resizeMapLayers(map, info.Width, info.Height);
        if (!string.IsNullOrWhiteSpace(info.MapName))
            map["mapName"] = info.MapName.Trim();
        map["width"] = info.Width;
        map["height"] = info.Height;
        map["ambientLight"] = normaliseAmbientLight(info.AmbientLight);
        map["bgm"] = info.Bgm.Trim();
        map["bgmFilter"] = cloneObject(info.BgmFilter);
        map["bgs"] = info.Bgs.Trim();
        map["bgsFilter"] = cloneObject(info.BgsFilter);
        map["fog"] = info.Fog.Trim();
        map["fogPower"] = string.IsNullOrWhiteSpace(info.Fog) ? 0 : info.FogPower;
        map["fogOx"] = string.IsNullOrWhiteSpace(info.Fog) ? 0.0 : info.FogOx;
        map["fogOy"] = string.IsNullOrWhiteSpace(info.Fog) ? 0.0 : info.FogOy;
        map["fogDistort"] = string.IsNullOrWhiteSpace(info.Fog) ? 0 : info.FogDistort;
        if (newKey != currentKey)
            renameMapKey(currentKey, newKey, map);
        refreshModifiedState();
        return true;
    }

    public string? CopyMap(string key)
    {
        if (getMap(key) is not JsonObject source)
            return null;
        return PasteMap(source, key);
    }

    public string? PasteMap(JsonObject source, string sourceKey)
    {
        if (source is null || string.IsNullOrWhiteSpace(sourceKey))
            return null;
        string baseKey = sourceKey + " (copy)";
        string copyKey = baseKey;
        for (int index = 1; MapData.ContainsKey(copyKey); index += 1)
            copyKey = $"{baseKey} ({index})";
        JsonObject copy = (JsonObject)source.DeepClone();
        string mapName = copy["mapName"]?.GetValue<string>() ?? sourceKey;
        copy["mapName"] = mapName + " (copy)";
        RecordSnapshot();
        sections["Maps"].Data[copyKey] = copy;
        refreshModifiedState();
        return copyKey;
    }

    public bool DeleteMap(string key)
    {
        if (!sections["Maps"].Data.ContainsKey(key))
            return false;
        RecordSnapshot();
        sections["Maps"].Data.Remove(key);
        refreshModifiedState();
        return true;
    }

    public bool CreateAnimation(string key, string name)
    {
        key = normalizeDataKey(key);
        if (string.IsNullOrWhiteSpace(key) || key.EndsWith(".anim", StringComparison.OrdinalIgnoreCase)
            || sections["Animations"].Data.ContainsKey(key))
            return false;
        RecordSnapshot();
        sections["Animations"].Data[key] = new JsonObject
        {
            ["name"] = name,
            ["frameRate"] = 30,
            ["assets"] = new JsonArray(),
            ["timeLines"] = new JsonArray(),
            ["timeTags"] = new JsonArray(),
        };
        refreshModifiedState();
        return true;
    }

    public bool UpdateAnimation(string key, JsonObject animation)
    {
        key = normalizeDataKey(key);
        if (string.IsNullOrWhiteSpace(key) || !sections["Animations"].Data.ContainsKey(key))
            return false;
        JsonObject copy = (JsonObject)animation.DeepClone();
        copy.Remove("type");
        if (nodesEqual(sections["Animations"].Data[key], copy))
            return false;
        RecordSnapshot();
        sections["Animations"].Data[key] = copy;
        refreshModifiedState();
        return true;
    }

    public bool CreateCurve(string key, string name, string type = "curve")
    {
        key = normalizeDataKey(key);
        if (string.IsNullOrWhiteSpace(key)
            || !isCurveType(type)
            || sections["Curves"].Data.ContainsKey(key))
        {
            return false;
        }
        int componentCount = curveComponentCount(type);
        RecordSnapshot();
        sections["Curves"].Data[key] = new JsonObject
        {
            ["type"] = type,
            ["name"] = name,
            ["defaultValue"] = createCurveValue(componentCount, 0.0),
            ["preInfinity"] = "constant",
            ["postInfinity"] = "constant",
            ["keys"] = new JsonArray
            {
                createCurveKey(0.0, createCurveValue(componentCount, 0.0)),
                createCurveKey(1.0, createCurveValue(componentCount, 1.0)),
            },
        };
        refreshModifiedState();
        return true;
    }

    public bool CreateTextConfig(string key, string type, string name)
    {
        key = normalizeDataKey(key);
        if (string.IsNullOrWhiteSpace(key)
            || !isTextConfigType(type)
            || sections["TextConfigs"].Data.ContainsKey(key))
        {
            return false;
        }
        RecordSnapshot();
        sections["TextConfigs"].Data[key] = type == "plainTextConfig"
            ? createPlainTextConfig(name)
            : createRichTextConfig(name);
        refreshModifiedState();
        return true;
    }

    public bool UpdateTextConfig(string key, JsonObject textConfig)
    {
        key = normalizeDataKey(key);
        string? type = textConfig["type"]?.GetValue<string>();
        if (string.IsNullOrWhiteSpace(key)
            || !isTextConfigType(type)
            || !sections["TextConfigs"].Data.ContainsKey(key))
        {
            return false;
        }
        JsonObject copy = (JsonObject)textConfig.DeepClone();
        if (nodesEqual(sections["TextConfigs"].Data[key], copy))
            return false;
        RecordSnapshot();
        sections["TextConfigs"].Data[key] = copy;
        refreshModifiedState();
        return true;
    }

    public bool DeleteTextConfig(string key)
    {
        return DeleteTextConfigs([key]);
    }

    public bool DeleteTextConfigs(IEnumerable<string> keys)
    {
        string[] normalizedKeys = keys
            .Select(normalizeJsonKey)
            .Where(key => sections["TextConfigs"].Data.ContainsKey(key))
            .Distinct(StringComparer.Ordinal)
            .ToArray();
        if (normalizedKeys.Length == 0)
            return false;
        RecordSnapshot();
        foreach (string key in normalizedKeys)
            sections["TextConfigs"].Data.Remove(key);
        refreshModifiedState();
        return true;
    }

    public bool CreateTileset(string key)
    {
        key = normalizeDataKey(key);
        if (string.IsNullOrWhiteSpace(key) || sections["Tilesets"].Data.ContainsKey(key))
            return false;
        RecordSnapshot();
        sections["Tilesets"].Data[key] = new JsonObject
        {
            ["name"] = key,
            ["fileName"] = string.Empty,
            ["passable"] = new JsonArray(),
            ["materials"] = new JsonArray(),
            ["dir4"] = new JsonArray(),
        };
        refreshModifiedState();
        return true;
    }

    public bool CreateAutoTile(string key)
    {
        key = normalizeDataKey(key);
        if (string.IsNullOrWhiteSpace(key) || sections["AutoTiles"].Data.ContainsKey(key))
            return false;
        RecordSnapshot();
        sections["AutoTiles"].Data[key] = new JsonObject
        {
            ["name"] = key,
            ["fileName"] = string.Empty,
            ["passable"] = true,
            ["material"] = createDefaultMaterial(),
        };
        refreshModifiedState();
        return true;
    }

    public bool UpdateCurve(string key, JsonObject curve)
    {
        key = normalizeDataKey(key);
        string? type = curve["type"]?.GetValue<string>();
        if (string.IsNullOrWhiteSpace(key)
            || !isCurveType(type)
            || !sections["Curves"].Data.ContainsKey(key))
        {
            return false;
        }
        JsonObject copy = (JsonObject)curve.DeepClone();
        if (nodesEqual(sections["Curves"].Data[key], copy))
            return false;
        RecordSnapshot();
        sections["Curves"].Data[key] = copy;
        refreshModifiedState();
        return true;
    }

    public IReadOnlyList<string> getLayerNames(string mapKey)
    {
        if (getMap(mapKey) is not JsonObject map)
            return Array.Empty<string>();
        return getLayerOrder(map).Select(name => name!.GetValue<string>()).ToArray();
    }

    public string? getLayerTilesetKey(string mapKey, string layerName)
    {
        return getMap(mapKey)?["layers"]?[layerName]?["layerTileset"]?.GetValue<string>();
    }

    public bool setLayerTilesetKey(string mapKey, string layerName, string tilesetKey)
    {
        JsonObject? layer = getMap(mapKey)?["layers"]?[layerName] as JsonObject;
        if (layer is null || !TilesetData.ContainsKey(tilesetKey))
            return false;
        if (string.Equals(layer["layerTileset"]?.GetValue<string>(), tilesetKey, StringComparison.Ordinal))
            return false;
        RecordSnapshot();
        layer["layerTileset"] = tilesetKey;
        refreshModifiedState();
        return true;
    }

    public string getLayerShaderPath(string mapKey, string layerName)
    {
        return getMap(mapKey)?["layers"]?[layerName]?["shaderPath"]?.GetValue<string>() ?? string.Empty;
    }

    public bool setLayerShaderPath(string mapKey, string layerName, string shaderPath)
    {
        JsonObject? layer = getMap(mapKey)?["layers"]?[layerName] as JsonObject;
        if (layer is null)
            return false;
        string normalizedPath = (shaderPath ?? string.Empty).Replace('\\', '/').Trim('/');
        if (string.Equals(layer["shaderPath"]?.GetValue<string>() ?? string.Empty, normalizedPath, StringComparison.Ordinal))
            return false;
        RecordSnapshot();
        layer["shaderPath"] = normalizedPath;
        refreshModifiedState();
        return true;
    }

    public bool SetLayerVisible(string mapKey, string layerName, bool visible)
    {
        JsonObject? layer = getMap(mapKey)?["layers"]?[layerName] as JsonObject;
        if (layer is null)
            return false;
        bool current = layer["visible"]?.GetValue<bool?>() ?? true;
        if (current == visible)
            return false;
        RecordSnapshot();
        if (visible)
            layer.Remove("visible");
        else
            layer["visible"] = false;
        refreshModifiedState();
        return true;
    }

    public JsonObject? copyLayer(string mapKey, string layerName)
    {
        return getMap(mapKey)?["layers"]?[layerName] is JsonObject layer
            ? (JsonObject)layer.DeepClone()
            : null;
    }

    public bool addEmptyLayer(string mapKey, string layerName, string? insertAfterLayer = null)
    {
        JsonObject? map = getMap(mapKey);
        JsonObject? layers = map?["layers"] as JsonObject;
        if (layers is null || string.IsNullOrWhiteSpace(layerName)
            || layers.ContainsKey(layerName)
            || TilesetData.Keys.FirstOrDefault() is not { } tilesetKey)
            return false;
        int width = map?["width"]?.GetValue<int?>() ?? 0;
        int height = map?["height"]?.GetValue<int?>() ?? 0;
        if (width <= 0 || height <= 0)
            return false;
        return insertLayer(mapKey, layerName, createEmptyLayer(layerName, tilesetKey, width, height), insertAfterLayer);
    }

    public bool pasteLayer(string mapKey, string layerName, JsonObject layer, string? insertAfterLayer)
    {
        if (string.IsNullOrWhiteSpace(layerName))
            return false;
        JsonObject copy = (JsonObject)layer.DeepClone();
        copy["layerName"] = layerName;
        return insertLayer(mapKey, layerName, copy, insertAfterLayer);
    }

    public bool renameLayer(string mapKey, string oldName, string newName)
    {
        JsonObject? map = getMap(mapKey);
        JsonObject? layers = map?["layers"] as JsonObject;
        JsonObject? actorGroups = map?["actors"] as JsonObject;
        if (map is null || layers is null || string.IsNullOrWhiteSpace(newName)
            || !layers.ContainsKey(oldName)
            || layers.ContainsKey(newName)
            || actorGroups?.ContainsKey(newName) == true)
            return false;
        RecordSnapshot();
        JsonArray layerOrder = getLayerOrder(map);
        int layerOrderIndex = layerOrder.IndexOf(oldName);
        if (layerOrderIndex < 0)
            throw new InvalidDataException($"Map layerOrder does not contain '{oldName}'.");
        layerOrder[layerOrderIndex] = newName;
        List<KeyValuePair<string, JsonNode?>> entries = layers.Select(entry => new KeyValuePair<string, JsonNode?>(entry.Key, entry.Value)).ToList();
        layers.Clear();
        foreach (KeyValuePair<string, JsonNode?> entry in entries)
        {
            if (entry.Key == oldName && entry.Value is JsonObject layer)
            {
                layer["layerName"] = newName;
                layers.Add(newName, layer);
            }
            else
                layers.Add(entry.Key, entry.Value);
        }
        if (actorGroups is not null && actorGroups.ContainsKey(oldName))
        {
            List<KeyValuePair<string, JsonNode?>> actorEntries = actorGroups
                .Select(entry => new KeyValuePair<string, JsonNode?>(entry.Key, entry.Value))
                .ToList();
            actorGroups.Clear();
            foreach (KeyValuePair<string, JsonNode?> entry in actorEntries)
                actorGroups.Add(entry.Key == oldName ? newName : entry.Key, entry.Value);
        }
        refreshModifiedState();
        return true;
    }

    public bool removeLayer(string mapKey, string layerName)
    {
        if (getMap(mapKey) is not JsonObject map || map["layers"] is not JsonObject layers || !layers.ContainsKey(layerName))
            return false;
        RecordSnapshot();
        layers.Remove(layerName);
        if (map["actors"] is JsonObject actorGroups)
            actorGroups.Remove(layerName);
        JsonArray layerOrder = getLayerOrder(map);
        if (!layerOrder.Remove(layerName))
            throw new InvalidDataException($"Map layerOrder does not contain '{layerName}'.");
        refreshModifiedState();
        return true;
    }

    public int getCellSize()
    {
        int? value = SystemConfigData.TryGetValue("System", out JsonObject? system)
            ? system["cellSize"]?["value"]?.GetValue<int?>()
            : null;
        return value is > 0 ? value.Value : 32;
    }

    public (int Width, int Height) getGameSize()
    {
        if (!SystemConfigData.TryGetValue("System", out JsonObject? system)
            || system["gameSize"]?["value"] is not JsonArray values
            || values.Count < 2)
        {
            return (640, 480);
        }
        int width = values[0]?.GetValue<int?>() ?? 640;
        int height = values[1]?.GetValue<int?>() ?? 480;
        return (width > 0 ? width : 640, height > 0 ? height : 480);
    }

    public string getGameTitle()
    {
        if (!SystemConfigData.TryGetValue("System", out JsonObject? system))
            return "Ludork";
        string? title = system["title"]?["value"]?.GetValue<string>();
        return string.IsNullOrWhiteSpace(title) ? "Ludork" : $"Ludork - {title.Trim()}";
    }

    public bool reorderLayers(string mapKey, string movingLayer, string targetLayer)
    {
        if (movingLayer == targetLayer || getMap(mapKey) is not JsonObject map || map["layers"] is not JsonObject layers)
            return false;
        JsonArray layerOrder = getLayerOrder(map);
        int movingIndex = layerOrder.IndexOf(movingLayer);
        int targetIndex = layerOrder.IndexOf(targetLayer);
        if (movingIndex < 0 || targetIndex < 0)
            return false;
        RecordSnapshot();
        JsonNode moving = layerOrder[movingIndex]!.DeepClone();
        layerOrder.RemoveAt(movingIndex);
        layerOrder.Insert(Math.Min(targetIndex, layerOrder.Count), moving);
        List<KeyValuePair<string, JsonNode?>> entries = layerOrder
            .Select(name => new KeyValuePair<string, JsonNode?>(name!.GetValue<string>(), layers[name.GetValue<string>()]))
            .ToList();
        layers.Clear();
        foreach (KeyValuePair<string, JsonNode?> entry in entries)
            layers.Add(entry.Key, entry.Value);
        refreshModifiedState();
        return true;
    }

    public bool CreateGeneralType(string key, string? linkedType = null)
    {
        if (string.IsNullOrWhiteSpace(key) || sections["General"].Data.ContainsKey(key))
            return false;
        RecordSnapshot();
        JsonObject entry = new()
        {
            ["params"] = new JsonObject(),
            ["members"] = new JsonObject(),
        };
        if (!string.IsNullOrWhiteSpace(linkedType))
            entry["linkedType"] = linkedType;
        sections["General"].Data[key] = entry;
        refreshModifiedState();
        return true;
    }

    public bool RenameGeneralType(string oldKey, string newKey)
    {
        Dictionary<string, JsonObject> data = sections["General"].Data;
        if (string.IsNullOrWhiteSpace(newKey) || !data.ContainsKey(oldKey) || data.ContainsKey(newKey))
            return false;
        RecordSnapshot();
        List<KeyValuePair<string, JsonObject>> entries = data.Select(e => e).ToList();
        data.Clear();
        foreach (KeyValuePair<string, JsonObject> entry in entries)
            data.Add(entry.Key == oldKey ? newKey : entry.Key, entry.Value);
        refreshModifiedState();
        return true;
    }

    public bool DeleteGeneralType(string key)
    {
        Dictionary<string, JsonObject> data = sections["General"].Data;
        if (!data.ContainsKey(key))
            return false;
        RecordSnapshot();
        data.Remove(key);
        refreshModifiedState();
        return true;
    }

    public SaveResult SaveAllModified()
    {
        BreakHistoryGesture();
        List<string> added = [];
        List<string> updated = [];
        List<string> deleted = [];
        List<string> failed = [];
        bool generalSaveFailed = false;
        foreach (KeyValuePair<string, DataSection> pair in sections)
        {
            Dictionary<string, JsonObject> originSection = originData[pair.Key];
            foreach (KeyValuePair<string, JsonObject> dataPair in pair.Value.Data)
            {
                if (originSection.TryGetValue(dataPair.Key, out JsonObject? origin) && nodesEqual(dataPair.Value, origin))
                    continue;
                try
                {
                    JsonObject payload = (JsonObject)dataPair.Value.DeepClone();
                    if (pair.Value.WriteType && pair.Value.ExpectedType is not null)
                        payload["type"] = pair.Value.ExpectedType;
                    string path = Path.Combine(ProjectPath, "Data", pair.Key, dataPair.Key.Replace('/', Path.DirectorySeparatorChar) + ".json");
                    Directory.CreateDirectory(Path.GetDirectoryName(path)!);
                    File.WriteAllText(path, payload.ToJsonString(WriteOptions) + Environment.NewLine);
                    if (origin is null)
                        added.Add(dataPair.Key);
                    else
                        updated.Add(dataPair.Key);
                    originSection[dataPair.Key] = (JsonObject)dataPair.Value.DeepClone();
                }
                catch (Exception)
                {
                    failed.Add(dataPair.Key);
                    if (pair.Key == "General")
                        generalSaveFailed = true;
                }
            }
            foreach (string removedKey in originSection.Keys.Except(pair.Value.Data.Keys, StringComparer.Ordinal).ToArray())
            {
                try
                {
                    string path = Path.Combine(ProjectPath, "Data", pair.Key, removedKey.Replace('/', Path.DirectorySeparatorChar) + ".json");
                    if (File.Exists(path))
                        File.Delete(path);
                    deleted.Add(removedKey);
                    originSection.Remove(removedKey);
                }
                catch (Exception)
                {
                    failed.Add(removedKey);
                    if (pair.Key == "General")
                        generalSaveFailed = true;
                }
            }
        }
        if (!generalSaveFailed)
        {
            GeneralEnumSaveResult generalEnumResult = generalEnums.Save(sections["General"].Data);
            generalEnumSavePending = !generalEnumResult.Success;
            if (!generalEnumResult.Success)
                failed.Add("GeneralEnum (" + generalEnumResult.Detail + ")");
            else if (generalEnumResult.Changed)
            {
                updated.Add(GeneralEnumService.RuntimeRelativePath);
                updated.Add(GeneralEnumService.StubRelativePath);
            }
        }
        refreshModifiedState();
        DataSaved?.Invoke(this, EventArgs.Empty);
        return new SaveResult(failed.Count == 0, formatSaveDetails(added, updated, deleted, failed));
    }

    public SaveResult ReplaceMapLayerAndSave(
        string mapKey,
        string layerName,
        JsonArray tiles,
        JsonArray autoTiles)
    {
        BreakHistoryGesture();
        Dictionary<string, JsonObject> maps = sections["Maps"].Data;
        if (!maps.TryGetValue(mapKey, out JsonObject? current)
            || current["layers"]?[layerName] is not JsonObject)
        {
            return new SaveResult(false, "The map or layer no longer exists.");
        }

        JsonObject candidate = (JsonObject)current.DeepClone();
        if (candidate["layers"]?[layerName] is not JsonObject candidateLayer)
            return new SaveResult(false, "The map layer could not be copied.");
        candidateLayer["tiles"] = tiles.DeepClone();
        candidateLayer["autoTiles"] = autoTiles.DeepClone();

        JsonObject payload = (JsonObject)candidate.DeepClone();
        payload["type"] = "map";
        string mapsDirectory = Path.GetFullPath(Path.Combine(
            ProjectPath,
            "Data",
            "Maps"));
        string path = Path.GetFullPath(Path.Combine(
            mapsDirectory,
            mapKey.Replace('/', Path.DirectorySeparatorChar) + ".json"));
        StringComparison pathComparison = OperatingSystem.IsWindows()
            ? StringComparison.OrdinalIgnoreCase
            : StringComparison.Ordinal;
        if (!path.StartsWith(
            Path.TrimEndingDirectorySeparator(mapsDirectory)
                + Path.DirectorySeparatorChar,
            pathComparison))
        {
            return new SaveResult(false, "The map path is outside the project Maps directory.");
        }
        string directory = Path.GetDirectoryName(path)!;
        string temporaryPath = Path.Combine(
            directory,
            $".{Path.GetFileName(path)}.{Guid.NewGuid():N}.tmp");
        try
        {
            Directory.CreateDirectory(directory);
            using (FileStream stream = new(
                temporaryPath,
                FileMode.CreateNew,
                FileAccess.Write,
                FileShare.None))
            {
                using StreamWriter writer = new(
                    stream,
                    new UTF8Encoding(false),
                    1024,
                    true);
                writer.Write(payload.ToJsonString(WriteOptions));
                writer.Write(Environment.NewLine);
                writer.Flush();
                stream.Flush(true);
            }
            File.Move(temporaryPath, path, true);
        }
        catch (Exception exception) when (
            exception is IOException
            or UnauthorizedAccessException
            or System.Security.SecurityException
            or JsonException
            or NotSupportedException)
        {
            deleteTemporaryFile(temporaryPath);
            return new SaveResult(false, exception.Message);
        }

        RecordSnapshot();
        maps[mapKey] = candidate;
        originData["Maps"][mapKey] = (JsonObject)candidate.DeepClone();
        refreshModifiedState();
        DataSaved?.Invoke(this, EventArgs.Empty);
        return new SaveResult(true, string.Empty);
    }

    public void refreshModifiedState()
    {
        DataChanged?.Invoke(this, EventArgs.Empty);
        bool modified = generalEnumSavePending
            || sections.Any(section => !sectionEqual(section.Value.Data, originData[section.Key]));
        if (modified == isModified)
            return;
        isModified = modified;
        ModifiedChanged?.Invoke(this, EventArgs.Empty);
    }

    public void RecordSnapshot()
    {
        if (activeHistoryGestureId != 0 && activeHistoryGestureHasSnapshot)
            return;
        pushHistory(undoStack, cloneAllData());
        if (activeHistoryGestureId != 0)
            activeHistoryGestureHasSnapshot = true;
        redoStack.Clear();
        UndoRedoStateChanged?.Invoke(this, EventArgs.Empty);
    }

    public long BeginHistoryGesture()
    {
        clearHistoryGesture();
        nextHistoryGestureId += 1;
        if (nextHistoryGestureId == 0)
            nextHistoryGestureId = 1;
        activeHistoryGestureId = nextHistoryGestureId;
        return activeHistoryGestureId;
    }

    public void EndHistoryGesture(long gestureId)
    {
        if (gestureId == 0 || gestureId != activeHistoryGestureId)
            return;
        clearHistoryGesture();
    }

    public void BreakHistoryGesture()
    {
        activeHistoryGestureHasSnapshot = false;
    }

    internal bool IsHistoryGestureActive(long gestureId)
    {
        return gestureId != 0 && gestureId == activeHistoryGestureId;
    }

    public IReadOnlyList<string> Undo()
    {
        BreakHistoryGesture();
        if (undoStack.Count == 0)
            return Array.Empty<string>();
        Dictionary<string, Dictionary<string, JsonObject>> current = cloneAllData();
        Dictionary<string, Dictionary<string, JsonObject>> snapshot = undoStack.Pop();
        pushHistory(redoStack, current);
        IReadOnlyList<string> differences = getDiff(current, snapshot);
        restoreSnapshot(snapshot);
        return differences;
    }

    public IReadOnlyList<string> Redo()
    {
        BreakHistoryGesture();
        if (redoStack.Count == 0)
            return Array.Empty<string>();
        Dictionary<string, Dictionary<string, JsonObject>> current = cloneAllData();
        Dictionary<string, Dictionary<string, JsonObject>> snapshot = redoStack.Pop();
        pushHistory(undoStack, current);
        IReadOnlyList<string> differences = getDiff(current, snapshot);
        restoreSnapshot(snapshot);
        return differences;
    }

    private void loadAll()
    {
        invalidLoadPaths.Clear();
        foreach (KeyValuePair<string, DataSection> pair in sections)
        {
            pair.Value.Data.Clear();
            string root = Path.Combine(ProjectPath, "Data", pair.Key);
            if (!Directory.Exists(root))
                continue;
            foreach (string path in Directory.EnumerateFiles(root, "*.json", SearchOption.AllDirectories)
                         .Where(path => !DataConfig.isAnimationCache(path))
                         .OrderBy(path => path, StringComparer.Ordinal))
            {
                if (pair.Key == "UI" && !hasDataFileExtension(pair.Key, path))
                {
                    invalidLoadPaths.Add(Path.GetRelativePath(ProjectPath, path));
                    continue;
                }
                try
                {
                    if (JsonNode.Parse(File.ReadAllText(path)) is not JsonObject data)
                    {
                        if (pair.Key is "TextConfigs" or "UI")
                            invalidLoadPaths.Add(Path.GetRelativePath(ProjectPath, path));
                        continue;
                    }
                    string? type = data["type"] is JsonValue typeValue
                        && typeValue.TryGetValue<string>(out string? parsedType)
                            ? parsedType
                            : null;
                    if (!pair.Value.AcceptsType(type))
                    {
                        if (pair.Key is "TextConfigs" or "UI")
                            invalidLoadPaths.Add(Path.GetRelativePath(ProjectPath, path));
                        continue;
                    }
                    if (!pair.Value.PreserveType)
                        data.Remove("type");
                    string relativePath = Path.GetRelativePath(root, path);
                    string key = Path.ChangeExtension(relativePath, null)!.Replace('\\', '/');
                    pair.Value.Data[key] = data;
                }
                catch (JsonException)
                {
                    invalidLoadPaths.Add(Path.GetRelativePath(ProjectPath, path));
                }
            }
        }
        originData = cloneAllData();
        isModified = false;
        clearHistoryGesture();
        undoStack.Clear();
        redoStack.Clear();
        UndoRedoStateChanged?.Invoke(this, EventArgs.Empty);
    }

    private string? getDataKey(string absolutePath)
    {
        string fullPath = Path.GetFullPath(absolutePath);
        foreach (KeyValuePair<string, DataSection> pair in sections)
        {
            string root = Path.Combine(ProjectPath, "Data", pair.Key);
            if (fullPath.StartsWith(root + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase))
                return Path.ChangeExtension(Path.GetRelativePath(root, fullPath), null)?.Replace('\\', '/');
        }
        return null;
    }

    private Dictionary<string, string> createUiAssetMoveMap(
        IReadOnlyList<(string OldPath, string NewPath)> movedPaths)
    {
        Dictionary<string, string> result = new Dictionary<string, string>(StringComparer.Ordinal);
        foreach ((string oldPath, string newPath) in movedPaths)
        {
            if (!tryGetDataLocation(oldPath, out string oldSection, out string oldRelative)
                || !tryGetDataLocation(newPath, out string newSection, out string newRelative)
                || oldSection != "UI"
                || newSection != "UI")
            {
                continue;
            }
            if (Directory.Exists(newPath))
            {
                string oldPrefix = normalizeDataKey(oldRelative);
                string newPrefix = normalizeDataKey(newRelative);
                string[] keys = collectUiAssetKeysAcrossHistory()
                    .Where(key => keyMatchesPrefix(key, oldPrefix))
                    .ToArray();
                foreach (string key in keys)
                {
                    string suffix = oldPrefix.Length == 0
                        ? key
                        : key[oldPrefix.Length..].TrimStart('/');
                    string destinationKey = newPrefix.Length == 0
                        ? suffix
                        : suffix.Length == 0 ? newPrefix : newPrefix + "/" + suffix;
                    addUiAssetMove(result, key, destinationKey);
                }
                continue;
            }
            if (!string.Equals(
                    Path.GetExtension(oldPath),
                    DataConfig.DataFileExtension,
                    StringComparison.Ordinal)
                || !string.Equals(
                    Path.GetExtension(newPath),
                    DataConfig.DataFileExtension,
                    StringComparison.Ordinal))
            {
                continue;
            }
            string oldKey = Path.ChangeExtension(oldRelative, null)!.Replace('\\', '/');
            string newKey = Path.ChangeExtension(newRelative, null)!.Replace('\\', '/');
            addUiAssetMove(result, oldKey, newKey);
        }
        return result;
    }

    private static void addUiAssetMove(
        IDictionary<string, string> moves,
        string oldKey,
        string newKey)
    {
        string oldLogicalKey = UiAssetSchema.ToLogicalAssetKey(oldKey);
        string newLogicalKey = UiAssetSchema.ToLogicalAssetKey(newKey);
        if (oldLogicalKey.Length != 0 && newLogicalKey.Length != 0)
            moves[oldLogicalKey] = newLogicalKey;
    }

    private HashSet<string> collectUiAssetKeysAcrossHistory()
    {
        HashSet<string> result = new HashSet<string>(
            sections["UI"].Data.Keys,
            StringComparer.Ordinal);
        result.UnionWith(originData["UI"].Keys);
        foreach (Dictionary<string, Dictionary<string, JsonObject>> snapshot in undoStack)
            result.UnionWith(snapshot["UI"].Keys);
        foreach (Dictionary<string, Dictionary<string, JsonObject>> snapshot in redoStack)
            result.UnionWith(snapshot["UI"].Keys);
        return result;
    }

    private void remapUiAssetReferences(
        IReadOnlyDictionary<string, string> moves)
    {
        rewriteUiAssetReferences(originData["UI"], moves);
        rewriteUiAssetReferences(sections["UI"].Data, moves);
        foreach (Dictionary<string, Dictionary<string, JsonObject>> snapshot in undoStack)
            rewriteUiAssetReferences(snapshot["UI"], moves);
        foreach (Dictionary<string, Dictionary<string, JsonObject>> snapshot in redoStack)
            rewriteUiAssetReferences(snapshot["UI"], moves);
    }

    private static HashSet<string> rewriteUiAssetReferences(
        IReadOnlyDictionary<string, JsonObject> assets,
        IReadOnlyDictionary<string, string> moves)
    {
        HashSet<string> changed = new HashSet<string>(StringComparer.Ordinal);
        if (moves.Count == 0)
            return changed;
        foreach (KeyValuePair<string, JsonObject> pair in assets)
        {
            foreach (JsonObject node in UiAssetSchema.EnumerateNodes(pair.Value))
            {
                string? controlId = getString(node["controlId"]);
                if (controlId is null
                    || !controlId.StartsWith(UiAssetSchema.ProjectControlPrefix, StringComparison.Ordinal))
                {
                    continue;
                }
                string oldKey = controlId[UiAssetSchema.ProjectControlPrefix.Length..];
                if (!moves.TryGetValue(oldKey, out string? newKey))
                    continue;
                node["controlId"] = UiAssetSchema.ProjectControlPrefix + newKey;
                changed.Add(pair.Key);
            }
        }
        return changed;
    }

    private void writeUiDiskAssetReferences(
        IReadOnlyDictionary<string, string> moves,
        IDictionary<string, string> originals)
    {
        Dictionary<string, string> replacements = new Dictionary<string, string>(StringComparer.Ordinal);
        string assetsRoot = Path.Combine(ProjectPath, "Data", "UI", "Assets");
        if (!Directory.Exists(assetsRoot))
            throw new IOException($"UI assets directory was not found after move: {assetsRoot}");
        foreach (string path in Directory.EnumerateFiles(
                     assetsRoot,
                     "*",
                     SearchOption.AllDirectories)
                 .Where(path => string.Equals(
                     Path.GetExtension(path),
                     DataConfig.DataFileExtension,
                     StringComparison.Ordinal))
                 .OrderBy(path => path, StringComparer.Ordinal))
        {
            string original = File.ReadAllText(path);
            if (JsonNode.Parse(original) is not JsonObject diskAsset)
                throw new IOException($"UI asset file root must be an object: {path}");
            Dictionary<string, JsonObject> diskAssets = new Dictionary<string, JsonObject>(StringComparer.Ordinal)
            {
                [path] = diskAsset,
            };
            if (rewriteUiAssetReferences(diskAssets, moves).Count == 0)
                continue;
            originals[path] = original;
            replacements[path] = diskAsset.ToJsonString(WriteOptions) + Environment.NewLine;
        }
        foreach (KeyValuePair<string, string> pair in replacements)
            writeTextAtomically(pair.Key, pair.Value);
    }

    private static void restoreUiOriginAssets(
        IReadOnlyDictionary<string, string> originals)
    {
        List<string> failures = [];
        foreach (KeyValuePair<string, string> pair in originals)
        {
            try
            {
                writeTextAtomically(pair.Key, pair.Value);
            }
            catch (Exception exception) when (
                exception is IOException
                or UnauthorizedAccessException
                or System.Security.SecurityException
                or NotSupportedException)
            {
                failures.Add(pair.Key + ": " + exception.Message);
            }
        }
        if (failures.Count != 0)
            throw new IOException(string.Join(Environment.NewLine, failures));
    }

    private static void writeTextAtomically(string path, string content)
    {
        string directory = Path.GetDirectoryName(path)!;
        string temporaryPath = Path.Combine(
            directory,
            $".{Path.GetFileName(path)}.{Guid.NewGuid():N}.tmp");
        try
        {
            using (FileStream stream = new(
                       temporaryPath,
                       FileMode.CreateNew,
                       FileAccess.Write,
                       FileShare.None))
            {
                using StreamWriter writer = new(
                    stream,
                    new UTF8Encoding(false),
                    1024,
                    true);
                writer.Write(content);
                writer.Flush();
                stream.Flush(true);
            }
            File.Move(temporaryPath, path, true);
        }
        catch
        {
            deleteTemporaryFile(temporaryPath);
            throw;
        }
    }

    private bool applyExternalMove(string oldPath, string newPath)
    {
        bool directory = Directory.Exists(newPath);
        bool file = File.Exists(newPath);
        if (!directory && !file)
            return false;
        bool hasOldLocation = tryGetDataLocation(oldPath, out string oldSection, out string oldRelative);
        bool hasNewLocation = tryGetDataLocation(newPath, out string newSection, out string newRelative);
        if (directory)
        {
            if (hasOldLocation && hasNewLocation)
            {
                if (oldSection == newSection)
                {
                    return moveDataPrefix(
                        oldSection,
                        normalizeDataKey(oldRelative),
                        newSection,
                        normalizeDataKey(newRelative));
                }
                bool removed = removeDataPrefix(oldSection, normalizeDataKey(oldRelative));
                return applyExternalAdd(newPath) || removed;
            }
            if (hasOldLocation)
                return removeDataPrefix(oldSection, normalizeDataKey(oldRelative));
            return hasNewLocation && applyExternalAdd(newPath);
        }
        bool oldJson = hasOldLocation
            && hasDataFileExtension(oldSection, oldPath);
        bool newJson = hasNewLocation
            && hasDataFileExtension(newSection, newPath);
        if (oldJson && newJson)
        {
            string oldKey = Path.ChangeExtension(oldRelative, null)!.Replace('\\', '/');
            string newKey = Path.ChangeExtension(newRelative, null)!.Replace('\\', '/');
            if (oldSection != newSection)
            {
                bool removed = removeDataKey(oldSection, oldKey);
                return applyExternalAdd(newPath) || removed;
            }
            return moveDataKey(oldSection, oldKey, newSection, newKey);
        }
        if (oldJson)
        {
            string oldKey = Path.ChangeExtension(oldRelative, null)!.Replace('\\', '/');
            return removeDataKey(oldSection, oldKey);
        }
        return newJson && applyExternalAdd(newPath);
    }

    private bool applyExternalDelete(string path)
    {
        if (!tryGetDataLocation(path, out string sectionName, out string relativePath))
            return false;
        if (hasDataFileExtension(sectionName, path))
        {
            string key = Path.ChangeExtension(relativePath, null)!.Replace('\\', '/');
            bool removed = removeDataKey(sectionName, key);
            return removeDataPrefix(sectionName, normalizeDataKey(relativePath)) || removed;
        }
        return removeDataPrefix(sectionName, normalizeDataKey(relativePath));
    }

    private bool applyExternalAdd(string path)
    {
        if (Directory.Exists(path))
        {
            bool changed = false;
            foreach (string filePath in Directory.EnumerateFiles(path, "*.json", SearchOption.AllDirectories)
                         .Where(filePath => !DataConfig.isAnimationCache(filePath)))
            {
                changed |= applyExternalAdd(filePath);
            }
            return changed;
        }
        if (!File.Exists(path)
            || DataConfig.isAnimationCache(path)
            || !tryGetDataLocation(path, out string sectionName, out string relativePath)
            || !hasDataFileExtension(sectionName, path))
        {
            return false;
        }
        JsonObject? data = readExternalDataFile(path, sections[sectionName]);
        if (data is null)
            return false;
        string key = Path.ChangeExtension(relativePath, null)!.Replace('\\', '/');
        setDataKeyAcrossHistory(sectionName, key, data);
        return true;
    }

    private JsonObject? readExternalDataFile(string path, DataSection section)
    {
        try
        {
            if (JsonNode.Parse(File.ReadAllText(path)) is not JsonObject data)
                return null;
            string? type = data["type"] is JsonValue typeValue
                && typeValue.TryGetValue<string>(out string? parsedType)
                    ? parsedType
                    : null;
            if (!section.AcceptsType(type))
                return null;
            if (!section.PreserveType)
                data.Remove("type");
            return data;
        }
        catch (JsonException)
        {
            return null;
        }
        catch (IOException)
        {
            return null;
        }
        catch (UnauthorizedAccessException)
        {
            return null;
        }
    }

    private bool moveDataKey(string oldSection, string oldKey, string newSection, string newKey)
    {
        bool changed = moveDataKey(sections[oldSection].Data, oldKey, sections[newSection].Data, newKey);
        changed |= moveDataKey(originData[oldSection], oldKey, originData[newSection], newKey);
        foreach (Dictionary<string, Dictionary<string, JsonObject>> snapshot in undoStack)
            moveDataKey(snapshot[oldSection], oldKey, snapshot[newSection], newKey);
        foreach (Dictionary<string, Dictionary<string, JsonObject>> snapshot in redoStack)
            moveDataKey(snapshot[oldSection], oldKey, snapshot[newSection], newKey);
        return changed;
    }

    private static bool moveDataKey(
        Dictionary<string, JsonObject> oldData,
        string oldKey,
        Dictionary<string, JsonObject> newData,
        string newKey)
    {
        if (!oldData.TryGetValue(oldKey, out JsonObject? value))
            return false;
        if ((!ReferenceEquals(oldData, newData)
                || !string.Equals(oldKey, newKey, StringComparison.Ordinal))
            && newData.ContainsKey(newKey))
        {
            throw new InvalidOperationException($"Data move target already exists: {newKey}");
        }
        oldData.Remove(oldKey);
        newData[newKey] = value;
        return true;
    }

    private bool moveDataPrefix(
        string oldSection,
        string oldPrefix,
        string newSection,
        string newPrefix)
    {
        bool changed = moveDataPrefix(
            sections[oldSection].Data,
            oldPrefix,
            sections[newSection].Data,
            newPrefix);
        changed |= moveDataPrefix(originData[oldSection], oldPrefix, originData[newSection], newPrefix);
        foreach (Dictionary<string, Dictionary<string, JsonObject>> snapshot in undoStack)
            moveDataPrefix(snapshot[oldSection], oldPrefix, snapshot[newSection], newPrefix);
        foreach (Dictionary<string, Dictionary<string, JsonObject>> snapshot in redoStack)
            moveDataPrefix(snapshot[oldSection], oldPrefix, snapshot[newSection], newPrefix);
        return changed;
    }

    private static bool moveDataPrefix(
        Dictionary<string, JsonObject> oldData,
        string oldPrefix,
        Dictionary<string, JsonObject> newData,
        string newPrefix)
    {
        string[] keys = oldData.Keys.Where(key => keyMatchesPrefix(key, oldPrefix)).ToArray();
        HashSet<string> sourceKeys = keys.ToHashSet(StringComparer.Ordinal);
        HashSet<string> destinationKeys = new HashSet<string>(StringComparer.Ordinal);
        List<(string DestinationKey, JsonObject Value)> moved = [];
        foreach (string key in keys)
        {
            string suffix = oldPrefix.Length == 0 ? key : key[oldPrefix.Length..].TrimStart('/');
            string destinationKey = newPrefix.Length == 0
                ? suffix
                : suffix.Length == 0 ? newPrefix : newPrefix + "/" + suffix;
            if (!destinationKeys.Add(destinationKey)
                || newData.ContainsKey(destinationKey)
                    && (!ReferenceEquals(oldData, newData)
                        || !sourceKeys.Contains(destinationKey)))
            {
                throw new InvalidOperationException($"Data move target already exists: {destinationKey}");
            }
            moved.Add((destinationKey, oldData[key]));
        }
        foreach (string key in keys)
            oldData.Remove(key);
        foreach ((string destinationKey, JsonObject value) in moved)
            newData[destinationKey] = value;
        return keys.Length != 0;
    }

    private bool removeDataKey(string sectionName, string key)
    {
        bool changed = sections[sectionName].Data.Remove(key);
        changed |= originData[sectionName].Remove(key);
        foreach (Dictionary<string, Dictionary<string, JsonObject>> snapshot in undoStack)
            changed |= snapshot[sectionName].Remove(key);
        foreach (Dictionary<string, Dictionary<string, JsonObject>> snapshot in redoStack)
            changed |= snapshot[sectionName].Remove(key);
        return changed;
    }

    private bool removeDataPrefix(string sectionName, string prefix)
    {
        bool changed = removeDataPrefix(sections[sectionName].Data, prefix);
        changed |= removeDataPrefix(originData[sectionName], prefix);
        foreach (Dictionary<string, Dictionary<string, JsonObject>> snapshot in undoStack)
            changed |= removeDataPrefix(snapshot[sectionName], prefix);
        foreach (Dictionary<string, Dictionary<string, JsonObject>> snapshot in redoStack)
            changed |= removeDataPrefix(snapshot[sectionName], prefix);
        return changed;
    }

    private static bool removeDataPrefix(Dictionary<string, JsonObject> data, string prefix)
    {
        string[] keys = data.Keys.Where(key => keyMatchesPrefix(key, prefix)).ToArray();
        foreach (string key in keys)
            data.Remove(key);
        return keys.Length != 0;
    }

    private void setDataKeyAcrossHistory(string sectionName, string key, JsonObject data)
    {
        sections[sectionName].Data[key] = (JsonObject)data.DeepClone();
        originData[sectionName][key] = (JsonObject)data.DeepClone();
        foreach (Dictionary<string, Dictionary<string, JsonObject>> snapshot in undoStack)
            snapshot[sectionName][key] = (JsonObject)data.DeepClone();
        foreach (Dictionary<string, Dictionary<string, JsonObject>> snapshot in redoStack)
            snapshot[sectionName][key] = (JsonObject)data.DeepClone();
    }

    private bool isDataPath(string path)
    {
        return tryGetDataLocation(path, out _, out _);
    }

    private bool tryGetDataLocation(string path, out string sectionName, out string relativePath)
    {
        string fullPath = Path.GetFullPath(path);
        foreach (KeyValuePair<string, DataSection> pair in sections)
        {
            string root = Path.GetFullPath(Path.Combine(ProjectPath, "Data", pair.Key));
            string relative = Path.GetRelativePath(root, fullPath);
            if (Path.IsPathRooted(relative)
                || relative == ".."
                || relative.StartsWith(".." + Path.DirectorySeparatorChar, StringComparison.Ordinal))
            {
                continue;
            }
            sectionName = pair.Key;
            relativePath = relative == "." ? string.Empty : relative;
            return true;
        }
        sectionName = string.Empty;
        relativePath = string.Empty;
        return false;
    }

    private static bool keyMatchesPrefix(string key, string prefix)
    {
        return prefix.Length == 0
            || string.Equals(key, prefix, StringComparison.Ordinal)
            || key.StartsWith(prefix + "/", StringComparison.Ordinal);
    }

    private bool tryGetSectionAndKey(string absolutePath, out DataSection section, out string key)
    {
        section = null!;
        key = string.Empty;
        string fullPath = Path.GetFullPath(absolutePath);
        foreach (KeyValuePair<string, DataSection> pair in sections)
        {
            string root = Path.Combine(ProjectPath, "Data", pair.Key);
            if (!fullPath.StartsWith(root + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase))
                continue;
            section = pair.Value;
            key = Path.ChangeExtension(Path.GetRelativePath(root, fullPath), null)!.Replace('\\', '/');
            return true;
        }
        return false;
    }

    private bool insertLayer(string mapKey, string layerName, JsonObject layer, string? insertAfterLayer)
    {
        if (getMap(mapKey) is not JsonObject map || map["layers"] is not JsonObject layers || layers.ContainsKey(layerName))
            return false;
        JsonArray layerOrder = getLayerOrder(map);
        int insertIndex = string.IsNullOrWhiteSpace(insertAfterLayer)
            ? layerOrder.Count
            : layerOrder.IndexOf(insertAfterLayer) + 1;
        if (insertIndex <= 0)
            insertIndex = layerOrder.Count;
        RecordSnapshot();
        layerOrder.Insert(insertIndex, layerName);
        layers.Add(layerName, layer);
        List<KeyValuePair<string, JsonNode?>> entries = layerOrder
            .Select(name => new KeyValuePair<string, JsonNode?>(name!.GetValue<string>(), layers[name.GetValue<string>()]))
            .ToList();
        layers.Clear();
        foreach (KeyValuePair<string, JsonNode?> entry in entries)
            layers.Add(entry.Key, entry.Value);
        refreshModifiedState();
        return true;
    }

    private static JsonArray getLayerOrder(JsonObject map)
    {
        return map["layerOrder"] as JsonArray
            ?? throw new InvalidDataException("Map data must contain a layerOrder array.");
    }

    private static JsonObject createEmptyLayer(string layerName, string tilesetKey, int width, int height)
    {
        JsonArray tiles = new JsonArray();
        JsonArray autoTiles = new JsonArray();
        for (int y = 0; y < height; y++)
        {
            JsonArray tileRow = new JsonArray();
            JsonArray autoTileRow = new JsonArray();
            for (int x = 0; x < width; x++)
            {
                tileRow.Add(null);
                autoTileRow.Add(null);
            }
            tiles.Add(tileRow);
            autoTiles.Add(autoTileRow);
        }
        return new JsonObject
        {
            ["layerName"] = layerName,
            ["layerTileset"] = tilesetKey,
            ["tiles"] = tiles,
            ["autoTiles"] = autoTiles,
            ["shaderPath"] = string.Empty,
            ["actors"] = new JsonArray(),
        };
    }

    private static string normalizeDataKey(string key)
    {
        return key.Replace('\\', '/').Trim().Trim('/');
    }

    private static string normalizeJsonKey(string key)
    {
        string normalized = normalizeDataKey(key);
        return normalized.EndsWith(".json", StringComparison.OrdinalIgnoreCase)
            ? normalized[..^5]
            : normalized;
    }

    private bool renameDataEntry(string sectionName, string oldKey, string newKey)
    {
        string normalizedOldKey = normalizeJsonKey(oldKey);
        string normalizedNewKey = normalizeJsonKey(newKey);
        Dictionary<string, JsonObject> data = sections[sectionName].Data;
        if (normalizedNewKey.Length == 0
            || !data.TryGetValue(normalizedOldKey, out JsonObject? value)
            || data.ContainsKey(normalizedNewKey))
        {
            return false;
        }
        RecordSnapshot();
        data.Remove(normalizedOldKey);
        data[normalizedNewKey] = value;
        refreshModifiedState();
        if (sectionName == "UI")
            UiAssetsChanged?.Invoke(this, EventArgs.Empty);
        return true;
    }

    private bool deleteDataEntry(string sectionName, string key)
    {
        string normalizedKey = normalizeJsonKey(key);
        Dictionary<string, JsonObject> data = sections[sectionName].Data;
        if (!data.ContainsKey(normalizedKey))
            return false;
        RecordSnapshot();
        data.Remove(normalizedKey);
        refreshModifiedState();
        if (sectionName == "UI")
            UiAssetsChanged?.Invoke(this, EventArgs.Empty);
        return true;
    }

    private static string normaliseMapKey(string fileName)
    {
        string key = normalizeDataKey(fileName);
        return key.EndsWith(".json", StringComparison.OrdinalIgnoreCase) ? key[..^5] : key;
    }

    private static bool isValidMapSize(int width, int height)
    {
        return width is >= 1 and <= 32768 && height is >= 1 and <= 32768;
    }

    private static JsonObject cloneObject(JsonNode? value)
    {
        return value is JsonObject objectValue ? (JsonObject)objectValue.DeepClone() : new JsonObject();
    }

    private static JsonArray normaliseAmbientLight(JsonArray? values)
    {
        JsonArray result = new JsonArray();
        for (int index = 0; index < 4; index += 1)
        {
            JsonNode? value = values?[index];
            int component = value is JsonValue jsonValue
                && jsonValue.TryGetValue<byte>(out byte byteValue)
                    ? byteValue
                    : value?.GetValue<int?>() ?? 255;
            result.Add(Math.Clamp(component, 0, 255));
        }
        return result;
    }

    private static void resizeMapLayers(JsonObject map, int width, int height)
    {
        if (map["layers"] is not JsonObject layers)
            return;
        foreach (JsonNode? value in layers.Select(entry => entry.Value))
        {
            if (value is not JsonObject layer)
                continue;
            layer["tiles"] = resizeGrid(layer["tiles"] as JsonArray, width, height);
            layer["autoTiles"] = resizeGrid(layer["autoTiles"] as JsonArray, width, height);
        }
    }

    private static JsonArray resizeGrid(JsonArray? source, int width, int height)
    {
        JsonArray result = new JsonArray();
        for (int y = 0; y < height; y += 1)
        {
            JsonArray row = new JsonArray();
            JsonArray? sourceRow = source is not null && y < source.Count ? source[y] as JsonArray : null;
            for (int x = 0; x < width; x += 1)
                row.Add(sourceRow is not null && x < sourceRow.Count ? sourceRow[x]?.DeepClone() : null);
            result.Add(row);
        }
        return result;
    }

    private void renameMapKey(string currentKey, string newKey, JsonObject map)
    {
        Dictionary<string, JsonObject> maps = sections["Maps"].Data;
        List<KeyValuePair<string, JsonObject>> entries = maps
            .Select(entry => new KeyValuePair<string, JsonObject>(entry.Key == currentKey ? newKey : entry.Key, entry.Key == currentKey ? map : entry.Value))
            .ToList();
        maps.Clear();
        foreach (KeyValuePair<string, JsonObject> entry in entries)
            maps.Add(entry.Key, entry.Value);
    }

    private static JsonObject createDefaultMaterial()
    {
        return new JsonObject
        {
            ["lightBlock"] = 0.0,
            ["mirror"] = false,
            ["reflectionStrength"] = 0.5,
            ["opacity"] = 1.0,
            ["speedRate"] = 1.0,
        };
    }

    private static JsonObject createCurveKey(double time, JsonNode value)
    {
        int componentCount = value is JsonArray values ? values.Count : 1;
        return new JsonObject
        {
            ["time"] = time,
            ["value"] = value,
            ["interpolation"] = "linear",
            ["arriveTangent"] = createCurveValue(componentCount, 0.0),
            ["leaveTangent"] = createCurveValue(componentCount, 0.0),
        };
    }

    private static JsonNode createCurveValue(int componentCount, double value)
    {
        if (componentCount == 1)
            return JsonValue.Create(value);
        JsonArray result = new();
        for (int index = 0; index < componentCount; index += 1)
            result.Add(value);
        return result;
    }

    private static JsonObject createPlainTextConfig(string name)
    {
        return new JsonObject
        {
            ["type"] = "plainTextConfig",
            ["name"] = name,
            ["font"] = string.Empty,
            ["characterSize"] = 22,
            ["style"] = createTextStyleFlags(),
            ["slantAngle"] = 0.0,
            ["fillColor"] = createColour(255, 255, 255, 255),
            ["letterSpacing"] = 1.0,
            ["lineSpacing"] = 1.0,
            ["lineAlignment"] = "default",
            ["outline"] = createOutline(),
            ["glow"] = createGlow(),
            ["gradient"] = createGradient(),
        };
    }

    private static JsonObject createRichTextConfig(string name)
    {
        return new JsonObject
        {
            ["type"] = "richTextConfig",
            ["name"] = name,
            ["font"] = string.Empty,
            ["lineAlignment"] = "default",
            ["defaultStyle"] = new JsonObject
            {
                ["characterSize"] = 22,
                ["style"] = createTextStyleFlags(),
                ["fillColor"] = createColour(255, 255, 255, 255),
                ["letterSpacing"] = 1.0,
                ["lineSpacing"] = 1.0,
                ["outline"] = createOutline(),
            },
            ["styleOrder"] = new JsonArray(),
            ["styles"] = new JsonObject(),
            ["glow"] = createGlow(),
            ["gradient"] = createGradient(),
        };
    }

    private static JsonObject createTextStyleFlags()
    {
        return new JsonObject
        {
            ["bold"] = false,
            ["italic"] = false,
            ["underlined"] = false,
            ["strikeThrough"] = false,
        };
    }

    private static JsonArray createColour(int red, int green, int blue, int alpha)
    {
        return new JsonArray(red, green, blue, alpha);
    }

    private static JsonObject createOutline()
    {
        return new JsonObject
        {
            ["color"] = createColour(0, 0, 0, 255),
            ["thickness"] = 0.0,
        };
    }

    private static JsonObject createGlow()
    {
        return new JsonObject
        {
            ["enabled"] = false,
            ["color"] = createColour(255, 255, 255, 0),
            ["radius"] = 0.0,
            ["intensity"] = 0.0,
        };
    }

    private static JsonObject createGradient()
    {
        return new JsonObject
        {
            ["enabled"] = false,
            ["direction"] = "vertical",
            ["curve"] = string.Empty,
        };
    }

    private static bool isCurveType(string? type)
    {
        return type is "curve" or "vector2Curve" or "vector3Curve" or "vector4Curve";
    }

    private static int curveComponentCount(string type)
    {
        return type switch
        {
            "vector2Curve" => 2,
            "vector3Curve" => 3,
            "vector4Curve" => 4,
            _ => 1,
        };
    }

    private static bool isTextConfigType(string? type)
    {
        return type is "plainTextConfig" or "richTextConfig";
    }

    private Dictionary<string, Dictionary<string, JsonObject>> cloneAllData()
    {
        Dictionary<string, Dictionary<string, JsonObject>> source = sections.ToDictionary(
            section => section.Key,
            section => section.Value.Data,
            StringComparer.Ordinal);
        return cloneData(source);
    }

    private static bool hasDataFileExtension(string sectionName, string path)
    {
        StringComparison comparison = sectionName == "UI"
            ? StringComparison.Ordinal
            : StringComparison.OrdinalIgnoreCase;
        return string.Equals(
            Path.GetExtension(path),
            DataConfig.DataFileExtension,
            comparison);
    }

    private static Dictionary<string, Dictionary<string, JsonObject>> cloneData(
        IReadOnlyDictionary<string, Dictionary<string, JsonObject>> source)
    {
        return source.ToDictionary(
            section => section.Key,
            section => section.Value.ToDictionary(
                item => item.Key,
                item => (JsonObject)item.Value.DeepClone(),
                StringComparer.Ordinal),
            StringComparer.Ordinal);
    }

    private static IReadOnlyList<Dictionary<string, Dictionary<string, JsonObject>>> cloneHistory(
        IEnumerable<Dictionary<string, Dictionary<string, JsonObject>>> history)
    {
        return history.Select(cloneData).ToArray();
    }

    private static void restoreHistory(
        Stack<Dictionary<string, Dictionary<string, JsonObject>>> history,
        IReadOnlyList<Dictionary<string, Dictionary<string, JsonObject>>> snapshots)
    {
        history.Clear();
        for (int index = snapshots.Count - 1; index >= 0; index--)
            pushHistory(history, cloneData(snapshots[index]));
    }

    private static void pushHistory(
        Stack<Dictionary<string, Dictionary<string, JsonObject>>> history,
        Dictionary<string, Dictionary<string, JsonObject>> snapshot)
    {
        history.Push(snapshot);
        if (history.Count <= MaximumHistoryEntries)
            return;
        Dictionary<string, Dictionary<string, JsonObject>>[] newest = history
            .Take(MaximumHistoryEntries)
            .ToArray();
        history.Clear();
        for (int index = newest.Length - 1; index >= 0; index--)
            history.Push(newest[index]);
    }

    private void clearHistoryGesture()
    {
        activeHistoryGestureId = 0;
        activeHistoryGestureHasSnapshot = false;
    }

    private void restoreSnapshot(
        Dictionary<string, Dictionary<string, JsonObject>> snapshot,
        bool notifyUiAssets = true)
    {
        bool uiAssetsChanged = !sectionEqual(
            sections["UI"].Data,
            snapshot["UI"]);
        foreach (KeyValuePair<string, DataSection> section in sections)
        {
            section.Value.Data.Clear();
            if (!snapshot.TryGetValue(section.Key, out Dictionary<string, JsonObject>? source))
                continue;
            foreach (KeyValuePair<string, JsonObject> item in source)
                section.Value.Data[item.Key] = (JsonObject)item.Value.DeepClone();
        }
        refreshModifiedState();
        if (notifyUiAssets && uiAssetsChanged)
            UiAssetsChanged?.Invoke(this, EventArgs.Empty);
        DataRestored?.Invoke(this, EventArgs.Empty);
        UndoRedoStateChanged?.Invoke(this, EventArgs.Empty);
    }

    private IReadOnlyList<string> getDiff(
        IReadOnlyDictionary<string, Dictionary<string, JsonObject>> current,
        IReadOnlyDictionary<string, Dictionary<string, JsonObject>> target
    )
    {
        List<string> differences = [];
        foreach (KeyValuePair<string, DataSection> section in sections)
        {
            IReadOnlyDictionary<string, JsonObject> oldData = current[section.Key];
            IReadOnlyDictionary<string, JsonObject> newData = target[section.Key];
            string[] changed = oldData.Keys.Union(newData.Keys, StringComparer.Ordinal)
                .Where(key => !oldData.TryGetValue(key, out JsonObject? oldValue)
                    || !newData.TryGetValue(key, out JsonObject? newValue)
                    || !nodesEqual(oldValue, newValue))
                .OrderBy(key => key, StringComparer.Ordinal)
                .ToArray();
            if (changed.Length != 0)
                differences.Add($"{section.Key}: {string.Join(", ", changed)}");
        }
        return differences;
    }

    private static bool sectionEqual(
        IReadOnlyDictionary<string, JsonObject> current,
        IReadOnlyDictionary<string, JsonObject> origin
    )
    {
        if (current.Count != origin.Count || current.Keys.Except(origin.Keys, StringComparer.Ordinal).Any())
            return false;
        return current.All(item => nodesEqual(item.Value, origin[item.Key]));
    }

    private static string formatSaveDetails(
        IReadOnlyList<string> added,
        IReadOnlyList<string> updated,
        IReadOnlyList<string> deleted,
        IReadOnlyList<string> failed
    )
    {
        List<string> lines = [];
        if (added.Count != 0)
            lines.Add($"A [{string.Join(", ", added)}]");
        if (updated.Count != 0)
            lines.Add($"U [{string.Join(", ", updated)}]");
        if (deleted.Count != 0)
            lines.Add($"D [{string.Join(", ", deleted)}]");
        if (failed.Count != 0)
            lines.Add($"Failed [{string.Join(", ", failed)}]");
        return "\n" + string.Join("\n", lines);
    }

    private static bool nodesEqual(JsonNode current, JsonNode origin)
    {
        return string.Equals(current.ToJsonString(), origin.ToJsonString(), StringComparison.Ordinal);
    }

    private static void deleteTemporaryFile(string path)
    {
        try
        {
            if (File.Exists(path))
                File.Delete(path);
        }
        catch (Exception exception) when (
            exception is IOException
            or UnauthorizedAccessException
            or System.Security.SecurityException)
        {
        }
    }

    private static bool isUiDataType(JsonObject data, string expectedType)
    {
        return string.Equals(getString(data["type"]), expectedType, StringComparison.Ordinal);
    }

    private static string? getString(JsonNode? value)
    {
        return value is JsonValue scalar && scalar.TryGetValue(out string? text) ? text : null;
    }

    private sealed class DataSection
    {
        private readonly HashSet<string>? acceptedTypes;

        public DataSection(string? expectedType, bool writeType)
        {
            ExpectedType = expectedType;
            WriteType = writeType;
        }

        public DataSection(IEnumerable<string> acceptedTypes)
        {
            this.acceptedTypes = new HashSet<string>(acceptedTypes, StringComparer.Ordinal);
            PreserveType = true;
        }

        public string? ExpectedType { get; }
        public bool WriteType { get; }
        public bool PreserveType { get; }
        public Dictionary<string, JsonObject> Data { get; } = new(StringComparer.Ordinal);

        public bool AcceptsType(string? type)
        {
            if (acceptedTypes is not null)
                return type is not null && acceptedTypes.Contains(type);
            return ExpectedType is null
                || type is null
                || string.Equals(type, ExpectedType, StringComparison.Ordinal);
        }
    }
}

public sealed record DataFileInfo(string Type, string? Key);
public sealed record SaveResult(bool Success, string Details);
