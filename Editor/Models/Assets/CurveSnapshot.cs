using System.Collections.Generic;
using System.Text.Json.Nodes;

namespace Ludork.Models;

public sealed class CurveSnapshot(JsonObject data) : EditorDataSnapshot(data)
{
    public string Type => Text("type", "curve");
    public string Name => Text("name");
    public CurveValue DefaultValue => CurveValue.Read(SnapshotData["defaultValue"]);
    public string PreInfinity => Text("preInfinity", "constant");
    public string PostInfinity => Text("postInfinity", "constant");
    public IReadOnlyList<CurveKeySnapshot> Keys => Objects("keys", value => new CurveKeySnapshot(value));
}
