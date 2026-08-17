using Ludork.Models;
using Ludork.Plugin.Abstractions;
using Ludork.Views.Utils.BlueprintGraph;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;
using System.Threading.Tasks;

namespace Ludork.Services.BlueprintAssistant;

public sealed record BlueprintAssistantBlueprint(
    string Key,
    string Hash,
    string Json);

public sealed record BlueprintAssistantSearchMatch(
    string Path,
    int Line,
    string Text);

public sealed record BlueprintAssistantValidation(
    bool IsValid,
    IReadOnlyList<string> Errors);

public sealed class BlueprintAssistantWorkspace : IBlueprintAssistantWorkspace
{
    private const int MaximumReadBytes = 1024 * 1024;
    private const int MaximumReadLines = 1200;
    private const int MaximumSearchResults = 100;
    private static readonly JsonSerializerOptions WriteOptions = new()
    {
        WriteIndented = true,
    };
    private static readonly HashSet<string> AllowedRootNames = new(
        StringComparer.OrdinalIgnoreCase)
    {
        "C_Extensions",
        "Core",
        "docs",
        "Engine",
        "Global",
        "GlobalCore",
        "GlobalFunctions",
        "include",
        "Scripts",
        "Source",
        "src",
        "Standard",
    };
    private static readonly HashSet<string> AllowedExtensions = new(
        StringComparer.OrdinalIgnoreCase)
    {
        ".c",
        ".cc",
        ".cpp",
        ".h",
        ".hpp",
        ".json",
        ".lua",
        ".md",
        ".txt",
    };
    private static readonly HashSet<string> ExcludedSegments = new(
        StringComparer.OrdinalIgnoreCase)
    {
        ".cache",
        ".codex",
        ".data",
        ".git",
        ".svn",
        ".tools",
        ".venv",
        ".vscode",
        "bin",
        "build",
        "build-verify",
        "dist",
        "Log",
        "logs",
        "obj",
        "Plugins",
        "Temp",
        "ThirdPartySource",
    };

    private readonly GameDataService gameData;
    private readonly LuaMetadataService metadataService;
    private readonly BlueprintClassResolver classResolver;
    private readonly BlueprintValidationService validationService;
    private readonly Action<string> flushBlueprint;
    private readonly Action<string> refreshBlueprint;
    private readonly string projectPath;
    private readonly StringComparison pathComparison;
    private readonly string targetBlueprintKey;
    private readonly JsonObject baseBlueprint;
    private readonly string baseRevision;
    private readonly Dictionary<string, JsonObject> proposals = new(StringComparer.Ordinal);

    public BlueprintAssistantWorkspace(
        GameDataService gameData,
        LuaMetadataService metadataService,
        BlueprintClassResolver classResolver,
        BlueprintValidationService validationService,
        string targetBlueprintKey,
        Action<string> flushBlueprint,
        Action<string> refreshBlueprint)
    {
        this.gameData = gameData;
        this.metadataService = metadataService;
        this.classResolver = classResolver;
        this.validationService = validationService;
        this.flushBlueprint = flushBlueprint;
        this.refreshBlueprint = refreshBlueprint;
        projectPath = Path.GetFullPath(gameData.ProjectPath);
        this.targetBlueprintKey = normalizeBlueprintKey(targetBlueprintKey);
        if (!gameData.BlueprintsData.TryGetValue(this.targetBlueprintKey, out JsonObject? blueprint))
            throw new ArgumentException("The target Blueprint was not found.", nameof(targetBlueprintKey));
        baseBlueprint = (JsonObject)blueprint.DeepClone();
        baseRevision = GetBlueprintHash(baseBlueprint);
        pathComparison = OperatingSystem.IsWindows()
            ? StringComparison.OrdinalIgnoreCase
            : StringComparison.Ordinal;
    }

    public string ProjectPath => projectPath;
    public string BlueprintKey => targetBlueprintKey;
    public string BaseRevision => baseRevision;

    public IReadOnlyList<string> ListBlueprints()
    {
        return gameData.BlueprintsData.Keys
            .OrderBy(key => key, StringComparer.Ordinal)
            .ToArray();
    }

    public BlueprintAssistantBlueprint? ReadBlueprint(string blueprintKey)
    {
        string key = normalizeBlueprintKey(blueprintKey);
        if (!gameData.BlueprintsData.TryGetValue(key, out JsonObject? blueprint))
            return null;
        JsonObject clone = (JsonObject)blueprint.DeepClone();
        return new BlueprintAssistantBlueprint(
            key,
            GetBlueprintHash(clone),
            clone.ToJsonString(WriteOptions));
    }

    public BlueprintAssistantValidation ValidateCandidate(
        string blueprintKey,
        string candidateJson)
    {
        if (!tryParseCandidate(candidateJson, out JsonObject? candidate, out string error))
            return new BlueprintAssistantValidation(false, [error]);
        BlueprintValidationResult result = validationService.ValidateBlueprint(
            normalizeBlueprintKey(blueprintKey),
            candidate);
        return new BlueprintAssistantValidation(result.IsValid, result.Errors);
    }

