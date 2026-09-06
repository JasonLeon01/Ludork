using Avalonia.Controls;
using Ludork.Views.Utils;
using System;
using System.Text.Json.Nodes;

namespace Ludork.Views;

public sealed class BlueprintGraphRequestedEventArgs(
    BlueprintEditorDocument document,
    string eventName,
    JsonObject eventGraph) : EventArgs
{
    public BlueprintEditorDocument Document { get; } = document;
    public string EventName { get; } = eventName;
    public JsonObject EventGraph { get; } = eventGraph;
    public Control? Content { get; set; }
}
