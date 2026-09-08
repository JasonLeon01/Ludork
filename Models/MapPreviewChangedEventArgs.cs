using System;

namespace Ludork.Models;

public sealed class MapPreviewChangedEventArgs(string? mapKey, MapDataEditedEventArgs? edit = null, bool reloadData = true) : EventArgs
{
    public string? MapKey { get; } = mapKey;
    public bool ReloadData { get; } = reloadData;
    public MapDataEditedEventArgs? Edit { get; } = edit;
}
