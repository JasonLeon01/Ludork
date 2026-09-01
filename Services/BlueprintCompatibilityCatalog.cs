using System;
using System.Collections.Generic;
using System.Text.Json.Nodes;

namespace Ludork.Services;

internal static class BlueprintCompatibilityCatalog
{
    private static readonly IReadOnlyDictionary<string, BlueprintCompatibilityType> Types =
        new Dictionary<string, BlueprintCompatibilityType>(StringComparer.Ordinal)
        {
            ["Engine.Actor"] = new(null, [], new Dictionary<string, JsonNode?>(StringComparer.Ordinal)
            {
                ["switchInterval"] = JsonValue.Create(0.2),
                ["animatable"] = JsonValue.Create(false),
                ["shaderPath"] = JsonValue.Create(string.Empty),
                ["hue"] = JsonValue.Create(0.0),
                ["tickable"] = JsonValue.Create(false),
                ["speed"] = JsonValue.Create(64.0),
                ["autoSound"] = JsonValue.Create(string.Empty),
                ["autoSoundInterval"] = JsonValue.Create(0.0),
                ["texturePath"] = JsonValue.Create(string.Empty),
                ["defaultRect"] = new JsonArray(new JsonArray(0, 0, 32, 32)),
                ["defaultTranslation"] = new JsonArray(0.0, 0.0),
                ["defaultRotation"] = JsonValue.Create(0.0),
                ["defaultScale"] = new JsonArray(1.0, 1.0),
                ["defaultOrigin"] = new JsonArray(0.0, 0.0),
            }),
            ["Engine.Character"] = new("Engine.Actor", [], new Dictionary<string, JsonNode?>(StringComparer.Ordinal)
            {
                ["direction"] = JsonValue.Create(0),
                ["directionFix"] = JsonValue.Create(false),
                ["animateWithoutMoving"] = JsonValue.Create(false),
            }),
            ["Source.Enemy"] = new(
                "Engine.Actor",
                [],
                new Dictionary<string, JsonNode?>(StringComparer.Ordinal)
            {
                ["ID"] = JsonValue.Create("FILL_IT_BY_YOURSELF"),
                ["tickable"] = JsonValue.Create(true),
                ["collisionEnabled"] = JsonValue.Create(true),
                ["animatable"] = JsonValue.Create(true),
                ["animateWithoutMoving"] = JsonValue.Create(true),
            }),
            ["Source.Item"] = new(
                "Engine.Actor",
                [],
                new Dictionary<string, JsonNode?>(StringComparer.Ordinal)
            {
                ["ID"] = JsonValue.Create("FILL_IT_BY_YOURSELF"),
                ["getSE"] = JsonValue.Create(string.Empty),
            }),
            ["Source.Player"] = new(
                "Engine.Character",
                [],
                new Dictionary<string, JsonNode?>(StringComparer.Ordinal)
            {
                ["ID"] = JsonValue.Create("FILL_IT_BY_YOURSELF"),
                ["tickable"] = JsonValue.Create(true),
                ["collisionEnabled"] = JsonValue.Create(true),
                ["animatable"] = JsonValue.Create(true),
                ["speed"] = JsonValue.Create(96.0),
            }),
            ["Source.Teleporter.Teleporter"] = new("Engine.Actor", [], new Dictionary<string, JsonNode?>(StringComparer.Ordinal)
            {
                ["Offset"] = new JsonArray(0, 0),
                ["stairSE"] = JsonValue.Create(string.Empty),
                ["transitionName"] = JsonValue.Create(string.Empty),
                ["transitionTime"] = JsonValue.Create(0.5),
            }),
        };

    public static bool TryGet(string qualifiedTypeName, out BlueprintCompatibilityType? type)
    {
        return Types.TryGetValue(qualifiedTypeName, out type);
    }

    public static IEnumerable<string> GetTypeNames()
    {
        return Types.Keys;
    }

    public static bool IsDerivedFrom(string qualifiedTypeName, string qualifiedBaseTypeName)
    {
        HashSet<string> visited = new(StringComparer.Ordinal);
        Stack<string> pending = new();
        pending.Push(qualifiedTypeName);
        while (pending.Count != 0)
        {
            string current = pending.Pop();
            if (!visited.Add(current))
                continue;
            if (string.Equals(current, qualifiedBaseTypeName, StringComparison.Ordinal))
                return true;
            if (!Types.TryGetValue(current, out BlueprintCompatibilityType? type) || type is null)
                continue;
            if (!string.IsNullOrWhiteSpace(type.Parent))
                pending.Push(type.Parent);
            for (int index = type.MetadataBases.Count - 1; index >= 0; --index)
                pending.Push(type.MetadataBases[index]);
        }
        return false;
    }
}

internal sealed record BlueprintCompatibilityType(
    string? Parent,
    IReadOnlyList<string> MetadataBases,
    IReadOnlyDictionary<string, JsonNode?> Attrs
);
