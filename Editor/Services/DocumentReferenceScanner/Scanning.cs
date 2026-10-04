using Ludork.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

internal sealed partial class DocumentReferenceScanner
{
    private void scanTextConfigReferences(string sourceId, string key, JsonObject data)
    {
        addAssetReference(
            sourceId,
            data["font"],
            "font",
            $"TextConfigs/{key}.font");
        if (data["gradient"] is JsonObject gradient
            && ReferenceIdentity.NormalizeParameter(gradient["curve"]) is string curve
            && curve.Length != 0)
        {
            addReference(
                sourceId,
                ReferenceIdentity.NodeId("curve", normalizeDataReference(curve, "Curves")),
                "curve",
                $"TextConfigs/{key}.gradient.curve");
        }
    }

    private void scanUiAssetReferences(
        string sourceId,
        string key,
        JsonObject data)
    {
        if (data["root"] is JsonObject root)
            scanUiNodeReferences(sourceId, key, root, "root");
    }

    private void scanUiNodeReferences(
        string sourceId,
        string key,
        JsonObject node,
        string path)
    {
        cancellationToken.ThrowIfCancellationRequested();
        string? controlId = JsonScalar.String(node["controlId"]);
        if (controlId is not null
            && UiAssetSchema.TryGetProjectAssetKey(controlId, out string targetAssetKey))
        {
            string targetKey = UiAssetSchema.ToAssetDataKey(targetAssetKey);
            addReference(
                sourceId,
                ReferenceIdentity.NodeId("uiAsset", targetKey),
                "nestedUiAsset",
                $"UI/{key}.{path}.controlId");
        }
        if (node["properties"] is JsonObject properties)
        {
            if (controlId == "Engine.EmitterView"
                && ReferenceIdentity.NormalizeParameter(properties["particle"]) is string particle && particle.Length != 0)
                addReference(sourceId, ReferenceIdentity.NodeId("particle", particle), "particle", $"UI/{key}.{path}.properties.particle");
            foreach (string propertyName in new[]
                     {
                         "texture",
                         "backgroundTexture",
                         "fillTexture",
                         "windowSkin",
                         "lineTexture",
                         "handleTexture",
                         "font",
                         "shader",
                     })
            {
                addAssetReference(
                    sourceId,
                    properties[propertyName],
                    "uiResource",
                    $"UI/{key}.{path}.properties.{propertyName}");
            }
            if (ReferenceIdentity.NormalizeParameter(properties["textConfig"]) is string textConfig
                && textConfig.Length != 0)
            {
                addReference(
                    sourceId,
                    ReferenceIdentity.NodeId("textConfig", normalizeDataReference(textConfig, "TextConfigs")),
                    "textConfig",
                    $"UI/{key}.{path}.properties.textConfig");
            }
            if (ReferenceIdentity.NormalizeParameter(properties["opacityCurve"]) is string curve
                && curve.Length != 0)
            {
                addReference(
                    sourceId,
                    ReferenceIdentity.NodeId("curve", normalizeDataReference(curve, "Curves")),
                    "curve",
                    $"UI/{key}.{path}.properties.opacityCurve");
            }
        }
        if (node["children"] is not JsonArray children)
            return;
        for (int index = 0; index < children.Count; index++)
        {
            if (children[index] is JsonObject child)
            {
                scanUiNodeReferences(
                    sourceId,
                    key,
                    child,
                    $"{path}.children[{index}]");
            }
        }
    }

    private void scanConfigReferences(string sourceId, string key, JsonObject data)
    {
        foreach (KeyValuePair<string, JsonNode?> pair in data)
        {
            cancellationToken.ThrowIfCancellationRequested();
            if (pair.Value is not JsonObject setting)
                continue;
            string? valueType = JsonScalar.String(setting["type"]);
            if (valueType is null || !valueType.StartsWith("file", StringComparison.Ordinal))
                continue;
            JsonArray values = setting["value"] is JsonArray array
                ? array
                : new JsonArray(setting["value"]?.DeepClone());
            string root = JsonScalar.String(setting["root"]) ?? "Assets";
            string baseDirectory = JsonScalar.String(setting["base"]) ?? string.Empty;
            for (int index = 0; index < values.Count; index++)
            {
                string path = setting["value"] is JsonArray
                    ? $"Configs/{key}.{pair.Key}.value[{index}]"
                    : $"Configs/{key}.{pair.Key}.value";
                if (root.Equals("Data", StringComparison.OrdinalIgnoreCase)
                    && baseDirectory.Equals("Maps", StringComparison.OrdinalIgnoreCase))
                {
                    addMapReference(sourceId, values[index], "configFile", path);
                }
                else if (root.Equals("Data", StringComparison.OrdinalIgnoreCase)
                    && ReferenceIdentity.NormalizeParameter(values[index]) is string dataReference
                    && dataReference.Length != 0)
                {
                    if (baseDirectory.Equals("Blueprints", StringComparison.OrdinalIgnoreCase))
                    {
                        string normalized = normalizeDataReference(dataReference, "Blueprints");
                        string target = ReferenceIdentity.BlueprintNodeId(normalized);
                        addReference(sourceId, target, "configFile", path);
                    }
                    else if (getSectionType(baseDirectory) is string sectionType)
                    {
                        addReference(sourceId,
                            ReferenceIdentity.NodeId(sectionType, normalizeDataReference(dataReference, baseDirectory)),
                            "configFile", path);
                    }
                }
                else if (string.IsNullOrWhiteSpace(root)
                    || root.Equals("Assets", StringComparison.Ordinal))
                {
                    addAssetReference(sourceId, values[index], "configFile", path);
                }
            }
        }
    }

