using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Text.Json;

namespace Ludork.Services.Plugins;

internal sealed record PluginPackage(
    PluginManifest Manifest,
    string SourcePath,
    IReadOnlyList<string> SourceFiles);

internal static class PluginPackageInspector
{
    private const long MaximumManifestBytes = 1024 * 1024;

    private static readonly HashSet<string> UnsupportedExtensions = new(
        [".fsproj", ".vbproj", ".xaml", ".axaml"],
        StringComparer.OrdinalIgnoreCase);
    private static readonly HashSet<string> ExcludedSourceDirectories = new(
        ["bin", "obj", ".git", ".vs"],
        StringComparer.OrdinalIgnoreCase);
    private static readonly string EditorAssemblyFileName =
        (typeof(PluginPackageInspector).Assembly.GetName().Name ?? "Ludork") + ".dll";

    private static readonly JsonSerializerOptions ManifestSerializerOptions = new()
    {
        PropertyNameCaseInsensitive = false,
    };

    public static PluginPackage Inspect(string sourcePath)
    {
        string root = Path.GetFullPath(sourcePath);
        if (!Directory.Exists(root))
            throw new InvalidDataException($"Plugin directory does not exist: {root}");
        return inspectDirectory(root);
    }

    public static bool IsSafeDirectoryName(string directory)
    {
        if (string.IsNullOrWhiteSpace(directory)
            || directory == "."
            || directory == ".."
            || directory.Equals(".data", StringComparison.OrdinalIgnoreCase)
            || Path.IsPathRooted(directory)
            || directory.IndexOfAny(Path.GetInvalidFileNameChars()) >= 0
            || directory.Contains(Path.DirectorySeparatorChar)
            || directory.Contains(Path.AltDirectorySeparatorChar))
        {
            return false;
        }
        return string.Equals(Path.GetFileName(directory), directory, StringComparison.Ordinal);
    }

    public static bool IsValidIdentifier(string id)
    {
        if (string.IsNullOrWhiteSpace(id)
            || id == "."
            || id == ".."
            || id.EndsWith(".", StringComparison.Ordinal))
        {
            return false;
        }
        foreach (char character in id)
        {
            if (!char.IsLetterOrDigit(character)
                && character != '.'
                && character != '-'
                && character != '_')
            {
                return false;
            }
        }
        return true;
    }

    public static void EnsureManagedDirectoryIsSafe(string pluginsDirectory, string directory)
    {
        if (!IsSafeDirectoryName(directory))
            throw new InvalidDataException($"Invalid plugin directory in registry: {directory}");
        string pluginsRoot = Path.GetFullPath(pluginsDirectory);
        ensureExistingDirectoryIsNotReparsePoint(pluginsRoot);
        string root = appendDirectorySeparator(pluginsRoot);
        string path = Path.GetFullPath(Path.Combine(pluginsRoot, directory));
        StringComparison comparison = OperatingSystem.IsWindows()
            ? StringComparison.OrdinalIgnoreCase
            : StringComparison.Ordinal;
        if (!path.StartsWith(root, comparison))
            throw new InvalidDataException($"Plugin path escapes the managed directory: {directory}");
    }

    public static void EnsureDirectoryTreeIsSafe(string rootPath)
    {
        ensureDirectoryTreeIsSafe(Path.GetFullPath(rootPath));
    }

    public static bool IsEditorVersionSupported(string minimumEditorVersion)
    {
        if (!Version.TryParse(minimumEditorVersion, out Version? minimum))
            return false;
        Version current = Assembly.GetEntryAssembly()?.GetName().Version ?? new Version(1, 0, 0);
        return current >= minimum;
    }

    private static PluginPackage inspectDirectory(string root)
    {
        ensureDirectoryTreeIsSafe(root);
        string manifestPath = Path.Combine(root, "plugin.json");
        if (!File.Exists(manifestPath))
            throw new InvalidDataException($"Plugin manifest was not found: {manifestPath}");
        if (new FileInfo(manifestPath).Length > MaximumManifestBytes)
            throw new InvalidDataException("Plugin manifest is too large.");

        PluginManifest manifest = deserializeManifest(File.ReadAllText(manifestPath, Encoding.UTF8));
        validateManifest(manifest);

        IReadOnlyList<string> sourceFiles = enumerateSourceFiles(root);
        foreach (string path in Directory.EnumerateFiles(root, "*", SearchOption.AllDirectories))
        {
            string relativePath = Path.GetRelativePath(root, path);
            if (UnsupportedExtensions.Contains(Path.GetExtension(path)))
            {
                throw new InvalidDataException($"Unsupported plugin file: {relativePath}");
            }
            if (isHostSharedAssembly(relativePath))
            {
                throw new InvalidDataException(
                    $"Host-shared assemblies cannot be included with a plugin: {relativePath}");
            }
        }

        if (Directory
            .EnumerateDirectories(root, "*", SearchOption.AllDirectories)
            .Any(path => Path.GetFileName(path).Equals(
                ".data",
                StringComparison.OrdinalIgnoreCase)))
        {
            throw new InvalidDataException("Plugin directories cannot contain a .data directory.");
        }
        if (sourceFiles.Count == 0)
            throw new InvalidDataException("A plugin must contain C# source.");

        return new PluginPackage(
            manifest,
            root,
            sourceFiles);
    }

