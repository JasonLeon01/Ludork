using Ludork.Models;
using System.Collections.Generic;
using System.Text.Json.Nodes;

namespace Ludork.Models;

internal sealed class BlueprintGraphClipboard
{
    public BlueprintGraphClipboard(
        IReadOnlyList<BlueprintGraphClipboardNode> nodes,
        IReadOnlyList<BlueprintGraphClipboardConnection> connections)
    {
        Nodes = nodes;
        Connections = connections;
    }

    public IReadOnlyList<BlueprintGraphClipboardNode> Nodes { get; }
    public IReadOnlyList<BlueprintGraphClipboardConnection> Connections { get; }
}

internal sealed record BlueprintGraphClipboardConnection(
    int SourceIndex,
    int TargetIndex,
    int SourcePinIndex,
    int TargetPinIndex,
    BlueprintGraphPortKind Kind,
    JsonObject RawData);
