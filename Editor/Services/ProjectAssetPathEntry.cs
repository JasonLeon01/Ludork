using System;

namespace Ludork.Services;

public sealed record ProjectAssetPathEntry(string Path, string ReadPath, bool IsDirectory, long Length, DateTime LastWriteTimeUtc);
