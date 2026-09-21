using System;
using System.Collections.Generic;

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

internal readonly record struct LuaMetadataFileStamp(bool Exists, DateTime ModifiedAt, long Length);
