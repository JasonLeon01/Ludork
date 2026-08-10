using Ludork.Plugin.Abstractions;
using System;
using System.Collections.Generic;
using System.Collections.Concurrent;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Plugins.OfficialBlueprintAI.History;

public sealed record BlueprintAssistantConversationSummary(
    string Id,
    string Title,
    string ProjectPath,
    string BlueprintKey,
    DateTimeOffset CreatedAt,
    DateTimeOffset UpdatedAt);

public sealed record BlueprintAssistantConversationEntry(
    string Type,
    BlueprintAssistantMessageRole? Role,
    string Content,
    DateTimeOffset CreatedAt,
    string ProposalId,
    string ProposalTitle,
    string ProposalDiff,
    string ProposalCandidate,
    string ProposalBaseRevision,
    bool ProposalValid,
    IReadOnlyList<string> Diagnostics,
    string ToolName,
    string Action,
    string Provider = "",
    string Model = "",
    string Endpoint = "",
    string BaseRevision = "");

public sealed class BlueprintAssistantConversationStore
{
    private static readonly ConcurrentDictionary<string, SemaphoreSlim>
        WriteLocks = new ConcurrentDictionary<string, SemaphoreSlim>(
            OperatingSystem.IsWindows()
                ? StringComparer.OrdinalIgnoreCase
                : StringComparer.Ordinal);
    private static readonly JsonSerializerOptions JsonOptions = new()
    {
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
        WriteIndented = true,
        DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull,
    };
    private static readonly JsonSerializerOptions JsonLineOptions = new()
    {
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
        DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull,
    };
    private readonly string conversationsDirectory;
    private readonly string summaryDirectory;
    private readonly string indexPath;
    private readonly SemaphoreSlim writeLock;

    public BlueprintAssistantConversationStore(string pluginDataDirectory)
    {
        string canonicalDataDirectory = Path.GetFullPath(pluginDataDirectory)
            .TrimEnd(
                Path.DirectorySeparatorChar,
                Path.AltDirectorySeparatorChar);
        conversationsDirectory = Path.Combine(
            canonicalDataDirectory,
            "conversations");
        summaryDirectory = Path.Combine(
            canonicalDataDirectory,
            "summaries");
        indexPath = Path.Combine(conversationsDirectory, "index.json");
        writeLock = WriteLocks.GetOrAdd(
            canonicalDataDirectory,
            static _ => new SemaphoreSlim(1, 1));
    }

    public async Task<IReadOnlyList<BlueprintAssistantConversationSummary>> LoadIndexAsync(
        CancellationToken cancellationToken)
    {
        cancellationToken.ThrowIfCancellationRequested();
        if (!File.Exists(indexPath))
            return [];
        await using FileStream stream = File.OpenRead(indexPath);
        ConversationIndex? index = await JsonSerializer.DeserializeAsync<ConversationIndex>(
            stream,
            JsonOptions,
            cancellationToken);
        return index?.Conversations
            .OrderByDescending(conversation => conversation.UpdatedAt)
            .ToArray()
            ?? [];
    }

    public async Task<IReadOnlyList<BlueprintAssistantConversationSummary>>
        LoadProjectIndexAsync(
            string projectPath,
            CancellationToken cancellationToken)
    {
        string canonicalProjectPath = Path.GetFullPath(projectPath);
        StringComparison comparison = OperatingSystem.IsWindows()
            ? StringComparison.OrdinalIgnoreCase
            : StringComparison.Ordinal;
        IReadOnlyList<BlueprintAssistantConversationSummary> conversations =
            await LoadIndexAsync(cancellationToken);
        return conversations
            .Where(conversation => string.Equals(
                Path.GetFullPath(conversation.ProjectPath),
                canonicalProjectPath,
                comparison))
            .ToArray();
    }

