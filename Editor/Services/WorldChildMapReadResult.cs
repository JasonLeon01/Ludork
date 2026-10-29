using System;
using System.IO;
using System.Text.Json.Nodes;

namespace Ludork.Services;

internal sealed record WorldChildMapReadResult(
    string Key,
    JsonObject? Snapshot,
    WorldMapPreviewCache.Version Version,
    string? Path,
    WorldChildMapReadResult.FileVersion? File,
    bool Retry = false)
{
    internal readonly record struct FileVersion(long Length, DateTime LastWriteTimeUtc)
    {
        internal static FileVersion? Read(string path)
        {
            FileInfo file = new(path);
            return file.Exists ? new FileVersion(file.Length, file.LastWriteTimeUtc) : null;
        }
    }
}
