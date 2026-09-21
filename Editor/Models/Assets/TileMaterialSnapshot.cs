using System.Text.Json.Nodes;

namespace Ludork.Models;

public sealed class TileMaterialSnapshot(JsonObject data) : EditorDataSnapshot(data)
{
    public double LightBlock => Number("lightBlock");
    public bool Mirror => Boolean("mirror");
    public double ReflectionStrength => Number("reflectionStrength", 0.5);
    public double Opacity => Number("opacity", 1);
    public double SpeedRate => Number("speedRate", 1);
}
