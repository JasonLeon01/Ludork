using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Models;

public sealed class CurveKeySnapshot(JsonObject data) : EditorDataSnapshot(data)
{
    public double Time => Number("time");
    public CurveValue Value => CurveValue.Read(SnapshotData["value"]);
    public string Interpolation => Text("interpolation", "linear");
    public CurveValue ArriveTangent => CurveValue.Read(SnapshotData["arriveTangent"]);
    public CurveValue LeaveTangent => CurveValue.Read(SnapshotData["leaveTangent"]);
}

public sealed record CurveValue(bool IsVector, IReadOnlyList<double> Components)
{
    internal static CurveValue Read(JsonNode? value) => value is JsonArray vector
        ? new(true, vector.Select(item => MapSnapshotValues.Number(item)).ToArray())
        : new(false, new[] { MapSnapshotValues.Number(value) });
}
