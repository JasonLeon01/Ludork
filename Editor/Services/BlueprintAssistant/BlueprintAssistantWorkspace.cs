using Ludork.Models;
using Ludork.Plugin.Abstractions;
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
        "Application",
        "docs",
        "Engine",
        "Global",
        "GlobalCore",
        "GlobalFunctions",
        "Scripts",
        "Source",
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
        ProjectToolConstants.EditorCacheDirectory,
        "ThirdParty",
        "ThirdPartySource",
    };

    private readonly ProjectDataStore gameData;
    private readonly LuaMetadataService metadataService;
    private readonly BlueprintClassResolver classResolver;
    private readonly BlueprintValidationService validationService;
    private readonly Action<string> flushBlueprint;
    private readonly Action<string> refreshBlueprint;
    private readonly string projectPath;
    private readonly string targetBlueprintKey;
    private readonly JsonObject baseBlueprint;
    private readonly string baseRevision;
    private readonly Dictionary<string, JsonObject> proposals = new(StringComparer.Ordinal);

    public BlueprintAssistantWorkspace(
        ProjectDataStore gameData,
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
        this.targetBlueprintKey = BlueprintReference.NormalizeKey(targetBlueprintKey);
        if (!gameData.Blueprints.BlueprintsData.TryGetValue(this.targetBlueprintKey, out BlueprintDefinitionSnapshot? blueprint))
            throw new ArgumentException("The target Blueprint was not found.", nameof(targetBlueprintKey));
        baseBlueprint = blueprint.ToJson();
        baseRevision = GetBlueprintHash(baseBlueprint);
    }

    public string ProjectPath => projectPath;
    public string BlueprintKey => targetBlueprintKey;
    public string BaseRevision => baseRevision;

    public IReadOnlyList<string> ListBlueprints()
    {
        return gameData.Blueprints.BlueprintsData.Keys
            .OrderBy(key => key, StringComparer.Ordinal)
            .ToArray();
    }

    public BlueprintAssistantBlueprint? ReadBlueprint(string blueprintKey)
    {
        string key = BlueprintReference.NormalizeKey(blueprintKey);
        if (!gameData.Blueprints.BlueprintsData.TryGetValue(key, out BlueprintDefinitionSnapshot? blueprint))
            return null;
        JsonObject clone = blueprint.ToJson();
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
            BlueprintReference.NormalizeKey(blueprintKey),
            candidate);
        return new BlueprintAssistantValidation(result.IsValid, result.Errors);
    }

    public string QueryApiCatalog(
        string blueprintKey,
        string? query,
        int maximumResults = 80)
    {
        string key = BlueprintReference.NormalizeKey(blueprintKey);
        if (!gameData.Blueprints.BlueprintsData.TryGetValue(key, out BlueprintDefinitionSnapshot? blueprint))
            return "[]";
        BlueprintGraphContext context = new(blueprint.ToJson(), key);
        BlueprintNodeDefinitionCatalog catalog = new(
            metadataService,
            classResolver);
        string filter = query?.Trim() ?? string.Empty;
        int limit = Math.Clamp(maximumResults, 1, 5000);
        JsonArray result = [];
        foreach (BlueprintGraphNodeDefinition definition in catalog.GetNodeDefinitionSet(context).Definitions
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
                ["title"] = BlueprintNodeDisplayText.GetTitle(definition),
                ["memberName"] = definition.MemberName,
                ["description"] = string.Empty,
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
        string key = BlueprintReference.NormalizeKey(blueprintKey);
        flushBlueprint(key);
        if (!gameData.Blueprints.BlueprintsData.TryGetValue(key, out BlueprintDefinitionSnapshot? current))
        {
            return new BlueprintAssistantApplyResult(
                false,
                false,
                "The target Blueprint no longer exists.",
                string.Empty);
        }
        string currentHash = GetBlueprintHash(current.ToJson());
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
        bool updated = gameData.Blueprints.UpdateBlueprint(key, candidate!);
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
        try
        {
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
        catch (Exception exception) when (EditorPathSandbox.IsPathFailure(exception))
        {
            return Task.FromResult(BlueprintAssistantToolResult.Failed(exception.Message));
        }
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
            EditorPathSandbox.IsPathFailure(exception) || exception is InvalidDataException)
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
            if (!BlueprintPatchEngine.TryApply(
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
            if (!EditorPathSandbox.TryResolve(projectPath, root, out _) || !Directory.Exists(root))
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
                        && EditorPathSandbox.TryResolve(projectPath, childDirectory, out _))
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
        if (string.IsNullOrWhiteSpace(relativePath) || Path.IsPathRooted(relativePath))
            throw new UnauthorizedAccessException("Only project-relative paths are allowed.");
        if (!EditorPathSandbox.TryResolve(projectPath, relativePath, out string path)
            || !isReadablePath(path) || !File.Exists(path))
        {
            throw new UnauthorizedAccessException("The requested path is not readable by Blueprint AI.");
        }
        return path;
    }

    private bool isReadablePath(string path)
    {
        if (!EditorPathSandbox.TryResolve(projectPath, path, out _)
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

    private string normalizeRelativePath(string path)
    {
        return Path.GetRelativePath(projectPath, path).Replace('\\', '/');
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
            || BlueprintNodeDisplayText.GetTitle(definition).Contains(query, StringComparison.OrdinalIgnoreCase)
            || definition.MemberName.Contains(query, StringComparison.OrdinalIgnoreCase)
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