    public async Task<BlueprintAssistantConversationSummary> CreateAsync(
        string projectPath,
        string blueprintKey,
        CancellationToken cancellationToken)
    {
        DateTimeOffset now = DateTimeOffset.UtcNow;
        BlueprintAssistantConversationSummary conversation = new(
            Guid.NewGuid().ToString("N"),
            blueprintKey,
            Path.GetFullPath(projectPath),
            blueprintKey,
            now,
            now);
        await mutateIndexAsync(
            conversations => conversations.Add(conversation),
            cancellationToken);
        return conversation;
    }

    public async Task<BlueprintAssistantConversationSummary?> RenameAsync(
        string conversationId,
        string title,
        CancellationToken cancellationToken)
    {
        string value = title.Trim();
        if (value.Length == 0)
            return null;
        BlueprintAssistantConversationSummary? updated = null;
        await mutateIndexAsync(
            conversations =>
            {
                int index = conversations.FindIndex(conversation =>
                    string.Equals(conversation.Id, conversationId, StringComparison.Ordinal));
                if (index < 0)
                    return;
                BlueprintAssistantConversationSummary existing = conversations[index];
                updated = existing with
                {
                    Title = value,
                    UpdatedAt = DateTimeOffset.UtcNow,
                };
                conversations[index] = updated;
            },
            cancellationToken);
        return updated;
    }

    public async Task DeleteAsync(
        string conversationId,
        CancellationToken cancellationToken)
    {
        BlueprintAssistantConversationSummary? deletedConversation = null;
        await mutateIndexAsync(
            conversations =>
            {
                deletedConversation = conversations.FirstOrDefault(
                    conversation => string.Equals(
                        conversation.Id,
                        conversationId,
                        StringComparison.Ordinal));
                conversations.RemoveAll(conversation =>
                    string.Equals(
                        conversation.Id,
                        conversationId,
                        StringComparison.Ordinal));
            },
            cancellationToken);
        string path = getConversationPath(conversationId);
        if (File.Exists(path))
            File.Delete(path);
        if (deletedConversation is not null)
        {
            string summaryPath = getSummaryPath(deletedConversation);
            if (File.Exists(summaryPath))
                File.Delete(summaryPath);
        }
    }

    public async Task<IReadOnlyList<BlueprintAssistantConversationEntry>> LoadEntriesAsync(
        string conversationId,
        CancellationToken cancellationToken)
    {
        string path = getConversationPath(conversationId);
        if (!File.Exists(path))
            return [];
        await writeLock.WaitAsync(cancellationToken);
        try
        {
            List<BlueprintAssistantConversationEntry> result = [];
            List<string> validLines = [];
            bool corrupted = false;
            using (StreamReader reader = File.OpenText(path))
            {
                while (await reader.ReadLineAsync(cancellationToken) is string line)
                {
                    if (line.Length == 0)
                        continue;
                    try
                    {
                        BlueprintAssistantConversationEntry? entry =
                            JsonSerializer.Deserialize<BlueprintAssistantConversationEntry>(
                                line,
                                JsonLineOptions);
                        if (entry is null)
                        {
                            corrupted = true;
                            break;
                        }
                        result.Add(entry);
                        validLines.Add(line);
                    }
                    catch (JsonException)
                    {
                        corrupted = true;
                        break;
                    }
                }
            }
            if (corrupted)
            {
                string repairPath =
                    path + $".{Guid.NewGuid():N}.repair";
                bool moved = false;
                try
                {
                    await File.WriteAllLinesAsync(
                        repairPath,
                        validLines,
                        cancellationToken);
                    File.Move(repairPath, path, true);
                    moved = true;
                }
                finally
                {
                    if (!moved && File.Exists(repairPath))
                    {
                        File.Delete(repairPath);
                    }
                }
            }
            return result;
        }
        finally
        {
            writeLock.Release();
        }
    }

    public Task AppendMessageAsync(
        string conversationId,
        BlueprintAssistantMessage message,
        CancellationToken cancellationToken)
    {
        BlueprintAssistantConversationEntry entry = new(
            "message",
            message.Role,
            message.Content,
            message.CreatedAt,
            string.Empty,
            string.Empty,
            string.Empty,
            string.Empty,
            string.Empty,
            false,
            [],
            string.Empty,
            string.Empty);
        return appendAsync(conversationId, entry, cancellationToken);
    }

