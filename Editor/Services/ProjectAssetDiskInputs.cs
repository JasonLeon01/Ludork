using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;
using System.Threading;

namespace Ludork.Services;

internal sealed class ProjectAssetDiskInputs
{
    private readonly ConcurrentDictionary<string, JsonObject?> entries = new(StringComparer.Ordinal);
    private readonly Dictionary<string, string[]> directories = new(StringComparer.Ordinal);

    internal JsonObject? Read(string path) => entries.GetOrAdd(path, readMetadata);

    internal IReadOnlyList<string> ReadDirectory(string path, CancellationToken token = default)
    {
        if (!directories.TryGetValue(path, out string[]? children))
        {
            if (Read(path)?["directory"]?.GetValue<bool>() != true)
                throw new IOException($"The asset directory no longer exists: {path}");
            children = Directory.GetFileSystemEntries(path).Order(StringComparer.Ordinal).ToArray();
            EditorValidationBatch.ForEachFile(children, child => Read(child), token);
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
        EditorValidationBatch.ForEachFile(entries.OrderBy(entry => entry.Key, StringComparer.Ordinal).ToArray(), entry =>
        {
            if (!JsonNode.DeepEquals(entry.Value, readMetadata(entry.Key)))
                throw new IOException(LocaleService.Get("EDIT_OPERATION_INPUTS_CHANGED"));
        });
        EditorValidationBatch.ForEachFile(directories.ToArray(), entry =>
        {
            if (!Directory.GetFileSystemEntries(entry.Key).Order(StringComparer.Ordinal).SequenceEqual(entry.Value))
                throw new IOException(LocaleService.Get("EDIT_OPERATION_INPUTS_CHANGED"));
        });
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
