using Ludork.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed partial class AssetDataService
{
    internal static JsonObject createDefaultMaterial()
    {
        return TilesetMetadata.CreateDefaultMaterial();
    }

    internal static JsonObject createCurveKey(double time, JsonNode value)
    {
        int componentCount = value is JsonArray values ? values.Count : 1;
        return new JsonObject
        {
            ["time"] = time,
            ["value"] = value,
            ["interpolation"] = "linear",
            ["arriveTangent"] = createCurveValue(componentCount, 0.0),
            ["leaveTangent"] = createCurveValue(componentCount, 0.0),
        };
    }

    internal static JsonNode createCurveValue(int componentCount, double value)
    {
        if (componentCount == 1)
            return JsonValue.Create(value);
        JsonArray result = new();
        for (int index = 0; index < componentCount; index += 1)
            result.Add(value);
        return result;
    }

    internal static JsonObject createPlainTextConfig(string name)
    {
        return new JsonObject
        {
            ["type"] = "plainTextConfig",
            ["name"] = name,
            ["font"] = string.Empty,
            ["characterSize"] = 22,
            ["style"] = createTextStyleFlags(),
            ["slantAngle"] = 0.0,
            ["fillColor"] = createColour(255, 255, 255, 255),
            ["letterSpacing"] = 1.0,
            ["lineSpacing"] = 1.0,
            ["lineAlignment"] = "default",
            ["outline"] = createOutline(),
            ["glow"] = createGlow(),
            ["gradient"] = createGradient(),
        };
    }

    internal static JsonObject createRichTextConfig(string name)
    {
        return new JsonObject
        {
            ["type"] = "richTextConfig",
            ["name"] = name,
            ["font"] = string.Empty,
            ["lineAlignment"] = "default",
            ["defaultStyle"] = new JsonObject
            {
                ["characterSize"] = 22,
                ["style"] = createTextStyleFlags(),
                ["fillColor"] = createColour(255, 255, 255, 255),
                ["letterSpacing"] = 1.0,
                ["lineSpacing"] = 1.0,
                ["outline"] = createOutline(),
            },
            ["styleOrder"] = new JsonArray(),
            ["styles"] = new JsonObject(),
            ["glow"] = createGlow(),
            ["gradient"] = createGradient(),
        };
    }

    internal static JsonObject createTextStyleFlags()
    {
        return new JsonObject
        {
            ["bold"] = false,
            ["italic"] = false,
            ["underlined"] = false,
            ["strikeThrough"] = false,
        };
    }

    internal static JsonArray createColour(int red, int green, int blue, int alpha)
    {
        return new JsonArray(red, green, blue, alpha);
    }

    internal static JsonObject createOutline()
    {
        return new JsonObject
        {
            ["color"] = createColour(0, 0, 0, 255),
            ["thickness"] = 0.0,
        };
    }

    internal static JsonObject createGlow()
    {
        return new JsonObject
        {
            ["enabled"] = false,
            ["color"] = createColour(255, 255, 255, 0),
            ["radius"] = 0.0,
            ["intensity"] = 0.0,
        };
    }

    internal static JsonObject createGradient()
    {
        return new JsonObject
        {
            ["enabled"] = false,
            ["direction"] = "vertical",
            ["curve"] = string.Empty,
        };
    }

    internal static bool isCurveType(string? type)
    {
        return type is "curve" or "vector2Curve" or "vector3Curve" or "vector4Curve";
    }

    internal static int curveComponentCount(string type)
    {
        return type switch
        {
            "vector2Curve" => 2,
            "vector3Curve" => 3,
            "vector4Curve" => 4,
            _ => 1,
        };
    }

    internal static bool isTextConfigType(string? type)
    {
        return type is "plainTextConfig" or "richTextConfig";
    }

}
