using Ludork.Models;
using System;
using System.Collections.Generic;
using System.IO;
using System.Numerics;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed class ProjectConfigService
{
    private static readonly JsonSerializerOptions ViewportJsonOptions = new(JsonSerializerDefaults.Web);
    private readonly string configPath;
    private readonly (BigInteger Major, BigInteger Minor) projectVersion;
    private JsonObject data;

    public ProjectConfigService(string projectPath)
    {
        configPath = Path.Combine(Path.GetFullPath(projectPath), "Main.proj");
        data = File.Exists(configPath) && JsonNode.Parse(File.ReadAllText(configPath)) is JsonObject loaded
            ? loaded
            : [];
        string? version = data["version"] is JsonValue versionValue && versionValue.TryGetValue(out string? text)
            ? text : null;
        if (!ProjectVersionService.TryParse(version, out projectVersion))
            throw new InvalidDataException("The project version must use the v<major>.<minor> format.");
        Version = version!;
        bool changed = false;
        if (data["editor"] is JsonObject editor)
        {
            bool editorChanged = editor.Remove("lockedMapLayers");
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

    public string Version { get; }

    public bool IsVersionInRange(string minimumVersion, string? maximumVersion = null)
    {
        if (!ProjectVersionService.TryParse(minimumVersion, out (BigInteger Major, BigInteger Minor) minimum))
            throw new ArgumentException("The minimum version must use the v<major>.<minor> format.", nameof(minimumVersion));
        if (string.IsNullOrWhiteSpace(maximumVersion))
            return projectVersion.CompareTo(minimum) >= 0;
        if (!ProjectVersionService.TryParse(maximumVersion, out (BigInteger Major, BigInteger Minor) maximum))
            throw new ArgumentException("The maximum version must use the v<major>.<minor> format.", nameof(maximumVersion));
        if (maximum.CompareTo(minimum) < 0)
            throw new ArgumentException("The maximum version must not be lower than the minimum version.", nameof(maximumVersion));
        return projectVersion.CompareTo(minimum) >= 0 && projectVersion.CompareTo(maximum) <= 0;
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

    public string PackagingVersion => data["packaging"] is JsonObject packaging
        && packaging["version"] is JsonValue value && value.TryGetValue(out string? version)
            ? version ?? "1.0.0" : "1.0.0";

    public bool PackagingDev => !IsStandalone && data["packaging"] is JsonObject packaging
        && packaging["dev"] is JsonValue value && value.TryGetValue(out bool dev) && dev;

    public void SetPackaging(string version, bool dev)
    {
        JsonObject updated = (JsonObject)data.DeepClone();
        if (updated["packaging"] is not JsonObject packaging)
        {
            packaging = [];
            updated["packaging"] = packaging;
        }
        packaging["version"] = version;
        packaging["dev"] = !IsStandalone && dev;
        if (JsonNode.DeepEquals(data, updated))
            return;
        save(updated);
    }

    public FileExplorerWorkspaceState? FileExplorerWorkspace
    {
        get
        {
            if (getEditor(false)?["fileExplorer"] is not JsonObject workspace
                || workspace["tabs"] is not JsonArray tabs)
                return null;
            List<FileExplorerTabState> states = [];
            foreach (JsonNode? node in tabs)
            {
                if (node is not JsonObject tab
                    || tab["number"] is not JsonValue numberValue || !numberValue.TryGetValue(out int number)
                    || number < 1
                    || tab["path"] is not JsonValue pathValue || !pathValue.TryGetValue(out string? path)
                    || string.IsNullOrWhiteSpace(path))
                    continue;
                bool iconView = tab["iconView"] is not JsonValue iconValue
                    || !iconValue.TryGetValue(out bool icons) || icons;
                double zoom = tab["zoom"] is JsonValue zoomValue && zoomValue.TryGetValue(out double scale)
                    ? scale : 100;
                bool sourcesExpanded = tab["sourcesExpanded"] is JsonValue sourcesValue
                    && sourcesValue.TryGetValue(out bool expanded) && expanded;
                states.Add(new FileExplorerTabState(number, path, iconView, zoom, sourcesExpanded));
            }
            int active = workspace["activeTabNumber"] is JsonValue activeValue
                && activeValue.TryGetValue(out int activeNumber) ? activeNumber : 1;
            return new FileExplorerWorkspaceState(states, active);
        }
        set
        {
            JsonObject updated = (JsonObject)data.DeepClone();
            if (updated["editor"] is not JsonObject editor)
            {
                editor = [];
                updated["editor"] = editor;
            }
            if (value is null)
            {
                editor.Remove("fileExplorer");
            }
            else
            {
                JsonArray tabs = [];
                foreach (FileExplorerTabState tab in value.Tabs)
                {
                    tabs.Add(new JsonObject
                    {
                        ["number"] = tab.Number,
                        ["path"] = tab.Path.Replace('\\', '/'),
                        ["iconView"] = tab.IconView,
                        ["zoom"] = tab.Zoom,
                        ["sourcesExpanded"] = tab.SourcesExpanded,
                    });
                }
                editor["fileExplorer"] = new JsonObject
                {
                    ["tabs"] = tabs,
                    ["activeTabNumber"] = value.ActiveTabNumber,
                };
            }
            removeObsoleteEditorState(updated);
            if (!JsonNode.DeepEquals(data, updated))
                save(updated);
        }
    }

    public string? LastOpenedMapKey
    {
        get => data["lastOpenedMapKey"] is JsonValue value && value.TryGetValue(out string? key) ? key : null;
        set
        {
            string? key = string.IsNullOrWhiteSpace(value) ? null : value.Replace('\\', '/');
            if (LastOpenedMapKey == key)
                return;
            setValue("lastOpenedMapKey", key);
        }
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
            if (!resolved)
                data["LiveDebug"] = false;
            save();
        }
    }

    public bool CanConfigureIndividualWindow => OperatingSystem.IsWindows();

    public MapPreviewOptions ViewportOptions
    {
        get => getEditor(false)?["mapViewport"]?.Deserialize<MapPreviewOptions>(ViewportJsonOptions) ?? new();
        set
        {
            if (ViewportOptions == value)
                return;
            getEditor(true)!["mapViewport"] = JsonSerializer.SerializeToNode(value, ViewportJsonOptions);
            save();
        }
    }

    public bool LiveDebug
    {
        get => IndividualWindow && data["LiveDebug"] is JsonValue value
            && value.TryGetValue(out bool enabled) && enabled;
        set
        {
            bool resolved = IndividualWindow && value;
            if (LiveDebug == resolved)
                return;
            data["LiveDebug"] = resolved;
            save();
        }
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

    private void save() => save(data);

    private void save(JsonObject updated)
    {
        removeObsoleteEditorState(updated);
        File.WriteAllText(configPath, updated.ToJsonString(new() { WriteIndented = true }));
        data = updated;
    }

    private static void removeObsoleteEditorState(JsonObject updated)
    {
        updated.Remove("lastFileExplorerPath");
        if (updated["editor"] is not JsonObject editor)
            return;
        editor.Remove("actorFavorites");
        if (editor.Count == 0)
            updated.Remove("editor");
    }
}