    private void scanAutoTileReferences(string sourceId, JsonNode? value, string path)
    {
        cancellationToken.ThrowIfCancellationRequested();
        string? key = JsonScalar.String(value);
        if (!string.IsNullOrWhiteSpace(key))
        {
            addReference(sourceId, ReferenceIdentity.NodeId("autoTile", key), "autoTile", path);
            return;
        }
        if (value is not JsonArray array)
            return;
        for (int index = 0; index < array.Count; index++)
            scanAutoTileReferences(sourceId, array[index], $"{path}[{index}]");
    }

    private void scanMapActorReferences(string sourceId, JsonNode? value, string path,
        JsonObject? overrides, string overridesPath)
    {
        if (value is not JsonArray actors)
            return;
        for (int index = 0; index < actors.Count; index++)
        {
            cancellationToken.ThrowIfCancellationRequested();
            if (actors[index] is not JsonObject actor)
                continue;
            string? blueprintId = blueprintNodeIdFromClassPath(actor["bp"]);
            if (blueprintId is not null)
                addReference(sourceId, blueprintId, "mapActor", $"{path}[{index}].bp");
            string? classReference = JsonScalar.String(actor["bp"]);
            string? tag = JsonScalar.String(actor["tag"]);
            JsonObject? actorOverrides = tag is null ? null : overrides?[tag] as JsonObject;
            if (!string.IsNullOrWhiteSpace(classReference) && (actorOverrides is not null || blueprintId is null))
            {
                ResolvedBlueprintClass resolved = classResolver.Resolve(classReference, actorOverrides);
                scanResolvedFieldReferences(sourceId, resolved,
                    actorOverrides is null ? $"{path}[{index}]" : $"{overridesPath}.{tag}",
                    "mapActor", actorOverrides);
            }
            scanGenericReferences(sourceId, actor, $"{path}[{index}]");
        }
    }

    private void scanBlueprintReferences(string key, JsonObject data)
    {
        string sourceId = ReferenceIdentity.BlueprintNodeId(key);
        string? parentId = blueprintNodeIdFromClassPath(data["parent"]);
        if (parentId is not null)
            addReference(sourceId, parentId, "parent", $"Blueprints/{key}.parent");

        if (data["attrDefs"] is JsonObject declarations)
        {
            foreach (KeyValuePair<string, JsonNode?> declaration in declarations)
                scanEnumSchemaReferences(sourceId, declaration.Value?["type"], $"Blueprints/{key}.attrDefs.{declaration.Key}.type");
        }
        JsonObject attrs = data["attrs"] as JsonObject ?? [];
        ResolvedBlueprintClass resolved = classResolver.ResolveBlueprint(data, key);
        scanResolvedFieldReferences(sourceId, resolved, $"Blueprints/{key}.attrs", "attribute");
        foreach (string name in new[] { "texturePath", "shaderPath" })
        {
            JsonNode? value = attrs[name] ?? resolved.GetValue(name);
            if (value is not null)
            {
                addAssetReference(
                    sourceId,
                    value,
                    "asset",
                    $"Blueprints/{key}.attrs.{name}");
            }
        }
        scanGenericReferences(sourceId, attrs, $"Blueprints/{key}.attrs");

        if (data["graph"] is JsonObject graph)
        {
            if (graph["nodeGraph"] is JsonObject)
            {
                BlueprintGraphContext context = new(data, key);
                BlueprintNodeDefinitionSet definitions =
                    blueprintDefinitions.GetNodeDefinitionSet(context, resolved);
                scanNodeGraphReferences(sourceId, graph, $"Blueprints/{key}.graph", definitions);
            }
            scanGenericReferences(sourceId, graph, $"Blueprints/{key}.graph");
        }
    }

