using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;
using Ludork.Plugin.Abstractions;

namespace Ludork.Plugins.OfficialBlueprintAI.Agent;

internal sealed class BlueprintAgentTools : IAgentToolExecutor
{
    private const int ToolResultCharacterLimit = 12000;

    private static readonly HashSet<string> ReadBlueprintProperties =
        new HashSet<string>(StringComparer.Ordinal)
        {
            "blueprint_key",
        };

    private static readonly HashSet<string> SearchProjectProperties =
        new HashSet<string>(StringComparer.Ordinal)
        {
            "query",
            "max_results",
        };

    private static readonly HashSet<string> ApiCatalogProperties =
        new HashSet<string>(StringComparer.Ordinal)
        {
            "query",
            "max_results",
        };

    private static readonly HashSet<string> ReadFileProperties =
        new HashSet<string>(StringComparer.Ordinal)
        {
            "path",
            "start_line",
            "line_count",
        };

    private static readonly HashSet<string> CandidateProperties =
        new HashSet<string>(StringComparer.Ordinal)
        {
            "candidate_json",
        };

    private static readonly HashSet<string> PatchProperties =
        new HashSet<string>(StringComparer.Ordinal)
        {
            "patch_json",
        };

    private static readonly HashSet<string> ReplacementProperties =
        new HashSet<string>(StringComparer.Ordinal)
        {
            "blueprint_json",
        };

    public BlueprintAgentTools()
    {
        Definitions = CreateDefinitions();
    }

    public IReadOnlyList<AgentToolDefinition> Definitions { get; }

    public async Task<AgentToolExecution> ExecuteAsync(
        AgentToolCall call,
        IBlueprintAssistantWorkspace workspace,
        CancellationToken cancellationToken)
    {
        ArgumentNullException.ThrowIfNull(call);
        ArgumentNullException.ThrowIfNull(workspace);

        JsonDocument document;
        try
        {
            document = JsonDocument.Parse(call.ArgumentsJson);
        }
        catch (JsonException exception)
        {
            return FormatError($"Invalid JSON arguments: {exception.Message}");
        }

        using (document)
        {
            JsonElement root = document.RootElement;
            if (root.ValueKind != JsonValueKind.Object)
            {
                return FormatError("Tool arguments must be a JSON object.");
            }

            return call.Name switch
            {
                "list_blueprints" => await ListBlueprintsAsync(
                    root,
                    workspace,
                    cancellationToken),
                "read_blueprint" => await ReadBlueprintAsync(
                    root,
                    workspace,
                    cancellationToken),
                "get_api_catalog" => await GetApiCatalogAsync(
                    root,
                    workspace,
                    cancellationToken),
                "search_project" => await SearchProjectAsync(
                    root,
                    workspace,
                    cancellationToken),
                "read_file" => await ReadFileAsync(
                    root,
                    workspace,
                    cancellationToken),
                "validate_blueprint" => await ValidateBlueprintAsync(
                    root,
                    workspace,
                    cancellationToken),
                "propose_blueprint_patch" => await ProposePatchAsync(
                    root,
                    workspace,
                    cancellationToken),
                "propose_blueprint_replace" => await ProposeReplacementAsync(
                    root,
                    workspace,
                    cancellationToken),
                _ => FormatError($"Unknown tool \"{call.Name}\"."),
            };
        }
    }

    private static async Task<AgentToolExecution> ListBlueprintsAsync(
        JsonElement root,
        IBlueprintAssistantWorkspace workspace,
        CancellationToken cancellationToken)
    {
        string? error = ValidateProperties(root, new HashSet<string>());
        if (error is not null)
        {
            return FormatError(error);
        }

        BlueprintAssistantToolResult result =
            await workspace.ListBlueprintsAsync(cancellationToken);
        return FromReadResult(result);
    }

    private static async Task<AgentToolExecution> ReadBlueprintAsync(
        JsonElement root,
        IBlueprintAssistantWorkspace workspace,
        CancellationToken cancellationToken)
    {
        string? error = ValidateProperties(root, ReadBlueprintProperties);
        if (error is not null)
        {
            return FormatError(error);
        }
        if (!TryGetRequiredString(root, "blueprint_key", out string blueprintKey))
        {
            return FormatError("blueprint_key must be a non-empty string.");
        }

        BlueprintAssistantToolResult result =
            await workspace.ReadBlueprintAsync(blueprintKey, cancellationToken);
        return FromReadResult(result);
    }