    public string QueryApiCatalog(
        string blueprintKey,
        string? query,
        int maximumResults = 80)
    {
        string key = normalizeBlueprintKey(blueprintKey);
        if (!gameData.BlueprintsData.TryGetValue(key, out JsonObject? blueprint))
            return "[]";
        BlueprintGraphContext context = new((JsonObject)blueprint.DeepClone(), key);
        BlueprintNodeDefinitionCatalog catalog = new(
            metadataService,
            classResolver,
            context);
        string filter = query?.Trim() ?? string.Empty;
        int limit = Math.Clamp(maximumResults, 1, 5000);
        JsonArray result = [];
        foreach (BlueprintGraphNodeDefinition definition in catalog.GetNodeDefinitionSet().Definitions
                     .Where(definition => matchesDefinition(definition, filter))
                     .OrderByDescending(definition => definition.IsContextRelevant)
                     .ThenBy(definition => definition.RuntimePath, StringComparer.Ordinal)
                     .Take(limit))
        {
            JsonArray ports = [];
            foreach (BlueprintGraphPortDefinition port in definition.Ports)
            {
                ports.Add(new JsonObject
                {
                    ["name"] = port.Name,
                    ["kind"] = port.Kind.ToString(),
                    ["direction"] = port.Direction.ToString(),
                    ["pinIndex"] = port.PinIndex,
                    ["type"] = port.TypeName,
                    ["parameterIndex"] = port.ParameterIndex,
                    ["supportsEditor"] = port.SupportsEditor,
                    ["default"] = port.DefaultValue?.DeepClone(),
                    ["meta"] = port.Meta.DeepClone(),
                });
            }
            result.Add(new JsonObject
            {
                ["runtimePath"] = definition.RuntimePath,
                ["title"] = definition.Title,
                ["memberName"] = definition.MemberName,
                ["description"] = definition.Description,
                ["declaringType"] = definition.DeclaringType?.QualifiedName,
                ["isParent"] = definition.IsParent,
                ["isContextRelevant"] = definition.IsContextRelevant,
                ["aliases"] = new JsonArray(definition.RuntimeAliases
                    .Select(alias => JsonValue.Create(alias))
                    .ToArray<JsonNode?>()),
                ["ports"] = ports,
                ["meta"] = definition.Meta.DeepClone(),
            });
        }
        return result.ToJsonString(WriteOptions);
    }

    public IReadOnlyList<BlueprintAssistantSearchMatch> SearchProject(
        string query,
        int maximumResults = 50)
    {
        string term = query.Trim();
        if (term.Length == 0)
            return [];
        int limit = Math.Clamp(maximumResults, 1, MaximumSearchResults);
        List<BlueprintAssistantSearchMatch> result = [];
        foreach (string path in enumerateReadableFiles())
        {
            if (result.Count >= limit)
                break;
            FileInfo info = new(path);
            if (info.Length > MaximumReadBytes)
                continue;
            using StreamReader reader = File.OpenText(path);
            int lineNumber = 0;
            while (reader.ReadLine() is string line)
            {
                lineNumber++;
                if (!line.Contains(term, StringComparison.OrdinalIgnoreCase))
                    continue;
                result.Add(new BlueprintAssistantSearchMatch(
                    normalizeRelativePath(path),
                    lineNumber,
                    truncate(line.Trim(), 500)));
                if (result.Count >= limit)
                    break;
            }
        }
        return result;
    }

    public string ReadProjectFile(
        string relativePath,
        int startLine = 1,
        int maximumLines = 400)
    {
        string path = resolveReadablePath(relativePath);
        FileInfo info = new(path);
        if (info.Length > MaximumReadBytes)
            throw new InvalidDataException("The requested file is too large.");
        int firstLine = Math.Max(1, startLine);
        int lineLimit = Math.Clamp(maximumLines, 1, MaximumReadLines);
        StringBuilder result = new();
        using StreamReader reader = File.OpenText(path);
        int lineNumber = 0;
        int written = 0;
        while (reader.ReadLine() is string line)
        {
            lineNumber++;
            if (lineNumber < firstLine)
                continue;
            result.Append(lineNumber);
            result.Append(": ");
            result.AppendLine(line);
            written++;
            if (written >= lineLimit)
                break;
        }
        return result.ToString();
    }

