using System.Text.Json.Nodes;
using System.Collections.Generic;

namespace Ludork.Models;

public sealed class WorldMapInfo
{
    public string DirectoryName { get; set; } = string.Empty;
    public string WorldName { get; set; } = string.Empty;
    public int Width { get; set; } = 13;
    public int Height { get; set; } = 13;
    public JsonObject Fog { get; set; } = MapFogSettings.CreateDefault();
    public string Panorama { get; set; } = string.Empty;
    public IReadOnlyList<string> LayerOrder { get; set; } = [];
    public IReadOnlyList<WorldMapPlacement> Placements { get; set; } = [];
}
