using Ludork.Models;
using Ludork.Views.Utils.BlueprintGraph;
using MoonSharp.Interpreter;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed record BlueprintValidationResult(
    string BlueprintKey,
    bool IsValid,
    IReadOnlyList<string> Errors
);

public sealed class BlueprintValidationService
{
    private const string BlueprintPrefix = "Data.Blueprints.";
    private readonly GameDataService gameData;
    private readonly LuaMetadataService metadataService;
    private readonly BlueprintClassResolver classResolver;

    public BlueprintValidationService(
        GameDataService gameData,
        LuaMetadataService metadataService,
        BlueprintClassResolver classResolver)
    {
        this.gameData = gameData;
        this.metadataService = metadataService;
        this.classResolver = classResolver;
    }

    public BlueprintValidationResult ValidateBlueprint(string blueprintKey, JsonObject? data = null)
    {
        string key = normalizeBlueprintKey(blueprintKey);
        List<string> errors = [];
        if (data is null && !gameData.BlueprintsData.TryGetValue(key, out data))
        {
            errors.Add($"Blueprint \"{key}\" was not found in project data");
            return new BlueprintValidationResult(key, false, errors);
        }

        string? type = getString(data["type"]);
        if (type is not null && type != "blueprint")
            errors.Add("Blueprint \"type\" must be \"blueprint\" when present");

        string? parent = getString(data["parent"]);
        if (string.IsNullOrWhiteSpace(parent))
        {
            errors.Add("Blueprint \"parent\" must be a non-empty string");
            return new BlueprintValidationResult(key, false, errors);
        }
        validateParent(key, data, parent.Trim(), errors);
        validateBlueprintModeChain(key, data, errors);
        ResolvedBlueprintClass resolved = validateScriptMixin(key, data, errors);

        bool hasGraph = data.TryGetPropertyValue("graph", out JsonNode? graphNode);
        if (!hasGraph)
        {
            if (!resolved.ScriptMixin)
                errors.Add("Blueprint \"graph\" must be an object");
            return new BlueprintValidationResult(key, errors.Count == 0, errors);
        }
        if (graphNode is not JsonObject graph)
        {
            errors.Add("Blueprint \"graph\" must be an object");
            return new BlueprintValidationResult(key, false, errors);
        }
        validateGraphStructure(graph, errors);
        if (errors.Count == 0 && !resolved.ScriptMixin)
            validateGraphDefinitions(key, data, graph, errors);
        return new BlueprintValidationResult(key, errors.Count == 0, errors);
    }

    private ResolvedBlueprintClass validateScriptMixin(
        string blueprintKey,
        JsonObject data,
        ICollection<string> errors)
    {
        JsonObject? attrs = data["attrs"] as JsonObject;
        if (attrs is null)
        {
            errors.Add("Blueprint \"attrs\" must be an object");
            attrs = [];
        }
        if (attrs.ContainsKey("scriptMixin") && !tryGetBoolean(attrs["scriptMixin"], out bool _))
            errors.Add("scriptMixin must be a boolean");

        ResolvedBlueprintClass resolved = classResolver.ResolveBlueprint(data, blueprintKey);
        if (!resolved.ScriptMixin)
            return resolved;

        string? localPath = getString(attrs["scriptPath"]);
        if (!resolved.HasBlueprintParent && string.IsNullOrWhiteSpace(localPath))
            errors.Add("A root ScriptMixin blueprint must declare scriptPath");
        if (attrs.ContainsKey("scriptPath") && string.IsNullOrWhiteSpace(localPath))
            errors.Add("A local scriptPath must be a non-empty string");
        if (!string.IsNullOrWhiteSpace(localPath))
            validateScriptMixinPath(localPath, errors);
        if (!string.IsNullOrWhiteSpace(resolved.ScriptMixinError))
            errors.Add("Mixin metadata is invalid: " + resolved.ScriptMixinError);
        return resolved;
    }

