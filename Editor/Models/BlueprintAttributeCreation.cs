using System.Text.Json.Nodes;

namespace Ludork.Models;

public sealed record BlueprintAttributeCreation(
    string Name,
    LuaMetadataType Type,
    JsonNode? Value,
    string? FileBase);