    public BlueprintAssistantApplyResult ApplyCandidate(
        string blueprintKey,
        string baseHash,
        string candidateJson)
    {
        string key = normalizeBlueprintKey(blueprintKey);
        flushBlueprint(key);
        if (!gameData.BlueprintsData.TryGetValue(key, out JsonObject? current))
        {
            return new BlueprintAssistantApplyResult(
                false,
                false,
                "The target Blueprint no longer exists.",
                string.Empty);
        }
        string currentHash = GetBlueprintHash(current);
        if (baseHash.Length != currentHash.Length
            || !CryptographicOperations.FixedTimeEquals(
                Encoding.UTF8.GetBytes(currentHash),
                Encoding.UTF8.GetBytes(baseHash)))
        {
            return new BlueprintAssistantApplyResult(
                false,
                true,
                "The target Blueprint changed after the proposal was created.",
                currentHash);
        }
        if (!tryParseCandidate(candidateJson, out JsonObject? candidate, out string parseError))
        {
            return new BlueprintAssistantApplyResult(
                false,
                false,
                parseError,
                currentHash);
        }
        BlueprintValidationResult validation = validationService.ValidateBlueprint(key, candidate);
        if (!validation.IsValid)
        {
            return new BlueprintAssistantApplyResult(
                false,
                false,
                string.Join(Environment.NewLine, validation.Errors),
                currentHash);
        }
        bool updated = gameData.UpdateBlueprint(key, candidate!);
        if (!updated)
        {
            return new BlueprintAssistantApplyResult(
                false,
                false,
                "The proposal does not change the target Blueprint.",
                currentHash);
        }
        refreshBlueprint(key);
        BlueprintAssistantBlueprint? updatedBlueprint = ReadBlueprint(key);
        return new BlueprintAssistantApplyResult(
            true,
            false,
            string.Empty,
            updatedBlueprint?.Hash ?? string.Empty);
    }

    public BlueprintAssistantApplyResult ApplyProposal(string proposalId)
    {
        if (!proposals.TryGetValue(proposalId, out JsonObject? candidate))
        {
            return new BlueprintAssistantApplyResult(
                false,
                false,
                "The proposal is no longer available.",
                string.Empty);
        }
        BlueprintAssistantApplyResult result = ApplyCandidate(
            targetBlueprintKey,
            baseRevision,
            candidate.ToJsonString());
        if (result.Success)
            proposals.Remove(proposalId);
        return result;
    }

    public bool DiscardProposal(string proposalId)
    {
        return proposals.Remove(proposalId);
    }

    public string? GetProposalCandidate(string proposalId)
    {
        return proposals.TryGetValue(proposalId, out JsonObject? candidate)
            ? candidate.ToJsonString(WriteOptions)
            : null;
    }

    public Task<BlueprintAssistantToolResult> ListBlueprintsAsync(
        System.Threading.CancellationToken cancellationToken)
    {
        cancellationToken.ThrowIfCancellationRequested();
        JsonArray blueprints = new(ListBlueprints()
            .Select(blueprint => JsonValue.Create(blueprint))
            .ToArray<JsonNode?>());
        return Task.FromResult(BlueprintAssistantToolResult.Completed(
            blueprints.ToJsonString()));
    }

    public Task<BlueprintAssistantToolResult> ReadBlueprintAsync(
        string blueprintKey,
        System.Threading.CancellationToken cancellationToken)
    {
        cancellationToken.ThrowIfCancellationRequested();
        BlueprintAssistantBlueprint? blueprint = ReadBlueprint(blueprintKey);
        return Task.FromResult(blueprint is null
            ? BlueprintAssistantToolResult.Failed("The Blueprint was not found.")
            : BlueprintAssistantToolResult.Completed(blueprint.Json));
    }

    public Task<BlueprintAssistantToolResult> GetApiCatalogAsync(
        string query,
        int maxResults,
        System.Threading.CancellationToken cancellationToken)
    {
        cancellationToken.ThrowIfCancellationRequested();
        return Task.FromResult(BlueprintAssistantToolResult.Completed(
            QueryApiCatalog(
                targetBlueprintKey,
                query,
                Math.Clamp(maxResults, 1, 100))));
    }

    public Task<BlueprintAssistantToolResult> SearchProjectAsync(
        string query,
        int maxResults,
        System.Threading.CancellationToken cancellationToken)
    {
        cancellationToken.ThrowIfCancellationRequested();
        IReadOnlyList<BlueprintAssistantSearchMatch> matches = SearchProject(
            query,
            maxResults);
        JsonArray result = [];
        foreach (BlueprintAssistantSearchMatch match in matches)
        {
            result.Add(new JsonObject
            {
                ["path"] = match.Path,
                ["line"] = match.Line,
                ["text"] = match.Text,
            });
        }
        return Task.FromResult(BlueprintAssistantToolResult.Completed(
            result.ToJsonString(WriteOptions)));
    }

    public Task<BlueprintAssistantToolResult> ReadProjectFileAsync(
        string relativePath,
        int startLine,
        int lineCount,
        System.Threading.CancellationToken cancellationToken)
    {
        cancellationToken.ThrowIfCancellationRequested();
        try
        {
            string content = ReadProjectFile(relativePath, startLine, lineCount);
            return Task.FromResult(BlueprintAssistantToolResult.Completed(content));
        }
        catch (Exception exception) when (
            exception is IOException
            or UnauthorizedAccessException
            or InvalidDataException
            or ArgumentException
            or NotSupportedException)
        {
            return Task.FromResult(BlueprintAssistantToolResult.Failed(exception.Message));
        }
    }