    public Task AppendTurnStartedAsync(
        string conversationId,
        string provider,
        string model,
        string endpoint,
        string baseRevision,
        CancellationToken cancellationToken)
    {
        BlueprintAssistantConversationEntry entry = new(
            "turnStarted",
            null,
            string.Empty,
            DateTimeOffset.UtcNow,
            string.Empty,
            string.Empty,
            string.Empty,
            string.Empty,
            string.Empty,
            false,
            [],
            string.Empty,
            string.Empty,
            provider,
            model,
            endpoint,
            baseRevision);
        return appendAsync(conversationId, entry, cancellationToken);
    }

    public Task AppendProposalAsync(
        string conversationId,
        BlueprintAssistantProposal proposal,
        string candidateJson,
        CancellationToken cancellationToken)
    {
        BlueprintAssistantConversationEntry entry = new(
            "proposal",
            null,
            string.Empty,
            DateTimeOffset.UtcNow,
            proposal.Id,
            proposal.Title,
            proposal.Diff,
            candidateJson,
            proposal.BaseRevision,
            proposal.IsValid,
            proposal.Diagnostics,
            string.Empty,
            string.Empty);
        return appendAsync(conversationId, entry, cancellationToken);
    }

    public Task AppendProposalActionAsync(
        string conversationId,
        string proposalId,
        string action,
        CancellationToken cancellationToken)
    {
        BlueprintAssistantConversationEntry entry = new(
            "proposalAction",
            null,
            string.Empty,
            DateTimeOffset.UtcNow,
            proposalId,
            string.Empty,
            string.Empty,
            string.Empty,
            string.Empty,
            false,
            [],
            string.Empty,
            action);
        return appendAsync(conversationId, entry, cancellationToken);
    }

    public Task AppendInterruptedAsync(
        string conversationId,
        CancellationToken cancellationToken)
    {
        BlueprintAssistantConversationEntry entry = new(
            "interrupted",
            null,
            string.Empty,
            DateTimeOffset.UtcNow,
            string.Empty,
            string.Empty,
            string.Empty,
            string.Empty,
            string.Empty,
            false,
            [],
            string.Empty,
            "interrupted");
        return appendAsync(conversationId, entry, cancellationToken);
    }

    public Task AppendFailedAsync(
        string conversationId,
        string error,
        CancellationToken cancellationToken)
    {
        BlueprintAssistantConversationEntry entry = new(
            "failed",
            null,
            BlueprintAssistantText.SanitizeError(error),
            DateTimeOffset.UtcNow,
            string.Empty,
            string.Empty,
            string.Empty,
            string.Empty,
            string.Empty,
            false,
            [],
            string.Empty,
            "failed");
        return appendAsync(conversationId, entry, cancellationToken);
    }

    public Task AppendToolAuditAsync(
        string conversationId,
        string toolName,
        string status,
        bool started,
        CancellationToken cancellationToken)
    {
        BlueprintAssistantConversationEntry entry = new(
            started ? "toolStarted" : "toolCompleted",
            null,
            BlueprintAssistantText.SanitizeError(status),
            DateTimeOffset.UtcNow,
            string.Empty,
            string.Empty,
            string.Empty,
            string.Empty,
            string.Empty,
            false,
            [],
            toolName,
            string.Empty);
        return appendAsync(conversationId, entry, cancellationToken);
    }

    public Task AppendTurnCompletedAsync(
        string conversationId,
        CancellationToken cancellationToken)
    {
        BlueprintAssistantConversationEntry entry = new(
            "turnCompleted",
            null,
            string.Empty,
            DateTimeOffset.UtcNow,
            string.Empty,
            string.Empty,
            string.Empty,
            string.Empty,
            string.Empty,
            false,
            [],
            string.Empty,
            "completed");
        return appendAsync(conversationId, entry, cancellationToken);
    }

