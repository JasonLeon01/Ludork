using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

internal sealed class ProjectAssetDiskInputs
{
    private readonly Dictionary<string, JsonObject?> entries = new(StringComparer.Ordinal);
    private readonly Dictionary<string, string[]> directories = new(StringComparer.Ordinal);

    internal JsonObject? Read(string path)
    {
        if (!entries.TryGetValue(path, out JsonObject? metadata))
        {
            metadata = readMetadata(path);
            entries.Add(path, metadata);
        }
        return metadata;
    }

    internal IReadOnlyList<string> ReadDirectory(string path)
    {
        if (!directories.TryGetValue(path, out string[]? children))
        {
            if (Read(path)?["directory"]?.GetValue<bool>() != true)
                throw new IOException($"The asset directory no longer exists: {path}");
            children = Directory.GetFileSystemEntries(path).Order(StringComparer.Ordinal).ToArray();
            directories.Add(path, children);
        }
        return children;
    }

    internal bool IsCurrent()
    {
        try
        {
            Validate();
            return true;
        }
        catch (IOException)
        {
            return false;
        }
    }

    internal void Validate()
    {
        foreach ((string path, JsonObject? metadata) in entries)
            if (!JsonNode.DeepEquals(metadata, readMetadata(path)))
                throw new IOException(LocaleService.Get("EDIT_OPERATION_INPUTS_CHANGED"));
        foreach ((string path, string[] children) in directories)
            if (!Directory.GetFileSystemEntries(path).Order(StringComparer.Ordinal).SequenceEqual(children))
                throw new IOException(LocaleService.Get("EDIT_OPERATION_INPUTS_CHANGED"));
    }

    private static JsonObject? readMetadata(string path)
    {
        string? parent = Path.GetDirectoryName(path);
        if (parent is not null && !Directory.Exists(parent))
            return null;
        FileSystemInfo entry;
        try
        {
            FileAttributes attributes = File.GetAttributes(path);
            if ((attributes & FileAttributes.ReparsePoint) != 0)
                throw new IOException($"The asset path contains a symbolic link: {path}");
            entry = (attributes & FileAttributes.Directory) != 0 ? new DirectoryInfo(path) : new FileInfo(path);
        }
        catch (FileNotFoundException)
        {
            return null;
        }
        catch (DirectoryNotFoundException)
        {
            return null;
        }
        JsonObject metadata = new() { ["directory"] = entry is DirectoryInfo };
        if (entry is FileInfo file)
        {
            metadata["length"] = file.Length;
            metadata["modified"] = file.LastWriteTimeUtc.Ticks;
        }
        return metadata;
    }
}