    private void validateBlueprintModeChain(
        string blueprintKey,
        JsonObject data,
        ICollection<string> errors)
    {
        List<(string Key, JsonObject Data)> chain = [(blueprintKey, data)];
        HashSet<string> visited = new(StringComparer.Ordinal) { blueprintKey };
        string? parent = getString(data["parent"]);
        while (!string.IsNullOrWhiteSpace(parent)
            && parent.StartsWith(BlueprintPrefix, StringComparison.Ordinal))
        {
            string parentKey = normalizeBlueprintKey(parent);
            if (!visited.Add(parentKey)
                || !gameData.BlueprintsData.TryGetValue(parentKey, out JsonObject? parentData))
            {
                return;
            }
            chain.Add((parentKey, parentData));
            parent = getString(parentData["parent"]);
        }

        bool parentMode = false;
        for (int index = chain.Count - 1; index >= 0; index--)
        {
            (string key, JsonObject blueprint) = chain[index];
            if (blueprint["attrs"] is not JsonObject attrs
                || !attrs.ContainsKey("scriptMixin"))
            {
                continue;
            }
            if (!tryGetBoolean(attrs["scriptMixin"], out bool localMode))
            {
                if (index != 0)
                    errors.Add($"Parent blueprint '{key}' scriptMixin must be a boolean");
                continue;
            }
            if (index != chain.Count - 1 && localMode != parentMode)
            {
                errors.Add(
                    $"Blueprint inheritance cannot mix ScriptMixin and Blueprint graph modes at '{key}'");
            }
            parentMode = localMode;
        }
    }

    private void validateScriptMixinPath(string scriptPath, ICollection<string> errors)
    {
        try
        {
            string normalized = ScriptMixinPaths.Normalize(scriptPath);
            string fullPath = ScriptMixinPaths.GetScriptPath(gameData.ProjectPath, normalized);
            if (!File.Exists(fullPath))
            {
                errors.Add($"Mixin script '{normalized}' was not found");
                return;
            }
            metadataService.LoadScriptMixinMetadata(normalized);
        }
        catch (InterpreterException exception)
        {
            errors.Add("Mixin metadata is invalid: " + (exception.DecoratedMessage ?? exception.Message));
        }
        catch (InvalidDataException exception)
        {
            errors.Add("Mixin path or metadata is invalid: " + exception.Message);
        }
        catch (IOException exception)
        {
            errors.Add("Mixin path or metadata could not be read: " + exception.Message);
        }
        catch (UnauthorizedAccessException exception)
        {
            errors.Add("Mixin path or metadata could not be read: " + exception.Message);
        }
    }

    public IReadOnlyList<BlueprintValidationResult> ValidateBlueprints(IEnumerable<string> blueprintKeys)
    {
        return blueprintKeys
            .Distinct(StringComparer.Ordinal)
            .OrderBy(key => key, StringComparer.Ordinal)
            .Select(key => ValidateBlueprint(key))
            .ToArray();
    }

    public IReadOnlyList<BlueprintValidationResult> ValidateGeneralDataGraphs()
    {
        List<BlueprintValidationResult> results = [];
        IReadOnlyList<string> schemaErrors = GeneralDataSchemaValidation.Validate(gameData.GeneralData);
        if (schemaErrors.Count != 0)
            results.Add(new BlueprintValidationResult("GeneralData", false, schemaErrors));

        using IDisposable metadataBatch = classResolver.BeginBatch();
        BlueprintNodeDefinitionSet definitionSet = BlueprintNodeDefinitionCatalog
            .CreateGlobal(metadataService, classResolver)
            .GetNodeDefinitionSet();
        foreach (KeyValuePair<string, JsonObject> typeEntry in gameData.GeneralData
            .OrderBy(entry => entry.Key, StringComparer.Ordinal))
        {
            if (typeEntry.Value["members"] is not JsonObject members)
                continue;
            foreach (KeyValuePair<string, JsonNode?> memberEntry in members)
            {
                if (memberEntry.Value?["_graph"] is not JsonObject graph)
                    continue;
                List<string> errors = [];
                validateGraphStructure(graph, errors);
                if (errors.Count == 0)
                {
                    JsonObject graphDocument = new()
                    {
                        ["attrs"] = new JsonObject(),
                        ["graph"] = graph.DeepClone(),
                    };
                    validateGraphDefinitions(
                        $"General/{typeEntry.Key}/{memberEntry.Key}",
                        graphDocument,
                        graph,
                        errors);
                    validateGeneralDataLatentNodes(graph, definitionSet.RuntimeLookup, errors);
                }
                results.Add(new BlueprintValidationResult(
                    $"General/{typeEntry.Key}/{memberEntry.Key}",
                    errors.Count == 0,
                    errors));
            }
        }
        return results;
    }

