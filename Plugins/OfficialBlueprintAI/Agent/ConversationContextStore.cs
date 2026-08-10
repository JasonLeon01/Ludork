using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;
using Ludork.Plugin.Abstractions;

namespace Ludork.Plugins.OfficialBlueprintAI.Agent;

internal sealed record ConversationSummaryState(
    int SchemaVersion,
    int SourceMessageCount,
    string SourceFingerprint,
    string Summary);

internal sealed class ConversationContextStore
{
    private const int CurrentSchemaVersion = 1;
    private const int RecentMessageCount = 20;
    private const int SummaryMessageCount = 36;
    private const int SummaryMessageCharacterLimit = 320;

    private static readonly JsonSerializerOptions JsonOptions = new JsonSerializerOptions
    {
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
        WriteIndented = true,
    };

    private readonly string summaryDirectory;

    public ConversationContextStore(string pluginDataDirectory)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(pluginDataDirectory);
        summaryDirectory = Path.Combine(pluginDataDirectory, "summaries");
    }

    public async Task<IReadOnlyList<AgentInput>> BuildAsync(
        string conversationId,
        string projectPath,
        string blueprintKey,
        IReadOnlyList<BlueprintAssistantMessage> messages,
        CancellationToken cancellationToken)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(conversationId);
        ArgumentException.ThrowIfNullOrWhiteSpace(projectPath);
        ArgumentException.ThrowIfNullOrWhiteSpace(blueprintKey);
        ArgumentNullException.ThrowIfNull(messages);

        List<AgentInput> input = new List<AgentInput>();
        int recentStart = Math.Max(0, messages.Count - RecentMessageCount);
        if (recentStart > 0)
        {
            IReadOnlyList<BlueprintAssistantMessage> olderMessages =
                messages.Take(recentStart).ToList();
            string summary = await GetSummaryAsync(
                conversationId,
                projectPath,
                blueprintKey,
                olderMessages,
                cancellationToken);
            input.Add(AgentInput.Developer(summary));
        }

        for (int index = recentStart; index < messages.Count; index++)
        {
            BlueprintAssistantMessage message = messages[index];
            AgentInput converted =
                message.Role == BlueprintAssistantMessageRole.User
                    ? AgentInput.User(message.Content)
                    : AgentInput.Assistant(message.Content);
            input.Add(converted);
        }

        return input;
    }

    private async Task<string> GetSummaryAsync(
        string conversationId,
        string projectPath,
        string blueprintKey,
        IReadOnlyList<BlueprintAssistantMessage> messages,
        CancellationToken cancellationToken)
    {
        string identity = $"{projectPath}\0{blueprintKey}\0{conversationId}";
        string fileName = $"{Convert.ToHexString(SHA256.HashData(
            Encoding.UTF8.GetBytes(identity))).ToLowerInvariant()}.json";
        string path = Path.Combine(summaryDirectory, fileName);
        string fingerprint = CreateFingerprint(messages);

        if (File.Exists(path))
        {
            await using FileStream readStream = new FileStream(
                path,
                FileMode.Open,
                FileAccess.Read,
                FileShare.Read,
                4096,
                FileOptions.Asynchronous | FileOptions.SequentialScan);
            ConversationSummaryState? current =
                await JsonSerializer.DeserializeAsync<ConversationSummaryState>(
                    readStream,
                    JsonOptions,
                    cancellationToken);
            if (current is not null &&
                current.SchemaVersion == CurrentSchemaVersion &&
                current.SourceMessageCount == messages.Count &&
                string.Equals(
                    current.SourceFingerprint,
                    fingerprint,
                    StringComparison.Ordinal))
            {
                return current.Summary;
            }
        }

        string summary = CreateSummary(messages);
        ConversationSummaryState state = new ConversationSummaryState(
            CurrentSchemaVersion,
            messages.Count,
            fingerprint,
            summary);
        await SaveAsync(path, state, cancellationToken);
        return summary;
    }

    private static string CreateSummary(
        IReadOnlyList<BlueprintAssistantMessage> messages)
    {
        int start = Math.Max(0, messages.Count - SummaryMessageCount);
        StringBuilder builder = new StringBuilder();
        builder.AppendLine("Earlier conversation summary:");
        if (start > 0)
        {
            builder.AppendLine(
                $"{start} earlier messages were compacted out of the model context.");
        }

        for (int index = start; index < messages.Count; index++)
        {
            BlueprintAssistantMessage message = messages[index];
            string role = message.Role == BlueprintAssistantMessageRole.User
                ? "User"
                : "Assistant";
            string content = NormalizeContent(message.Content);
            builder.Append(role);
            builder.Append(": ");
            builder.AppendLine(content);
        }

        builder.AppendLine(
            "This summary is context only. Re-read current project data before proposing changes.");
        return builder.ToString();
    }

    private static string NormalizeContent(string content)
    {
        string normalized = content
            .Replace("\r\n", "\n", StringComparison.Ordinal)
            .Replace('\r', '\n')
            .Trim();
        if (normalized.Length <= SummaryMessageCharacterLimit)
        {
            return normalized;
        }

        int omitted = normalized.Length - SummaryMessageCharacterLimit;
        return $"{normalized[..SummaryMessageCharacterLimit]}… [{omitted} characters omitted]";
    }

    private static string CreateFingerprint(
        IReadOnlyList<BlueprintAssistantMessage> messages)
    {
        using IncrementalHash hash = IncrementalHash.CreateHash(HashAlgorithmName.SHA256);
        foreach (BlueprintAssistantMessage message in messages)
        {
            byte[] content = Encoding.UTF8.GetBytes(
                $"{(int)message.Role}\0{message.CreatedAt:O}\0{message.Content}\0");
            hash.AppendData(content);
        }

        return Convert.ToHexString(hash.GetHashAndReset()).ToLowerInvariant();
    }

    private static async Task SaveAsync(
        string path,
        ConversationSummaryState state,
        CancellationToken cancellationToken)
    {
        string? directory = Path.GetDirectoryName(path);
        if (string.IsNullOrWhiteSpace(directory))
        {
            throw new InvalidOperationException("Conversation summary path has no directory.");
        }

        Directory.CreateDirectory(directory);
        string temporaryPath = Path.Combine(
            directory,
            $".summary-{Guid.NewGuid():N}.tmp");
        bool moved = false;
        try
        {
            await using (FileStream stream = new FileStream(
                temporaryPath,
                FileMode.CreateNew,
                FileAccess.Write,
                FileShare.None,
                4096,
                FileOptions.Asynchronous | FileOptions.WriteThrough))
            {
                await JsonSerializer.SerializeAsync(
                    stream,
                    state,
                    JsonOptions,
                    cancellationToken);
                await stream.FlushAsync(cancellationToken);
            }

            File.Move(temporaryPath, path, true);
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
}
