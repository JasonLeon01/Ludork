using Ludork.Models;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed class UiAssetValidationService
{
    private static readonly HashSet<string> AssetFields = new HashSet<string>(
        ["type", "designSize", "palette", "root"],
        StringComparer.Ordinal);
    private static readonly HashSet<string> DesignSizeFields = new HashSet<string>(
        ["width", "height"],
        StringComparer.Ordinal);
    private static readonly HashSet<string> PaletteFields = new HashSet<string>(
        ["exposed", "displayName", "category"],
        StringComparer.Ordinal);
    private static readonly HashSet<string> NodeFields = new HashSet<string>(
        ["id", "name", "controlId", "properties", "slot", "editor", "children"],
        StringComparer.Ordinal);
    private static readonly HashSet<string> CanvasSlotFields = new HashSet<string>(
        ["anchors", "offsets", "alignment", "autoSize", "zOrder"],
        StringComparer.Ordinal);
    private static readonly HashSet<string> AnchorFields = new HashSet<string>(
        ["min", "max"],
        StringComparer.Ordinal);
    private static readonly HashSet<string> OffsetFields = new HashSet<string>(
        ["left", "top", "right", "bottom"],
        StringComparer.Ordinal);
    private static readonly HashSet<string> EditorFields = new HashSet<string>(
        ["previewText"],
        StringComparer.Ordinal);

    private readonly GameDataService gameData;
    private readonly UiControlRegistryService controlRegistry;

    public UiAssetValidationService(
        GameDataService gameData,
        UiControlRegistryService controlRegistry)
    {
        this.gameData = gameData;
        this.controlRegistry = controlRegistry;
    }

    public UiAssetValidationResult ValidateAsset(string assetKey, JsonObject? data = null)
    {
        string normalizedKey = UiAssetSchema.NormalizeAssetKey(assetKey);
        string dataKey = UiAssetSchema.ToAssetDataKey(normalizedKey);
        List<UiValidationIssue> issues = [];
        if (normalizedKey.Length == 0)
        {
            add(issues, "assetKey", string.Empty, "UI asset key must be under Assets");
            return new UiAssetValidationResult(assetKey, issues);
        }
        if (data is null && !gameData.UiAssetsData.TryGetValue(dataKey, out data))
        {
            add(issues, "missingAsset", string.Empty, $"UI asset \"{normalizedKey}\" was not found");
            return new UiAssetValidationResult(normalizedKey, issues);
        }
        IReadOnlyDictionary<string, UiControlDescriptor> controls =
            controlRegistry.CreateControlLookup();
        validateAssetStructure(normalizedKey, data, controls, issues);
        return new UiAssetValidationResult(normalizedKey, issues);
    }

    public IReadOnlyList<UiAssetValidationResult> ValidateAll()
    {
        Dictionary<string, List<UiValidationIssue>> issuesByKey =
            new Dictionary<string, List<UiValidationIssue>>(StringComparer.Ordinal);
        IReadOnlyDictionary<string, UiControlDescriptor> controls =
            controlRegistry.CreateControlLookup();
        foreach (KeyValuePair<string, JsonObject> pair in gameData.UiAssetsData
                     .OrderBy(item => item.Key, StringComparer.Ordinal))
        {
            List<UiValidationIssue> issues = [];
            string logicalKey = UiAssetSchema.ToLogicalAssetKey(pair.Key);
            if (logicalKey.Length == 0)
            {
                add(
                    issues,
                    "assetKey",
                    string.Empty,
                    "UI assets must be stored under Data/UI/Assets");
            }
            validateAssetStructure(logicalKey, pair.Value, controls, issues);
            issuesByKey[logicalKey.Length == 0 ? pair.Key : logicalKey] = issues;
        }
        validateProjectIdentities(issuesByKey);
        validateProjectCycles(issuesByKey);
        foreach (string invalidPath in gameData.InvalidLoadPaths
                     .Where(isUiPath)
                     .OrderBy(path => path, StringComparer.Ordinal))
        {
            string key = "Invalid/" + invalidPath.Replace('\\', '/');
            issuesByKey[key] =
            [
                new UiValidationIssue(
                    "invalidJson",
                    string.Empty,
                    $"UI data file could not be loaded: {invalidPath}"),
            ];
        }
        return issuesByKey
            .OrderBy(pair => pair.Key, StringComparer.Ordinal)
            .Select(pair => new UiAssetValidationResult(pair.Key, pair.Value))
            .ToArray();
    }

    private void validateAssetStructure(
        string assetKey,
        JsonObject data,
        IReadOnlyDictionary<string, UiControlDescriptor> controls,
        ICollection<UiValidationIssue> issues)
    {
        rejectUnknownFields(data, AssetFields, string.Empty, issues);
        if (getString(data["type"]) != UiAssetSchema.UiAssetType)
            add(issues, "assetType", "type", "UI asset type must be uiAsset");
        validateDesignSize(data["designSize"], issues);
        validatePalette(data["palette"], issues);
        if (data["root"] is not JsonObject root)
        {
            add(issues, "root", "root", "UI asset root must be an object");
            return;
        }
        HashSet<string> nodeIds = new HashSet<string>(StringComparer.Ordinal);
        HashSet<string> names = new HashSet<string>(StringComparer.Ordinal);
        validateNode(assetKey, root, "root", true, null, controls, nodeIds, names, issues);
    }

    private void validateNode(
        string assetKey,
        JsonObject node,
        string path,
        bool root,
        UiControlDescriptor? parent,
        IReadOnlyDictionary<string, UiControlDescriptor> controls,
        ISet<string> nodeIds,
        ISet<string> names,
        ICollection<UiValidationIssue> issues)
    {
        rejectUnknownFields(node, NodeFields, path, issues);
        string? nodeId = validateUuid(node["id"], path + ".id", issues);
        if (nodeId is not null && !nodeIds.Add(nodeId))
            add(issues, "duplicateNodeId", path + ".id", $"Duplicate node id \"{nodeId}\"");
        string? name = getString(node["name"]);
        if (string.IsNullOrWhiteSpace(name))
        {
            add(issues, "nodeName", path + ".name", "Node name must be a non-empty string");
        }
        else if (!names.Add(name))
        {
            add(issues, "duplicateNodeName", path + ".name", $"Duplicate node name \"{name}\"");
        }

        if (root)
        {
            if (node.ContainsKey("slot"))
                add(issues, "rootSlot", path + ".slot", "Root node must not declare a slot");
        }
        else
        {
            validateSlot(node["slot"], parent, path + ".slot", issues);
        }

        JsonObject? properties = node["properties"] as JsonObject;
        if (properties is null)
        {
            add(issues, "properties", path + ".properties", "Node properties must be an object");
            properties = [];
        }
        JsonObject? editor = node["editor"] as JsonObject;
        if (node.ContainsKey("editor") && editor is null)
            add(issues, "editor", path + ".editor", "Node editor data must be an object");
        editor ??= [];
        JsonArray? children = node["children"] as JsonArray;
        if (children is null)
        {
            add(issues, "children", path + ".children", "Node children must be an array");
            children = [];
        }

        string? controlId = getString(node["controlId"]);
        if (string.IsNullOrWhiteSpace(controlId))
        {
            add(issues, "controlId", path + ".controlId", "Node controlId must be a non-empty string");
            return;
        }
        if (controlId.StartsWith(UiAssetSchema.ProjectControlPrefix, StringComparison.Ordinal))
        {
            if (!UiAssetSchema.TryGetProjectAssetKey(controlId, out string targetKey))
            {
                add(issues, "assetKey", path + ".controlId", $"Invalid nested UI asset key \"{controlId}\"");
                return;
            }
            string targetDataKey = UiAssetSchema.ToAssetDataKey(targetKey);
            if (!gameData.UiAssetsData.TryGetValue(targetDataKey, out JsonObject? targetAsset))
            {
                add(issues, "missingAsset", path + ".controlId", $"Missing nested UI asset: {assetKey} -> {targetKey}");
                return;
            }
            if (targetAsset["palette"] is not JsonObject targetPalette
                || targetPalette["exposed"] is not JsonValue exposedValue
                || !exposedValue.TryGetValue<bool>(out bool exposed)
                || !exposed)
            {
                add(issues, "assetNotExposed", path + ".controlId", $"Nested UI asset is not exposed: {assetKey} -> {targetKey}");
                return;
            }
        }
        if (!controls.TryGetValue(controlId, out UiControlDescriptor? descriptor))
        {
            add(issues, "unknownControl", path + ".controlId", $"Unknown UI control \"{controlId}\"");
            return;
        }

        bool projectControl = descriptor.Source == "project";
        if (projectControl)
        {
            if (properties.Count != 0)
                add(issues, "nestedProperties", path + ".properties", "Nested UI assets cannot override properties");
            if (editor.Count != 0)
                add(issues, "nestedEditor", path + ".editor", "Nested UI assets cannot override editor data");
            if (children.Count != 0)
                add(issues, "nestedChildren", path + ".children", "Nested UI assets cannot override children");
        }
        else
        {
            validateProperties(properties, descriptor, path + ".properties", issues);
            validateEditor(editor, descriptor, path + ".editor", issues);
        }

        if (descriptor.ChildPolicy == "none" && children.Count != 0)
            add(issues, "childPolicy", path + ".children", $"{controlId} cannot contain child nodes");
        if (descriptor.ChildPolicy == "single" && children.Count > 1)
            add(issues, "childPolicy", path + ".children", $"{controlId} can contain only one child node");
        if (descriptor.ChildPolicy is not ("none" or "single" or "multiple"))
            add(issues, "childPolicy", path + ".controlId", $"{controlId} declares an invalid child policy");

        for (int index = 0; index < children.Count; index++)
        {
            if (children[index] is not JsonObject child)
            {
                add(issues, "child", $"{path}.children[{index}]", "Child node must be an object");
                continue;
            }
            validateNode(
                assetKey,
                child,
                $"{path}.children[{index}]",
                false,
                descriptor,
                controls,
                nodeIds,
                names,
                issues);
        }
    }

    private void validateProperties(
        JsonObject properties,
        UiControlDescriptor descriptor,
        string path,
        ICollection<UiValidationIssue> issues)
    {
        IReadOnlyDictionary<string, UiControlPropertyDescriptor> propertyLookup = descriptor.Properties
            .Where(property => !property.EditorOnly)
            .ToDictionary(property => property.Id, StringComparer.Ordinal);
        foreach (KeyValuePair<string, JsonNode?> pair in properties)
        {
            if (!propertyLookup.TryGetValue(pair.Key, out UiControlPropertyDescriptor? property))
            {
                add(issues, "unknownProperty", path + "." + pair.Key, $"Unknown property for {descriptor.ControlId}");
                continue;
            }
            validatePropertyType(pair.Value, property.Type, path + "." + pair.Key, issues);
            validatePropertySemantics(
                descriptor.ControlId,
                pair.Key,
                pair.Value,
                path + "." + pair.Key,
                issues);
            validatePropertyReference(
                descriptor.ControlId,
                pair.Key,
                pair.Value,
                path + "." + pair.Key,
                issues);
        }
        foreach (UiControlPropertyDescriptor property in propertyLookup.Values)
        {
            if (property.Required && !properties.ContainsKey(property.Id))
                add(issues, "requiredProperty", path + "." + property.Id, "Required property is missing");
        }
    }

    private static void validateEditor(
        JsonObject editor,
        UiControlDescriptor descriptor,
        string path,
        ICollection<UiValidationIssue> issues)
    {
        rejectUnknownFields(editor, EditorFields, path, issues);
        if (!editor.TryGetPropertyValue("previewText", out JsonNode? previewText))
            return;
        UiControlPropertyDescriptor? previewProperty = descriptor.Properties
            .FirstOrDefault(property =>
                property.EditorOnly
                && property.Id == "previewText");
        if (previewProperty is null)
        {
            add(issues, "previewTextControl", path + ".previewText", "Preview Text is only valid on text controls");
            return;
        }
        validatePropertyType(
            previewText,
            previewProperty.Type,
            path + ".previewText",
            issues);
    }

    private static void validateSlot(
        JsonNode? slotNode,
        UiControlDescriptor? parent,
        string path,
        ICollection<UiValidationIssue> issues)
    {
        if (slotNode is not JsonObject slot)
        {
            add(issues, "slot", path, "Non-root nodes must declare a slot object");
            return;
        }
        if (parent?.SlotType == "canvas")
        {
            validateCanvasSlot(slot, path, issues);
            return;
        }
        if (parent?.SlotType == "list")
        {
            if (slot.Count != 0)
                add(issues, "listSlot", path, "List slots do not declare per-child properties");
            return;
        }
        add(issues, "slotType", path, "Parent control does not provide a valid slot type");
    }

    private static void validateCanvasSlot(
        JsonObject slot,
        string path,
        ICollection<UiValidationIssue> issues)
    {
        rejectUnknownFields(slot, CanvasSlotFields, path, issues);
        if (slot["anchors"] is not JsonObject anchors)
        {
            add(issues, "anchors", path + ".anchors", "Canvas slot anchors must be an object");
        }
        else
        {
            rejectUnknownFields(anchors, AnchorFields, path + ".anchors", issues);
            double[]? minimum = validatePair(anchors["min"], path + ".anchors.min", false, issues);
            double[]? maximum = validatePair(anchors["max"], path + ".anchors.max", false, issues);
            if (minimum is not null && maximum is not null)
            {
                for (int index = 0; index < 2; index++)
                {
                    if (minimum[index] < 0.0 || maximum[index] > 1.0)
                    {
                        add(issues, "anchorRange", path + ".anchors", "Anchors must stay within 0..1");
                        break;
                    }
                    if (minimum[index] > maximum[index])
                    {
                        add(issues, "anchorOrder", path + ".anchors", "Anchor minimum must not exceed maximum");
                        break;
                    }
                }
            }
        }
        if (slot["offsets"] is not JsonObject offsets)
        {
            add(issues, "offsets", path + ".offsets", "Canvas slot offsets must be an object");
        }
        else
        {
            rejectUnknownFields(offsets, OffsetFields, path + ".offsets", issues);
            foreach (string key in OffsetFields)
            {
                if (!tryGetFloatNumber(offsets[key], out double _))
                    add(issues, "offset", path + ".offsets." + key, "Offset must be a finite number");
            }
        }
        double[]? alignment = validatePair(slot["alignment"], path + ".alignment", false, issues);
        if (alignment is not null
            && alignment.Any(component => component < 0.0 || component > 1.0))
        {
            add(issues, "alignmentRange", path + ".alignment", "Alignment must stay within 0..1");
        }
        if (!tryGetBoolean(slot["autoSize"], out bool _))
            add(issues, "autoSize", path + ".autoSize", "autoSize must be a boolean");
        if (!tryGetInteger(slot["zOrder"], out long zOrder)
            || zOrder < int.MinValue
            || zOrder > int.MaxValue)
        {
            add(issues, "zOrder", path + ".zOrder", "zOrder must be a signed 32-bit integer");
        }
    }

    private static void validatePropertyType(
        JsonNode? value,
        string type,
        string path,
        ICollection<UiValidationIssue> issues)
    {
        bool valid = type switch
        {
            "bool" => tryGetBoolean(value, out bool _),
            "int" => tryGetInteger(value, out long integer)
                && integer >= int.MinValue
                && integer <= int.MaxValue,
            "float" => tryGetFloatNumber(value, out double _),
            "string" => getString(value) is not null,
            "sf.Vector2f" => validatePair(value, path, false, null) is not null,
            "sf.Vector2u" => validatePair(value, path, true, null) is not null,
            "sf.IntRect" => value is null || validateIntegerArray(value, 4, false),
            "sf.Color" => validateIntegerArray(value, 4, true),
            _ => false,
        };
        if (!valid)
            add(issues, "propertyType", path, $"Value must match declared type {type}");
    }

    private static void validatePropertySemantics(
        string controlId,
        string propertyId,
        JsonNode? value,
        string path,
        ICollection<UiValidationIssue> issues)
    {
        if (controlId == "Engine.ListView"
            && propertyId == "columns"
            && tryGetInteger(value, out long columns)
            && columns >= int.MinValue
            && columns <= int.MaxValue
            && columns <= 0)
        {
            add(issues, "propertyRange", path, "ListView columns must be positive");
        }
        if (controlId is "Engine.Canvas" or "Engine.Window"
            && propertyId == "size"
            && value is JsonArray size
            && size.Any(component =>
                tryGetInteger(component, out long number)
                && number > int.MaxValue))
        {
            add(issues, "propertyRange", path, $"Canvas and Window size components must not exceed {int.MaxValue}");
        }
        if (propertyId == "scale"
            && value is JsonArray scale
            && scale.Any(component =>
                tryGetFloatNumber(component, out double number)
                && number < 0.0))
        {
            add(issues, "propertyRange", path, "Scale components cannot be negative");
        }
    }

    private void validatePropertyReference(
        string controlId,
        string propertyId,
        JsonNode? value,
        string path,
        ICollection<UiValidationIssue> issues)
    {
        string? text = getString(value);
        if (text is null || text.Length == 0)
            return;
        if (text != text.Trim() || text.Contains('\\'))
        {
            add(issues, "resourcePath", path, "UI resources must use a canonical project-relative path");
            return;
        }
        if (propertyId is "texture"
            or "windowSkin"
            or "lineTexture"
            or "handleTexture")
        {
            if (!text.StartsWith("Assets/", StringComparison.Ordinal))
            {
                add(issues, "assetPath", path, "UI asset resources must use a canonical project-relative Assets/ path");
                return;
            }
            string fullPath = Path.GetFullPath(Path.Combine(
                gameData.ProjectPath,
                text.Replace('/', Path.DirectorySeparatorChar)));
            string assetsRoot = Path.GetFullPath(Path.Combine(gameData.ProjectPath, "Assets"));
            string relative = Path.GetRelativePath(assetsRoot, fullPath);
            if (Path.IsPathRooted(relative)
                || relative == ".."
                || relative.StartsWith(".." + Path.DirectorySeparatorChar, StringComparison.Ordinal))
            {
                add(issues, "assetPath", path, "UI asset resource path escapes the Assets directory");
            }
            else if (!File.Exists(fullPath))
            {
                add(issues, "missingResource", path, $"Resource \"{text}\" was not found");
            }
            return;
        }
        if (propertyId == "textConfig")
        {
            if (!tryGetCanonicalDataKey(text, "TextConfigs", out string key))
                add(issues, "textConfigKey", path, "Text config must use a canonical TextConfigs key without an extension");
            else if (!gameData.TextConfigsData.TryGetValue(key, out JsonObject? textConfig))
                add(issues, "missingTextConfig", path, $"Text config \"{text}\" was not found");
            else
                validateTextConfigType(controlId, textConfig, path, issues);
            return;
        }
        if (propertyId == "opacityCurve")
        {
            if (!tryGetCanonicalDataKey(text, "Curves", out string key))
                add(issues, "curveKey", path, "Curve must use a canonical Curves key without an extension");
            else if (!gameData.CurvesData.TryGetValue(key, out JsonObject? curve))
                add(issues, "missingCurve", path, $"Curve \"{text}\" was not found");
            else if (getString(curve["type"]) != "curve")
                add(issues, "curveType", path, "Rect opacityCurve must reference a scalar curve");
        }
    }

    private static void validateTextConfigType(
        string controlId,
        JsonObject textConfig,
        string path,
        ICollection<UiValidationIssue> issues)
    {
        string? expectedType = controlId switch
        {
            "Engine.CheckBox" => "plainTextConfig",
            "Engine.DropBox" => "plainTextConfig",
            "Engine.PlainText" => "plainTextConfig",
            "Engine.FunctionalPlainText" => "plainTextConfig",
            "Engine.RichText" => "richTextConfig",
            "Engine.FunctionalRichText" => "richTextConfig",
            _ => null,
        };
        if (expectedType is not null
            && getString(textConfig["type"]) != expectedType)
        {
            add(issues, "textConfigType", path, $"Text config must have type {expectedType}");
        }
    }

    private void validateProjectIdentities(
        IDictionary<string, List<UiValidationIssue>> issuesByKey)
    {
        Dictionary<string, List<(string Key, string Path)>> nodesById =
            new Dictionary<string, List<(string Key, string Path)>>(StringComparer.Ordinal);
        foreach (KeyValuePair<string, JsonObject> pair in gameData.UiAssetsData)
        {
            string logicalKey = UiAssetSchema.ToLogicalAssetKey(pair.Key);
            if (logicalKey.Length == 0)
                continue;
            if (pair.Value["root"] is JsonObject root)
                collectNodeIdentities(logicalKey, root, "root", nodesById);
        }
        foreach (KeyValuePair<string, List<(string Key, string Path)>> pair in
                 nodesById.Where(item => item.Value.Count > 1))
        {
            string locations = string.Join(
                ", ",
                pair.Value
                    .OrderBy(item => item.Key, StringComparer.Ordinal)
                    .ThenBy(item => item.Path, StringComparer.Ordinal)
                    .Select(item => item.Key + "." + item.Path));
            foreach ((string key, string path) in pair.Value)
            {
                add(
                    issuesByKey[key],
                    "duplicateProjectNodeId",
                    path + ".id",
                    $"Node id \"{pair.Key}\" is also used by {locations}");
            }
        }
    }

    private static void collectNodeIdentities(
        string assetKey,
        JsonObject node,
        string path,
        IDictionary<string, List<(string Key, string Path)>> nodesById)
    {
        string? nodeId = canonicalUuid(node["id"]);
        if (nodeId is not null)
        {
            if (!nodesById.TryGetValue(nodeId, out List<(string Key, string Path)>? locations))
            {
                locations = [];
                nodesById[nodeId] = locations;
            }
            locations.Add((assetKey, path));
        }
        if (node["children"] is not JsonArray children)
            return;
        for (int index = 0; index < children.Count; index++)
        {
            if (children[index] is JsonObject child)
                collectNodeIdentities(assetKey, child, $"{path}.children[{index}]", nodesById);
        }
    }

    private void validateProjectCycles(
        IDictionary<string, List<UiValidationIssue>> issuesByKey)
    {
        Dictionary<string, List<(string Target, string Path)>> edges =
            new Dictionary<string, List<(string Target, string Path)>>(StringComparer.Ordinal);
        foreach (KeyValuePair<string, JsonObject> pair in gameData.UiAssetsData)
        {
            string logicalKey = UiAssetSchema.ToLogicalAssetKey(pair.Key);
            if (logicalKey.Length != 0)
                edges[logicalKey] = collectNestedReferences(pair.Value);
        }
        Dictionary<string, int> states = new Dictionary<string, int>(StringComparer.Ordinal);
        List<string> stack = [];
        foreach (string assetKey in edges.Keys.OrderBy(value => value, StringComparer.Ordinal))
            visitCycle(assetKey, edges, states, stack, issuesByKey);
    }

    private static void visitCycle(
        string assetKey,
        IReadOnlyDictionary<string, List<(string Target, string Path)>> edges,
        IDictionary<string, int> states,
        IList<string> stack,
        IDictionary<string, List<UiValidationIssue>> issuesByKey)
    {
        if (states.TryGetValue(assetKey, out int state) && state == 2)
            return;
        if (state == 1)
            return;
        states[assetKey] = 1;
        stack.Add(assetKey);
        foreach ((string target, string path) in edges[assetKey])
        {
            if (!edges.ContainsKey(target))
                continue;
            if (states.TryGetValue(target, out int targetState) && targetState == 1)
            {
                int start = stack.IndexOf(target);
                string cycle = string.Join(
                    " -> ",
                    stack.Skip(start)
                        .Append(target));
                add(
                    issuesByKey[assetKey],
                    "assetCycle",
                    path,
                    $"Nested UI asset cycle: {cycle}");
                continue;
            }
            visitCycle(target, edges, states, stack, issuesByKey);
        }
        stack.RemoveAt(stack.Count - 1);
        states[assetKey] = 2;
    }

    private static List<(string Target, string Path)> collectNestedReferences(JsonObject asset)
    {
        List<(string Target, string Path)> references = [];
        if (asset["root"] is JsonObject root)
            collectNestedReferences(root, "root", references);
        return references;
    }

    private static void collectNestedReferences(
        JsonObject node,
        string path,
        ICollection<(string Target, string Path)> references)
    {
        string? controlId = getString(node["controlId"]);
        if (controlId is not null
            && UiAssetSchema.TryGetProjectAssetKey(controlId, out string assetKey))
            references.Add((assetKey, path + ".controlId"));
        if (node["children"] is not JsonArray children)
            return;
        for (int index = 0; index < children.Count; index++)
        {
            if (children[index] is JsonObject child)
                collectNestedReferences(child, $"{path}.children[{index}]", references);
        }
    }

    private static void validateDesignSize(
        JsonNode? value,
        ICollection<UiValidationIssue> issues)
    {
        if (value is not JsonObject designSize)
        {
            add(issues, "designSize", "designSize", "designSize must be an object");
            return;
        }
        rejectUnknownFields(designSize, DesignSizeFields, "designSize", issues);
        if (!tryGetFloatNumber(designSize["width"], out double width)
            || width <= 0.0
            || width > int.MaxValue)
        {
            add(issues, "designWidth", "designSize.width", $"Design width must be positive and not exceed {int.MaxValue}");
        }
        if (!tryGetFloatNumber(designSize["height"], out double height)
            || height <= 0.0
            || height > int.MaxValue)
        {
            add(issues, "designHeight", "designSize.height", $"Design height must be positive and not exceed {int.MaxValue}");
        }
    }

    private static void validatePalette(
        JsonNode? value,
        ICollection<UiValidationIssue> issues)
    {
        if (value is not JsonObject palette)
        {
            add(issues, "palette", "palette", "palette must be an object");
            return;
        }
        rejectUnknownFields(palette, PaletteFields, "palette", issues);
        if (!tryGetBoolean(palette["exposed"], out bool _))
            add(issues, "paletteExposed", "palette.exposed", "palette.exposed must be a boolean");
        if (string.IsNullOrWhiteSpace(getString(palette["displayName"])))
            add(issues, "paletteDisplayName", "palette.displayName", "Palette display name must be non-empty");
        if (string.IsNullOrWhiteSpace(getString(palette["category"])))
            add(issues, "paletteCategory", "palette.category", "Palette category must be non-empty");
    }

    private static string? validateUuid(
        JsonNode? value,
        string path,
        ICollection<UiValidationIssue> issues)
    {
        string? canonical = canonicalUuid(value);
        if (canonical is null)
            add(issues, "uuid", path, "Value must be a canonical UUID string");
        return canonical;
    }

    private static string? canonicalUuid(JsonNode? value)
    {
        string? text = getString(value);
        if (text is null
            || !Guid.TryParseExact(text, "D", out Guid parsed)
            || !string.Equals(text, parsed.ToString("D"), StringComparison.Ordinal))
        {
            return null;
        }
        return text;
    }

    private static double[]? validatePair(
        JsonNode? value,
        string path,
        bool unsignedInteger,
        ICollection<UiValidationIssue>? issues)
    {
        if (value is not JsonArray array || array.Count != 2)
        {
            if (issues is not null)
                add(issues, "pair", path, "Value must be a two-item array");
            return null;
        }
        double[] result = new double[2];
        for (int index = 0; index < 2; index++)
        {
            bool valid = unsignedInteger
                ? tryGetInteger(array[index], out long integer)
                    && integer >= 0
                    && integer <= uint.MaxValue
                : tryGetFloatNumber(array[index], out double _);
            if (!valid)
            {
                if (issues is not null)
                    add(issues, "pairValue", $"{path}[{index}]", "Component has an invalid numeric value");
                return null;
            }
            result[index] = unsignedInteger
                ? getInteger(array[index])
                : getNumber(array[index]);
        }
        return result;
    }

    private static bool validateIntegerArray(JsonNode? value, int count, bool colour)
    {
        if (value is not JsonArray array || array.Count != count)
            return false;
        foreach (JsonNode? item in array)
        {
            if (!tryGetInteger(item, out long number)
                || colour && (number < 0 || number > 255)
                || !colour && (number < int.MinValue || number > int.MaxValue))
            {
                return false;
            }
        }
        return true;
    }

    private static bool tryGetCanonicalDataKey(
        string value,
        string section,
        out string key)
    {
        key = string.Empty;
        if (value.Length == 0
            || value.StartsWith("/", StringComparison.Ordinal)
            || value.EndsWith("/", StringComparison.Ordinal)
            || value.Contains("//", StringComparison.Ordinal)
            || value.StartsWith(section + "/", StringComparison.Ordinal)
            || value.StartsWith("Data." + section + ".", StringComparison.Ordinal)
            || Path.GetExtension(value).Length != 0)
        {
            return false;
        }
        string[] parts = value.Split('/');
        if (parts.Any(part => part.Length == 0 || part is "." or ".."))
            return false;
        key = value;
        return true;
    }

    private static void rejectUnknownFields(
        JsonObject value,
        ISet<string> allowed,
        string path,
        ICollection<UiValidationIssue> issues)
    {
        foreach (string key in value.Select(pair => pair.Key).Where(key => !allowed.Contains(key)))
        {
            string fieldPath = path.Length == 0 ? key : path + "." + key;
            add(issues, "unknownField", fieldPath, "Field is not part of the declarative UI schema");
        }
    }

    private static bool tryGetBoolean(JsonNode? value, out bool result)
    {
        result = false;
        return value is JsonValue scalar && scalar.TryGetValue(out result);
    }

    private static bool tryGetInteger(JsonNode? value, out long result)
    {
        result = 0;
        if (value is not JsonValue scalar)
            return false;
        if (scalar.TryGetValue(out int integer))
        {
            result = integer;
            return true;
        }
        if (scalar.TryGetValue(out long longInteger))
        {
            result = longInteger;
            return true;
        }
        if (scalar.TryGetValue(out uint unsignedInteger))
        {
            result = unsignedInteger;
            return true;
        }
        if (scalar.TryGetValue(out ulong unsignedLong) && unsignedLong <= long.MaxValue)
        {
            result = (long)unsignedLong;
            return true;
        }
        if (scalar.TryGetValue(out short shortInteger))
        {
            result = shortInteger;
            return true;
        }
        if (scalar.TryGetValue(out ushort unsignedShort))
        {
            result = unsignedShort;
            return true;
        }
        if (scalar.TryGetValue(out byte byteInteger))
        {
            result = byteInteger;
            return true;
        }
        if (scalar.TryGetValue(out sbyte signedByte))
        {
            result = signedByte;
            return true;
        }
        return false;
    }

    private static long getInteger(JsonNode? value)
    {
        tryGetInteger(value, out long result);
        return result;
    }

    private static bool tryGetFiniteNumber(JsonNode? value, out double result)
    {
        result = 0.0;
        if (value is not JsonValue scalar)
            return false;
        if (scalar.TryGetValue(out double number))
        {
            result = number;
            return double.IsFinite(result);
        }
        if (scalar.TryGetValue(out int integer32))
        {
            result = integer32;
            return true;
        }
        if (scalar.TryGetValue(out long integer))
        {
            result = integer;
            return true;
        }
        return false;
    }

    private static bool tryGetFloatNumber(JsonNode? value, out double result)
    {
        return tryGetFiniteNumber(value, out result)
            && result >= -float.MaxValue
            && result <= float.MaxValue;
    }

    private static double getNumber(JsonNode? value)
    {
        tryGetFiniteNumber(value, out double result);
        return result;
    }

    private static string? getString(JsonNode? value)
    {
        return value is JsonValue scalar && scalar.TryGetValue(out string? text) ? text : null;
    }

    private static bool isUiPath(string path)
    {
        string normalized = path.Replace('\\', '/');
        return normalized.StartsWith("Data/UI/", StringComparison.OrdinalIgnoreCase);
    }

    private static void add(
        ICollection<UiValidationIssue> issues,
        string code,
        string path,
        string message)
    {
        issues.Add(new UiValidationIssue(code, path, message));
    }
}