    public Task<BlueprintAssistantToolResult> ValidateCandidateAsync(
        string candidateJson,
        System.Threading.CancellationToken cancellationToken)
    {
        cancellationToken.ThrowIfCancellationRequested();
        BlueprintAssistantValidation validation = ValidateCandidate(
            targetBlueprintKey,
            candidateJson);
        string content = JsonSerializer.Serialize(new
        {
            valid = validation.IsValid,
            errors = validation.Errors,
        });
        return Task.FromResult(BlueprintAssistantToolResult.Completed(content));
    }

    public Task<BlueprintAssistantProposalResult> ProposePatchAsync(
        string patchJson,
        System.Threading.CancellationToken cancellationToken)
    {
        cancellationToken.ThrowIfCancellationRequested();
        JsonDocument document;
        try
        {
            document = JsonDocument.Parse(patchJson);
        }
        catch (JsonException exception)
        {
            return Task.FromResult(BlueprintAssistantProposalResult.Failed(
                "The patch is not valid JSON: " + exception.Message));
        }
        using (document)
        {
            JsonElement patch = document.RootElement;
            if (patch.ValueKind == JsonValueKind.Object
                && patch.EnumerateObject().Any(property =>
                    !string.Equals(property.Name, "ops", StringComparison.Ordinal)))
            {
                return Task.FromResult(BlueprintAssistantProposalResult.Failed(
                    "The patch wrapper contains unknown fields."));
            }
            if (!tryApplyPatch(
                    (JsonObject)baseBlueprint.DeepClone(),
                    patch,
                    out JsonObject? candidate,
                    out string error))
            {
                return Task.FromResult(BlueprintAssistantProposalResult.Failed(error));
            }
            return Task.FromResult(createProposal("Blueprint patch", candidate));
        }
    }

    public Task<BlueprintAssistantProposalResult> ProposeReplacementAsync(
        string replacementJson,
        System.Threading.CancellationToken cancellationToken)
    {
        cancellationToken.ThrowIfCancellationRequested();
        JsonNode? parsed;
        try
        {
            parsed = JsonNode.Parse(replacementJson);
        }
        catch (JsonException exception)
        {
            return Task.FromResult(BlueprintAssistantProposalResult.Failed(
                "The replacement is not valid JSON: " + exception.Message));
        }
        JsonObject? candidate = parsed is JsonObject wrapper
            && wrapper["blueprint"] is JsonObject wrappedBlueprint
                ? (JsonObject)wrappedBlueprint.DeepClone()
                : parsed as JsonObject;
        if (candidate is null)
        {
            return Task.FromResult(BlueprintAssistantProposalResult.Failed(
                "The replacement must be a Blueprint JSON object."));
        }
        if (parsed is JsonObject replacementObject
            && replacementObject.ContainsKey("blueprint")
            && replacementObject.Any(pair =>
                !string.Equals(pair.Key, "blueprint", StringComparison.Ordinal)))
        {
            return Task.FromResult(BlueprintAssistantProposalResult.Failed(
                "The replacement wrapper contains unknown fields."));
        }
        string[] unknownFields = candidate
            .Select(pair => pair.Key)
            .Where(field => field is not "parent" and not "attrs" and not "graph" and not "type")
            .OrderBy(field => field, StringComparer.Ordinal)
            .ToArray();
        if (unknownFields.Length != 0)
        {
            return Task.FromResult(BlueprintAssistantProposalResult.Failed(
                "The replacement contains unknown fields: "
                + string.Join(", ", unknownFields)));
        }
        if (candidate["type"] is JsonValue typeValue
            && typeValue.TryGetValue(out string? typeName)
            && !string.Equals(typeName, "blueprint", StringComparison.Ordinal))
        {
            return Task.FromResult(BlueprintAssistantProposalResult.Failed(
                "The replacement type must be \"blueprint\" when present."));
        }
        candidate.Remove("type");
        return Task.FromResult(createProposal("Blueprint replacement", candidate));
    }

    public static string GetBlueprintHash(JsonObject blueprint)
    {
        byte[] payload = Encoding.UTF8.GetBytes(blueprint.ToJsonString());
        return Convert.ToHexString(SHA256.HashData(payload));
    }

    private BlueprintAssistantProposalResult createProposal(
        string title,
        JsonObject candidate)
    {
        BlueprintValidationResult validation = validationService.ValidateBlueprint(
            targetBlueprintKey,
            candidate);
        string id = Guid.NewGuid().ToString("N");
        proposals[id] = (JsonObject)candidate.DeepClone();
        BlueprintAssistantProposal proposal = new(
            id,
            title,
            createDiff(baseBlueprint, candidate),
            baseRevision,
            validation.IsValid,
            validation.Errors);
        return BlueprintAssistantProposalResult.Completed(proposal);
    }

