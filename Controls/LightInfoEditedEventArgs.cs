using System;
using System.Text.Json.Nodes;

namespace Ludork.Controls;

public sealed class LightInfoEditedEventArgs(JsonObject lightData) : EventArgs
{
    public JsonObject LightData { get; } = lightData;
}
