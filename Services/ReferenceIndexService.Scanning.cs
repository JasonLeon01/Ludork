using Ludork.Models;
using Ludork.Views.Utils.BlueprintGraph;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed partial class ReferenceIndexService
{
    private void scanTextConfigReferences(string sourceId, string key, JsonObject data)
    {
        addAssetReference(
            sourceId,
            data["font"],
            "Fonts",
            "font",
            $"TextConfigs/{key}.font");
        if (data["gradient"] is JsonObject gradient
            && normalizeReferenceParam(gradient["curve"]) is string curve
            && curve.Length != 0)
        {
            addReference(
                sourceId,
                nodeId("curve", normalizeDataReference(curve, "Curves")),
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
        string? controlId = getString(node["controlId"]);
        if (controlId is not null
            && UiAssetSchema.TryGetProjectAssetKey(controlId, out string targetAssetKey))
        {
            string targetKey = UiAssetSchema.ToAssetDataKey(targetAssetKey);
            if (gameData.UiAssetsData.ContainsKey(targetKey))
            {
                addReference(
                    sourceId,
                    nodeId("uiAsset", targetKey),
                    "nestedUiAsset",
                    $"UI/{key}.{path}.controlId");
            }
        }
        if (node["properties"] is JsonObject properties)
        {
            addAssetReference(
                sourceId,
                properties["texture"],
                string.Empty,
                "uiResource",
                $"UI/{key}.{path}.properties.texture");
            addAssetReference(
                sourceId,
                properties["windowSkin"],
                string.Empty,
                "uiResource",
                $"UI/{key}.{path}.properties.windowSkin");
            if (normalizeReferenceParam(properties["textConfig"]) is string textConfig
                && textConfig.Length != 0)
            {
                addReference(
                    sourceId,
                    nodeId("textConfig", normalizeDataReference(textConfig, "TextConfigs")),
                    "textConfig",
                    $"UI/{key}.{path}.properties.textConfig");
            }
            if (normalizeReferenceParam(properties["opacityCurve"]) is string curve
                && curve.Length != 0)
            {
                addReference(
                    sourceId,
                    nodeId("curve", normalizeDataReference(curve, "Curves")),
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
            if (pair.Value is not JsonObject setting)
                continue;
            string? valueType = getString(setting["type"]);
            if (valueType is null || !valueType.StartsWith("file", StringComparison.Ordinal))
                continue;
            JsonArray values = setting["value"] is JsonArray array
                ? array
                : new JsonArray(setting["value"]?.DeepClone());
            string root = getString(setting["root"]) ?? "Assets";
            string baseDirectory = getString(setting["base"]) ?? string.Empty;
            for (int index = 0; index < values.Count; index++)
            {
                string path = $"Configs/{key}.{pair.Key}[{index}]";
                if (root.Equals("Data", StringComparison.OrdinalIgnoreCase)
                    && baseDirectory.Equals("Maps", StringComparison.OrdinalIgnoreCase))
                {
                    addMapReference(sourceId, values[index], "configFile", path);
                }
                else
                {
                    addAssetReference(sourceId, values[index], baseDirectory, "configFile", path);
                }
            }
        }
    }

    private void scanAutoTileReferences(string sourceId, JsonNode? value, string path)
    {
        string? key = getString(value);
        if (!string.IsNullOrWhiteSpace(key))
        {
            addReference(sourceId, nodeId("autoTile", key), "autoTile", path);
            return;
        }
        if (value is not JsonArray array)
            return;
        for (int index = 0; index < array.Count; index++)
            scanAutoTileReferences(sourceId, array[index], $"{path}[{index}]");
    }

    private void scanMapActorReferences(string sourceId, JsonNode? value, string path)
    {
        if (value is not JsonArray actors)
            return;
        for (int index = 0; index < actors.Count; index++)
        {
            if (actors[index] is not JsonObject actor)
                continue;
            string? blueprintId = blueprintNodeIdFromClassPath(actor["bp"]);
            if (blueprintId is not null)
                addReference(sourceId, blueprintId, "mapActor", $"{path}[{index}].bp");
            scanGenericReferences(sourceId, actor, $"{path}[{index}]");
        }
    }

    private void scanBlueprintReferences(string key, JsonObject data)
    {
        string sourceId = blueprintNodeIdFromKey(key);
        string? parentId = blueprintNodeIdFromClassPath(data["parent"]);
        if (parentId is not null)
            addReference(sourceId, parentId, "parent", $"Blueprints/{key}.parent");

        if (data["attrs"] is JsonObject attrs)
        {
            Dictionary<string, string> pathVariables = new(StringComparer.Ordinal)
            {
                ["texturePath"] = "Characters",
                ["shaderPath"] = "Shaders",
            };
            ResolvedBlueprintClass resolved = classResolver.ResolveBlueprint(data, key);
            collectPathVariables(resolved.Meta["PathVars"], pathVariables);
            foreach (ResolvedBlueprintField field in resolved.Fields)
            {
                if (field.Metadata is null)
                    continue;
                if (getMetaReference(field.Metadata.Meta["PathRoot"], field.Name) == "Project")
                    continue;
                string? baseDirectory = getMetaReference(field.Metadata.Meta["PathVars"], field.Name);
                if (baseDirectory is not null)
                    pathVariables[field.Name] = baseDirectory;
            }
            foreach (KeyValuePair<string, string> pair in pathVariables)
            {
                if (attrs.ContainsKey(pair.Key))
                {
                    addAssetReference(
                        sourceId,
                        attrs[pair.Key],
                        pair.Value,
                        "asset",
                        $"Blueprints/{key}.attrs.{pair.Key}");
                }
            }
            scanGenericReferences(sourceId, attrs, $"Blueprints/{key}.attrs");
        }

        if (data["graph"] is JsonObject graph)
        {
            BlueprintGraphContext context = new(data, key);
            BlueprintNodeDefinitionSet definitions =
                new BlueprintNodeDefinitionCatalog(metadataService, classResolver, context).GetNodeDefinitionSet();
            scanNodeGraphReferences(sourceId, graph, $"Blueprints/{key}.graph", definitions);
            scanGenericReferences(sourceId, graph, $"Blueprints/{key}.graph");
        }
    }

    private void scanAnimationReferences(string sourceId, JsonObject data, string key)
    {
        if (data["assets"] is JsonArray assets)
        {
            for (int index = 0; index < assets.Count; index++)
            {
                string? assetName = getString(assets[index]);
                string baseDirectory = isAudioAsset(assetName) ? "Sounds" : "Animations";
                addAssetReference(sourceId, assets[index], baseDirectory, "animationAsset", $"assets[{index}]");
            }
        }
        scanGenericReferences(sourceId, data, $"Animations/{key}");
    }

    private void scanGeneralReferences(
        string key,
        JsonObject data,
        BlueprintNodeDefinitionSet globalDefinitions)
    {
        string sourceId = nodeId("general", key);
        JsonObject parameterSchema = data["params"] as JsonObject ?? [];
        if (data["members"] is not JsonObject members)
            return;
        foreach (KeyValuePair<string, JsonNode?> pair in members)
        {
            string memberId = nodeId("generalMember", $"{key}/{pair.Key}");
            addReference(sourceId, memberId, "member", $"General/{key}.members.{pair.Key}");
            if (pair.Value is not JsonObject member)
                continue;
            addAssetReference(memberId, member["icon"], string.Empty, "asset", "icon");
            scanGeneralParameterReferences(memberId, key, pair.Key, member, parameterSchema);
            if (member["_graph"] is JsonObject graph)
            {
                scanNodeGraphReferences(
                    memberId,
                    graph,
                    $"General/{key}/{pair.Key}._graph",
                    globalDefinitions);
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
            string type = getString(definition["type"]) ?? "string";
            if (type is not "string" and not "list" and not "dict")
                continue;
            JsonNode? value = member[pair.Key];
            if (type == "list" && value is JsonArray list)
            {
                for (int index = 0; index < list.Count; index++)
                    addGeneralParameterReference(sourceId, definition, list[index], $"General/{dataKey}/{memberKey}.{pair.Key}[{index}]");
                continue;
            }
            if (type == "dict" && value is JsonObject dictionary)
            {
                foreach (string itemKey in dictionary.Select(entry => entry.Key))
                    addGeneralParameterReference(sourceId, definition, JsonValue.Create(itemKey), $"General/{dataKey}/{memberKey}.{pair.Key}.{itemKey}");
                continue;
            }
            addGeneralParameterReference(sourceId, definition, value, $"General/{dataKey}/{memberKey}.{pair.Key}");
        }
    }

    private void addGeneralParameterReference(
        string sourceId,
        JsonObject definition,
        JsonNode? value,
        string path)
    {
        if (definition["reference"] is not JsonObject reference)
            return;
        string? kind = getString(reference["kind"]);
        string? text = normalizeReferenceParam(value);
        if (string.IsNullOrWhiteSpace(text))
            return;
        if (kind == "animation")
        {
            addReference(sourceId, nodeId("animation", text), "reference", path);
            return;
        }
        if (kind == "general" && getString(reference["key"]) is string generalKey)
            addReference(sourceId, nodeId("generalMember", $"{generalKey}/{text}"), "member", path);
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
                if (graphNodes[index] is not JsonObject node)
                    continue;
                string nodePath = $"{path}.nodeGraph.{graphPair.Key}.nodes[{index}]";
                string? nodeFunction = getString(node["nodeFunction"]);
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
                || parameterIndex < 0
                || parameterIndex >= parameters.Count)
            {
                continue;
            }
            JsonNode? value = parameters[parameterIndex];
            string referencePath = $"{path}.params[{parameterIndex}]";
            if (getMetaReference(port.Meta["PathVars"], port.Name) is string baseDirectory)
            {
                addAssetReference(sourceId, value, baseDirectory, "nodeParam", referencePath);
            }
            if (hasMetaReference(port.Meta["BlueprintClassVars"], port.Name))
            {
                string? blueprintId = blueprintNodeIdFromClassPath(value);
                if (blueprintId is not null)
                    addReference(sourceId, blueprintId, "nodeParam", referencePath);
            }
            if (hasMetaReference(port.Meta["CommonFunctionVars"], port.Name)
                && normalizeReferenceParam(value) is string functionName)
            {
                addReference(sourceId, nodeId("commonFunction", functionName), "nodeParam", referencePath);
            }
            if (getMetaReference(port.Meta["GeneralDataVars"], port.Name) is string generalType)
            {
                string? generalValue = normalizeReferenceParam(value);
                if (generalValue is not null)
                {
                    string targetId = generalType.Equals("ANIMATION", StringComparison.OrdinalIgnoreCase)
                        ? nodeId("animation", generalValue)
                        : nodeId("generalMember", $"{generalType}/{generalValue}");
                    addReference(sourceId, targetId, "nodeParam", referencePath);
                }
            }
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

        (string[] Suffixes, int Parameter, string Type, string Base)[] rules =
        [
            ([".AddPlayerByClass", ".RemovePlayerByClass", ".CreateActorFromBPPath", ".CreateActorFromBPPathWithDefaults"], 0, "blueprint", ""),
            ([".AddAnim", ".AddAnimOn", ".GetAnimLength"], 0, "animation", ""),
            ([".RunCommonFunction"], 0, "commonFunction", ""),
            ([".PlaySound"], 0, "asset", "Sounds"),
            ([".ShowVoiceMessageByTag", ".ShowVoiceMessage"], 2, "asset", "Voices"),
            ([".PlayMusic"], 0, "asset", "Musics"),
            ([".PlayVideo"], 0, "asset", "Videos"),
            ([".GetItemCount", ".AddItem", ".RemoveItem", ".HasItem"], 0, "generalMember", "Item"),
            ([".AddEquip", ".RemoveEquip", ".HasEquip", ".EquipItem"], 0, "generalMember", "Equip"),
        ];
        foreach ((string[] suffixes, int parameter, string type, string baseValue) in rules)
        {
            if (!suffixes.Any(suffix => nodeFunction.EndsWith(suffix, StringComparison.Ordinal)))
                continue;
            JsonNode? value = parameterAt(parameters, parameter);
            string referencePath = $"{path}.params[{parameter}]";
            string? text = normalizeReferenceParam(value);
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
                addAssetReference(sourceId, value, baseValue, "nodeParam", referencePath);
            }
            else if (type == "generalMember")
            {
                addReference(sourceId, nodeId("generalMember", $"{baseValue}/{text}"), "nodeParam", referencePath);
            }
            else
            {
                addReference(sourceId, nodeId(type, text), "nodeParam", referencePath);
            }
        }
    }

    private void scanGenericReferences(string sourceId, JsonNode? value, string path)
    {
        if (getString(value) is string text)
        {
            string? blueprintId = blueprintNodeIdFromClassPath(value);
            if (blueprintId is not null)
            {
                addReference(sourceId, blueprintId, "blueprintPath", path);
                return;
            }
            string assetPath = normalizeExplicitAssetPath(text);
            if (assetPath.Length != 0)
                addReference(sourceId, nodeId("asset", assetPath), "asset", path);
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