    private static async Task<AgentToolExecution> GetApiCatalogAsync(
        JsonElement root,
        IBlueprintAssistantWorkspace workspace,
        CancellationToken cancellationToken)
    {
        string? error = ValidateProperties(root, ApiCatalogProperties);
        if (error is not null)
        {
            return FormatError(error);
        }
        if (!TryGetRequiredString(root, "query", out string query))
        {
            return FormatError("query must be a non-empty string.");
        }
        if (!TryGetRequiredInteger(root, "max_results", 1, 100, out int maxResults))
        {
            return FormatError("max_results must be an integer from 1 to 100.");
        }

        BlueprintAssistantToolResult result =
            await workspace.GetApiCatalogAsync(
                query,
                maxResults,
                cancellationToken);
        return FromReadResult(result);
    }

    private static async Task<AgentToolExecution> SearchProjectAsync(
        JsonElement root,
        IBlueprintAssistantWorkspace workspace,
        CancellationToken cancellationToken)
    {
        string? error = ValidateProperties(root, SearchProjectProperties);
        if (error is not null)
        {
            return FormatError(error);
        }
        if (!TryGetRequiredString(root, "query", out string query))
        {
            return FormatError("query must be a non-empty string.");
        }
        if (!TryGetRequiredInteger(root, "max_results", 1, 30, out int maxResults))
        {
            return FormatError("max_results must be an integer from 1 to 30.");
        }

        BlueprintAssistantToolResult result =
            await workspace.SearchProjectAsync(
                query,
                maxResults,
                cancellationToken);
        return FromReadResult(result);
    }

    private static async Task<AgentToolExecution> ReadFileAsync(
        JsonElement root,
        IBlueprintAssistantWorkspace workspace,
        CancellationToken cancellationToken)
    {
        string? error = ValidateProperties(root, ReadFileProperties);
        if (error is not null)
        {
            return FormatError(error);
        }
        if (!TryGetRequiredString(root, "path", out string path))
        {
            return FormatError("path must be a non-empty relative path.");
        }
        if (!TryGetRequiredInteger(root, "start_line", 1, int.MaxValue, out int startLine))
        {
            return FormatError("start_line must be a positive integer.");
        }
        if (!TryGetRequiredInteger(root, "line_count", 1, 400, out int lineCount))
        {
            return FormatError("line_count must be an integer from 1 to 400.");
        }

        BlueprintAssistantToolResult result =
            await workspace.ReadProjectFileAsync(
                path,
                startLine,
                lineCount,
                cancellationToken);
        return FromReadResult(result);
    }

    private static async Task<AgentToolExecution> ValidateBlueprintAsync(
        JsonElement root,
        IBlueprintAssistantWorkspace workspace,
        CancellationToken cancellationToken)
    {
        string? error = ValidateProperties(root, CandidateProperties);
        if (error is not null)
        {
            return FormatError(error);
        }
        if (!TryGetRequiredString(root, "candidate_json", out string candidateJson))
        {
            return FormatError("candidate_json must be a non-empty JSON string.");
        }
        if (!TryParseJson(candidateJson, JsonValueKind.Object, out JsonElement candidate))
        {
            return FormatError(
                "candidate_json must contain one valid Blueprint JSON object.");
        }

        BlueprintAssistantToolResult result =
            await workspace.ValidateCandidateAsync(
                candidateJson,
                cancellationToken);
        return FromReadResult(result);
    }

    private static async Task<AgentToolExecution> ProposePatchAsync(
        JsonElement root,
        IBlueprintAssistantWorkspace workspace,
        CancellationToken cancellationToken)
    {
        string? error = ValidateProperties(root, PatchProperties);
        if (error is not null)
        {
            return FormatError(error);
        }
        if (!TryGetRequiredString(root, "patch_json", out string patchJson))
        {
            return FormatError("patch_json must be a non-empty JSON string.");
        }
        if (!TryParseJson(patchJson, JsonValueKind.Array, out JsonElement patch))
        {
            return FormatError("patch_json must contain one valid JSON array.");
        }
        if (patch.GetArrayLength() == 0)
        {
            return FormatError("patch must contain at least one operation.");
        }

        BlueprintAssistantProposalResult result =
            await workspace.ProposePatchAsync(
                patchJson,
                cancellationToken);
        return FromProposalResult(result);
    }

