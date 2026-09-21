using Avalonia.Threading;
using Ludork.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Services;

public sealed class LiveDebugSession : IMapEditingContext, IDisposable
{
    public MapDocumentSnapshot? ReadMapDocument(string mapKey)
    {
        return ReadMapSnapshot(mapKey) is JsonObject snapshot ? new MapDocumentSnapshot(snapshot) : null;
    }

    private readonly ProjectRunnerService runner;
    private readonly long generation;
    private readonly CancellationTokenSource lifetime = new();
    private readonly DispatcherTimer timer = new() { Interval = TimeSpan.FromMilliseconds(100) };
    private readonly List<JsonObject> commands = [];
    private JsonObject? snapshot;
    private long connection;
    private bool connected;
    private bool pumping;
    private bool disposed;
    private int pollCount;
    private string? selectedActorId;

    public LiveDebugSession(ProjectRunnerService runner)
    {
        this.runner = runner;
        generation = runner.RunGeneration;
        timer.Tick += onTick;
    }

    public bool IsRuntime => true;
    public bool IsEditable { get; private set; }
    public string? MapKey { get; private set; }
    public string Context { get; private set; } = string.Empty;
    public string Status { get; private set; } = "waiting";
    public event EventHandler<MapPreviewChangedEventArgs>? Changed;
    public event EventHandler? StateChanged;
    public event EventHandler<string>? ErrorReceived;

    public JsonObject? ReadMapSnapshot(string mapKey)
    {
        return string.Equals(mapKey, MapKey, StringComparison.Ordinal)
            ? snapshot?.DeepClone() as JsonObject : null;
    }

    public void SetConnection(bool available)
    {
        connected = false;
        IsEditable = false;
        commands.Clear();
        timer.Stop();
        Status = available ? "waiting" : "disconnected";
        StateChanged?.Invoke(this, EventArgs.Empty);
        if (!disposed && available)
        {
            connection = runner.ConnectionGeneration;
            Context = string.Empty;
            timer.Start();
            _ = pumpAsync();
        }
    }

    public void SelectActor(string? actorId)
    {
        if (selectedActorId == actorId)
            return;
        selectedActorId = actorId;
        if (IsEditable && actorId is not null)
            enqueue(new JsonObject { ["action"] = "info", ["actorId"] = actorId });
    }

    public bool PaintMapCells(string mapKey, string layerName, IReadOnlyList<MapTileEdit> cells)
    {
        if (!canEdit(mapKey) || cells.Count == 0
            || snapshot?["layers"]?[layerName] is not JsonObject layer
            || layer["visible"]?.GetValue<bool>() == false)
            return false;
        int width = snapshot["width"]!.GetValue<int>();
        int height = snapshot["height"]!.GetValue<int>();
        MapTileEdit[] validCells = cells.Where(cell => cell.X >= 0 && cell.Y >= 0
            && cell.X < width && cell.Y < height).ToArray();
        foreach (MapTileEdit[] batch in validCells.Chunk(128))
        {
            JsonArray values = [];
            foreach (MapTileEdit cell in batch)
            {
                JsonNode? tileId = cell.AutoTileKey is not null
                    ? JsonValue.Create(cell.AutoTileKey) : JsonValue.Create(cell.TileNumber);
                values.Add(new JsonObject { ["x"] = cell.X, ["y"] = cell.Y, ["tileId"] = tileId });
            }
            enqueue(new JsonObject { ["action"] = "tiles", ["layer"] = layerName, ["tiles"] = values });
        }
        return validCells.Length != 0;
    }

    public bool MoveActor(string mapKey, string layerName, string actorId, int x, int y)
    {
        if (snapshot?["actors"] is JsonObject groups && groups.SelectMany(
            group => (group.Value as JsonArray)?.OfType<JsonObject>() ?? []).Any(
                actor => actor["runtimeId"]?.GetValue<string>() == actorId && actor["parentRuntimeId"] is not null))
            return false;
        return canEdit(mapKey) && enqueue(new JsonObject
        {
            ["action"] = "move", ["layer"] = layerName, ["actorId"] = actorId, ["x"] = x, ["y"] = y,
        });
    }

