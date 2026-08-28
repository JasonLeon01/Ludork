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

public sealed partial class GameDataService
{
    private bool commitMapStructureChange(string mapKey, JsonObject current, JsonObject candidate)
    {
        mapKey = normaliseMapKey(mapKey);
        JsonObject? worldCandidate = null;
        IReadOnlyList<string>? worldLayerOrder = null;
        if (TryGetWorldForMap(mapKey, out string worldKey)
            && getWorldMap(worldKey) is JsonObject world)
        {
            WorldMapValidationResult validation = ValidateWorldMap(worldKey);
            if (!validation.IsValid)
                return false;
            Dictionary<string, MapCatalogEntry> children = getWorldChildCatalog(worldKey)
                .ToDictionary(item => item.Key, item => item.Value, StringComparer.Ordinal);
            children[mapKey] = createMapCatalogEntry(
                mapKey,
                MapCatalogEntryKind.WorldChildMap,
                worldKey,
                candidate);
            worldLayerOrder = worldMapValidation.TryMergeLayerOrder(
                worldKey,
                validation.Placements,
                children);
            if (worldLayerOrder is null)
                return false;
            worldCandidate = replaceWorldComposition(world, worldLayerOrder, validation.Placements);
            if (!worldMapValidation.Validate(worldKey, worldCandidate, children).IsValid)
                return false;
        }
        RecordMapSnapshot(mapKey);
        replaceJsonObject(current, candidate);
        MapCatalogEntryKind kind = TryGetWorldForMap(mapKey, out string parentWorld)
            ? MapCatalogEntryKind.WorldChildMap
            : MapCatalogEntryKind.StandaloneMap;
        setMapCatalogEntry(createMapCatalogEntry(
            mapKey,
            kind,
            kind == MapCatalogEntryKind.WorldChildMap ? parentWorld : null,
            current));
        if (worldCandidate is not null && worldLayerOrder is not null)
        {
            sections["WorldMaps"].Data[parentWorld] = worldCandidate;
            setWorldCatalogLayerOrder(parentWorld, worldLayerOrder);
        }
        NotifyMapContentChanged(mapKey);
        refreshModifiedState();
        return true;
    }

    private static void replaceJsonObject(JsonObject target, JsonObject source)
    {
        target.Clear();
        foreach (KeyValuePair<string, JsonNode?> item in source)
            target[item.Key] = item.Value?.DeepClone();
    }

    private static JsonObject createMapData(MapInfo info, string tilesetKey)
    {
        return new JsonObject
        {
            ["mapName"] = string.IsNullOrWhiteSpace(info.MapName)
                ? LocaleService.Get("NEW_MAP_DEFAULT_NAME")
                : info.MapName.Trim(),
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
    }

    private static bool pathsEqual(string left, string right)
    {
        return string.Equals(
            Path.TrimEndingDirectorySeparator(Path.GetFullPath(left)),
            Path.TrimEndingDirectorySeparator(Path.GetFullPath(right)),
            OperatingSystem.IsWindows() ? StringComparison.OrdinalIgnoreCase : StringComparison.Ordinal);
    }

    private static StringComparer getPathComparer()
    {
        return OperatingSystem.IsWindows() ? StringComparer.OrdinalIgnoreCase : StringComparer.Ordinal;
    }

    private string getSectionDataPath(string sectionName, string key)
    {
        if (sectionName == "WorldMaps")
        {
            return Path.Combine(
                ProjectPath,
                "Data",
                "Maps",
                key.Replace('/', Path.DirectorySeparatorChar),
                "_world.json");
        }
        return Path.Combine(
            ProjectPath,
            "Data",
            sectionName,
            key.Replace('/', Path.DirectorySeparatorChar) + ".json");
    }

    private void addInvalidLoadPath(string path)
    {
        string relative = Path.GetRelativePath(ProjectPath, path).Replace('\\', '/');
        if (!invalidLoadPaths.Contains(relative, StringComparer.Ordinal))
            invalidLoadPaths.Add(relative);
    }
}