    private static void validateGeneralDataLatentNodes(
        JsonObject graph,
        IReadOnlyDictionary<string, BlueprintGraphNodeDefinition> definitions,
        ICollection<string> errors)
    {
        if (graph["nodeGraph"] is not JsonObject nodeGraph)
            return;
        foreach (KeyValuePair<string, JsonNode?> eventEntry in nodeGraph)
        {
            if (eventEntry.Value?["nodes"] is not JsonArray nodes)
                continue;
            for (int index = 0; index < nodes.Count; index++)
            {
                string? nodeFunction = getString(nodes[index]?["nodeFunction"]);
                if (nodeFunction is not null
                    && definitions.TryGetValue(nodeFunction, out BlueprintGraphNodeDefinition? definition)
                    && definition.IsLatent)
                {
                    errors.Add(
                        $"graph.nodeGraph[\"{eventEntry.Key}\"].nodes[{index}] uses latent node '{nodeFunction}'");
                }
            }
        }
    }

    private void validateParent(
        string blueprintKey,
        JsonObject data,
        string parent,
        ICollection<string> errors)
    {
        HashSet<string> visited = new(StringComparer.Ordinal) { blueprintKey };
        string currentParent = parent;
        while (currentParent.StartsWith(BlueprintPrefix, StringComparison.Ordinal))
        {
            string parentKey = normalizeBlueprintKey(currentParent);
            if (!visited.Add(parentKey))
            {
                errors.Add($"Blueprint parent chain contains a cycle at '{currentParent}'");
                return;
            }
            JsonObject? parentData = parentKey == blueprintKey
                ? data
                : gameData.BlueprintsData.TryGetValue(parentKey, out JsonObject? loaded)
                    ? loaded
                    : null;
            if (parentData is null)
            {
                errors.Add($"Parent blueprint '{currentParent}' was not found in project data");
                return;
            }
            string? nextParent = getString(parentData["parent"]);
            if (string.IsNullOrWhiteSpace(nextParent))
            {
                errors.Add($"Parent blueprint '{currentParent}' has no valid parent class");
                return;
            }
            currentParent = nextParent.Trim();
        }

        if (!isKnownNativeType(currentParent))
            errors.Add($"Parent class '{currentParent}' did not resolve to a type");
    }

    private bool isKnownNativeType(string reference)
    {
        LuaTypeReference parsed;
        try
        {
            parsed = LuaTypeReference.Parse(reference);
        }
        catch (ArgumentException)
        {
            return false;
        }
        if (metadataService.GetType(parsed) is not null)
            return true;
        LuaTypeReference fileClassReference = new(reference, parsed.TypeName);
        return metadataService.GetType(fileClassReference) is not null
            || BlueprintCompatibilityCatalog.TryGet(parsed.QualifiedName, out BlueprintCompatibilityType? compatibility)
                && compatibility is not null;
    }

