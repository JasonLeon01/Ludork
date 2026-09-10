using System;
using System.IO;

namespace Ludork.Services;

public static class ScriptMixinPaths
{
    public static string Normalize(string scriptPath)
    {
        string normalized = (scriptPath ?? string.Empty).Trim().Replace('\\', '/');
        if (string.IsNullOrWhiteSpace(normalized))
            return string.Empty;
        if (Path.IsPathRooted(normalized)
            || normalized.Contains(':', StringComparison.Ordinal)
            || !normalized.EndsWith(".lua", StringComparison.Ordinal)
            || normalized.EndsWith("_meta.lua", StringComparison.Ordinal))
            throw new InvalidDataException("scriptPath must be a relative .lua path");

        string[] parts = normalized.Split('/', StringSplitOptions.RemoveEmptyEntries);
        if (parts.Length == 0 || Array.Exists(parts, part => part is "." or ".."))
            throw new InvalidDataException("scriptPath cannot leave Scripts/Mixins");
        return string.Join('/', parts);
    }

    public static string GetScriptsRoot(string projectPath)
    {
        return Path.GetFullPath(Path.Combine(projectPath, "Scripts"));
    }

    public static string GetMixinsRoot(string projectPath)
    {
        return Path.GetFullPath(Path.Combine(GetScriptsRoot(projectPath), "Mixins"));
    }

    public static string GetScriptPath(string projectPath, string scriptPath)
    {
        string normalized = Normalize(scriptPath);
        if (string.IsNullOrEmpty(normalized))
            return string.Empty;
        string root = GetMixinsRoot(projectPath);
        string fullPath = Path.GetFullPath(Path.Combine(root, normalized.Replace('/', Path.DirectorySeparatorChar)));
        string relative = Path.GetRelativePath(root, fullPath);
        if (relative.StartsWith(".." + Path.DirectorySeparatorChar, StringComparison.Ordinal)
            || relative == ".."
            || Path.IsPathRooted(relative))
            throw new InvalidDataException("scriptPath cannot leave Scripts/Mixins");
        return fullPath;
    }

    public static string GetMetadataPath(string projectPath, string scriptPath)
    {
        string scriptFullPath = GetScriptPath(projectPath, scriptPath);
        return string.IsNullOrEmpty(scriptFullPath)
            ? string.Empty
            : scriptFullPath[..^".lua".Length] + "_meta.lua";
    }

    public static string GetModuleName(string scriptPath)
    {
        string normalized = Normalize(scriptPath);
        return "Mixins." + normalized[..^".lua".Length].Replace('/', '.');
    }

    public static string GetTypeName(string scriptPath)
    {
        string normalized = Normalize(scriptPath);
        return Path.GetFileNameWithoutExtension(normalized);
    }
}
