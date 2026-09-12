using System;
using System.Collections.Generic;
using System.Text.Json.Nodes;
using Ludork.Models;

namespace Ludork.Services;

public sealed class ProjectMapEditingContext(GameDataService gameData) : IMapEditingContext
{
    public bool IsRuntime => false;
    public bool IsEditable => true;
    public event EventHandler<MapPreviewChangedEventArgs>? Changed
    {
        add => gameData.MapPreviewChanged += value;
        remove => gameData.MapPreviewChanged -= value;
    }

    public JsonObject? ReadMapSnapshot(string mapKey) => gameData.ReadMapSnapshot(mapKey);

    public bool PaintMapCells(string mapKey, string layerName, IReadOnlyList<MapTileEdit> cells)
        => gameData.PaintMapCells(mapKey, layerName, cells);

    public bool MoveActor(string mapKey, string layerName, string actorId, int x, int y)
        => findActor(mapKey, layerName, actorId) is int index
            && gameData.MoveMapActor(mapKey, layerName, index, actorId, x, y);

    public bool DeleteActor(string mapKey, string layerName, string actorId)
        => findActor(mapKey, layerName, actorId) is int index
            && gameData.DeleteMapActor(mapKey, layerName, index, actorId);

    public bool SetActorVariable(string mapKey, string layerName, string actorId, string name, JsonNode? value)
        => findActor(mapKey, layerName, actorId) is int index
            && gameData.SetMapActorOverride(mapKey, layerName, index, actorId, name, value);

    private int? findActor(string mapKey, string layerName, string actorId)
    {
        if (gameData.ReadMapSnapshot(mapKey)?["actors"]?[layerName] is not JsonArray actors)
            return null;
        for (int index = 0; index < actors.Count; index++)
            if (string.Equals(actors[index]?["tag"]?.GetValue<string>(), actorId, StringComparison.Ordinal))
                return index;
        return null;
    }
}
