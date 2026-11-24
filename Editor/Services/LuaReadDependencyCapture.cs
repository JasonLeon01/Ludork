using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;

namespace Ludork.Services;

internal sealed class LuaReadDependencyCapture
{
    private readonly Dictionary<string, LuaMetadataFileStamp> files = new(StringComparer.OrdinalIgnoreCase);
    private bool consistent = true;

    internal void Record(string path, LuaMetadataFileStamp stamp)
    {
        lock (files)
        {
            if (files.TryGetValue(path, out LuaMetadataFileStamp previous))
                consistent &= previous == stamp;
            else
                files.Add(path, stamp);
        }
    }

    internal LuaMetadataDependencySnapshot Capture()
    {
        lock (files)
            return new(new ReadOnlyDictionary<string, LuaMetadataFileStamp>(
                new Dictionary<string, LuaMetadataFileStamp>(files, StringComparer.OrdinalIgnoreCase)), consistent, false);
    }
}
