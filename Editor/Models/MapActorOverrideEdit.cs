using System.Text.Json.Nodes;

namespace Ludork.Models;

public sealed record MapActorOverrideEdit(MapActorReference Actor, string Name, JsonNode? Value, bool Remove = false);
