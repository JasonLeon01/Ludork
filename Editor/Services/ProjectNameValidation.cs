using System;
using System.Collections.Generic;
using System.IO;

namespace Ludork.Services;

internal static class ProjectNameValidation
{
    private static readonly HashSet<string> reservedNames = new(StringComparer.OrdinalIgnoreCase)
    {
        "CON", "PRN", "AUX", "NUL",
        "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
        "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9",
    };

    public static bool IsValidDirectoryName(string name)
    {
        if (name.Length == 0 || name is "." or ".." || name.IndexOfAny(Path.GetInvalidFileNameChars()) >= 0)
            return false;
        if (!OperatingSystem.IsWindows())
            return true;
        string stem = name.Split('.')[0].TrimEnd(' ');
        return !name.EndsWith('.') && !name.EndsWith(' ') && !reservedNames.Contains(stem);
    }

    public static string? GetAppNameError(string value)
    {
        string name = value.Trim();
        if (name.Length == 0)
            return "NEW_PROJECT_APP_NAME_REQUIRED";
        if (!IsValidDirectoryName(name) || name.IndexOfAny(['/', '\\', ':']) >= 0)
            return "NEW_PROJECT_APP_NAME_INVALID";
        foreach (char character in name)
            if (character < 32 || character == 127)
                return "NEW_PROJECT_APP_NAME_INVALID";
        return null;
    }
}
