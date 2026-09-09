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
    public bool CreateGeneralType(string key)
    {
        if (!canCreateDocument("General", key))
            return false;
        RecordDocumentSnapshot("General", key);
        JsonObject entry = new()
        {
            ["params"] = new JsonObject(),
            ["members"] = new JsonObject(),
        };
        sections["General"].Data[key] = entry;
        refreshModifiedState();
        return true;
    }

    public bool RenameGeneralType(string oldKey, string newKey)
    {
        return RenameDocumentResource("General", oldKey, newKey);
    }

    public bool DeleteGeneralType(string key)
    {
        return DeleteDocumentResource("General", key);
    }

    public SaveResult SaveAllModified()
    {
        BreakHistoryGesture();
        CompleteDocumentChanges();
        IReadOnlyList<string> errors = GeneralDataSchemaValidation.Validate(sections["General"].Data);
        if (errors.Count != 0)
            return new SaveResult(false, string.Join(Environment.NewLine, errors));
        IReadOnlyList<string> pathErrors = validateGameAssetPaths();
        if (pathErrors.Count != 0)
            return new SaveResult(false, string.Join(Environment.NewLine, pathErrors));
        WorldMapMutationResult worldValidation = ValidateAllWorldMaps();
        if (!worldValidation.Success)
            return new SaveResult(false, worldValidation.Details);
        EditorDocument[] modified = Documents.ModifiedDocuments
            .Where(document => sections.ContainsKey(document.Section)).ToArray();
        EditorFileSaveBatch batch = new();
        List<(string Source, string Destination)> worldMoves = [];
        foreach (EditorDocument document in modified.Where(document => document.Section == "WorldMaps"))
        {
            if (document.SavedState.InternalData is not null && document.Path != document.SavedState.Path)
            {
                string source = Path.GetDirectoryName(document.SavedState.Path)!;
                string destination = Path.GetDirectoryName(document.Path)!;
                worldMoves.Add((source, destination));
                batch.MoveDirectory(source, destination);
            }
        }
        foreach (EditorDocument document in modified)
        {
            foreach (KeyValuePair<string, byte[]> output in document.PrepareSave())
                batch.Write(output.Key, output.Value);
            if (document.SavedState.InternalData is not null)
            {
                string savedPath = document.SavedPath;
                foreach ((string source, string destination) in worldMoves)
                {
                    if (!isDocumentInsidePath(document.SavedPath, source))
                        continue;
                    savedPath = Path.Combine(destination, Path.GetRelativePath(source, document.SavedPath));
                    break;
                }
                if (document.IsDeleted || savedPath != document.Path)
                    batch.Delete(savedPath);
            }
        }
        bool generateGeneral = generalDataGenerationPending || modified.Any(document => document.Section == "General");
        if (generateGeneral)
        {
            try
            {
                foreach (KeyValuePair<string, byte[]> output in generalEnums.PrepareOutputs(sections["General"].Data))
                    batch.Write(output.Key, output.Value);
            }
            catch (Exception exception) when (exception is InvalidDataException or ArgumentException)
            {
                return new SaveResult(false, exception.Message);
            }
        }
        SaveResult result = batch.Execute();
        if (!result.Success)
            return result;
        foreach (EditorDocument document in modified)
            originData[document.SavedState.Section].Remove(document.SavedState.Key);
        foreach (EditorDocument document in modified)
        {
            if (document.InternalData is JsonObject data)
                originData[document.Section][document.Key] = (JsonObject)data.DeepClone();
            MarkDocumentSaved(document.Section, document.Key);
            if (document.Section == "Maps" && document.InternalData is JsonObject map)
                updateLoadedMapMetadata(document.Key, map, document.Path);
        }
        if (generateGeneral)
            generalDataGenerationPending = false;
        originData["MapCatalog"] = sections["MapCatalog"].Data.ToDictionary(
            pair => pair.Key, pair => (JsonObject)pair.Value.DeepClone(), StringComparer.Ordinal);
        pendingWorldDirectoryMoves.Clear();
        refreshModifiedState();
        DataSaved?.Invoke(this, EventArgs.Empty);
        return new SaveResult(true, string.Join(Environment.NewLine,
            modified.Select(document => document.Path).Append(result.Details).Where(value => !string.IsNullOrEmpty(value))));
    }

    private IReadOnlyDictionary<string, byte[]> SerializeDocument(EditorDocument document)
    {
        if (document.InternalData is not JsonObject data)
            return new Dictionary<string, byte[]>();
        JsonObject payload = (JsonObject)data.DeepClone();
        DataSection section = sections[document.Section];
        if (section.WriteType && section.ExpectedType is not null)
            payload["type"] = section.ExpectedType;
        return new Dictionary<string, byte[]>
        {
            [document.Path] = Encoding.UTF8.GetBytes(payload.ToJsonString(WriteOptions) + Environment.NewLine),
        };
    }

    private IReadOnlyList<string> validateGameAssetPaths()
    {
        List<string> errors = [];
        foreach (KeyValuePair<string, JsonObject> entry in sections["Tilesets"].Data)
            validateAssetPath(entry.Value["fileName"], $"Tilesets/{entry.Key}.fileName", errors);
        foreach (KeyValuePair<string, JsonObject> entry in sections["AutoTiles"].Data)
            validateAssetPath(entry.Value["fileName"], $"AutoTiles/{entry.Key}.fileName", errors);
        foreach (KeyValuePair<string, JsonObject> entry in sections["Animations"].Data)
        {
            if (entry.Value["assets"] is not JsonArray assets)
                continue;
            for (int index = 0; index < assets.Count; index++)
                validateAssetPath(assets[index], $"Animations/{entry.Key}.assets[{index}]", errors);
        }
        foreach (KeyValuePair<string, JsonObject> entry in sections["Configs"].Data)
            validateConfigAssetPaths(entry.Key, entry.Value, errors);
        foreach (KeyValuePair<string, JsonObject> entry in sections["Maps"].Data)
            validateMapAssetPaths(entry.Key, entry.Value, errors);
        foreach (KeyValuePair<string, JsonObject> entry in sections["WorldMaps"].Data)
            validateAssetPath(entry.Value["fog"], $"Maps/{entry.Key}/_world.fog", errors);
        foreach (KeyValuePair<string, JsonObject> entry in sections["TextConfigs"].Data)
            validateAssetPath(entry.Value["font"], $"TextConfigs/{entry.Key}.font", errors);
        return errors;
    }

    private static void validateConfigAssetPaths(
        string key,
        JsonObject config,
        ICollection<string> errors)
    {
        foreach (KeyValuePair<string, JsonNode?> entry in config)
        {
            if (entry.Value is not JsonObject setting
                || getString(setting["type"])?.StartsWith("file", StringComparison.Ordinal) != true)
            {
                continue;
            }
            string? root = getString(setting["root"]);
            if (!string.IsNullOrWhiteSpace(root)
                && !string.Equals(root, "Assets", StringComparison.Ordinal))
            {
                continue;
            }
            if (setting["value"] is JsonArray values)
            {
                for (int index = 0; index < values.Count; index++)
                    validateAssetPath(values[index], $"Configs/{key}.{entry.Key}[{index}]", errors);
            }
            else
            {
                validateAssetPath(setting["value"], $"Configs/{key}.{entry.Key}", errors);
            }
        }
    }

    private static void validateMapAssetPaths(
        string key,
        JsonObject map,
        ICollection<string> errors)
    {
        validateAssetPath(map["bgm"], $"Maps/{key}.bgm", errors);
        validateAssetPath(map["bgs"], $"Maps/{key}.bgs", errors);
        validateAssetPath(map["fog"], $"Maps/{key}.fog", errors);
        if (map["layers"] is not JsonObject layers)
            return;
        foreach (KeyValuePair<string, JsonNode?> entry in layers)
        {
            if (entry.Value is JsonObject layer)
            {
                validateAssetPath(
                    layer["shaderPath"],
                    $"Maps/{key}.layers.{entry.Key}.shaderPath",
                    errors);
            }
        }
    }

    private static void validateAssetPath(
        JsonNode? value,
        string path,
        ICollection<string> errors)
    {
        string? text = getString(value);
        if (!string.IsNullOrEmpty(text) && !GameAssetPath.IsCanonical(text))
            errors.Add(path + " must use a canonical /Game/Assets/ path");
    }

    public SaveResult ReplaceMapLayerAndSave(
        string mapKey,
        string layerName,
        JsonArray tiles,
        JsonArray autoTiles)
    {
        BreakHistoryGesture();
        CompleteDocumentChanges();
        EditorDocument? document = GetDocument("Maps", mapKey);
        Dictionary<string, JsonObject> maps = sections["Maps"].Data;
        if (document is null || !maps.TryGetValue(mapKey, out JsonObject? current)
            || current["layers"]?[layerName] is not JsonObject)
        {
            return new SaveResult(false, "The map or layer no longer exists.");
        }
        if (hasPendingPathDependencies(document))
            return new SaveResult(false, "This map has unsaved related resource paths. Save all before running this plug-in operation.");

        JsonObject candidate = (JsonObject)current.DeepClone();
        if (candidate["layers"]?[layerName] is not JsonObject candidateLayer)
            return new SaveResult(false, "The map layer could not be copied.");
        candidateLayer["tiles"] = tiles.DeepClone();
        candidateLayer["autoTiles"] = autoTiles.DeepClone();

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
        using EditorDocumentTransaction transaction = Documents.BeginTransaction([document]);
        RecordMapSnapshot(mapKey);
        maps[mapKey] = candidate;
        CompleteDocumentChanges();
        EditorFileSaveBatch batch = new();
        foreach (KeyValuePair<string, byte[]> output in document.PrepareSave())
            batch.Write(output.Key, output.Value);
        SaveResult result = batch.Execute();
        if (!result.Success)
            return result;
        updateLoadedMapMetadata(mapKey, candidate, path);
        originData["Maps"][mapKey] = (JsonObject)candidate.DeepClone();
        MarkDocumentSaved("Maps", mapKey);
        transaction.Commit();
        refreshModifiedState();
        NotifyMapContentChanged(mapKey);
        DataSaved?.Invoke(this, EventArgs.Empty);
        return result;
    }

    private bool hasPendingPathDependencies(EditorDocument document)
    {
        if (document.SavedState.InternalData is not null && document.Path != document.SavedPath)
            return true;
        EditorDocument[] changedPaths = Documents.ModifiedDocuments.Where(candidate =>
            candidate.Path != candidate.SavedPath || candidate.SavedState.InternalData is null).ToArray();
        if (changedPaths.Length == 0)
            return false;
        LuaMetadataService metadata = new(ProjectPath);
        using BlueprintClassResolver resolver = new(this, metadata);
        using ReferenceIndexService references = new(this, metadata, resolver);
        string? source = references.GetNodeIdForPath(document.Path);
        return source is not null && references.GetOutgoing(source).Any(reference =>
            changedPaths.Any(candidate => pathsEqual(candidate.Path, references.GetNodePath(reference.Target))));
    }

}
