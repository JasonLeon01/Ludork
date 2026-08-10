using System;
using System.IO;
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
        bool forcedIndividual = !OperatingSystem.IsWindows();
        if (data["IndividualWindow"] is not JsonValue individualValue
            || !individualValue.TryGetValue(out bool individualWindow)
            || (forcedIndividual && !individualWindow))
        {
            data["IndividualWindow"] = forcedIndividual;
            save();
        }
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

    private void setValue(string key, string? value)
    {
        if (string.IsNullOrWhiteSpace(value))
            data.Remove(key);
        else
            data[key] = value.Replace('\\', '/');
        save();
    }

    private void save() => File.WriteAllText(configPath, data.ToJsonString(new() { WriteIndented = true }));
}
