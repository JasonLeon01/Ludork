using Ludork.Plugin.Abstractions;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net.Http;
using System.Text;
using System.Text.Encodings.Web;
using System.Text.Json;
using System.Text.Json.Nodes;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Services.BlueprintAssistant;

internal sealed class BlueprintAssistantDocumentationService
{
    private static readonly HttpClient client = new(new HttpClientHandler { AllowAutoRedirect = false })
    {
        Timeout = TimeSpan.FromSeconds(30),
        MaxResponseContentBufferSize = 16 * 1024 * 1024,
    };
    private static readonly JsonSerializerOptions jsonOptions = new(JsonSerializerDefaults.Web)
    {
        Encoder = JavaScriptEncoder.UnsafeRelaxedJsonEscaping,
    };
    private readonly HttpClient httpClient;
    private readonly string version;
    private readonly string language;
    private readonly SemaphoreSlim indexLock = new(1, 1);
    private DocumentationIndex? index;

    public BlueprintAssistantDocumentationService()
        : this(client, EditorVersionService.DocumentationVersion, LocaleService.CurrentLanguage)
    {
    }

    internal BlueprintAssistantDocumentationService(HttpClient httpClient, string version, string language)
    {
        this.httpClient = httpClient;
        this.version = version;
        this.language = language;
    }

    public Task<BlueprintAssistantToolResult> SearchAsync(
        string query,
        int maxResults,
        CancellationToken cancellationToken)
    {
        if (string.IsNullOrWhiteSpace(query))
            return Task.FromResult(BlueprintAssistantToolResult.Failed("A documentation search query is required."));
        string[] terms = query.Split((char[]?)null, StringSplitOptions.RemoveEmptyEntries);
        return executeAsync(documents => search(documents, terms, Math.Clamp(maxResults, 1, 10)), cancellationToken);
    }

    public Task<BlueprintAssistantToolResult> ReadAsync(
        string documentKey,
        int startLine,
        int lineCount,
        CancellationToken cancellationToken)
    {
        if (string.IsNullOrWhiteSpace(documentKey))
            return Task.FromResult(BlueprintAssistantToolResult.Failed("A documentation key is required."));
        return executeAsync(documents => read(documents, documentKey, Math.Max(1, startLine), Math.Clamp(lineCount, 1, 400)),
            cancellationToken);
    }

    private async Task<BlueprintAssistantToolResult> executeAsync(
        Func<IReadOnlyList<DocumentationPage>, BlueprintAssistantToolResult> action,
        CancellationToken cancellationToken)
    {
        cancellationToken.ThrowIfCancellationRequested();
        try
        {
            await indexLock.WaitAsync(cancellationToken);
            try
            {
                index ??= await loadAsync(cancellationToken);
            }
            finally
            {
                indexLock.Release();
            }
            cancellationToken.ThrowIfCancellationRequested();
            return action(index.Documents);
        }
        catch (OperationCanceledException) when (!cancellationToken.IsCancellationRequested)
        {
            return BlueprintAssistantToolResult.Failed($"Official documentation for Ludork {version} ({language}) timed out.");
        }
        catch (Exception exception) when (exception is HttpRequestException or JsonException or InvalidDataException)
        {
            return BlueprintAssistantToolResult.Failed(
                $"Official documentation for Ludork {version} ({language}) could not be read: {exception.Message}");
        }
    }

    private async Task<DocumentationIndex> loadAsync(CancellationToken cancellationToken)
    {
        using HttpResponseMessage response = await httpClient.GetAsync(
            DocumentationService.GetSearchIndexUri(version, language), cancellationToken);
        response.EnsureSuccessStatusCode();
        string json = await response.Content.ReadAsStringAsync(cancellationToken);
        DocumentationIndex? loaded = JsonSerializer.Deserialize<DocumentationIndex>(json, jsonOptions);
        if (loaded is null || loaded.Version != version || loaded.Language != language
            || loaded.Documents is null || loaded.Documents.Count == 0)
        {
            throw new InvalidDataException("The documentation index does not match the requested version and language.");
        }
        HashSet<string> keys = new(StringComparer.Ordinal);
        foreach (DocumentationPage page in loaded.Documents)
        {
            if (page is null || string.IsNullOrWhiteSpace(page.Key) || !keys.Add(page.Key)
                || string.IsNullOrWhiteSpace(page.Title) || string.IsNullOrWhiteSpace(page.Path) || page.Content is null)
            {
                throw new InvalidDataException("The documentation index contains an invalid or duplicate page.");
            }
        }
        return loaded;
    }

