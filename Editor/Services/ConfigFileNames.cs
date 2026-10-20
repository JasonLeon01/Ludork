using System;
using System.IO;

namespace Ludork.Services;

internal static class ConfigFileNames
{
    public static string GetFileName(string name) => name + (IsPropertyList() ? ".plist" : ".ini");

    public static bool IsPropertyList(string? path = null) => path is null
        ? OperatingSystem.IsMacOS()
        : Path.GetExtension(path).Equals(".plist", StringComparison.OrdinalIgnoreCase);
}
