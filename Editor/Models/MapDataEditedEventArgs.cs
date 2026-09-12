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
    public bool ChangesLights => Edits.Any(edit => edit.Path[0] is "lights");
    public IEnumerable<string> Layers => Edits
        .Where(edit => edit.Path.Count > 1 && edit.Path[0] is "layers")
        .Select(edit => (string)edit.Path[1]).Distinct(StringComparer.Ordinal);

    public void ApplyTo(JsonObject snapshot)
    {
        foreach (JsonDataEdit edit in Edits)
            edit.Apply(snapshot);
    }
}
