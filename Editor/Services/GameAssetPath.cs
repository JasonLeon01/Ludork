using System;
using System.IO;
using System.Linq;

namespace Ludork.Services;

public static class GameAssetPath
{
    public const string Root = "/Game/Assets";
    private const string Prefix = Root + "/";

    public static bool IsCanonical(string? value)
    {
        return TryGetRelativePath(value, out _);
    }

    public static bool IsValidBaseHint(string? value)
    {
        if (string.IsNullOrEmpty(value))
            return true;
        if (value.StartsWith("/", StringComparison.Ordinal))
            return TryGetDirectoryRelativePath(value, out _);
        return value == value.Trim()
            && !value.Contains('\\')
            && isCanonicalRelativePath(value);
    }

    public static bool TryGetRelativePath(string? value, out string relativePath)
    {
        relativePath = string.Empty;
        if (string.IsNullOrEmpty(value)
            || value != value.Trim()
            || value.Contains('\\')
            || !value.StartsWith(Prefix, StringComparison.Ordinal))
        {
            return false;
        }
        string relative = value[Prefix.Length..];
        if (!isCanonicalRelativePath(relative)
            || Path.GetExtension(relative).Length <= 1)
        {
            return false;
        }
        relativePath = relative;
        return true;
    }

    public static bool TryGetDirectoryRelativePath(string? value, out string relativePath)
    {
        relativePath = string.Empty;
        if (string.Equals(value, Root, StringComparison.Ordinal))
            return true;
        if (string.IsNullOrEmpty(value)
            || value != value.Trim()
            || value.Contains('\\')
            || !value.StartsWith(Prefix, StringComparison.Ordinal))
        {
            return false;
        }
        string relative = value[Prefix.Length..];
        if (!isCanonicalRelativePath(relative))
            return false;
        relativePath = relative;
        return true;
    }

    public static string FromProjectFile(string projectDirectory, string filePath)
    {
        if (!TryFromProjectFile(projectDirectory, filePath, out string logicalPath))
            throw new ArgumentException("The file must be contained by the project Assets directory.", nameof(filePath));
        return logicalPath;
    }

    public static bool TryFromProjectFile(
        string projectDirectory,
        string filePath,
        out string logicalPath)
    {
        logicalPath = string.Empty;
        if (!tryGetAssetsRoot(projectDirectory, out string assetsRoot))
            return false;
        string fullPath;
        string relative;
        try
        {
            fullPath = Path.GetFullPath(filePath);
            relative = Path.GetRelativePath(assetsRoot, fullPath);
        }
        catch (Exception exception) when (isPathException(exception))
        {
            return false;
        }
        relative = relative.Replace('\\', '/');
        if (!isCanonicalRelativePath(relative)
            || Path.GetExtension(relative).Length <= 1
            || !File.Exists(fullPath)
            || !hasExactAssetsDirectory(projectDirectory)
            || !hasExactCase(assetsRoot, relative, false))
        {
            return false;
        }
        logicalPath = Prefix + relative;
        return true;
    }

    public static bool TryToProjectFile(
        string projectDirectory,
        string? logicalPath,
        out string filePath)
    {
        filePath = string.Empty;
        if (!TryGetRelativePath(logicalPath, out string relativePath)
            || !tryGetAssetsRoot(projectDirectory, out string assetsRoot))
        {
            return false;
        }
        try
        {
            string candidate = Path.GetFullPath(Path.Combine(
                assetsRoot,
                relativePath.Replace('/', Path.DirectorySeparatorChar)));
            if (!isContainedBy(assetsRoot, candidate))
                return false;
            filePath = candidate;
            return true;
        }
        catch (Exception exception) when (isPathException(exception))
        {
            return false;
        }
    }

    public static bool TryResolveExistingFile(
        string projectDirectory,
        string? logicalPath,
        out string filePath)
    {
        filePath = string.Empty;
        if (!TryGetRelativePath(logicalPath, out string relativePath)
            || !TryToProjectFile(projectDirectory, logicalPath, out string candidate)
            || !File.Exists(candidate)
            || !tryGetAssetsRoot(projectDirectory, out string assetsRoot)
            || !hasExactAssetsDirectory(projectDirectory)
            || !hasExactCase(assetsRoot, relativePath, false))
        {
            return false;
        }
        filePath = candidate;
        return true;
    }

    public static bool TryResolveSelectionDirectory(
        string projectDirectory,
        string? logicalDirectory,
        out string directoryPath)
    {
        directoryPath = string.Empty;
        if (!TryGetDirectoryRelativePath(logicalDirectory, out string relativePath)
            || !tryGetAssetsRoot(projectDirectory, out string assetsRoot))
        {
            return false;
        }
        try
        {
            string candidate = relativePath.Length == 0
                ? assetsRoot
                : Path.GetFullPath(Path.Combine(
                    assetsRoot,
                    relativePath.Replace('/', Path.DirectorySeparatorChar)));
            if (!isContainedBy(assetsRoot, candidate))
                return false;
            if (Directory.Exists(candidate)
                && (!hasExactAssetsDirectory(projectDirectory)
                    || relativePath.Length != 0
                    && !hasExactCase(assetsRoot, relativePath, true)))
            {
                return false;
            }
            directoryPath = candidate;
            return true;
        }
        catch (Exception exception) when (isPathException(exception))
        {
            return false;
        }
    }

