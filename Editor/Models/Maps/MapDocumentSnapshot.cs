using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Models;

public sealed class MapDocumentSnapshot : IEditorDataSnapshot
{
    internal MapDocumentSnapshot(JsonObject data)
    {
        Data = data;
        RefreshIndexes();
    }

    internal void RefreshIndexes(MapDataEditedEventArgs? edits = null)
    {
        if (edits is null || edits.Edits.Any(edit => edit.Path[0] is "layerOrder"))
            LayerOrder = (Data["layerOrder"] as JsonArray ?? []).Select(MapSnapshotValues.Text).ToArray();
        if (edits is null || edits.Edits.Any(edit => edit.Path[0] is "layers" && edit.Path.Count <= 2))
            Layers = (Data["layers"] as JsonObject ?? []).ToDictionary(pair => pair.Key,
                pair => new MapLayerSnapshot(pair.Value as JsonObject ?? []));
        if (edits is null || edits.Edits.Any(edit => edit.Path[0] is "actors" && edit.Path.Count <= 3))
            Actors = (Data["actors"] as JsonObject ?? []).ToDictionary(pair => pair.Key,
                pair => (IReadOnlyList<MapActorSnapshot>)(pair.Value as JsonArray ?? [])
                    .Select(value => new MapActorSnapshot(value as JsonObject ?? [])).ToArray());
        if (edits is null || edits.Edits.Any(edit => edit.Path[0] is "lights" && edit.Path.Count <= 2))
            Lights = (Data["lights"] as JsonArray ?? []).Select(value => new MapLightSnapshot(value as JsonObject ?? [])).ToArray();
    }

    internal JsonObject Data { get; }
    public string Name => MapSnapshotValues.Text(Data["mapName"]);
    public int Width => MapSnapshotValues.Integer(Data["width"]);
    public int Height => MapSnapshotValues.Integer(Data["height"]);
    public IReadOnlyList<string> LayerOrder { get; private set; } = [];
    public IReadOnlyDictionary<string, MapLayerSnapshot> Layers { get; private set; } = new Dictionary<string, MapLayerSnapshot>();
    public IReadOnlyDictionary<string, IReadOnlyList<MapActorSnapshot>> Actors { get; private set; } = new Dictionary<string, IReadOnlyList<MapActorSnapshot>>();
    public IReadOnlyList<MapLightSnapshot> Lights { get; private set; } = [];
    public MapColour AmbientLight => MapSnapshotValues.Colour(Data["ambientLight"], new(255, 255, 255, 255));
    public string Bgm => MapSnapshotValues.Text(Data["bgm"]);
    public string Bgs => MapSnapshotValues.Text(Data["bgs"]);
    public JsonObject BgmFilter => Data["bgmFilter"]?.DeepClone() as JsonObject ?? [];
    public JsonObject BgsFilter => Data["bgsFilter"]?.DeepClone() as JsonObject ?? [];
    public string Fog => MapSnapshotValues.Text(Data["fog"]);
    public int FogPower => MapSnapshotValues.Integer(Data["fogPower"]);
    public double FogOx => MapSnapshotValues.Number(Data["fogOx"]);
    public double FogOy => MapSnapshotValues.Number(Data["fogOy"]);
    public int FogDistort => MapSnapshotValues.Integer(Data["fogDistort"]);
    public string Panorama => MapSnapshotValues.Text(Data["panorama"]);

    public JsonObject? ReadActorOverrides(string tag) => Data["BPClassVarChanged"]?[tag]?.DeepClone() as JsonObject;
    public JsonObject? ReadRuntimeInfo(string runtimeId) => Data["runtimeInfo"]?[runtimeId]?.DeepClone() as JsonObject;
    public JsonObject ToJson() => (JsonObject)Data.DeepClone();
}