    private BlueprintAssistantToolResult search(IReadOnlyList<DocumentationPage> documents, string[] terms, int limit)
    {
        DocumentationPage[] matches = documents
            .Where(page => terms.All(term => contains(page.Title, term) || contains(page.Path, term) || contains(page.Content, term)))
            .OrderByDescending(page => terms.Count(term => contains(page.Title, term)))
            .ThenByDescending(page => terms.Count(term => contains(page.Path, term)))
            .ToArray();
        JsonArray results = [];
        JsonObject result = new()
        {
            ["version"] = version,
            ["language"] = language,
            ["totalMatches"] = matches.Length,
            ["results"] = results,
        };
        foreach (DocumentationPage page in matches.Take(limit))
        {
            int offset = terms.Select(term => page.Content.IndexOf(term, StringComparison.OrdinalIgnoreCase))
                .Where(position => position >= 0)
                .DefaultIfEmpty(0)
                .Min();
            int first = Math.Max(0, offset - 80);
            results.Add(new JsonObject
            {
                ["documentKey"] = page.Key,
                ["title"] = page.Title,
                ["path"] = page.Path,
                ["url"] = DocumentationService.GetDocumentUri(version, language, page.Key).AbsoluteUri,
                ["line"] = page.Content.AsSpan(0, offset).Count('\n') + 1,
                ["snippet"] = page.Content.Substring(first, Math.Min(320, page.Content.Length - first)).Trim(),
            });
            if (serialize(result).Length > BlueprintAssistantToolResult.MaximumContentCharacters)
            {
                results.RemoveAt(results.Count - 1);
                break;
            }
        }
        return BlueprintAssistantToolResult.Completed(serialize(result));
    }

    private BlueprintAssistantToolResult read(
        IReadOnlyList<DocumentationPage> documents,
        string documentKey,
        int startLine,
        int lineCount)
    {
        DocumentationPage? page = documents.FirstOrDefault(document => document.Key == documentKey);
        if (page is null)
            return BlueprintAssistantToolResult.Failed("The document key was not found in the current version and language. Search documentation first.");
        string[] lines = page.Content.Split('\n');
        JsonObject result = new()
        {
            ["version"] = version,
            ["language"] = language,
            ["documentKey"] = page.Key,
            ["title"] = page.Title,
            ["url"] = DocumentationService.GetDocumentUri(version, language, page.Key).AbsoluteUri,
            ["startLine"] = startLine,
            ["endLine"] = startLine - 1,
            ["totalLines"] = lines.Length,
            ["nextLine"] = null,
            ["truncatedLine"] = false,
            ["content"] = string.Empty,
        };
        string serialized = serialize(result);
        if (serialized.Length > BlueprintAssistantToolResult.MaximumContentCharacters - 100)
            return BlueprintAssistantToolResult.Failed("The document metadata exceeds the tool output limit.");
        StringBuilder content = new();
        for (int line = startLine - 1; line < lines.Length && line - (startLine - 1) < lineCount; line++)
        {
            content.Append(line + 1).Append(": ").Append(lines[line]).Append('\n');
            result["content"] = content.ToString();
            result["endLine"] = line + 1;
            result["nextLine"] = line + 1 < lines.Length ? JsonValue.Create(line + 2) : null;
            string candidate = serialize(result);
            if (candidate.Length > BlueprintAssistantToolResult.MaximumContentCharacters)
            {
                if (line == startLine - 1)
                    serialized = truncateLine(result, content.ToString());
                break;
            }
            serialized = candidate;
        }
        return BlueprintAssistantToolResult.Completed(serialized);
    }

    private static string truncateLine(JsonObject result, string content)
    {
        result["truncatedLine"] = true;
        int low = 0;
        int high = content.Length;
        while (low < high)
        {
            int length = low + (high - low + 1) / 2;
            result["content"] = content[..length] + "...[line truncated]";
            if (serialize(result).Length <= BlueprintAssistantToolResult.MaximumContentCharacters)
                low = length;
            else
                high = length - 1;
        }
        if (low > 0 && char.IsHighSurrogate(content[low - 1]))
            low--;
        result["content"] = content[..low] + "...[line truncated]";
        return serialize(result);
    }

    private static bool contains(string value, string term) => value.Contains(term, StringComparison.OrdinalIgnoreCase);

    private static string serialize(JsonObject value) => value.ToJsonString(jsonOptions);

    private sealed record DocumentationIndex(string Version, string Language, List<DocumentationPage> Documents);

    private sealed record DocumentationPage(string Key, string Title, string Path, string Content);
}
