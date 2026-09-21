using System;
using System.Collections.Generic;
using System.Text.Json.Nodes;
using Ludork.Models;

namespace Ludork.Services;

public sealed class ProjectMapEditingContext(ProjectDataStore gameData) : IMapEditingContext
{
    public bool IsRuntime => false;
    public bool IsEditable => true;
    public event EventHandler<MapPreviewChangedEventArgs>? Changed
    {
        add => gameData.Maps.MapPreviewChanged += value;
        remove => gameData.Maps.MapPreviewChanged -= value;
    }

    public JsonObject? ReadMapSnapshot(string mapKey) => gameData.Maps.ReadMapSnapshot(mapKey);
    public MapDocumentSnapshot? ReadMapDocument(string mapKey) => gameData.Maps.ReadMapDocument(mapKey);

    public bool PaintMapCells(string mapKey, string layerName, IReadOnlyList<MapTileEdit> cells)
        => gameData.Maps.PaintMapCells(mapKey, layerName, cells);

    public bool MoveActor(string mapKey, string layerName, string actorId, int x, int y)
        => findActor(mapKey, layerName, actorId) is int index
            && gameData.Maps.MoveMapActor(mapKey, layerName, index, actorId, x, y);

    public bool DeleteActor(string mapKey, string layerName, string actorId)
        => findActor(mapKey, layerName, actorId) is int index
            && gameData.Maps.DeleteMapActor(mapKey, layerName, index, actorId);

    public bool SetActorVariable(string mapKey, string layerName, string actorId, string name, JsonNode? value)
        => findActor(mapKey, layerName, actorId) is int index
            && gameData.Maps.SetMapActorOverride(mapKey, layerName, index, actorId, name, value);

    private int? findActor(string mapKey, string layerName, string actorId)
    {
        if (gameData.Maps.ReadMapDocument(mapKey)?.Actors.TryGetValue(layerName, out IReadOnlyList<MapActorSnapshot>? actors) != true || actors is null)
            return null;
        for (int index = 0; index < actors.Count; index++)
            if (string.Equals(actors[index].Tag, actorId, StringComparison.Ordinal))
                return index;
        return null;
    }
}
