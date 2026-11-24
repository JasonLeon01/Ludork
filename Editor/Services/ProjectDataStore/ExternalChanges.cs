using Ludork.Models;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed partial class ProjectDataStore
{
    internal Dictionary<(string Section, string Key), JsonObject?> prepareExternalChanges(
        IReadOnlyList<string> addedPaths,
        IReadOnlyList<(string OldPath, string NewPath)> movedPaths,
        IReadOnlyList<string> deletedPaths)
    {
        Dictionary<(string Section, string Key), JsonObject?> changes = [];
        Dictionary<string, string[]> keysBySection = new(StringComparer.Ordinal);
        List<(string Path, string? Section, string Key)> operations = [];
        foreach (string path in deletedPaths)
            operations.Add((path, null, string.Empty));
        foreach ((string oldPath, string newPath) in movedPaths)
        {
            operations.Add((oldPath, null, string.Empty));
            collectFiles(newPath);
        }
        foreach (string path in addedPaths)
            collectFiles(path);
        JsonObject?[] data = EditorValidationBatch.RunFiles(operations.Where(op => op.Section is not null).ToArray(),
            op => readExternalDataFile(op.Path, sections[op.Section!]));
        int index = 0;
        foreach ((string path, string? section, string key) in operations)
        {
            if (section is null)
            {
                prepareExternalDelete(path, changes, keysBySection);
                continue;
            }
            clearRejectedDataFile(path);
            if (data[index++] is JsonObject loaded)
                changes[(section, key)] = loaded;
            else if (section is "Blueprints" or "Maps")
            {
                changes[(section, key)] = null;
                rejectDataFile(path, "The data file could not be parsed or has an unexpected type");
            }
        }
        validateExternalBlueprintChanges(changes);
        foreach ((string section, string key) in changes.Keys)
        {
            if (Documents.Find(section, key) is { IsModified: true } document)
                throw new InvalidOperationException($"The file has unsaved editor changes: {document.Path}");
        }
        return changes;

        void collectFiles(string source)
        {
            IEnumerable<string> files = Directory.Exists(source)
                ? Directory.EnumerateFiles(source, "*.json", SearchOption.AllDirectories) : [source];
            foreach (string path in files)
                if (File.Exists(path) && !DataConfig.isAnimationCache(path)
                    && tryGetDataLocation(path, out string section, out string relativePath)
                    && hasDataFileExtension(section, path))
                    operations.Add((path, section, Path.ChangeExtension(relativePath, null)!.Replace('\\', '/')));
        }
    }

    internal void prepareExternalDelete(string path, IDictionary<(string Section, string Key), JsonObject?> changes,
        Dictionary<string, string[]>? keysBySection = null)
    {
        if (!tryGetDataLocation(path, out string section, out string relativePath))
            return;
        clearRejectedDataFile(path);
        if (hasDataFileExtension(section, path))
        {
            string key = section == "WorldMaps" ? normalizeDataKey(relativePath)
                : Path.ChangeExtension(relativePath, null)!.Replace('\\', '/');
            bool knownFile = sections[section].ContainsKey(key) || originData[section].ContainsKey(key)
                || Documents.Find(section, key) is not null || changes.ContainsKey((section, key));
            changes[(section, key)] = null;
            if (knownFile && !Directory.Exists(path))
                return;
        }
        string prefix = normalizeDataKey(relativePath);
        if (keysBySection is null || !keysBySection.TryGetValue(section, out string[]? sectionKeys))
        {
            sectionKeys = sections[section].Keys.Concat(originData[section].Keys)
                .Concat(Documents.All.Where(document => document.Section == section).Select(document => document.Key))
                .Distinct(StringComparer.Ordinal).ToArray();
            keysBySection?.Add(section, sectionKeys);
        }
        string[] keys = sectionKeys.Concat(changes.Keys.Where(location => location.Section == section).Select(location => location.Key))
            .Where(key => keyMatchesPrefix(key, prefix)).Distinct(StringComparer.Ordinal).ToArray();
        foreach (string key in keys)
            changes[(section, key)] = null;
    }

    internal void applyExternalChanges(IReadOnlyDictionary<(string Section, string Key), JsonObject?> changes,
        bool reset, Func<IReadOnlyList<ReferenceRewrite>>? prepareReferenceChanges)
    {
        List<Action> restoreEntries = [];
        List<EditorDocument> affectedDocuments = [];
        HashSet<(string Section, string Key)> captured = [];
        foreach ((string section, string key) in changes.Keys)
        {
            captured.Add((section, key));
            EditorDocument? document = captureExternalChange(section, key, restoreEntries);
            if (document is not null)
                affectedDocuments.Add(document);
        }
        bool committed = false;
        try
        {
            using EditorDocumentTransaction transaction = Documents.BeginTransaction(affectedDocuments);
            foreach (KeyValuePair<(string Section, string Key), JsonObject?> change in changes)
            {
                (string section, string key) = change.Key;
                if (change.Value is JsonObject data)
                    installExternalDocument(section, key, data);
                else
                    removeDataKey(section, key);
            }
            if (prepareReferenceChanges is not null)
            {
                IReadOnlyList<ReferenceRewrite> rewrites = prepareReferenceChanges();
                foreach (ReferenceRewrite rewrite in rewrites)
                    if (captured.Add((rewrite.Section, rewrite.Key)))
                        captureExternalChange(rewrite.Section, rewrite.Key, restoreEntries);
                ApplyReferenceRewrites(rewrites);
                IReadOnlyList<string> errors = ValidateBlueprintSchemas(includeRejectedFiles: false);
                if (errors.Count != 0)
                    throw new InvalidDataException(string.Join(Environment.NewLine, errors));
            }
            refreshModifiedState();
            if (reset)
                Documents.PublishReset();
            committed = true;
            transaction.Commit();
        }
        finally
        {
            if (!committed)
                foreach (Action restore in restoreEntries)
                    restore();
        }
    }

    internal EditorDocument? captureExternalChange(string section, string key, ICollection<Action> restoreEntries)
    {
        EditorDocumentCollection data = sections[section];
        EditorDocument? document = Documents.Find(section, key);
        EditorDocumentState? current = !data.TryGetValue(key, out JsonObject? value) ? null
            : document?.CaptureState() ?? new EditorDocumentState(section, key, getSectionDataPath(section, key), value);
        restoreEntries.Add(() =>
        {
            if (current is null)
                data.Remove(key);
            else
                data[key] = document?.InternalData ?? current.Data!;
        });
        restoreEntries.Add(captureExternalEntry(originData[section], key));
        if (section is "Maps" or "WorldMaps")
        {
            MapCatalogEntryKind kind = section == "WorldMaps" ? MapCatalogEntryKind.WorldMap
                : key.Contains('/') ? MapCatalogEntryKind.WorldChildMap : MapCatalogEntryKind.StandaloneMap;
            restoreEntries.Add(captureExternalEntry(sections["MapCatalog"], MapDataService.getMapCatalogDataKey(kind, key)));
            restoreEntries.Add(Maps.CaptureCacheState(key));
            restoreEntries.Add(Worlds.CaptureDirectoryMove(key));
        }
        return document;
    }

    internal static Action captureExternalEntry<TValue>(IDictionary<string, TValue> data, string key)
    {
        bool exists = data.TryGetValue(key, out TValue? value);
        return () =>
        {
            if (exists)
                data[key] = value!;
            else
                data.Remove(key);
        };
    }

}