    public static bool TryResolveBaseHint(
        string projectDirectory,
        string? baseHint,
        out string directoryPath)
    {
        if (string.IsNullOrEmpty(baseHint))
            return TryResolveSelectionDirectory(projectDirectory, Root, out directoryPath);
        if (baseHint.StartsWith("/", StringComparison.Ordinal))
            return TryResolveSelectionDirectory(projectDirectory, baseHint, out directoryPath);
        directoryPath = string.Empty;
        if (baseHint != baseHint.Trim()
            || baseHint.Contains('\\')
            || !isCanonicalRelativePath(baseHint)
            || !tryGetAssetsRoot(projectDirectory, out string assetsRoot))
        {
            return false;
        }
        try
        {
            string candidate = Path.GetFullPath(Path.Combine(
                assetsRoot,
                baseHint.Replace('/', Path.DirectorySeparatorChar)));
            if (!isContainedBy(assetsRoot, candidate))
                return false;
            if (Directory.Exists(candidate)
                && (!hasExactAssetsDirectory(projectDirectory)
                    || !hasExactCase(assetsRoot, baseHint, true)))
            {
                return false;
            }
            directoryPath = candidate;
            return true;
        }
        catch (Exception exception) when (isPathException(exception))
        {
            return false;
        }
    }

    private static bool tryGetAssetsRoot(string projectDirectory, out string assetsRoot)
    {
        assetsRoot = string.Empty;
        if (string.IsNullOrWhiteSpace(projectDirectory))
            return false;
        try
        {
            assetsRoot = Path.GetFullPath(Path.Combine(projectDirectory, "Assets"));
            if (Directory.Exists(assetsRoot)
                && (File.GetAttributes(assetsRoot) & FileAttributes.ReparsePoint) != 0)
            {
                assetsRoot = string.Empty;
                return false;
            }
            return true;
        }
        catch (Exception exception) when (isPathException(exception)
            || exception is IOException or UnauthorizedAccessException)
        {
            return false;
        }
    }

    private static bool isCanonicalRelativePath(string value)
    {
        if (value.Length == 0)
            return false;
        if (value.StartsWith("/", StringComparison.Ordinal)
            || value.EndsWith("/", StringComparison.Ordinal))
        {
            return false;
        }
        string[] parts = value.Split('/');
        return !parts[0].EndsWith(".ldpak", StringComparison.OrdinalIgnoreCase)
            && parts.All(part => part.Length != 0
            && part is not "." and not ".."
            && !part.Contains('\0'));
    }

    private static bool isContainedBy(string root, string path)
    {
        string relative = Path.GetRelativePath(root, path);
        return !Path.IsPathRooted(relative)
            && relative != ".."
            && !relative.StartsWith(".." + Path.DirectorySeparatorChar, StringComparison.Ordinal)
            && !relative.StartsWith("../", StringComparison.Ordinal);
    }

    private static bool hasExactCase(string root, string relativePath, bool directory)
    {
        string current = root;
        string[] parts = relativePath.Split('/');
        for (int index = 0; index < parts.Length; index++)
        {
            if (!Directory.Exists(current))
                return false;
            string? match;
            try
            {
                match = Directory.EnumerateFileSystemEntries(current)
                    .FirstOrDefault(path => string.Equals(
                        Path.GetFileName(path),
                        parts[index],
                        StringComparison.Ordinal));
            }
            catch (Exception exception) when (exception is IOException or UnauthorizedAccessException)
            {
                return false;
            }
            if (match is null)
                return false;
            try
            {
                if ((File.GetAttributes(match) & FileAttributes.ReparsePoint) != 0)
                    return false;
            }
            catch (Exception exception) when (exception is IOException or UnauthorizedAccessException)
            {
                return false;
            }
            current = match;
        }
        return directory ? Directory.Exists(current) : File.Exists(current);
    }

    private static bool hasExactAssetsDirectory(string projectDirectory)
    {
        try
        {
            string project = Path.GetFullPath(projectDirectory);
            return Directory.Exists(project)
                && Directory.EnumerateDirectories(project)
                    .Any(path => string.Equals(
                        Path.GetFileName(path),
                        "Assets",
                        StringComparison.Ordinal));
        }
        catch (Exception exception) when (exception is IOException
            or UnauthorizedAccessException
            or ArgumentException
            or NotSupportedException
            or PathTooLongException)
        {
            return false;
        }
    }

    private static bool isPathException(Exception exception)
    {
        return exception is ArgumentException or NotSupportedException or PathTooLongException;
    }
}
