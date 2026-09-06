using System;

namespace Ludork.Models;

public sealed class MapPreviewChangedEventArgs(string? mapKey) : EventArgs
{
    public string? MapKey { get; } = mapKey;
}