    public bool DeleteActor(string mapKey, string layerName, string actorId)
    {
        return canEdit(mapKey) && enqueue(new JsonObject
        {
            ["action"] = "delete", ["layer"] = layerName, ["actorId"] = actorId,
        });
    }

    public bool SetActorVariable(string mapKey, string layerName, string actorId, string name, JsonNode? value)
    {
        return canEdit(mapKey) && enqueue(new JsonObject
        {
            ["action"] = "setVariable", ["layer"] = layerName, ["actorId"] = actorId,
            ["name"] = name, ["value"] = value?.DeepClone(),
        });
    }

    private bool canEdit(string mapKey) => !disposed && IsEditable
        && string.Equals(mapKey, MapKey, StringComparison.Ordinal);

    private bool enqueue(JsonObject command)
    {
        if (disposed || !IsEditable)
            return false;
        command["context"] = Context;
        string? action = command["action"]?.GetValue<string>();
        if (action == "tiles" && commands.Count != 0 && mergeTileCommand(commands[^1], command))
            return true;
        if (action is "move" or "setVariable" or "info" && commands.Count != 0)
        {
            JsonObject previous = commands[^1];
            if (previous["action"]?.GetValue<string>() == action
                && previous["actorId"]?.GetValue<string>() == command["actorId"]?.GetValue<string>()
                && previous["name"]?.GetValue<string>() == command["name"]?.GetValue<string>())
            {
                commands[^1] = command;
                return true;
            }
        }
        commands.Add(command);
        return true;
    }

    private static bool mergeTileCommand(JsonObject previous, JsonObject next)
    {
        if (previous["action"]?.GetValue<string>() != "tiles"
            || previous["context"]?.GetValue<string>() != next["context"]?.GetValue<string>()
            || previous["layer"]?.GetValue<string>() != next["layer"]?.GetValue<string>()
            || previous["tiles"] is not JsonArray previousTiles || next["tiles"] is not JsonArray nextTiles)
            return false;
        Dictionary<(int X, int Y), JsonObject> cells = [];
        foreach (JsonObject cell in previousTiles.Concat(nextTiles).OfType<JsonObject>())
            cells[(cell["x"]!.GetValue<int>(), cell["y"]!.GetValue<int>())] = cell;
        if (cells.Count > 128)
            return false;
        previous["tiles"] = new JsonArray(cells.Values.Select(cell => cell.DeepClone()).ToArray());
        return true;
    }

    private async void onTick(object? sender, EventArgs args) => await pumpAsync();

    private async Task pumpAsync()
    {
        if (pumping || disposed || !runner.IsCurrentConnection(generation, connection))
            return;
        pumping = true;
        long activeConnection = connection;
        try
        {
            JsonObject request;
            if (!connected)
                request = new JsonObject { ["action"] = "start" };
            else if (commands.Count != 0)
            {
                request = commands[0];
                commands.RemoveAt(0);
            }
            else
            {
                request = new JsonObject
                {
                    ["action"] = "poll", ["context"] = Context,
                    ["actorId"] = selectedActorId, ["includeInfo"] = pollCount++ % 2 == 0,
                };
            }
            JsonObject? response = await runner.SendLiveDebugRequestAsync(request, generation, activeConnection, lifetime.Token);
            if (disposed || activeConnection != connection || !runner.IsCurrentConnection(generation, activeConnection))
                return;
            if (response is null)
            {
                SetConnection(false);
                return;
            }
            bool success = response["success"]?.GetValue<bool>() == true;
            if (success)
                connected = true;
            applyResponse(response);
            if (!success)
            {
                string error = response["error"]?.GetValue<string>() ?? "Live Debug request failed.";
                ErrorReceived?.Invoke(this, error);
                if (!connected)
                {
                    timer.Stop();
                    Status = "unavailable";
                    StateChanged?.Invoke(this, EventArgs.Empty);
                }
            }
        }
        finally
        {
            pumping = false;
            if (!disposed && connected && commands.Count != 0)
                Dispatcher.UIThread.Post(() => _ = pumpAsync());
        }
    }

