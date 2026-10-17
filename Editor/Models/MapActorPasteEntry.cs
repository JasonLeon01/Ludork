using System.Text.Json.Nodes;

namespace Ludork.Models;

public sealed record MapActorPasteEntry(JsonObject ActorData, JsonObject? Overrides, int X, int Y);
