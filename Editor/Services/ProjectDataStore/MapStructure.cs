using Ludork.Models;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed partial class ProjectDataStore
{
    internal static bool pathsEqual(string left, string right)
    {
        return string.Equals(
            Path.TrimEndingDirectorySeparator(Path.GetFullPath(left)),
            Path.TrimEndingDirectorySeparator(Path.GetFullPath(right)),
            OperatingSystem.IsWindows() ? StringComparison.OrdinalIgnoreCase : StringComparison.Ordinal);
    }

    internal static StringComparer getPathComparer()
    {
        return OperatingSystem.IsWindows() ? StringComparer.OrdinalIgnoreCase : StringComparer.Ordinal;
    }

    internal string getSectionDataPath(string sectionName, string key)
    {
        if (sectionName == "WorldMaps")
        {
            return Path.Combine(
                ProjectPath,
                "Data",
                "Maps",
                key.Replace('/', Path.DirectorySeparatorChar),
                "_world.json");
        }
        return Path.Combine(
            ProjectPath,
            "Data",
            sectionName,
            key.Replace('/', Path.DirectorySeparatorChar) + ".json");
    }

    internal void addInvalidLoadPath(string path)
    {
        string relative = Path.GetRelativePath(ProjectPath, path).Replace('\\', '/');
        if (!invalidLoadPaths.Contains(relative, StringComparer.Ordinal))
            invalidLoadPaths.Add(relative);
    }

}