    private static void validateGraphStructure(JsonObject graph, ICollection<string> errors)
    {
        if (graph["nodeGraph"] is not JsonObject nodeGraph)
        {
            errors.Add("graph.nodeGraph must be an object");
            return;
        }
        if (graph["startNodes"] is not JsonObject startNodes)
        {
            errors.Add("graph.startNodes must be an object");
            return;
        }

        foreach (KeyValuePair<string, JsonNode?> pair in nodeGraph)
        {
            string eventName = pair.Key;
            if (eventName.Length == 0)
            {
                errors.Add("graph.nodeGraph contains an invalid event name");
                continue;
            }
            if (pair.Value is not JsonObject eventData)
            {
                errors.Add($"graph.nodeGraph[\"{eventName}\"] must be an object");
                continue;
            }
            if (eventData["nodes"] is not JsonArray nodes)
            {
                errors.Add($"graph.nodeGraph[\"{eventName}\"].nodes must be a list");
                continue;
            }
            if (eventData["links"] is not JsonArray links)
            {
                errors.Add($"graph.nodeGraph[\"{eventName}\"].links must be a list");
                continue;
            }

            for (int index = 0; index < nodes.Count; index++)
            {
                if (nodes[index] is not JsonObject node)
                {
                    errors.Add($"graph.nodeGraph[\"{eventName}\"].nodes[{index}] must be an object");
                    continue;
                }
                string? nodeFunction = getString(node["nodeFunction"]);
                if (string.IsNullOrWhiteSpace(nodeFunction))
                {
                    errors.Add($"graph.nodeGraph[\"{eventName}\"].nodes[{index}].nodeFunction must be a non-empty string");
                }
                if (node["params"] is JsonNode parameters && parameters is not JsonArray)
                    errors.Add($"graph.nodeGraph[\"{eventName}\"].nodes[{index}].params must be a list");
            }

            for (int index = 0; index < links.Count; index++)
            {
                if (links[index] is not JsonObject link)
                {
                    errors.Add($"graph.nodeGraph[\"{eventName}\"].links[{index}] must be an object");
                    continue;
                }
                foreach (string field in new[] { "left", "right", "leftOutPin", "rightInPin", "linkType" })
                {
                    if (!link.ContainsKey(field))
                    {
                        errors.Add($"graph.nodeGraph[\"{eventName}\"].links[{index}] is missing required field \"{field}\"");
                    }
                }
                if (tryGetInteger(link["left"], out int left) && !isNodeIndexValid(left, nodes.Count))
                {
                    errors.Add($"graph.nodeGraph[\"{eventName}\"].links[{index}].left index {left} is out of range "
                        + $"(node count {nodes.Count}, valid indices 0-{Math.Max(nodes.Count - 1, 0)})");
                }
                if (tryGetInteger(link["right"], out int right) && !isNodeIndexValid(right, nodes.Count))
                {
                    errors.Add($"graph.nodeGraph[\"{eventName}\"].links[{index}].right index {right} is out of range "
                        + $"(node count {nodes.Count}, valid indices 0-{Math.Max(nodes.Count - 1, 0)})");
                }
            }

            if (startNodes.ContainsKey(eventName) && startNodes[eventName] is JsonNode startNode)
            {
                if (!tryGetInteger(startNode, out int startIndex))
                    errors.Add($"graph.startNodes[\"{eventName}\"] must be an integer or null");
                else if (!isNodeIndexValid(startIndex, nodes.Count))
                    errors.Add($"graph.startNodes[\"{eventName}\"] index {startIndex} is out of range");
            }
        }

        foreach (string eventName in startNodes.Select(pair => pair.Key))
        {
            if (!nodeGraph.ContainsKey(eventName))
                errors.Add($"graph.startNodes[\"{eventName}\"] has no matching graph in nodeGraph");
        }
    }

    private void validateGraphDefinitions(
        string key,
        JsonObject data,
        JsonObject graph,
        ICollection<string> errors)
    {
        using IDisposable metadataBatch = classResolver.BeginBatch();
        BlueprintGraphContext context = new(data, key);
        BlueprintNodeDefinitionCatalog catalog = new(metadataService, classResolver, context);
        BlueprintNodeDefinitionSet definitionSet = catalog.GetNodeDefinitionSet();
        IReadOnlyDictionary<string, BlueprintGraphNodeDefinition> lookup = definitionSet.RuntimeLookup;
        JsonObject nodeGraph = (JsonObject)graph["nodeGraph"]!;
        foreach (KeyValuePair<string, JsonNode?> pair in nodeGraph)
        {
            JsonObject eventData = (JsonObject)pair.Value!;
            JsonArray nodes = (JsonArray)eventData["nodes"]!;
            JsonArray links = (JsonArray)eventData["links"]!;
            IReadOnlyList<BlueprintGraphEventParameterDefinition> eventParameters =
                definitionSet.EventParameters.TryGetValue(
                    pair.Key,
                    out IReadOnlyList<BlueprintGraphEventParameterDefinition>? parameters)
                    ? parameters
                    : [];
            Dictionary<string, BlueprintGraphEventParameterDefinition> parametersByKey = eventParameters
                .ToDictionary(parameter => parameter.ExternalKey, StringComparer.Ordinal);
            BlueprintGraphNodeDefinition?[] nodeDefinitions = new BlueprintGraphNodeDefinition?[nodes.Count];
            for (int index = 0; index < nodes.Count; index++)
            {
                JsonObject node = (JsonObject)nodes[index]!;
                string nodeFunction = getString(node["nodeFunction"])!;
                if (!lookup.TryGetValue(nodeFunction, out BlueprintGraphNodeDefinition? definition))
                {
                    errors.Add($"graph.nodeGraph[\"{pair.Key}\"].nodes[{index}].nodeFunction '{nodeFunction}' was not found");
                    continue;
                }
                nodeDefinitions[index] = definition;
            }

            for (int linkIndex = 0; linkIndex < links.Count; linkIndex++)
            {
                JsonObject link = (JsonObject)links[linkIndex]!;
                string? linkType = getString(link["linkType"]);
                if (linkType is not "Exec" and not "Params")
                    continue;
                if (!tryGetInteger(link["leftOutPin"], out int leftOutPin))
                {
                    errors.Add($"graph.nodeGraph[\"{pair.Key}\"].links[{linkIndex}].leftOutPin must be an integer");
                    continue;
                }

                if (tryGetInteger(link["left"], out int leftIndex))
                {
                    if (!isNodeIndexValid(leftIndex, nodeDefinitions.Length)
                        || nodeDefinitions[leftIndex] is not BlueprintGraphNodeDefinition definition)
                    {
                        continue;
                    }
                    validateSourcePin(pair.Key, linkIndex, leftIndex, linkType, leftOutPin, definition, errors);
                    continue;
                }

                string? externalKey = getString(link["left"]);
                if (externalKey is null)
                    continue;
                if (linkType != "Params" || !parametersByKey.ContainsKey(externalKey))
                {
                    errors.Add($"graph.nodeGraph[\"{pair.Key}\"].links[{linkIndex}].left '{externalKey}' is not a valid event parameter");
                    continue;
                }
                if (leftOutPin != 0)
                {
                    errors.Add($"graph.nodeGraph[\"{pair.Key}\"].links[{linkIndex}]: event parameter '{externalKey}' only has data output pin 0");
                }
            }
        }
    }