    private static PluginManifest deserializeManifest(string json)
    {
        PluginManifest? manifest = JsonSerializer.Deserialize<PluginManifest>(
            json,
            ManifestSerializerOptions);
        if (manifest is null)
            throw new InvalidDataException("Plugin manifest is empty.");
        return manifest;
    }

    private static void validateManifest(PluginManifest manifest)
    {
        if (manifest.SchemaVersion != 1)
            throw new InvalidDataException($"Unsupported plugin manifest schema: {manifest.SchemaVersion}");
        if (!IsValidIdentifier(manifest.Id))
            throw new InvalidDataException($"Invalid plugin ID: {manifest.Id}");
        if (string.IsNullOrWhiteSpace(manifest.Name))
            throw new InvalidDataException("Plugin name is required.");
        if (!Version.TryParse(manifest.Version, out Version? _))
            throw new InvalidDataException($"Invalid plugin version: {manifest.Version}");
        if (!Version.TryParse(manifest.MinimumEditorVersion, out Version? _))
        {
            throw new InvalidDataException(
                $"Invalid minimum editor version: {manifest.MinimumEditorVersion}");
        }
        if (!IsEditorVersionSupported(manifest.MinimumEditorVersion))
        {
            throw new InvalidDataException(
                $"Plugin requires editor version {manifest.MinimumEditorVersion} or newer.");
        }
        if (string.IsNullOrWhiteSpace(manifest.EntryType))
            throw new InvalidDataException("Plugin entryType is required.");
    }

    private static IReadOnlyList<string> enumerateSourceFiles(string root)
    {
        List<string> sourceFiles = [];
        Stack<string> directories = new();
        directories.Push(root);
        while (directories.Count != 0)
        {
            string directory = directories.Pop();
            sourceFiles.AddRange(Directory.EnumerateFiles(
                directory,
                "*.cs",
                SearchOption.TopDirectoryOnly));
            foreach (string child in Directory.EnumerateDirectories(
                         directory,
                         "*",
                         SearchOption.TopDirectoryOnly))
            {
                if (!ExcludedSourceDirectories.Contains(Path.GetFileName(child)))
                    directories.Push(child);
            }
        }
        return sourceFiles
            .OrderBy(
                path => Path.GetRelativePath(root, path),
                StringComparer.Ordinal)
            .ToArray();
    }

    private static bool isHostSharedAssembly(string relativePath)
    {
        string fileName = Path.GetFileName(relativePath);
        if (!string.Equals(
                Path.GetExtension(fileName),
                ".dll",
                StringComparison.OrdinalIgnoreCase))
        {
            return false;
        }
        return fileName.Equals(
                "Ludork.Plugin.Abstractions.dll",
                StringComparison.OrdinalIgnoreCase)
            || fileName.Equals(
                "Ludork.Plugin.Avalonia.dll",
                StringComparison.OrdinalIgnoreCase)
            || fileName.Equals(
                EditorAssemblyFileName,
                StringComparison.OrdinalIgnoreCase)
            || fileName.StartsWith("Avalonia", StringComparison.OrdinalIgnoreCase);
    }

    private static void ensureDirectoryTreeIsSafe(string root)
    {
        Stack<string> directories = new();
        directories.Push(root);
        while (directories.Count != 0)
        {
            string directory = directories.Pop();
            FileAttributes directoryAttributes = File.GetAttributes(directory);
            if ((directoryAttributes & FileAttributes.ReparsePoint) != 0)
                throw new InvalidDataException($"Symbolic links are not allowed: {directory}");
            foreach (string path in Directory.EnumerateFileSystemEntries(directory))
            {
                FileAttributes attributes = File.GetAttributes(path);
                if ((attributes & FileAttributes.ReparsePoint) != 0)
                    throw new InvalidDataException($"Symbolic links are not allowed: {path}");
                if ((attributes & FileAttributes.Directory) != 0)
                    directories.Push(path);
            }
        }
    }

    private static string appendDirectorySeparator(string path)
    {
        if (path.EndsWith(Path.DirectorySeparatorChar)
            || path.EndsWith(Path.AltDirectorySeparatorChar))
        {
            return path;
        }
        return path + Path.DirectorySeparatorChar;
    }

    private static void ensureExistingDirectoryIsNotReparsePoint(string path)
    {
        if (!Directory.Exists(path))
            return;
        FileAttributes attributes = File.GetAttributes(path);
        if ((attributes & FileAttributes.ReparsePoint) != 0)
            throw new InvalidDataException($"Symbolic links are not allowed: {path}");
    }
}
