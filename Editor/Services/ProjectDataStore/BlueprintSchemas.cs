using Ludork.Models;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed partial class ProjectDataStore
{
    private readonly Dictionary<string, string> invalidLoadErrors = new(StringComparer.Ordinal);
    private readonly object invalidLoadLock = new();

    public IReadOnlyDictionary<string, string> InvalidLoadErrors
    {
        get
        {
            lock (invalidLoadLock)
                return new Dictionary<string, string>(invalidLoadErrors, StringComparer.Ordinal);
        }
    }

    public IReadOnlyList<string> InvalidLoadDetails
    {
        get
        {
            lock (invalidLoadLock)
                return invalidLoadPaths.Select(path => invalidLoadErrors.TryGetValue(path, out string? error)
                    ? path + ": " + error : path).ToArray();
        }
    }

    internal void rejectDataFile(string path, string error)
    {
        lock (invalidLoadLock)
        {
            addInvalidLoadPath(path);
            invalidLoadErrors[Path.GetRelativePath(ProjectPath, path).Replace('\\', '/')] = error;
        }
    }

    private void clearRejectedDataFile(string path)
    {
        lock (invalidLoadLock)
        {
            string relative = Path.GetRelativePath(ProjectPath, path).Replace('\\', '/');
            foreach (string rejected in invalidLoadPaths.Where(candidate => candidate == relative
                         || candidate.StartsWith(relative + "/", StringComparison.Ordinal)).ToArray())
            {
                invalidLoadPaths.Remove(rejected);
                invalidLoadErrors.Remove(rejected);
            }
        }
    }

    private BlueprintClassResolver createBlueprintSchemaResolver(IReadOnlyDictionary<string, JsonObject>? blueprints = null)
    {
        LuaMetadataService metadata = new(ProjectPath, strictReads: true, loadCancellationToken);
        return new BlueprintClassResolver(metadata, key => blueprints is null
            ? sections["Blueprints"].GetValueOrDefault(key)
            : blueprints.GetValueOrDefault(key));
    }

    private Dictionary<string, string> getBlueprintSchemaErrors(IReadOnlyDictionary<string, JsonObject> blueprints)
    {
        Dictionary<string, string> errors = new(StringComparer.Ordinal);
        using BlueprintClassResolver resolver = createBlueprintSchemaResolver(blueprints);
        using IDisposable metadataRead = resolver.BeginBatch();
        foreach ((string key, JsonObject data) in blueprints)
        {
            try
            {
                resolver.ResolveBlueprint(data, key);
            }
            catch (Exception exception) when (exception is InvalidDataException or IOException or UnauthorizedAccessException)
            {
                errors[key] = exception.Message;
            }
        }
        return errors;
    }

    private void rejectInvalidBlueprints()
    {
        Dictionary<string, JsonObject> blueprints = sections["Blueprints"].ToDictionary(
            entry => entry.Key, entry => entry.Value, StringComparer.Ordinal);
        foreach ((string key, string error) in getBlueprintSchemaErrors(blueprints))
        {
            sections["Blueprints"].Remove(key);
            rejectDataFile(getSectionDataPath("Blueprints", key), error);
        }
    }

    private bool isReadableBlueprintData(string section, string key, JsonObject data,
        IReadOnlyDictionary<string, JsonObject> blueprints)
    {
        try
        {
            using BlueprintClassResolver resolver = createBlueprintSchemaResolver(blueprints);
            if (section == "Blueprints")
                resolver.ResolveBlueprint(data, key);
            else if (section == "Maps")
                validateMapBlueprintData(key, data, resolver);
            return true;
        }
        catch (Exception exception) when (exception is InvalidDataException or IOException or UnauthorizedAccessException)
        {
            return false;
        }
    }

    internal IReadOnlyList<string> ValidateBlueprintSchemas(bool includeRejectedFiles = true)
    {
        List<string> errors = [];
        if (includeRejectedFiles)
        {
            lock (invalidLoadLock)
                errors.AddRange(invalidLoadPaths
                    .Where(path => path.StartsWith("Data/Blueprints/", StringComparison.Ordinal)
                        || path.StartsWith("Data/Maps/", StringComparison.Ordinal))
                    .Select(path => path + ": " + invalidLoadErrors.GetValueOrDefault(path, "The data file could not be loaded.")));
        }
        Dictionary<string, JsonObject> blueprints = sections["Blueprints"].ToDictionary(
            entry => entry.Key, entry => entry.Value, StringComparer.Ordinal);
        foreach ((string key, string error) in getBlueprintSchemaErrors(blueprints))
            errors.Add($"Data/Blueprints/{key}.json: {error}");
        using BlueprintClassResolver resolver = createBlueprintSchemaResolver(blueprints);
        using IDisposable metadataRead = resolver.BeginBatch();
        foreach (string key in getBlueprintMapKeys())
        {
            try
            {
                JsonObject map = sections["Maps"].TryGetValue(key, out JsonObject? loaded)
                    ? loaded : readMapBlueprintData(Maps.getReadableMapDataPath(key));
                validateMapBlueprintData(key, map, resolver);
            }
            catch (Exception exception) when (exception is InvalidDataException or IOException or UnauthorizedAccessException or JsonException)
            {
                errors.Add($"Data/Maps/{key}.json: {exception.Message}");
            }
        }
        return errors;
    }

    private IEnumerable<string> getBlueprintMapKeys()
    {
        return sections["Maps"].Keys.Concat(Maps.getMapCatalogEntries()
                .Where(entry => entry.Kind != MapCatalogEntryKind.WorldMap).Select(entry => entry.Key))
            .Distinct(StringComparer.Ordinal).OrderBy(key => key, StringComparer.Ordinal);
    }

    internal void ValidateBlueprintMapOverrides(string key, JsonObject candidate)
    {
        Dictionary<string, JsonObject> blueprints = sections["Blueprints"].ToDictionary(
            entry => entry.Key, entry => entry.Value, StringComparer.Ordinal);
        blueprints[key] = candidate;
        using BlueprintClassResolver resolver = createBlueprintSchemaResolver(blueprints);
        using IDisposable metadataRead = resolver.BeginBatch();
        foreach (string mapKey in getBlueprintMapKeys())
        {
            JsonObject map = sections["Maps"].TryGetValue(mapKey, out JsonObject? loaded)
                ? loaded : readMapBlueprintData(Maps.getReadableMapDataPath(mapKey));
            validateMapBlueprintData(mapKey, map, resolver);
        }
    }

    internal bool tryValidateMapBlueprintData(string path, JsonObject map)
    {
        try
        {
            using BlueprintClassResolver resolver = createBlueprintSchemaResolver();
            validateMapBlueprintData(getDataKey(path) ?? path, map, resolver);
            return true;
        }
        catch (Exception exception) when (exception is InvalidDataException or IOException or UnauthorizedAccessException)
        {
            rejectDataFile(path, exception.Message);
            return false;
        }
    }

    internal bool tryValidateMapBlueprintFile(string path)
    {
        try
        {
            return tryValidateMapBlueprintData(path, readMapBlueprintData(path));
        }
        catch (Exception exception) when (exception is InvalidDataException or IOException or UnauthorizedAccessException or JsonException)
        {
            rejectDataFile(path, exception.Message);
            return false;
        }
    }

    private static JsonObject readMapBlueprintData(string path)
    {
        using FileStream stream = File.OpenRead(path);
        using JsonDocument document = JsonDocument.Parse(stream);
        JsonElement root = document.RootElement;
        if (root.ValueKind != JsonValueKind.Object)
            throw new InvalidDataException("Map data must be an object");
        JsonObject data = [];
        if (root.TryGetProperty("actors", out JsonElement actors))
            data["actors"] = JsonNode.Parse(actors.GetRawText());
        if (root.TryGetProperty("BPClassVarChanged", out JsonElement overrides))
            data["BPClassVarChanged"] = JsonNode.Parse(overrides.GetRawText());
        if (root.TryGetProperty("layers", out JsonElement layers) && layers.ValueKind == JsonValueKind.Object)
        {
            JsonObject projectedLayers = [];
            foreach (JsonProperty layer in layers.EnumerateObject())
            {
                if (layer.Value.ValueKind == JsonValueKind.Object
                    && layer.Value.TryGetProperty("actors", out JsonElement layerActors))
                {
                    projectedLayers[layer.Name] = new JsonObject
                    {
                        ["actors"] = JsonNode.Parse(layerActors.GetRawText()),
                    };
                }
            }
            data["layers"] = projectedLayers;
        }
        return data;
    }

    private static void validateMapBlueprintData(string key, JsonObject map, BlueprintClassResolver resolver)
    {
        using IDisposable metadataRead = resolver.BeginBatch();
        JsonObject? overrides = map["BPClassVarChanged"] as JsonObject;
        if (map.ContainsKey("BPClassVarChanged") && overrides is null)
            throw new InvalidDataException($"Maps/{key}.BPClassVarChanged must be an object");
        HashSet<string> actorTags = new(StringComparer.Ordinal);
        foreach (MapDataService.MapActorCollection collection in WorldDataService.enumerateActorCollections(map))
        {
            foreach (JsonObject actor in collection.Actors.OfType<JsonObject>())
            {
                string tag = getString(actor["tag"]) ?? string.Empty;
                string? reference = getString(actor["bp"]);
                actorTags.Add(tag);
                JsonNode? values = overrides?[tag];
                if (overrides?.ContainsKey(tag) == true && values is not JsonObject)
                    throw new InvalidDataException($"Maps/{key}.BPClassVarChanged.{tag} must be an object");
                if (string.IsNullOrWhiteSpace(reference))
                    throw new InvalidDataException($"Maps/{key}.actors.{tag}.bp must be a class reference");
                try
                {
                    resolver.Resolve(reference, values as JsonObject);
                }
                catch (InvalidDataException exception)
                {
                    throw new InvalidDataException($"Maps/{key}.actors.{tag}: {exception.Message}", exception);
                }
            }
        }
        if (overrides is null)
            return;
        foreach (string tag in overrides.Select(entry => entry.Key))
            if (!actorTags.Contains(tag))
                throw new InvalidDataException($"Maps/{key}.BPClassVarChanged.{tag} has no matching actor");
    }

    private void validateExternalBlueprintChanges(IDictionary<(string Section, string Key), JsonObject?> changes)
    {
        if (!changes.Keys.Any(location => location.Section is "Blueprints" or "Maps"))
            return;
        foreach (string path in InvalidLoadPaths)
        {
            string absolutePath = Path.Combine(ProjectPath, path);
            if (!File.Exists(absolutePath)
                || !tryGetDataLocation(absolutePath, out string section, out string relative)
                || section is not ("Blueprints" or "Maps"))
            {
                continue;
            }
            string key = normalizeJsonKey(relative);
            if (!changes.ContainsKey((section, key))
                && readExternalDataFile(absolutePath, sections[section]) is JsonObject data)
            {
                changes[(section, key)] = data;
            }
        }
        Dictionary<string, JsonObject> blueprints = sections["Blueprints"].ToDictionary(
            entry => entry.Key, entry => entry.Value, StringComparer.Ordinal);
        foreach (KeyValuePair<(string Section, string Key), JsonObject?> change in changes)
        {
            if (change.Key.Section != "Blueprints")
                continue;
            if (change.Value is JsonObject data)
                blueprints[change.Key.Key] = data;
            else
                blueprints.Remove(change.Key.Key);
        }
        foreach ((string key, string error) in getBlueprintSchemaErrors(blueprints))
        {
            changes[("Blueprints", key)] = null;
            rejectDataFile(getSectionDataPath("Blueprints", key), error);
            blueprints.Remove(key);
        }
        using BlueprintClassResolver resolver = createBlueprintSchemaResolver(blueprints);
        using IDisposable metadataRead = resolver.BeginBatch();
        string[] mapKeys = getBlueprintMapKeys().Concat(changes.Keys
                .Where(location => location.Section == "Maps").Select(location => location.Key))
            .Distinct(StringComparer.Ordinal).ToArray();
        foreach (string key in mapKeys)
        {
            if (changes.TryGetValue(("Maps", key), out JsonObject? changed) && changed is null)
                continue;
            try
            {
                JsonObject map = changed ?? (sections["Maps"].TryGetValue(key, out JsonObject? loaded)
                    ? loaded : readMapBlueprintData(Maps.getReadableMapDataPath(key)));
                validateMapBlueprintData(key, map, resolver);
            }
            catch (Exception exception) when (exception is InvalidDataException or IOException or UnauthorizedAccessException or JsonException)
            {
                changes[("Maps", key)] = null;
                rejectDataFile(getSectionDataPath("Maps", key), exception.Message);
            }
        }
        foreach (KeyValuePair<(string Section, string Key), JsonObject?> change in changes)
            if (change.Value is not null && change.Key.Section is "Blueprints" or "Maps")
                clearRejectedDataFile(getSectionDataPath(change.Key.Section, change.Key.Key));
    }

    private void validateBlueprintResourceChanges(
        IReadOnlyDictionary<EditorDocument, (string Key, JsonObject Data)> changes)
    {
        bool blueprintChanged = changes.Keys.Any(document => document.Section == "Blueprints");
        if (!blueprintChanged && !changes.Keys.Any(document => document.Section == "Maps"))
            return;
        Dictionary<string, JsonObject> blueprints = sections["Blueprints"].ToDictionary(
            entry => entry.Key, entry => entry.Value, StringComparer.Ordinal);
        Dictionary<string, JsonObject> maps = new(StringComparer.Ordinal);
        HashSet<string> removedMapKeys = new(StringComparer.Ordinal);
        foreach (KeyValuePair<EditorDocument, (string Key, JsonObject Data)> change in changes)
        {
            if (change.Key.Section == "Blueprints")
            {
                blueprints.Remove(change.Key.Key);
                blueprints[change.Value.Key] = change.Value.Data;
            }
            else if (change.Key.Section == "Maps")
            {
                removedMapKeys.Add(change.Key.Key);
                maps[change.Value.Key] = change.Value.Data;
            }
        }
        if (blueprintChanged)
        {
            Dictionary<string, string> errors = getBlueprintSchemaErrors(blueprints);
            if (errors.Count != 0)
                throw new InvalidDataException(string.Join(Environment.NewLine,
                    errors.Select(entry => $"Data/Blueprints/{entry.Key}.json: {entry.Value}")));
            foreach (string key in getBlueprintMapKeys().Where(key => !removedMapKeys.Contains(key)))
                maps.TryAdd(key, sections["Maps"].TryGetValue(key, out JsonObject? loaded)
                    ? loaded : readMapBlueprintData(Maps.getReadableMapDataPath(key)));
        }
        using BlueprintClassResolver resolver = createBlueprintSchemaResolver(blueprints);
        using IDisposable metadataRead = resolver.BeginBatch();
        foreach ((string key, JsonObject map) in maps)
            validateMapBlueprintData(key, map, resolver);
    }
}