    private void applyResponse(JsonObject response)
    {
        string nextContext = response["context"]?.GetValue<string>() ?? Context;
        string? nextMapKey = response["mapKey"]?.GetValue<string>() ?? MapKey;
        bool contextChanged = nextContext != Context;
        if (contextChanged)
        {
            commands.Clear();
            selectedActorId = null;
            snapshot = null;
        }
        Context = nextContext;
        MapKey = nextMapKey;
        if (response["editable"] is JsonValue editable)
            IsEditable = connected && editable.GetValue<bool>();
        Status = response["status"]?.GetValue<string>() ?? (IsEditable ? "ready" : "waiting");
        List<JsonDataEdit> edits = [];
        bool replaced = response["map"] is JsonObject;
        if (response["map"] is JsonObject map)
        {
            JsonObject nextSnapshot = (JsonObject)map.DeepClone();
            if (!contextChanged && snapshot is not null)
            {
                nextSnapshot["BPClassVarChanged"] = snapshot["BPClassVarChanged"]?.DeepClone();
                nextSnapshot["runtimeInfo"] = snapshot["runtimeInfo"]?.DeepClone();
            }
            snapshot = nextSnapshot;
        }
        if (snapshot is not null)
        {
            if (response["tileChanges"] is JsonArray tileChanges)
            {
                foreach (JsonObject cell in tileChanges.OfType<JsonObject>())
                {
                    string layer = cell["layer"]!.GetValue<string>();
                    int x = cell["x"]!.GetValue<int>();
                    int y = cell["y"]!.GetValue<int>();
                    edits.Add(new JsonDataEdit(JsonDataEdit.Operation.Set, ["layers", layer, "tiles", y, x], cell["tile"]));
                    edits.Add(new JsonDataEdit(JsonDataEdit.Operation.Set, ["layers", layer, "autoTiles", y, x], cell["autoTile"]));
                }
            }
            if (response["actors"] is JsonObject actors)
                edits.Add(new JsonDataEdit(JsonDataEdit.Operation.Set, ["actors"], actors));
            JsonObject variables = snapshot["BPClassVarChanged"]?.DeepClone() as JsonObject ?? [];
            if (response["variables"] is JsonObject values)
            {
                foreach ((string id, JsonNode? value) in values)
                    variables[id] = value?.DeepClone();
            }
            if (response["info"] is JsonObject info && info["actorId"]?.GetValue<string>() is string actorId)
            {
                variables[actorId] = info["values"]?.DeepClone();
                JsonObject runtimeInfo = snapshot["runtimeInfo"]?.DeepClone() as JsonObject ?? [];
                runtimeInfo[actorId] = info.DeepClone();
                edits.Add(new JsonDataEdit(JsonDataEdit.Operation.Set, ["runtimeInfo"], runtimeInfo));
            }
            if (!JsonNode.DeepEquals(variables, snapshot["BPClassVarChanged"]))
                edits.Add(new JsonDataEdit(JsonDataEdit.Operation.Set, ["BPClassVarChanged"], variables));
            foreach (JsonDataEdit edit in edits)
                edit.Apply(snapshot);
        }
        StateChanged?.Invoke(this, EventArgs.Empty);
        if (MapKey is not null)
        {
            MapDataEditedEventArgs? change = replaced || contextChanged ? null : new(MapKey, edits);
            Changed?.Invoke(this, new MapPreviewChangedEventArgs(MapKey, change));
        }
    }

    public void Dispose()
    {
        if (disposed)
            return;
        disposed = true;
        timer.Stop();
        lifetime.Cancel();
        commands.Clear();
        snapshot = null;
        IsEditable = false;
        lifetime.Dispose();
    }
}
