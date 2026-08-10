using Ludork.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed class UiControlRegistryService
{
    private static readonly IReadOnlyList<UiControlPropertyDescriptor> CommonProperties =
    [
        property("visible", "Visible", "bool", false, JsonValue.Create(true)),
        property("rotation", "Rotation", "float", false, jsonNumber("0.0")),
        property("scale", "Scale", "sf.Vector2f", false, jsonArray("[1.0,1.0]")),
        property("origin", "Origin", "sf.Vector2f", false, jsonArray("[0.0,0.0]")),
    ];

    private static readonly IReadOnlyList<UiControlDescriptor> Descriptors = createSystemDescriptors();
    private readonly GameDataService gameData;

    public UiControlRegistryService(GameDataService gameData)
    {
        this.gameData = gameData;
    }

    public static IReadOnlyList<UiControlDescriptor> SystemDescriptors => Descriptors;
    public static string AdapterFingerprint { get; } = createAdapterFingerprint();

    public IReadOnlyList<UiControlDescriptor> GetDescriptors(
        bool includeUnexposedProjectAssets = false)
    {
        List<UiControlDescriptor> descriptors = new List<UiControlDescriptor>(Descriptors);
        descriptors.AddRange(createProjectDescriptors(!includeUnexposedProjectAssets));
        return descriptors
            .OrderBy(descriptor => descriptor.Category, CodePointComparer.Instance)
            .ThenBy(descriptor => descriptor.DisplayName, CodePointComparer.Instance)
            .ThenBy(descriptor => descriptor.ControlId, CodePointComparer.Instance)
            .ToArray();
    }

    public IReadOnlyDictionary<string, UiControlDescriptor> CreateControlLookup(
        bool includeUnexposedProjectAssets = false)
    {
        return GetDescriptors(includeUnexposedProjectAssets)
            .GroupBy(descriptor => descriptor.ControlId, StringComparer.Ordinal)
            .ToDictionary(
                group => group.Key,
                group => group.First(),
                StringComparer.Ordinal);
    }

    private IReadOnlyList<UiControlDescriptor> createProjectDescriptors(bool exposedOnly)
    {
        List<UiControlDescriptor> descriptors = [];
        foreach (KeyValuePair<string, JsonObject> pair in gameData.UiAssetsData
                     .OrderBy(item => item.Key, StringComparer.Ordinal))
        {
            JsonObject asset = pair.Value;
            bool exposed = isTrue((asset["palette"] as JsonObject)?["exposed"]);
            string logicalKey = UiAssetSchema.ToLogicalAssetKey(pair.Key);
            if (logicalKey.Length == 0 || exposedOnly && !exposed)
            {
                continue;
            }
            JsonObject? palette = asset["palette"] as JsonObject;
            string displayName = getString(palette?["displayName"])?.Trim()
                ?? pair.Key.Split('/').Last();
            string category = getString(palette?["category"])?.Trim() ?? "Project";
            UiDesignSize designSize = readDesignSize(asset["designSize"] as JsonObject);
            descriptors.Add(new UiControlDescriptor(
                UiAssetSchema.ProjectControlPrefix + logicalKey,
                "project",
                displayName.Length == 0 ? pair.Key.Split('/').Last() : displayName,
                category.Length == 0 ? "Project" : category,
                null,
                "none",
                null,
                [],
                logicalKey,
                designSize));
        }
        return descriptors;
    }

    private static IReadOnlyList<UiControlDescriptor> createSystemDescriptors()
    {
        return
        [
            descriptor(
                "Engine.Canvas",
                "Canvas",
                "Layout",
                "multiple",
                "canvas",
                [
                    property("size", "Size", "sf.Vector2u", false, new JsonArray(100, 100)),
                ]),
            descriptor(
                "Engine.ListView",
                "List View",
                "Layout",
                "multiple",
                "list",
                [
                    property("size", "Size", "sf.Vector2f", false, jsonArray("[100.0,100.0]")),
                    property("defaultItemHeight", "Default Item Height", "int", false, JsonValue.Create(32)),
                    property("fixItemHeight", "Fix Item Height", "bool", false, JsonValue.Create(false)),
                    property("columns", "Columns", "int", false, JsonValue.Create(1)),
                ]),
            descriptor(
                "Engine.Window",
                "Window",
                "Visual",
                "none",
                null,
                [
                    property("size", "Size", "sf.Vector2u", false, new JsonArray(160, 96)),
                    property("windowSkin", "Window Skin", "string", false, JsonValue.Create(string.Empty)),
                    property("repeated", "Repeated", "bool", false, JsonValue.Create(false)),
                ]),
            descriptor(
                "Engine.Rect",
                "Rect",
                "Visual",
                "none",
                null,
                [
                    property("size", "Size", "sf.Vector2f", false, jsonArray("[160.0,96.0]")),
                    property("windowSkin", "Window Skin", "string", false, JsonValue.Create(string.Empty)),
                    property("opacityCurve", "Opacity Curve", "string", false, JsonValue.Create(string.Empty)),
                ]),
            descriptor(
                "Engine.SolidRect",
                "Solid Rect",
                "Visual",
                "none",
                null,
                [
                    property("size", "Size", "sf.Vector2f", false, jsonArray("[100.0,32.0]")),
                    property("fillColor", "Fill Color", "sf.Color", false, new JsonArray(255, 255, 255, 255)),
                    property("outlineColor", "Outline Color", "sf.Color", false, new JsonArray(0, 0, 0, 0)),
                    property("outlineThickness", "Outline Thickness", "float", false, jsonNumber("0.0")),
                ]),
            descriptor(
                "Engine.ProgressBar",
                "Progress Bar",
                "Visual",
                "none",
                null,
                [
                    property("size", "Size", "sf.Vector2f", false, jsonArray("[100.0,12.0]")),
                    property("progress", "Progress", "float", false, jsonNumber("0.0")),
                    property("backgroundColor", "Background Color", "sf.Color", false, new JsonArray(255, 255, 255, 64)),
                    property("fillColor", "Fill Color", "sf.Color", false, new JsonArray(255, 255, 255, 255)),
                ]),
            descriptor(
                "Engine.Image",
                "Image",
                "Visual",
                "none",
                null,
                [
                    property("texture", "Texture", "string", false, JsonValue.Create(string.Empty)),
                    property("textureRect", "Texture Rect", "sf.IntRect", false, null),
                    property("colour", "Colour", "sf.Color", false, new JsonArray(255, 255, 255, 255)),
                ]),
            descriptor(
                "Engine.Button",
                "Button",
                "Input",
                "none",
                null,
                [
                    property("texture", "Texture", "string", false, JsonValue.Create(string.Empty)),
                    property("textureRect", "Texture Rect", "sf.IntRect", false, null),
                    property("colour", "Colour", "sf.Color", false, new JsonArray(255, 255, 255, 255)),
                    property("hoverColour", "Hover Colour", "sf.Color", false, new JsonArray(255, 255, 255, 255)),
                    property("pressedColour", "Pressed Colour", "sf.Color", false, new JsonArray(255, 255, 255, 255)),
                ]),
            descriptor(
                "Engine.CheckBox",
                "Check Box",
                "Input",
                "none",
                null,
                [
                    property("size", "Size", "sf.Vector2f", false, jsonArray("[32.0,32.0]")),
                    property("checked", "Checked", "bool", false, JsonValue.Create(false)),
                    property("windowSkin", "Window Skin", "string", false, JsonValue.Create(string.Empty)),
                    property("textConfig", "Text Config", "string", false, JsonValue.Create("UI/Text20")),
                ]),
            descriptor(
                "Engine.Slider",
                "Slider",
                "Input",
                "none",
                null,
                [
                    property("size", "Size", "sf.Vector2f", false, jsonArray("[64.0,8.0]")),
                    property("minValue", "Min Value", "int", false, JsonValue.Create(0)),
                    property("maxValue", "Max Value", "int", false, JsonValue.Create(100)),
                    property("value", "Value", "int", false, JsonValue.Create(0)),
                    property("lineTexture", "Line Texture", "string", false, JsonValue.Create("Assets/System/SliderLine.png")),
                    property("handleTexture", "Handle Texture", "string", false, JsonValue.Create("Assets/System/SliderHandle.png")),
                ]),
            descriptor(
                "Engine.DropBox",
                "Drop Box",
                "Input",
                "none",
                null,
                [
                    property("size", "Size", "sf.Vector2f", false, jsonArray("[200.0,32.0]")),
                    property("windowSkin", "Window Skin", "string", false, JsonValue.Create(string.Empty)),
                    property("textConfig", "Text Config", "string", false, JsonValue.Create("UI/Text20")),
                    property("previewText", "Preview Text", "string", false, JsonValue.Create("Option"), true),
                ]),
            descriptor(
                "Engine.PlainText",
                "Plain Text",
                "Text",
                "none",
                null,
                [
                    property("textConfig", "Text Config", "string", false, JsonValue.Create(string.Empty)),
                    property("text", "Text", "string", false, JsonValue.Create(string.Empty)),
                    property("previewText", "Preview Text", "string", false, JsonValue.Create(string.Empty), true),
                    property("colour", "Colour", "sf.Color", false, new JsonArray(255, 255, 255, 255)),
                    property("outlineColor", "Outline Color", "sf.Color", false, null),
                    property("outlineThickness", "Outline Thickness", "float", false, null),
                ]),
            descriptor(
                "Engine.RichText",
                "Rich Text",
                "Text",
                "none",
                null,
                [
                    property("textConfig", "Text Config", "string", false, JsonValue.Create(string.Empty)),
                    property("text", "Text", "string", false, JsonValue.Create(string.Empty)),
                    property("previewText", "Preview Text", "string", false, JsonValue.Create(string.Empty), true),
                    property("colour", "Colour", "sf.Color", false, new JsonArray(255, 255, 255, 255)),
                ]),
            descriptor(
                "Engine.FunctionalImage",
                "Functional Image",
                "Input",
                "none",
                null,
                [
                    property("texture", "Texture", "string", false, JsonValue.Create(string.Empty)),
                    property("textureRect", "Texture Rect", "sf.IntRect", false, null),
                    property("colour", "Colour", "sf.Color", false, new JsonArray(255, 255, 255, 255)),
                ]),
            descriptor(
                "Engine.FunctionalPlainText",
                "Functional Plain Text",
                "Input",
                "none",
                null,
                [
                    property("textConfig", "Text Config", "string", false, JsonValue.Create(string.Empty)),
                    property("text", "Text", "string", false, JsonValue.Create(string.Empty)),
                    property("previewText", "Preview Text", "string", false, JsonValue.Create(string.Empty), true),
                    property("colour", "Colour", "sf.Color", false, new JsonArray(255, 255, 255, 255)),
                    property("outlineColor", "Outline Color", "sf.Color", false, null),
                    property("outlineThickness", "Outline Thickness", "float", false, null),
                ]),
            descriptor(
                "Engine.FunctionalRichText",
                "Functional Rich Text",
                "Input",
                "none",
                null,
                [
                    property("textConfig", "Text Config", "string", false, JsonValue.Create(string.Empty)),
                    property("text", "Text", "string", false, JsonValue.Create(string.Empty)),
                    property("previewText", "Preview Text", "string", false, JsonValue.Create(string.Empty), true),
                    property("colour", "Colour", "sf.Color", false, new JsonArray(255, 255, 255, 255)),
                ]),
        ];
    }

    private static UiControlDescriptor descriptor(
        string controlId,
        string displayName,
        string category,
        string childPolicy,
        string? slotType,
        IReadOnlyList<UiControlPropertyDescriptor> properties)
    {
        return new UiControlDescriptor(
            controlId,
            "system",
            displayName,
            category,
            controlId,
            childPolicy,
            slotType,
            CommonProperties.Concat(properties).ToArray());
    }

    private static UiControlPropertyDescriptor property(
        string id,
        string displayName,
        string type,
        bool required,
        JsonNode? defaultValue,
        bool editorOnly = false)
    {
        return new UiControlPropertyDescriptor(
            id,
            displayName,
            type,
            required,
            defaultValue,
            editorOnly);
    }

    private static JsonNode jsonNumber(string value)
    {
        return JsonNode.Parse(value)!;
    }

    private static JsonArray jsonArray(string value)
    {
        return (JsonArray)JsonNode.Parse(value)!;
    }

    private static UiDesignSize readDesignSize(JsonObject? designSize)
    {
        double width = getFiniteNumber(designSize?["width"]) ?? 640.0;
        double height = getFiniteNumber(designSize?["height"]) ?? 480.0;
        return new UiDesignSize(
            width,
            height,
            designSize?["width"]?.ToJsonString(),
            designSize?["height"]?.ToJsonString());
    }

    private static string createAdapterFingerprint()
    {
        StringBuilder source = new StringBuilder();
        foreach (UiControlDescriptor descriptor in Descriptors
                     .OrderBy(item => item.ControlId, StringComparer.Ordinal))
        {
            source.Append(descriptor.ControlId);
            source.Append('|');
            source.Append(descriptor.Adapter ?? string.Empty);
            source.Append('|');
            source.Append(descriptor.ChildPolicy);
            source.Append('|');
            source.Append(descriptor.SlotType ?? string.Empty);
            source.Append('|');
            foreach (UiControlPropertyDescriptor propertyDescriptor in
                     descriptor.Properties.Where(property => !property.EditorOnly))
            {
                source.Append(propertyDescriptor.Id);
                source.Append(':');
                source.Append(propertyDescriptor.Type);
                source.Append(':');
                source.Append(propertyDescriptor.Required ? "true" : "false");
                source.Append(';');
            }
            source.Append('\n');
        }
        byte[] hash = SHA256.HashData(Encoding.UTF8.GetBytes(source.ToString()));
        return Convert.ToHexString(hash).ToLowerInvariant();
    }

    private static double? getFiniteNumber(JsonNode? value)
    {
        if (value is not JsonValue scalar || !scalar.TryGetValue(out double number) || !double.IsFinite(number))
            return null;
        return number;
    }

    private static string? getString(JsonNode? value)
    {
        return value is JsonValue scalar && scalar.TryGetValue(out string? text) ? text : null;
    }

    private static bool isTrue(JsonNode? value)
    {
        return value is JsonValue scalar && scalar.TryGetValue(out bool enabled) && enabled;
    }

    private sealed class CodePointComparer : IComparer<string>
    {
        public static CodePointComparer Instance { get; } = new CodePointComparer();

        public int Compare(string? left, string? right)
        {
            if (ReferenceEquals(left, right))
                return 0;
            if (left is null)
                return -1;
            if (right is null)
                return 1;
            StringRuneEnumerator leftRunes = left.EnumerateRunes();
            StringRuneEnumerator rightRunes = right.EnumerateRunes();
            while (leftRunes.MoveNext())
            {
                if (!rightRunes.MoveNext())
                    return 1;
                int comparison = leftRunes.Current.Value.CompareTo(rightRunes.Current.Value);
                if (comparison != 0)
                    return comparison;
            }
            return rightRunes.MoveNext() ? -1 : 0;
        }
    }
}