    private static void validateSourcePin(
        string eventName,
        int linkIndex,
        int nodeIndex,
        string linkType,
        int leftOutPin,
        BlueprintGraphNodeDefinition definition,
        ICollection<string> errors)
    {
        BlueprintGraphPortKind kind = linkType == "Exec"
            ? BlueprintGraphPortKind.Exec
            : BlueprintGraphPortKind.Params;
        string[] outputNames = definition.Ports
            .Where(port => port.Direction == BlueprintGraphPortDirection.Output && port.Kind == kind)
            .OrderBy(port => port.PinIndex)
            .Select(port => port.Name)
            .ToArray();
        if (outputNames.Length == 0)
        {
            string outputType = linkType == "Exec" ? "execution" : "data";
            errors.Add($"graph.nodeGraph[\"{eventName}\"].links[{linkIndex}]: {linkType} link from "
                + $"node {nodeIndex} ({definition.MemberName}) has no {outputType} output pins");
            return;
        }
        if (leftOutPin >= 0 && leftOutPin < outputNames.Length)
            return;
        string outputLabel = linkType == "Exec" ? "exec" : "data";
        errors.Add($"graph.nodeGraph[\"{eventName}\"].links[{linkIndex}]: {linkType} link leftOutPin "
            + $"{leftOutPin} is invalid for {definition.MemberName} — {outputLabel} outputs are "
            + $"[{string.Join(", ", outputNames)}] (use indices 0-{outputNames.Length - 1})");
    }

    private static bool isNodeIndexValid(int index, int count)
    {
        return index >= 0 && index < count;
    }

    private static bool tryGetInteger(JsonNode? value, out int result)
    {
        result = 0;
        if (value is not JsonValue scalar)
            return false;
        if (scalar.TryGetValue(out int integer))
        {
            result = integer;
            return true;
        }
        if (scalar.TryGetValue(out long longValue)
            && longValue >= int.MinValue
            && longValue <= int.MaxValue)
        {
            result = (int)longValue;
            return true;
        }
        return false;
    }

    private static bool tryGetBoolean(JsonNode? value, out bool result)
    {
        if (value is JsonValue scalar && scalar.TryGetValue(out result))
            return true;
        result = false;
        return false;
    }

    private static string? getString(JsonNode? value)
    {
        return value is JsonValue scalar && scalar.TryGetValue(out string? text) ? text : null;
    }

    private static string normalizeBlueprintKey(string reference)
    {
        string value = reference?.Trim() ?? string.Empty;
        if (value.StartsWith(BlueprintPrefix, StringComparison.Ordinal))
            value = value[BlueprintPrefix.Length..].Replace('.', '/');
        value = value.Replace('\\', '/').Trim('/');
        if (value.EndsWith(DataConfig.DataFileExtension, StringComparison.OrdinalIgnoreCase))
            value = value[..^DataConfig.DataFileExtension.Length];
        return value;
    }
}