    private void scanAnimationReferences(string sourceId, JsonObject data, string key)
    {
        if (data["assets"] is JsonArray assets)
        {
            for (int index = 0; index < assets.Count; index++)
            {
                addAssetReference(sourceId, assets[index], "animationAsset", $"assets[{index}]");
            }
        }
        scanGenericReferences(sourceId, data, $"Animations/{key}");
    }

    private void scanParticleReferences(string key, JsonObject data)
    {
        string source = ReferenceIdentity.NodeId("particle", key);
        scanGenericReferences(source, data, $"Particles/{key}");
        if (data["tracks"] is not JsonArray tracks)
            return;
        for (int index = 0; index < tracks.Count; index++)
        {
            if (tracks[index] is not JsonObject track || track["curves"] is not JsonObject curves)
                continue;
            foreach (KeyValuePair<string, JsonNode?> pair in curves)
            {
                if (JsonScalar.String(pair.Value) is string curve && curve.Length != 0)
                    addReference(source, ReferenceIdentity.NodeId("curve", curve), "curve", $"Particles/{key}.tracks[{index}].curves.{pair.Key}");
            }
        }
    }

    private void scanGeneralReferences(
        string key,
        JsonObject data)
    {
        string sourceId = ReferenceIdentity.NodeId("general", key);
        JsonObject parameterSchema = data["params"] as JsonObject ?? [];
        foreach (KeyValuePair<string, JsonNode?> parameter in parameterSchema)
        {
            if (parameter.Value is not JsonObject definition)
                continue;
            foreach (string property in new[] { "type", "itemType", "valueType" })
                scanEnumSchemaReferences(sourceId, definition[property], $"General/{key}.params.{parameter.Key}.{property}");
            scanTypedReferences(sourceId, GeneralDataParameterSchema.GetValueSchema(definition),
                definition["defaultValue"], parameter.Key, null, null, null,
                $"General/{key}.params.{parameter.Key}.defaultValue", "defaultValue", []);
        }
        if (data["members"] is not JsonObject members)
            return;
        foreach (KeyValuePair<string, JsonNode?> pair in members)
        {
            cancellationToken.ThrowIfCancellationRequested();
            string memberId = generalMemberNodeId(key, pair.Key);
            addReference(sourceId, memberId, "member", $"General/{key}.members.{pair.Key}");
            if (pair.Value is not JsonObject member)
                continue;
            scanGeneralParameterReferences(memberId, key, pair.Key, member, parameterSchema);
            if (member["_graph"] is JsonObject graph)
            {
                if (graph["nodeGraph"] is JsonObject)
                {
                    scanNodeGraphReferences(
                        memberId,
                        graph,
                        $"General/{key}/{pair.Key}._graph",
                        getGlobalDefinitions());
                }
                scanGenericReferences(memberId, graph, $"General/{key}/{pair.Key}._graph");
            }
        }
    }

    private void scanGeneralParameterReferences(
        string sourceId,
        string dataKey,
        string memberKey,
        JsonObject member,
        JsonObject schema)
    {
        foreach (KeyValuePair<string, JsonNode?> pair in schema)
        {
            if (pair.Value is not JsonObject definition)
                continue;
            LuaMetadataType type = GeneralDataParameterSchema.GetValueSchema(definition);
            scanTypedReferences(sourceId, type, member[pair.Key], pair.Key, null, null, null,
                $"General/{dataKey}/{memberKey}.{pair.Key}", "member", []);
        }
    }

    private void scanNodeGraphReferences(
        string sourceId,
        JsonObject graphData,
        string path,
        BlueprintNodeDefinitionSet definitions)
    {
        if (graphData["nodeGraph"] is not JsonObject nodeGraph)
            return;
        IReadOnlyDictionary<string, BlueprintGraphNodeDefinition> lookup = definitions.RuntimeLookup;
        foreach (KeyValuePair<string, JsonNode?> graphPair in nodeGraph)
        {
            if (graphPair.Value is not JsonObject graph || graph["nodes"] is not JsonArray graphNodes)
                continue;
            for (int index = 0; index < graphNodes.Count; index++)
            {
                cancellationToken.ThrowIfCancellationRequested();
                if (graphNodes[index] is not JsonObject node)
                    continue;
                string nodePath = $"{path}.nodeGraph.{graphPair.Key}.nodes[{index}]";
                string? nodeFunction = JsonScalar.String(node["nodeFunction"]);
                JsonArray parameters = node["params"] as JsonArray ?? [];
                if (nodeFunction is not null && lookup.TryGetValue(nodeFunction, out BlueprintGraphNodeDefinition? definition))
                    scanDefinitionParameterReferences(sourceId, definition, parameters, nodePath);
                scanKnownNodeParameterReferences(sourceId, nodeFunction, parameters, nodePath);
                scanGenericReferences(sourceId, parameters, $"{nodePath}.params");
            }
        }
    }