    private static bool tryApplyPatch(
        JsonObject candidate,
        JsonElement patch,
        out JsonObject result,
        out string error)
    {
        JsonElement operations = patch;
        if (patch.ValueKind == JsonValueKind.Object
            && patch.TryGetProperty("ops", out JsonElement wrappedOperations))
        {
            operations = wrappedOperations;
        }
        if (operations.ValueKind != JsonValueKind.Array
            || operations.GetArrayLength() == 0)
        {
            result = candidate;
            error = "Patch operations must be a non-empty array.";
            return false;
        }
        int index = 0;
        foreach (JsonElement operation in operations.EnumerateArray())
        {
            if (operation.ValueKind != JsonValueKind.Object
                || !operation.TryGetProperty("op", out JsonElement operationNameValue)
                || operationNameValue.ValueKind != JsonValueKind.String)
            {
                result = candidate;
                error = $"ops[{index}] must declare a string op.";
                return false;
            }
            string operationName = operationNameValue.GetString() ?? string.Empty;
            if (!applyPatchOperation(candidate, operationName, operation, index, out error))
            {
                result = candidate;
                return false;
            }
            index++;
        }
        result = candidate;
        error = string.Empty;
        return true;
    }

    private static bool applyPatchOperation(
        JsonObject candidate,
        string operationName,
        JsonElement operation,
        int operationIndex,
        out string error)
    {
        switch (operationName)
        {
            case "updateLink":
                if (!validateOperationFields(
                        operation,
                        operationIndex,
                        ["op", "event", "linkIndex", "left", "right", "leftOutPin", "rightInPin", "linkType"],
                        ["op", "event", "linkIndex"],
                        out error))
                {
                    return false;
                }
                if (!validateLinkUpdate(operation, operationIndex, out error))
                    return false;
                return updateGraphItem(
                    candidate,
                    operation,
                    operationIndex,
                    "links",
                    "linkIndex",
                    ["left", "right", "leftOutPin", "rightInPin", "linkType"],
                    out error);
            case "updateNode":
                if (!validateOperationFields(
                        operation,
                        operationIndex,
                        ["op", "event", "nodeIndex", "nodeFunction", "params", "pos"],
                        ["op", "event", "nodeIndex"],
                        out error))
                {
                    return false;
                }
                if (!validateNodeUpdate(operation, operationIndex, out error))
                    return false;
                return updateGraphItem(
                    candidate,
                    operation,
                    operationIndex,
                    "nodes",
                    "nodeIndex",
                    ["nodeFunction", "params", "pos"],
                    out error);
            case "setStartNode":
                if (!validateOperationFields(
                        operation,
                        operationIndex,
                        ["op", "event", "index"],
                        ["op", "event", "index"],
                        out error))
                {
                    return false;
                }
                return setStartNode(candidate, operation, operationIndex, out error);
            case "replaceEventGraph":
                if (!validateOperationFields(
                        operation,
                        operationIndex,
                        ["op", "event", "nodes", "links"],
                        ["op", "event", "nodes", "links"],
                        out error))
                {
                    return false;
                }
                return replaceEventGraph(candidate, operation, operationIndex, out error);
            case "setAttrs":
                if (!validateOperationFields(
                        operation,
                        operationIndex,
                        ["op", "attrs"],
                        ["op", "attrs"],
                        out error))
                {
                    return false;
                }
                return setAttributes(candidate, operation, operationIndex, out error);
            default:
                error = $"ops[{operationIndex}] has unknown op \"{operationName}\".";
                return false;
        }
    }

    private static bool updateGraphItem(
        JsonObject candidate,
        JsonElement operation,
        int operationIndex,
        string collectionName,
        string indexName,
        IReadOnlyList<string> fields,
        out string error)
    {
        if (!tryGetEventGraph(
                candidate,
                operation,
                operationIndex,
                out JsonObject? eventGraph,
                out error))
        {
            return false;
        }
        if (!operation.TryGetProperty(indexName, out JsonElement indexValue)
            || !indexValue.TryGetInt32(out int itemIndex)
            || eventGraph![collectionName] is not JsonArray items
            || itemIndex < 0
            || itemIndex >= items.Count
            || items[itemIndex] is not JsonObject item)
        {
            error = $"ops[{operationIndex}] has an invalid {indexName}.";
            return false;
        }
        foreach (string field in fields)
        {
            if (operation.TryGetProperty(field, out JsonElement value))
                item[field] = JsonNode.Parse(value.GetRawText());
        }
        error = string.Empty;
        return true;
    }

