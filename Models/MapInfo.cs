using System.Text.Json.Nodes;

namespace Ludork.Models;

public sealed class MapInfo
{
    public string FileName { get; set; } = string.Empty;
    public string MapName { get; set; } = string.Empty;
    public int Width { get; set; } = 13;
    public int Height { get; set; } = 13;
    public JsonArray AmbientLight { get; set; } = new JsonArray(255, 255, 255, 255);
    public string Bgm { get; set; } = string.Empty;
    public JsonObject BgmFilter { get; set; } = new JsonObject();
    public string Bgs { get; set; } = string.Empty;
    public JsonObject BgsFilter { get; set; } = new JsonObject();
    public string Fog { get; set; } = string.Empty;
    public int FogPower { get; set; }
    public double FogOx { get; set; }
    public double FogOy { get; set; }
    public int FogDistort { get; set; }
}
