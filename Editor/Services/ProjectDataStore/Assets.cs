using Ludork.Models;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;
using System.Text.Json.Nodes;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Services;

public sealed partial class ProjectDataStore
{
    public void Reload()
    {
        using EditorDocumentNotificationBatch notifications = Documents.BeginNotificationBatch();
        loadAll();
        Documents.PublishReset();
        notifications.Commit();
        Maps.NotifyAllMapPreviewsChanged();
        UiAssets.NotifyUiAssetsChanged();
        DataReloaded?.Invoke(this, EventArgs.Empty);
    }

    public void ApplyExternalFileChanges(
        IReadOnlyList<string> addedPaths,
        IReadOnlyList<(string OldPath, string NewPath)> movedPaths,
        IReadOnlyList<string> deletedPaths,
        Func<IReadOnlyList<ReferenceRewrite>>? prepareReferenceChanges = null)
    {
        if (addedPaths.Count == 0 && movedPaths.Count == 0 && deletedPaths.Count == 0
            && prepareReferenceChanges is null)
            return;
        foreach ((string oldPath, string newPath) in movedPaths)
        {
            if (IsManagedPath(oldPath))
                throw new InvalidOperationException("Managed resource moves must use the document rename command.");
        }
        bool externalChanges = addedPaths.Count != 0 || movedPaths.Count != 0 || deletedPaths.Count != 0;
        Dictionary<(string Section, string Key), JsonObject?> changes = prepareExternalChanges(addedPaths, movedPaths, deletedPaths);
        applyExternalChanges(changes, externalChanges, prepareReferenceChanges);
        if (addedPaths.Concat(deletedPaths).Any(path => path.Contains("UI", StringComparison.Ordinal)))
            UiAssets.NotifyUiAssetsChanged();
        if (externalChanges)
            DataReloaded?.Invoke(this, EventArgs.Empty);
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
            if (sectionName == "Maps")
                return Maps.containsMapKey(key);
            if (sectionName == "WorldMaps")
                return sections["WorldMaps"].ContainsKey(WorldDataService.normalizeWorldKey(key));
            return sections[sectionName].ContainsKey(key);
        }
        string prefix = normalizeDataKey(relativePath);
        if (sectionName == "Maps")
        {
            return Maps.getMapCatalogEntries().Any(entry =>
                entry.Kind != MapCatalogEntryKind.WorldMap
                && keyMatchesPrefix(entry.Key, prefix));
        }
        return sections[sectionName].Keys.Any(key => keyMatchesPrefix(key, prefix));
    }

    public DataFileInfo? TryLoadDataFile(string absolutePath) => prepareDataFileRead(absolutePath)();

    public Task<DataFileInfo?> TryLoadDataFileAsync(string absolutePath, CancellationToken cancellationToken = default)
    {
        Func<DataFileInfo?> read = prepareDataFileRead(absolutePath);
        return Task.Run(read, cancellationToken);
    }

    internal Func<DataFileInfo?> prepareDataFileRead(string absolutePath)
    {
        if (DataConfig.isAnimationCache(absolutePath)
            || !string.Equals(Path.GetExtension(absolutePath), DataConfig.DataFileExtension, StringComparison.OrdinalIgnoreCase))
            return () => null;
        EditorDocument? document = Documents.FindByPath(absolutePath);
        if (document?.InternalData is JsonObject current)
        {
            string? currentType = getString(current["type"]);
            DataFileInfo info = new(currentType ?? sections[document.Section].ExpectedType ?? "general", document.Key);
            return () => info;
        }
        bool dataFile = tryGetDataLocation(
            absolutePath,
            out string sectionName,
            out _);
        EditorDocumentCollection? section = dataFile ? sections[sectionName] : null;
        bool textConfigFile = dataFile && sectionName == "TextConfigs";
        bool uiFile = dataFile && sectionName == "UI";
        string? key = getDataKey(absolutePath);
        if (uiFile && !hasDataFileExtension(sectionName, absolutePath))
            return () => new DataFileInfo("invalidUiData", key);
        return () =>
        {
            if (!File.Exists(absolutePath))
                return null;
            try
            {
                if (JsonNode.Parse(File.ReadAllText(absolutePath)) is not JsonObject file)
                {
                    return textConfigFile
                        ? new DataFileInfo("invalidTextConfig", key)
                        : uiFile
                            ? new DataFileInfo("invalidUiData", key)
                        : null;
                }
                string? type = file["type"] is JsonValue typeValue
                    && typeValue.TryGetValue<string>(out string? parsedType)
                        ? parsedType
                        : null;
                if (section is not null && !section.AcceptsType(type))
                {
                    return textConfigFile
                        ? new DataFileInfo("invalidTextConfig", key)
                        : uiFile
                            ? new DataFileInfo("invalidUiData", key)
                        : null;
                }
                string? resolvedType = string.IsNullOrWhiteSpace(type)
                    ? section?.ExpectedType
                    : type;
                if (string.IsNullOrWhiteSpace(resolvedType)
                    && sectionName != "General")
                    return null;
                return new DataFileInfo(resolvedType ?? "general", key);
            }
            catch (JsonException)
            {
                return textConfigFile
                    ? new DataFileInfo("invalidTextConfig", key)
                    : uiFile
                        ? new DataFileInfo("invalidUiData", key)
                    : null;
            }
        };
    }

}
