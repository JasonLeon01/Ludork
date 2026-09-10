using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Controls;

public sealed class CurveKey
{
    private readonly JsonObject data;

    public CurveKey(JsonObject data, int componentCount)
    {
        this.data = (JsonObject)data.DeepClone();
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

    public JsonObject ToJson()
    {
        JsonObject result = (JsonObject)data.DeepClone();
        if (Time != CurveEditor.number(data["time"]))
            result["time"] = Time;
        if (!Value.SequenceEqual(CurveEditor.vector(data["value"], Value.Length)))
            result["value"] = CurveEditor.valueJson(Value);
        if (Interpolation != CurveEditor.interpolation(data["interpolation"]?.GetValue<string>()))
            result["interpolation"] = Interpolation;
        if (!ArriveTangent.SequenceEqual(CurveEditor.vector(data["arriveTangent"], ArriveTangent.Length)))
            result["arriveTangent"] = CurveEditor.valueJson(ArriveTangent);
        if (!LeaveTangent.SequenceEqual(CurveEditor.vector(data["leaveTangent"], LeaveTangent.Length)))
            result["leaveTangent"] = CurveEditor.valueJson(LeaveTangent);
        return result;
    }
}
