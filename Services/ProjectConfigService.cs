using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed class ProjectConfigService
{
    private readonly string configPath;
    private JsonObject data;

    public ProjectConfigService(string projectPath)
    {
        configPath = Path.Combine(Path.GetFullPath(projectPath), "Main.proj");
        data = File.Exists(configPath) && JsonNode.Parse(File.ReadAllText(configPath)) is JsonObject loaded
            ? loaded
            : [];
        bool changed = false;
        if (data["editor"] is JsonObject editor)
        {
            bool editorChanged = editor.Remove("lockedMapLayers");
            if (editor["actorFavorites"] is JsonArray { Count: 0 })
            {
                editor.Remove("actorFavorites");
                editorChanged = true;
            }
            if (editor.Count == 0)
            {
                data.Remove("editor");
                editorChanged = true;
            }
            changed |= editorChanged;
        }
        bool forcedIndividual = !OperatingSystem.IsWindows();
        if (data["IndividualWindow"] is not JsonValue individualValue
            || !individualValue.TryGetValue(out bool individualWindow)
            || (forcedIndividual && !individualWindow))
        {
            data["IndividualWindow"] = forcedIndividual;
            changed = true;
        }
        if (changed)
            save();
    }

    public bool IsStandalone
    {
        get
        {
            return data["Cpp"] is not JsonValue value
                || !value.TryGetValue(out bool cpp)
                || !cpp;
        }
    }

    public bool FfmpegEnabled
    {
        get
        {
            return data["ffmpeg"] is JsonValue value
                && value.TryGetValue(out bool enabled)
                && enabled;
        }
    }

    public string? LastFileExplorerPath
    {
        get => data["lastFileExplorerPath"]?.GetValue<string>();
        set => setValue("lastFileExplorerPath", value);
    }

    public bool IndividualWindow
    {
        get
        {
            if (!OperatingSystem.IsWindows())
                return true;
            return data["IndividualWindow"] is JsonValue value
                && value.TryGetValue(out bool individualWindow)
                && individualWindow;
        }
        set
        {
            bool resolved = !OperatingSystem.IsWindows() || value;
            if (IndividualWindow == resolved)
                return;
            data["IndividualWindow"] = resolved;
            save();
        }
    }

    public bool CanConfigureIndividualWindow => OperatingSystem.IsWindows();

    public IReadOnlyList<string> GetActorFavorites()
    {
        return readStringSet(getEditor(false)?["actorFavorites"])
            .OrderBy(value => value, StringComparer.Ordinal)
            .ToArray();
    }

    public bool IsActorFavorite(string blueprintReference)
    {
        string normalized = normalizeReference(blueprintReference);
        return normalized.Length != 0
            && readStringSet(getEditor(false)?["actorFavorites"]).Contains(normalized);
    }

    public void SetActorFavorite(string blueprintReference, bool favorite)
    {
        string normalized = normalizeReference(blueprintReference);
        if (normalized.Length == 0)
            return;
        HashSet<string> favorites = readStringSet(getEditor(false)?["actorFavorites"]);
        bool changed = favorite ? favorites.Add(normalized) : favorites.Remove(normalized);
        if (!changed)
            return;
        writeActorFavorites(favorites);
    }

    public void RemapActorFavorite(string oldReference, string newReference)
    {
        string normalizedOld = normalizeReference(oldReference);
        string normalizedNew = normalizeReference(newReference);
        if (normalizedOld.Length == 0 || normalizedNew.Length == 0)
            return;
        HashSet<string> favorites = readStringSet(getEditor(false)?["actorFavorites"]);
        if (!favorites.Remove(normalizedOld))
            return;
        favorites.Add(normalizedNew);
        writeActorFavorites(favorites);
    }

    private void setValue(string key, string? value)
    {
        if (string.IsNullOrWhiteSpace(value))
            data.Remove(key);
        else
            data[key] = value.Replace('\\', '/');
        save();
    }

    private JsonObject? getEditor(bool create)
    {
        if (data["editor"] is JsonObject editor)
            return editor;
        if (!create)
            return null;
        editor = [];
        data["editor"] = editor;
        return editor;
    }

    private void writeActorFavorites(IEnumerable<string> favorites)
    {
        string[] values = favorites
            .Where(value => value.Length != 0)
            .Distinct(StringComparer.Ordinal)
            .OrderBy(value => value, StringComparer.Ordinal)
            .ToArray();
        if (values.Length == 0)
        {
            getEditor(false)?.Remove("actorFavorites");
            cleanupEditorState();
            save();
            return;
        }
        JsonArray array = [];
        foreach (string value in values)
            array.Add(value);
        getEditor(true)!["actorFavorites"] = array;
        save();
    }

    private void cleanupEditorState()
    {
        JsonObject? editor = getEditor(false);
        if (editor is not null && editor.Count == 0)
            data.Remove("editor");
    }

    private static HashSet<string> readStringSet(JsonNode? node)
    {
        HashSet<string> result = new HashSet<string>(StringComparer.Ordinal);
        if (node is not JsonArray values)
            return result;
        foreach (JsonNode? value in values)
        {
            if (value is not JsonValue scalar || !scalar.TryGetValue(out string? text))
                continue;
            string normalized = text?.Trim() ?? string.Empty;
            if (normalized.Length != 0)
                result.Add(normalized);
        }
        return result;
    }

    private static string normalizeReference(string value)
    {
        return (value ?? string.Empty).Trim().Replace('/', '.').Replace('\\', '.');
    }

    private void save() => File.WriteAllText(configPath, data.ToJsonString(new() { WriteIndented = true }));
}