    private async Task appendAsync(
        string conversationId,
        BlueprintAssistantConversationEntry entry,
        CancellationToken cancellationToken)
    {
        await writeLock.WaitAsync(cancellationToken);
        try
        {
            Directory.CreateDirectory(conversationsDirectory);
            string line = JsonSerializer.Serialize(entry, JsonLineOptions);
            await File.AppendAllTextAsync(
                getConversationPath(conversationId),
                line + Environment.NewLine,
                cancellationToken);
            await touchConversationAsync(conversationId, cancellationToken);
        }
        finally
        {
            writeLock.Release();
        }
    }

    private async Task touchConversationAsync(
        string conversationId,
        CancellationToken cancellationToken)
    {
        List<BlueprintAssistantConversationSummary> conversations =
            (await loadIndexUnsafeAsync(cancellationToken)).ToList();
        int index = conversations.FindIndex(conversation =>
            string.Equals(conversation.Id, conversationId, StringComparison.Ordinal));
        if (index >= 0)
        {
            conversations[index] = conversations[index] with
            {
                UpdatedAt = DateTimeOffset.UtcNow,
            };
            await saveIndexUnsafeAsync(conversations, cancellationToken);
        }
    }

    private async Task mutateIndexAsync(
        Action<List<BlueprintAssistantConversationSummary>> mutation,
        CancellationToken cancellationToken)
    {
        await writeLock.WaitAsync(cancellationToken);
        try
        {
            List<BlueprintAssistantConversationSummary> conversations =
                (await loadIndexUnsafeAsync(cancellationToken)).ToList();
            mutation(conversations);
            await saveIndexUnsafeAsync(conversations, cancellationToken);
        }
        finally
        {
            writeLock.Release();
        }
    }

    private async Task<IReadOnlyList<BlueprintAssistantConversationSummary>> loadIndexUnsafeAsync(
        CancellationToken cancellationToken)
    {
        if (!File.Exists(indexPath))
            return [];
        await using FileStream stream = File.OpenRead(indexPath);
        ConversationIndex? index = await JsonSerializer.DeserializeAsync<ConversationIndex>(
            stream,
            JsonOptions,
            cancellationToken);
        return index?.Conversations ?? [];
    }

    private async Task saveIndexUnsafeAsync(
        IReadOnlyList<BlueprintAssistantConversationSummary> conversations,
        CancellationToken cancellationToken)
    {
        Directory.CreateDirectory(conversationsDirectory);
        string temporaryPath = indexPath + $".{Guid.NewGuid():N}.tmp";
        ConversationIndex index = new()
        {
            Conversations = conversations
                .OrderByDescending(conversation => conversation.UpdatedAt)
                .ToList(),
        };
        bool moved = false;
        try
        {
            await using (FileStream stream = File.Create(temporaryPath))
            {
                await JsonSerializer.SerializeAsync(
                    stream,
                    index,
                    JsonOptions,
                    cancellationToken);
                await stream.FlushAsync(cancellationToken);
            }
            File.Move(temporaryPath, indexPath, true);
            moved = true;
        }
        finally
        {
            if (!moved && File.Exists(temporaryPath))
            {
                File.Delete(temporaryPath);
            }
        }
    }

    private string getConversationPath(string conversationId)
    {
        if (!Guid.TryParseExact(conversationId, "N", out Guid id))
            throw new InvalidDataException("Conversation id is invalid.");
        return Path.Combine(conversationsDirectory, id.ToString("N") + ".jsonl");
    }

    private string getSummaryPath(
        BlueprintAssistantConversationSummary conversation)
    {
        string identity =
            $"{conversation.ProjectPath}\0{conversation.BlueprintKey}\0{conversation.Id}";
        string fileName = Convert.ToHexString(SHA256.HashData(
                Encoding.UTF8.GetBytes(identity)))
            .ToLowerInvariant() + ".json";
        return Path.Combine(summaryDirectory, fileName);
    }

    private sealed class ConversationIndex
    {
        public int SchemaVersion { get; set; } = 1;
        public List<BlueprintAssistantConversationSummary> Conversations { get; set; } = [];
    }
}
