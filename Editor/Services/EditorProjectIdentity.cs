using System;
using System.IO;
using System.Security.Cryptography;
using System.Text;

namespace Ludork.Services;

internal static class EditorProjectIdentity
{
    public static string NormalizePath(string projectPath)
    {
        string fullPath = Path.TrimEndingDirectorySeparator(Path.GetFullPath(projectPath));
        return OperatingSystem.IsWindows() ? fullPath.ToUpperInvariant() : fullPath;
    }

    public static string GetKey(string projectPath)
    {
        string normalized = NormalizePath(projectPath);
        string hash = Convert.ToHexString(SHA256.HashData(Encoding.UTF8.GetBytes(normalized)))
            [..16].ToLowerInvariant();
        string name = Path.GetFileName(normalized);
        StringBuilder safeName = new();
        foreach (char character in name)
        {
            if (safeName.Length == 48)
                break;
            safeName.Append(char.IsLetterOrDigit(character) || character is '-' or '_' ? character : '_');
        }
        return $"{(safeName.Length == 0 ? "Project" : safeName.ToString())}-{hash}";
    }
}