    private void scanDefinitionParameterReferences(
        string sourceId,
        BlueprintGraphNodeDefinition definition,
        JsonArray parameters,
        string path)
    {
        foreach (BlueprintGraphPortDefinition port in definition.Ports)
        {
            if (port.Direction != BlueprintGraphPortDirection.Input
                || port.Kind != BlueprintGraphPortKind.Params
                || port.ParameterIndex is not int parameterIndex
                || parameterIndex < 0)
            {
                continue;
            }
            JsonNode? value = parameterIndex < parameters.Count
                ? parameters[parameterIndex] ?? port.DefaultValue
                : port.DefaultValue;
            string referencePath = $"{path}.params[{parameterIndex}]";
            scanTypedReferences(sourceId, LuaMetadataType.Parse(port.TypeName), value, port.Name,
                port.Meta, null, null, referencePath, "nodeParam", []);
        }
    }

    private void scanKnownNodeParameterReferences(
        string sourceId,
        string? nodeFunction,
        JsonArray parameters,
        string path)
    {
        if (string.IsNullOrWhiteSpace(nodeFunction))
            return;
        if (isKnownMapNodeReference(nodeFunction))
            addMapReference(sourceId, parameterAt(parameters, 0), "nodeParam", $"{path}.params[0]");

        (string[] Suffixes, int Parameter, string Type)[] rules =
        [
            ([".AddPlayerByClass", ".RemovePlayerByClass", ".CreateActorFromBPPath", ".CreateActorFromBPPathWithDefaults"], 0, "blueprint"),
            ([".RunCommonFunction"], 0, "commonFunction"),
            ([".PlaySound"], 0, "asset"),
            ([".ShowVoiceMessageByTag", ".ShowVoiceMessage"], 2, "asset"),
            ([".PlayMusic"], 0, "asset"),
            ([".PlayVideo"], 0, "asset"),
            ([".PlayVideo"], 3, "subtitle"),
        ];
        foreach ((string[] suffixes, int parameter, string type) in rules)
        {
            if (!suffixes.Any(suffix => nodeFunction.EndsWith(suffix, StringComparison.Ordinal)))
                continue;
            JsonNode? value = parameterAt(parameters, parameter);
            string referencePath = $"{path}.params[{parameter}]";
            string? text = ReferenceIdentity.NormalizeParameter(value);
            if (string.IsNullOrWhiteSpace(text))
                continue;
            if (type == "blueprint")
            {
                string? blueprintId = blueprintNodeIdFromClassPath(value);
                if (blueprintId is not null)
                    addReference(sourceId, blueprintId, "nodeParam", referencePath);
            }
            else if (type == "asset")
            {
                addAssetReference(sourceId, value, "nodeParam", referencePath);
            }
            else if (type == "subtitle")
            {
                addSubtitleReference(sourceId, value, "nodeParam", referencePath);
            }
            else
            {
                addReference(sourceId, ReferenceIdentity.NodeId(type, text), "nodeParam", referencePath);
            }
        }
    }

    private void scanGenericReferences(string sourceId, JsonNode? value, string path)
    {
        cancellationToken.ThrowIfCancellationRequested();
        if (JsonScalar.String(value) is string text)
        {
            if (GameDataPath.TryGetSubtitleKey(text, out string subtitleKey))
            {
                addReference(sourceId, ReferenceIdentity.NodeId("subtitle", subtitleKey), "subtitle", path);
                return;
            }
            string? blueprintId = blueprintNodeIdFromClassPath(value);
            if (blueprintId is not null)
            {
                addReference(sourceId, blueprintId, "blueprintPath", path);
                return;
            }
            string assetPath = normalizeExplicitAssetPath(text);
            if (assetPath.Length != 0)
                addReference(sourceId, ReferenceIdentity.NodeId("asset", assetPath), "asset", path);
            return;
        }
        if (value is JsonObject objectValue)
        {
            foreach (KeyValuePair<string, JsonNode?> pair in objectValue)
                scanGenericReferences(sourceId, pair.Value, $"{path}.{pair.Key}");
            return;
        }
        if (value is not JsonArray arrayValue)
            return;
        for (int index = 0; index < arrayValue.Count; index++)
            scanGenericReferences(sourceId, arrayValue[index], $"{path}[{index}]");
    }

}
