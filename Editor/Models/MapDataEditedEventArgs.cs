using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Models;

public sealed class MapDataEditedEventArgs(string mapKey, IEnumerable<JsonDataEdit> edits) : EventArgs
{
    public string MapKey { get; } = mapKey;
    public IReadOnlyList<JsonDataEdit> Edits { get; } = Array.AsReadOnly(edits.ToArray());
    public bool ChangesActors => Edits.Any(edit => edit.Path[0] is "actors" or "BPClassVarChanged" or "runtimeInfo");
    internal bool ChangesOnlyActorPositions => Edits.Count != 0 && Edits.All(edit =>
        edit.Kind == JsonDataEdit.Operation.Set && edit.Path.Count == 4
        && edit.Path[0] is "actors" && edit.Path[1] is string && edit.Path[2] is int
        && edit.Path[3] is "position");
    public IEnumerable<string> Layers => Edits
        .Where(edit => edit.Path.Count > 1 && edit.Path[0] is "layers")
        .Select(edit => (string)edit.Path[1]).Distinct(StringComparer.Ordinal);

    public void ApplyTo(JsonObject snapshot)
    {
        foreach (JsonDataEdit edit in Edits)
            edit.Apply(snapshot);
    }
}
