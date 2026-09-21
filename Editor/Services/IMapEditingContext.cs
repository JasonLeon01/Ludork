using System;
using System.Collections.Generic;
using System.Text.Json.Nodes;
using Ludork.Models;

namespace Ludork.Services;

public interface IMapEditingContext
{
    bool IsRuntime { get; }
    bool IsEditable { get; }
    JsonObject? ReadMapSnapshot(string mapKey);
    MapDocumentSnapshot? ReadMapDocument(string mapKey);
    bool PaintMapCells(string mapKey, string layerName, IReadOnlyList<MapTileEdit> cells);
    bool MoveActor(string mapKey, string layerName, string actorId, int x, int y);
    bool DeleteActor(string mapKey, string layerName, string actorId);
    bool SetActorVariable(string mapKey, string layerName, string actorId, string name, JsonNode? value);
    event EventHandler<MapPreviewChangedEventArgs>? Changed;
}