    private static bool validateLinkUpdate(
        JsonElement operation,
        int operationIndex,
        out string error)
    {
        bool hasUpdate = false;
        foreach (string field in new[] { "left", "right", "leftOutPin", "rightInPin" })
        {
            if (!operation.TryGetProperty(field, out JsonElement value))
                continue;
            hasUpdate = true;
            bool valid = field == "left"
                ? value.ValueKind == JsonValueKind.String || value.TryGetInt32(out int _)
                : value.TryGetInt32(out int _);
            if (!valid)
            {
                error = $"ops[{operationIndex}].{field} has an invalid type.";
                return false;
            }
        }
        if (operation.TryGetProperty("linkType", out JsonElement linkType))
        {
            hasUpdate = true;
            if (linkType.ValueKind != JsonValueKind.String
                || linkType.GetString() is not "Exec" and not "Params")
            {
                error = $"ops[{operationIndex}].linkType must be \"Exec\" or \"Params\".";
                return false;
            }
        }
        error = hasUpdate
            ? string.Empty
            : $"ops[{operationIndex}] does not contain a link update.";
        return hasUpdate;
    }

    private static bool validateNodeUpdate(
        JsonElement operation,
        int operationIndex,
        out string error)
    {
        bool hasUpdate = false;
        if (operation.TryGetProperty("nodeFunction", out JsonElement nodeFunction))
        {
            hasUpdate = true;
            if (nodeFunction.ValueKind != JsonValueKind.String
                || string.IsNullOrWhiteSpace(nodeFunction.GetString()))
            {
                error = $"ops[{operationIndex}].nodeFunction must be a non-empty string.";
                return false;
            }
        }
        if (operation.TryGetProperty("params", out JsonElement parameters))
        {
            hasUpdate = true;
            if (parameters.ValueKind != JsonValueKind.Array)
            {
                error = $"ops[{operationIndex}].params must be an array.";
                return false;
            }
        }
        if (operation.TryGetProperty("pos", out JsonElement position))
        {
            hasUpdate = true;
            if (position.ValueKind != JsonValueKind.Array
                || position.GetArrayLength() != 2
                || position.EnumerateArray().Any(value =>
                    value.ValueKind != JsonValueKind.Number))
            {
                error = $"ops[{operationIndex}].pos must contain two numbers.";
                return false;
            }
        }
        error = hasUpdate
            ? string.Empty
            : $"ops[{operationIndex}] does not contain a node update.";
        return hasUpdate;
    }

    private static bool setStartNode(
        JsonObject candidate,
        JsonElement operation,
        int operationIndex,
        out string error)
    {
        if (!tryGetEventName(operation, operationIndex, out string eventName, out error)
            || !operation.TryGetProperty("index", out JsonElement indexValue)
            || indexValue.ValueKind is not JsonValueKind.Number and not JsonValueKind.Null)
        {
            if (error.Length == 0)
                error = $"ops[{operationIndex}] requires a numeric or null index.";
            return false;
        }
        if (candidate["graph"]?["nodeGraph"]?[eventName]?["nodes"] is not JsonArray nodes)
        {
            error = $"ops[{operationIndex}] references a missing event graph.";
            return false;
        }
        if (indexValue.ValueKind == JsonValueKind.Number
            && (!indexValue.TryGetInt32(out int index)
                || index < 0
                || index >= nodes.Count))
        {
            error = $"ops[{operationIndex}] start node index is out of range.";
            return false;
        }
        if (candidate["graph"] is not JsonObject graph)
        {
            error = $"ops[{operationIndex}] cannot apply because graph is missing.";
            return false;
        }
        JsonObject startNodes = graph["startNodes"] as JsonObject ?? [];
        graph["startNodes"] = startNodes;
        startNodes[eventName] = JsonNode.Parse(indexValue.GetRawText());
        error = string.Empty;
        return true;
    }

    private static bool validateOperationFields(
        JsonElement operation,
        int operationIndex,
        IReadOnlyList<string> allowed,
        IReadOnlyList<string> required,
        out string error)
    {
        foreach (JsonProperty property in operation.EnumerateObject())
        {
            if (!allowed.Contains(property.Name, StringComparer.Ordinal))
            {
                error = $"ops[{operationIndex}] contains unknown field \"{property.Name}\".";
                return false;
            }
        }
        foreach (string field in required)
        {
            if (!operation.TryGetProperty(field, out JsonElement _))
            {
                error = $"ops[{operationIndex}] is missing required field \"{field}\".";
                return false;
            }
        }
        error = string.Empty;
        return true;
    }

    private static bool replaceEventGraph(
        JsonObject candidate,
        JsonElement operation,
        int operationIndex,
        out string error)
    {
        if (!tryGetEventName(operation, operationIndex, out string eventName, out error)
            || !operation.TryGetProperty("nodes", out JsonElement nodes)
            || nodes.ValueKind != JsonValueKind.Array
            || !operation.TryGetProperty("links", out JsonElement links)
            || links.ValueKind != JsonValueKind.Array)
        {
            if (error.Length == 0)
                error = $"ops[{operationIndex}] requires nodes and links arrays.";
            return false;
        }
        if (candidate["graph"] is not JsonObject graph)
        {
            error = $"ops[{operationIndex}] cannot apply because graph is missing.";
            return false;
        }
        JsonObject nodeGraph = graph["nodeGraph"] as JsonObject ?? [];
        graph["nodeGraph"] = nodeGraph;
        nodeGraph[eventName] = new JsonObject
        {
            ["nodes"] = JsonNode.Parse(nodes.GetRawText()),
            ["links"] = JsonNode.Parse(links.GetRawText()),
        };
        error = string.Empty;
        return true;
    }

