using System;
using System.Collections.Generic;
using System.IO;

namespace Ludork.Services;

internal sealed class LuaMetadataDependencySnapshot
{
    public LuaMetadataDependencySnapshot(
        IReadOnlyDictionary<string, LuaMetadataFileStamp> stamps,
        bool isConsistent,
        bool coveredByReadSnapshot)
    {
        Stamps = stamps;
        IsConsistent = isConsistent;
        CoveredByReadSnapshot = coveredByReadSnapshot;
    }

    internal IReadOnlyDictionary<string, LuaMetadataFileStamp> Stamps { get; }
    internal bool IsConsistent { get; }
    internal bool CoveredByReadSnapshot { get; }
}

internal readonly record struct LuaMetadataFileStamp(bool Exists, DateTime ModifiedAt, long Length)
{
    internal static LuaMetadataFileStamp Read(string path)
    {
        FileInfo info = new(path);
        return info.Exists ? new LuaMetadataFileStamp(true, info.LastWriteTimeUtc, info.Length) : default;
    }
}