    private static async Task<AgentToolExecution> ProposeReplacementAsync(
        JsonElement root,
        IBlueprintAssistantWorkspace workspace,
        CancellationToken cancellationToken)
    {
        string? error = ValidateProperties(root, ReplacementProperties);
        if (error is not null)
        {
            return FormatError(error);
        }
        if (!TryGetRequiredString(root, "blueprint_json", out string blueprintJson))
        {
            return FormatError("blueprint_json must be a non-empty JSON string.");
        }
        if (!TryParseJson(
            blueprintJson,
            JsonValueKind.Object,
            out JsonElement blueprint))
        {
            return FormatError(
                "blueprint_json must contain one valid Blueprint JSON object.");
        }

        BlueprintAssistantProposalResult result =
            await workspace.ProposeReplacementAsync(
                blueprintJson,
                cancellationToken);
        return FromProposalResult(result);
    }

    private static AgentToolExecution FromReadResult(
        BlueprintAssistantToolResult result)
    {
        string content = result.Success
            ? Truncate(result.Content)
            : JsonSerializer.Serialize(new
            {
                success = false,
                error = result.Error,
            });
        string display = result.Success ? "Completed" : result.Error;
        return new AgentToolExecution(
            result.Success,
            true,
            false,
            content,
            display,
            null);
    }

    private static AgentToolExecution FromProposalResult(
        BlueprintAssistantProposalResult result)
    {
        if (!result.Success || result.Proposal is null)
        {
            string error = string.IsNullOrWhiteSpace(result.Error)
                ? "Blueprint proposal was rejected."
                : result.Error;
            return new AgentToolExecution(
                false,
                false,
                false,
                JsonSerializer.Serialize(new
                {
                    success = false,
                    error,
                }),
                error,
                null);
        }

        BlueprintAssistantProposal proposal = result.Proposal;
        string content = JsonSerializer.Serialize(new
        {
            success = true,
            proposalId = proposal.Id,
            valid = proposal.IsValid,
            diagnostics = proposal.Diagnostics,
        });
        return new AgentToolExecution(
            true,
            false,
            false,
            content,
            proposal.IsValid ? "Proposal ready" : "Proposal is invalid",
            proposal);
    }

    private static AgentToolExecution FormatError(string error)
    {
        return new AgentToolExecution(
            false,
            false,
            true,
            JsonSerializer.Serialize(new
            {
                success = false,
                error,
            }),
            error,
            null);
    }

    private static string? ValidateProperties(
        JsonElement root,
        IReadOnlySet<string> expected)
    {
        HashSet<string> actual = root
            .EnumerateObject()
            .Select(property => property.Name)
            .ToHashSet(StringComparer.Ordinal);
        if (!actual.SetEquals(expected))
        {
            string names = string.Join(", ", expected.Order());
            return $"Tool arguments must contain exactly: {names}.";
        }

        return null;
    }

    private static bool TryGetRequiredString(
        JsonElement root,
        string name,
        out string value)
    {
        value = string.Empty;
        if (!root.TryGetProperty(name, out JsonElement property) ||
            property.ValueKind != JsonValueKind.String)
        {
            return false;
        }

        value = property.GetString()?.Trim() ?? string.Empty;
        return value.Length > 0;
    }

    private static bool TryGetRequiredInteger(
        JsonElement root,
        string name,
        int minimum,
        int maximum,
        out int value)
    {
        value = 0;
        return root.TryGetProperty(name, out JsonElement property) &&
            property.ValueKind == JsonValueKind.Number &&
            property.TryGetInt32(out value) &&
            value >= minimum &&
            value <= maximum;
    }

    private static bool TryParseJson(
        string json,
        JsonValueKind expectedKind,
        out JsonElement value)
    {
        value = default;
        try
        {
            using JsonDocument document = JsonDocument.Parse(json);
            if (document.RootElement.ValueKind != expectedKind)
            {
                return false;
            }

            value = document.RootElement.Clone();
            return true;
        }
        catch (JsonException)
        {
            return false;
        }
    }

