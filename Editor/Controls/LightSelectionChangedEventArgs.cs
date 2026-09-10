using System;
using System.Text.Json.Nodes;

namespace Ludork.Controls;

public sealed class LightSelectionChangedEventArgs(string mapKey, int? index, JsonObject? lightData) : EventArgs
{
    public string MapKey { get; } = mapKey;
    public int? Index { get; } = index;
    public JsonObject? LightData { get; } = lightData;
}