    private static bool setAttributes(
        JsonObject candidate,
        JsonElement operation,
        int operationIndex,
        out string error)
    {
        if (!operation.TryGetProperty("attrs", out JsonElement attributes)
            || attributes.ValueKind != JsonValueKind.Object
            || JsonNode.Parse(attributes.GetRawText()) is not JsonObject updates)
        {
            error = $"ops[{operationIndex}] requires an attrs object.";
            return false;
        }
        JsonObject target = candidate["attrs"] as JsonObject ?? [];
        candidate["attrs"] = target;
        foreach (KeyValuePair<string, JsonNode?> pair in updates)
            target[pair.Key] = pair.Value?.DeepClone();
        error = string.Empty;
        return true;
    }

    private static bool tryGetEventGraph(
        JsonObject candidate,
        JsonElement operation,
        int operationIndex,
        out JsonObject? eventGraph,
        out string error)
    {
        if (!tryGetEventName(operation, operationIndex, out string eventName, out error))
        {
            eventGraph = null;
            return false;
        }
        eventGraph = candidate["graph"]?["nodeGraph"]?[eventName] as JsonObject;
        if (eventGraph is null)
        {
            error = $"ops[{operationIndex}] references a missing event graph.";
            return false;
        }
        error = string.Empty;
        return true;
    }

    private static bool tryGetEventName(
        JsonElement operation,
        int operationIndex,
        out string eventName,
        out string error)
    {
        if (!operation.TryGetProperty("event", out JsonElement eventValue)
            || eventValue.ValueKind != JsonValueKind.String
            || string.IsNullOrWhiteSpace(eventValue.GetString()))
        {
            eventName = string.Empty;
            error = $"ops[{operationIndex}] requires a non-empty event.";
            return false;
        }
        eventName = eventValue.GetString()!;
        error = string.Empty;
        return true;
    }

    private static string createDiff(JsonObject before, JsonObject after)
    {
        List<string> lines = [];
        collectDiff("$", before, after, lines);
        return lines.Count == 0
            ? "No changes."
            : string.Join(Environment.NewLine, lines.Take(400));
    }

    private static void collectDiff(
        string path,
        JsonNode? before,
        JsonNode? after,
        ICollection<string> result)
    {
        if (JsonNode.DeepEquals(before, after))
            return;
        if (before is JsonObject beforeObject && after is JsonObject afterObject)
        {
            foreach (string key in beforeObject.Select(pair => pair.Key)
                         .Union(afterObject.Select(pair => pair.Key), StringComparer.Ordinal)
                         .OrderBy(key => key, StringComparer.Ordinal))
            {
                collectDiff(
                    path + "." + key,
                    beforeObject[key],
                    afterObject[key],
                    result);
            }
            return;
        }
        if (before is JsonArray beforeArray && after is JsonArray afterArray)
        {
            if (beforeArray.Count == afterArray.Count && beforeArray.Count <= 50)
            {
                for (int index = 0; index < beforeArray.Count; index++)
                    collectDiff($"{path}[{index}]", beforeArray[index], afterArray[index], result);
                return;
            }
        }
        result.Add(path);
        result.Add("- " + formatDiffValue(before));
        result.Add("+ " + formatDiffValue(after));
    }

    private static string formatDiffValue(JsonNode? value)
    {
        if (value is null)
            return "null";
        return truncate(value.ToJsonString(), 800);
    }

    private IEnumerable<string> enumerateReadableFiles()
    {
        foreach (string rootName in AllowedRootNames.OrderBy(name => name, StringComparer.Ordinal))
        {
            string root = Path.Combine(projectPath, rootName);
            if (!Directory.Exists(root) || isLink(root))
                continue;
            Stack<string> pending = new();
            pending.Push(root);
            while (pending.Count != 0)
            {
                string directory = pending.Pop();
                foreach (string childDirectory in Directory.EnumerateDirectories(directory)
                             .OrderByDescending(path => path, StringComparer.Ordinal))
                {
                    if (!ExcludedSegments.Contains(Path.GetFileName(childDirectory))
                        && !isLink(childDirectory))
                    {
                        pending.Push(childDirectory);
                    }
                }
                foreach (string path in Directory.EnumerateFiles(directory)
                             .OrderBy(path => path, StringComparer.Ordinal))
                {
                    if (isReadablePath(path))
                        yield return path;
                }
            }
        }
    }

