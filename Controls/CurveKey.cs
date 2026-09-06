using System.Text.Json.Nodes;

namespace Ludork.Controls;

public sealed class CurveKey
{
    public CurveKey(JsonObject data, int componentCount)
    {
        Time = CurveEditor.number(data["time"]);
        Value = CurveEditor.vector(data["value"], componentCount);
        Interpolation = CurveEditor.interpolation(data["interpolation"]?.GetValue<string>());
        ArriveTangent = CurveEditor.vector(data["arriveTangent"], componentCount);
        LeaveTangent = CurveEditor.vector(data["leaveTangent"], componentCount);
    }

    public double Time { get; set; }
    public double[] Value { get; }
    public string Interpolation { get; set; }
    public double[] ArriveTangent { get; }
    public double[] LeaveTangent { get; }

    public JsonObject ToJson() => new()
    {
        ["time"] = Time,
        ["value"] = CurveEditor.valueJson(Value),
        ["interpolation"] = Interpolation,
        ["arriveTangent"] = CurveEditor.valueJson(ArriveTangent),
        ["leaveTangent"] = CurveEditor.valueJson(LeaveTangent),
    };
}
