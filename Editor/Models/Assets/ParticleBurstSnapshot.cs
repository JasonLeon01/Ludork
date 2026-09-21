using System.Text.Json.Nodes;

namespace Ludork.Models;

public sealed class ParticleBurstSnapshot(JsonObject data) : EditorDataSnapshot(data)
{
    public double Time => Number("time");
    public int Count => (int)Number("count");
    public int Cycles => (int)Number("cycles", 1);
    public double Interval => Number("interval");
}