    private static string Truncate(string content)
    {
        if (content.Length <= ToolResultCharacterLimit)
        {
            return content;
        }

        int omitted = content.Length - ToolResultCharacterLimit;
        return $"{content[..ToolResultCharacterLimit]}\n...[truncated {omitted} characters]";
    }

    private static IReadOnlyList<AgentToolDefinition> CreateDefinitions()
    {
        return new List<AgentToolDefinition>
        {
            new AgentToolDefinition(
                "list_blueprints",
                "List existing Blueprints in the current project.",
                """
                {
                  "type": "object",
                  "properties": {},
                  "required": [],
                  "additionalProperties": false
                }
                """),
            new AgentToolDefinition(
                "read_blueprint",
                "Read one existing Blueprint by its exact key. Other Blueprints are read-only context.",
                """
                {
                  "type": "object",
                  "properties": {
                    "blueprint_key": {
                      "type": "string"
                    }
                  },
                  "required": ["blueprint_key"],
                  "additionalProperties": false
                }
                """),
            new AgentToolDefinition(
                "get_api_catalog",
                "Search the current Lua metadata and Blueprint node API catalog by symbol, type, or purpose.",
                """
                {
                  "type": "object",
                  "properties": {
                    "query": {
                      "type": "string"
                    },
                    "max_results": {
                      "type": "integer",
                      "minimum": 1,
                      "maximum": 100
                    }
                  },
                  "required": ["query", "max_results"],
                  "additionalProperties": false
                }
                """),
            new AgentToolDefinition(
                "search_project",
                "Search allowed Blueprint, Lua, C, C++, metadata, JSON, and documentation text without modifying files.",
                """
                {
                  "type": "object",
                  "properties": {
                    "query": {
                      "type": "string"
                    },
                    "max_results": {
                      "type": "integer",
                      "minimum": 1,
                      "maximum": 30
                    }
                  },
                  "required": ["query", "max_results"],
                  "additionalProperties": false
                }
                """),
            new AgentToolDefinition(
                "read_file",
                "Read a bounded line range from one allowed project text file. Paths are relative to the project.",
                """
                {
                  "type": "object",
                  "properties": {
                    "path": {
                      "type": "string"
                    },
                    "start_line": {
                      "type": "integer",
                      "minimum": 1
                    },
                    "line_count": {
                      "type": "integer",
                      "minimum": 1,
                      "maximum": 400
                    }
                  },
                  "required": ["path", "start_line", "line_count"],
                  "additionalProperties": false
                }
                """),
            new AgentToolDefinition(
                "validate_blueprint",
                "Validate a complete candidate for the conversation's bound Blueprint without applying it.",
                """
                {
                  "type": "object",
                  "properties": {
                    "candidate_json": {
                      "type": "string",
                      "description": "A complete Blueprint JSON object encoded as a JSON string."
                    }
                  },
                  "required": ["candidate_json"],
                  "additionalProperties": false
                }
                """),
            new AgentToolDefinition(
                "propose_blueprint_patch",
                "Create a pending patch proposal for the bound Blueprint. patch_json is not RFC 6902. It is an encoded array of updateNode, updateLink, setStartNode, replaceEventGraph, or setAttrs operations exactly as defined by the system prompt. This never applies the change.",
                """
                {
                  "type": "object",
                  "properties": {
                    "patch_json": {
                      "type": "string",
                      "description": "A non-empty Blueprint patch array encoded as a JSON string."
                    }
                  },
                  "required": ["patch_json"],
                  "additionalProperties": false
                }
                """),
            new AgentToolDefinition(
                "propose_blueprint_replace",
                "Create a pending, validated full replacement proposal for the bound Blueprint. This never applies the change.",
                """
                {
                  "type": "object",
                  "properties": {
                    "blueprint_json": {
                      "type": "string",
                      "description": "A complete replacement Blueprint object encoded as a JSON string."
                    }
                  },
                  "required": ["blueprint_json"],
                  "additionalProperties": false
                }
                """),
        };
    }
}
