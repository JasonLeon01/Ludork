using Ludork.Models;
using Ludork.Services;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Controls;

internal static class MaterialPropertyForm
{
    public static BlueprintVariableForm Create(ProjectDataStore gameData, JsonObject descriptor, JsonNode? value, Action<JsonNode?> changed, bool showFieldNames = true)
    {
        string id = descriptor["id"]!.GetValue<string>();
        string type = descriptor["type"]?.GetValue<string>() ?? "float";
        JsonNode? displayed = value?.DeepClone() ?? descriptor["default"]?.DeepClone();
        if (type == "color" && displayed is JsonArray channels)
            displayed = new JsonArray(channels.Select(channel => JsonValue.Create((int)Math.Round(JsonScalar.Number(channel) * 255)) as JsonNode).ToArray());
        string editorType = type switch
        {
            "vec2" => "sf.Vector2f", "vec3" => "sf.Vector3f", "vec4" => "Tuple[float, float, float, float]",
            "color" => "sf.Color", "texture" => "string", "integer" => "int",
            "numeric" when displayed is JsonArray { Count: 2 } => "sf.Vector2f",
            "numeric" when displayed is JsonArray { Count: 3 } => "sf.Vector3f",
            "numeric" when displayed is JsonArray { Count: 4 } => "Tuple[float, float, float, float]",
            "numeric" => "float", _ => type,
        };
        JsonObject meta = new();
        if (type == "texture")
        {
            meta["PathVars"] = GameAssetPath.Root;
            meta["PathFilter"] = "*.png *.jpg *.jpeg";
        }
        IReadOnlyList<BlueprintVariableOption> options = (descriptor["options"] as JsonArray ?? []).Select(option =>
            option is JsonObject choice
                ? new BlueprintVariableOption(choice["label"]?.GetValue<string>() ?? string.Empty, choice["value"])
                : new BlueprintVariableOption(option?.ToString() ?? string.Empty, option)).ToArray();
        BlueprintVariableForm form = new()
        {
            ProjectDirectory = gameData.ProjectPath,
            AssetsDirectory = System.IO.Path.Combine(gameData.ProjectPath, "Assets"),
            HistoryGameData = gameData,
            ShowFieldNames = showFieldNames,
        };
        form.CustomValueEditorFactory = request =>
        {
            if (descriptor["editor"] is JsonObject editor && editor["kind"]?.GetValue<string>() == "swizzle")
                return new MaterialSwizzleEditor(editor, request.Value, request.Commit);
            if (descriptor["options"] is JsonArray)
                return new MaterialOptionEditor(options, request.Value, request.Commit);
            if (type is "float" or "integer" or "numeric" or "vec2" or "vec3" or "vec4"
                && options.Count == 0 && (descriptor["min"] is JsonValue || descriptor["max"] is JsonValue))
                return new MaterialNumericEditor(gameData, descriptor, request.Value, request.Commit);
            return null;
        };
        form.SetFields([new BlueprintVariableField(id, editorType, displayed)
        {
            DisplayName = descriptor["displayName"]?.GetValue<string>(),
            Description = descriptor["description"]?.GetValue<string>() ?? string.Empty,
            DefaultValue = displayed, Meta = meta, Options = options,
        }]);
        form.ValueChanged += (_, args) =>
        {
            JsonNode? next = args.Value?.DeepClone();
            if (type == "color" && next is JsonArray nextChannels)
                next = new JsonArray(nextChannels.Select(channel => JsonValue.Create(JsonScalar.Number(channel) / 255.0) as JsonNode).ToArray());
            changed(next);
        };
        return form;
    }
}
