using Ludork.Models;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed partial class GameDataService
{
    private Dictionary<(string Section, string Key), JsonObject?> prepareExternalChanges(
        IReadOnlyList<string> addedPaths,
        IReadOnlyList<(string OldPath, string NewPath)> movedPaths,
        IReadOnlyList<string> deletedPaths)
    {
        Dictionary<(string Section, string Key), JsonObject?> changes = [];
        foreach (string path in deletedPaths)
            prepareExternalDelete(path, changes);
        foreach ((string oldPath, string newPath) in movedPaths)
        {
            prepareExternalDelete(oldPath, changes);
            prepareExternalAdd(newPath, changes);
        }
        foreach (string path in addedPaths)
            prepareExternalAdd(path, changes);
        foreach ((string section, string key) in changes.Keys)
        {
            if (Documents.Find(section, key) is { IsModified: true } document)
                throw new InvalidOperationException($"The file has unsaved editor changes: {document.Path}");
        }
        return changes;
    }

    private void prepareExternalDelete(string path, IDictionary<(string Section, string Key), JsonObject?> changes)
    {
        if (!tryGetDataLocation(path, out string section, out string relativePath))
            return;
        if (hasDataFileExtension(section, path))
            changes[(section, Path.ChangeExtension(relativePath, null)!.Replace('\\', '/'))] = null;
        string prefix = normalizeDataKey(relativePath);
        string[] keys = sections[section].Data.Keys.Concat(originData[section].Keys)
            .Concat(Documents.All.Where(document => document.Section == section).Select(document => document.Key))
            .Concat(changes.Keys.Where(location => location.Section == section).Select(location => location.Key))
            .Where(key => keyMatchesPrefix(key, prefix)).Distinct(StringComparer.Ordinal).ToArray();
        foreach (string key in keys)
            changes[(section, key)] = null;
    }

    private void prepareExternalAdd(string path, IDictionary<(string Section, string Key), JsonObject?> changes)
    {
        if (Directory.Exists(path))
        {
            foreach (string filePath in Directory.EnumerateFiles(path, "*.json", SearchOption.AllDirectories)
                         .Where(filePath => !DataConfig.isAnimationCache(filePath)))
                prepareExternalAdd(filePath, changes);
            return;
        }
        if (!File.Exists(path) || DataConfig.isAnimationCache(path)
            || !tryGetDataLocation(path, out string section, out string relativePath)
            || !hasDataFileExtension(section, path))
            return;
        if (readExternalDataFile(path, sections[section]) is JsonObject data)
            changes[(section, Path.ChangeExtension(relativePath, null)!.Replace('\\', '/'))] = data;
    }

    private void applyExternalChanges(IReadOnlyDictionary<(string Section, string Key), JsonObject?> changes,
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

    private EditorDocument? captureExternalChange(string section, string key, ICollection<Action> restoreEntries)
    {
        Dictionary<string, JsonObject> data = sections[section].Data;
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
            restoreEntries.Add(captureExternalEntry(sections["MapCatalog"].Data, getMapCatalogDataKey(kind, key)));
            restoreEntries.Add(captureExternalEntry(mapAccessOrder, key));
            restoreEntries.Add(captureExternalEntry(mapLoadedBytes, key));
            restoreEntries.Add(captureExternalEntry(mapActorTagIndexes, key));
            restoreEntries.Add(captureExternalEntry(pendingWorldDirectoryMoves, key));
        }
        return document;
    }

    private static Action captureExternalEntry<TValue>(IDictionary<string, TValue> data, string key)
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
