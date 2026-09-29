using System;
using System.IO;
using System.Linq;

namespace Ludork.Services;

public static class GameDataPath
{
    public const string Root = "Data";
    public const string Subtitles = Root + "/Subtitles";

    public static bool IsCanonical(string? value, string directory = Root)
    {
        return isDirectory(directory)
            && value is not null
            && isDirectory(value)
            && value.StartsWith(directory + "/", StringComparison.Ordinal)
            && Path.GetExtension(value).Length > 1;
    }

    public static bool TryGetSubtitleKey(string? value, out string key)
    {
        key = string.Empty;
        if (!IsCanonical(value, Subtitles)
            || !value!.EndsWith(".json", StringComparison.Ordinal))
            return false;
        key = value[(Subtitles.Length + 1)..^".json".Length];
        return key.Length != 0 && !key.EndsWith("/", StringComparison.Ordinal);
    }

    public static bool TryResolveSelectionDirectory(string projectDirectory, string? directory, out string directoryPath)
    {
        directoryPath = string.Empty;
        return isDirectory(directory)
            && EditorPathSandbox.TryResolve(projectDirectory, directory!, out directoryPath, allowMissing: true);
    }

    public static bool TryResolveExistingFile(string projectDirectory, string? value, out string filePath)
    {
        filePath = string.Empty;
        return IsCanonical(value)
            && EditorPathSandbox.TryResolve(projectDirectory, value!, out filePath)
            && File.Exists(filePath);
    }

    public static bool TryFromProjectFile(string projectDirectory, string filePath, out string dataPath)
    {
        dataPath = string.Empty;
        if (!EditorPathSandbox.TryResolve(projectDirectory, filePath, out string fullPath)
            || !File.Exists(fullPath))
            return false;
        string relative = Path.GetRelativePath(projectDirectory, fullPath).Replace('\\', '/');
        if (!IsCanonical(relative))
            return false;
        dataPath = relative;
        return true;
    }

    private static bool isDirectory(string? value)
    {
        return !string.IsNullOrEmpty(value)
            && value == value.Trim()
            && !value.Contains('\\')
            && !value.Contains(':')
            && !value.Any(char.IsControl)
            && (value == Root || value.StartsWith(Root + "/", StringComparison.Ordinal))
            && value.Split('/').All(part => part.Length != 0 && part is not "." and not "..");
    }
}