    private string resolveReadablePath(string relativePath)
    {
        if (string.IsNullOrWhiteSpace(relativePath)
            || Path.IsPathFullyQualified(relativePath))
        {
            throw new UnauthorizedAccessException("Only project-relative paths are allowed.");
        }
        string path = Path.GetFullPath(Path.Combine(
            projectPath,
            relativePath.Replace('/', Path.DirectorySeparatorChar)));
        if (!isInsideProject(path) || !isReadablePath(path) || !File.Exists(path))
            throw new UnauthorizedAccessException("The requested path is not readable by Blueprint AI.");
        return path;
    }

    private bool isReadablePath(string path)
    {
        if (!isInsideProject(path)
            || !AllowedExtensions.Contains(Path.GetExtension(path)))
        {
            return false;
        }
        string relative = Path.GetRelativePath(projectPath, path);
        string[] segments = relative.Split(
            [Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar],
            StringSplitOptions.RemoveEmptyEntries);
        if (segments.Length < 2
            || !AllowedRootNames.Contains(segments[0])
            || segments.Any(ExcludedSegments.Contains))
        {
            return false;
        }
        string current = projectPath;
        foreach (string segment in segments)
        {
            current = Path.Combine(current, segment);
            if (isLink(current))
                return false;
        }
        string fileName = Path.GetFileName(path);
        string[] sensitiveNames =
        [
            "apikey",
            "api-key",
            "password",
            "passwd",
            "token",
            "auth",
            "secret",
            "credential",
        ];
        return !fileName.StartsWith(".env", StringComparison.OrdinalIgnoreCase)
            && !fileName.EndsWith(".ini", StringComparison.OrdinalIgnoreCase)
            && !sensitiveNames.Any(value =>
                fileName.Contains(value, StringComparison.OrdinalIgnoreCase));
    }

    private bool isInsideProject(string path)
    {
        string relative = Path.GetRelativePath(projectPath, Path.GetFullPath(path));
        return relative != "."
            && !Path.IsPathRooted(relative)
            && relative != ".."
            && !relative.StartsWith(
                ".." + Path.DirectorySeparatorChar,
                pathComparison)
            && !relative.StartsWith(
                ".." + Path.AltDirectorySeparatorChar,
                pathComparison);
    }

    private static bool isLink(string path)
    {
        return File.Exists(path) || Directory.Exists(path)
            ? File.GetAttributes(path).HasFlag(FileAttributes.ReparsePoint)
            : false;
    }

    private string normalizeRelativePath(string path)
    {
        return Path.GetRelativePath(projectPath, path).Replace('\\', '/');
    }

    private static string normalizeBlueprintKey(string blueprintKey)
    {
        string key = blueprintKey.Trim()
            .Replace('\\', '/')
            .Trim('/');
        const string prefix = "Data.Blueprints.";
        if (key.StartsWith(prefix, StringComparison.Ordinal))
            key = key[prefix.Length..].Replace('.', '/');
        if (key.EndsWith(".json", StringComparison.OrdinalIgnoreCase))
            key = key[..^5];
        return key;
    }

    private static bool tryParseCandidate(
        string candidateJson,
        out JsonObject? candidate,
        out string error)
    {
        try
        {
            candidate = JsonNode.Parse(candidateJson) as JsonObject;
        }
        catch (JsonException exception)
        {
            candidate = null;
            error = "The proposal is not valid JSON: " + exception.Message;
            return false;
        }
        if (candidate is null)
        {
            error = "The proposal must be a JSON object.";
            return false;
        }
        string[] unknownFields = candidate
            .Select(pair => pair.Key)
            .Where(field => field is not "parent" and not "attrs" and not "graph" and not "type")
            .OrderBy(field => field, StringComparer.Ordinal)
            .ToArray();
        if (unknownFields.Length != 0)
        {
            error = "The proposal contains unknown fields: "
                + string.Join(", ", unknownFields);
            return false;
        }
        if (candidate["type"] is JsonNode typeNode
            && (typeNode is not JsonValue typeValue
                || !typeValue.TryGetValue(out string? typeName)
                || !string.Equals(typeName, "blueprint", StringComparison.Ordinal)))
        {
            error = "The proposal type must be \"blueprint\" when present.";
            return false;
        }
        candidate.Remove("type");
        error = string.Empty;
        return true;
    }

    private static bool matchesDefinition(
        BlueprintGraphNodeDefinition definition,
        string query)
    {
        if (query.Length == 0)
            return true;
        return definition.RuntimePath.Contains(query, StringComparison.OrdinalIgnoreCase)
            || definition.Title.Contains(query, StringComparison.OrdinalIgnoreCase)
            || definition.MemberName.Contains(query, StringComparison.OrdinalIgnoreCase)
            || definition.Description.Contains(query, StringComparison.OrdinalIgnoreCase)
            || definition.Ports.Any(port =>
                port.Name.Contains(query, StringComparison.OrdinalIgnoreCase)
                || port.TypeName.Contains(query, StringComparison.OrdinalIgnoreCase));
    }

    private static string truncate(string value, int maximumLength)
    {
        return value.Length <= maximumLength
            ? value
            : value[..maximumLength];
    }
}
