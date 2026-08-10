using Ludork.Plugins.OfficialBlueprintAI.Agent;
using System;
using System.Collections.Generic;
using System.IO;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Plugins.OfficialBlueprintAI.Providers;

internal sealed class OpenAIResponsesSession : IAgentTransportSession
{
    private readonly HttpClient httpClient;
    private readonly string model;
    private readonly Uri responsesEndpoint;
    private readonly string organization;
    private readonly string project;
    private readonly string apiKey;
    private readonly string instructions;
    private readonly List<JsonNode> inputItems = [];

    public OpenAIResponsesSession(
        string model,
        string endpoint,
        string organization,
        string project,
        string apiKey,
        string instructions)
        : this(
            model,
            endpoint,
            organization,
            project,
            apiKey,
            instructions,
            ProviderHttp.CreateDefaultHandler())
    {
    }

    internal OpenAIResponsesSession(
        string model,
        string endpoint,
        string organization,
        string project,
        string apiKey,
        string instructions,
        HttpMessageHandler handler)
    {
        this.model = model;
        responsesEndpoint = CreateResponsesEndpoint(endpoint);
        this.organization = organization;
        this.project = project;
        this.apiKey = apiKey;
        this.instructions = instructions;
        httpClient = new HttpClient(handler)
        {
            Timeout = TimeSpan.FromMinutes(2),
        };
    }

    public async IAsyncEnumerable<AgentProviderEvent> StreamAsync(
        IReadOnlyList<AgentInput> input,
        IReadOnlyList<AgentToolDefinition> tools,
        [System.Runtime.CompilerServices.EnumeratorCancellation]
        CancellationToken cancellationToken)
    {
        AddInput(input);
        JsonObject body = CreateRequestBody(tools);
        using HttpRequestMessage request = new HttpRequestMessage(
            HttpMethod.Post,
            responsesEndpoint);
        request.Headers.Authorization = new AuthenticationHeaderValue(
            "Bearer",
            apiKey);
        request.Headers.Accept.Add(
            new MediaTypeWithQualityHeaderValue("text/event-stream"));
        if (!string.IsNullOrWhiteSpace(organization))
            request.Headers.TryAddWithoutValidation("OpenAI-Organization", organization);
        if (!string.IsNullOrWhiteSpace(project))
            request.Headers.TryAddWithoutValidation("OpenAI-Project", project);
        request.Content = new StringContent(
            body.ToJsonString(),
            Encoding.UTF8,
            "application/json");

        using HttpResponseMessage response = await ProviderHttp.SendAsync(
            httpClient,
            request,
            "OpenAI request failed",
            apiKey,
            cancellationToken);
        if (!response.IsSuccessStatusCode)
        {
            string errorBody = await response.Content.ReadAsStringAsync(
                cancellationToken);
            throw new InvalidOperationException(
                $"OpenAI returned HTTP {(int)response.StatusCode}: " +
                SensitiveDataSanitizer.Redact(
                    ProviderHttp.TruncateError(errorBody),
                    apiKey));
        }

        await using Stream stream =
            await response.Content.ReadAsStreamAsync(cancellationToken);
        using StreamReader reader = new StreamReader(stream, Encoding.UTF8);
        bool completed = false;
        while (true)
        {
            string? line = await ProviderHttp.ReadLineAsync(
                reader,
                "OpenAI response stream failed",
                apiKey,
                cancellationToken);
            if (line is null)
                break;
            if (!line.StartsWith("data:", StringComparison.Ordinal))
                continue;

            string payload = line[5..].Trim();
            if (payload.Length == 0)
                continue;
            if (string.Equals(payload, "[DONE]", StringComparison.Ordinal))
                break;

            using JsonDocument document = JsonDocument.Parse(payload);
            JsonElement root = document.RootElement;
            string eventType = getString(root, "type");
            if (string.Equals(
                    eventType,
                    "response.output_text.delta",
                    StringComparison.Ordinal))
            {
                string delta = getString(root, "delta");
                if (delta.Length != 0)
                    yield return AgentProviderEvent.TextDelta(delta);
                continue;
            }
            if (string.Equals(eventType, "error", StringComparison.Ordinal))
            {
                throw new InvalidOperationException(
                    "OpenAI response failed: " +
                    SensitiveDataSanitizer.Redact(
                        getErrorMessage(root),
                        apiKey));
            }
            if (string.Equals(eventType, "response.failed", StringComparison.Ordinal))
            {
                JsonElement responseElement = getObject(root, "response");
                throw new InvalidOperationException(
                    "OpenAI response failed: " +
                    SensitiveDataSanitizer.Redact(
                        getErrorMessage(responseElement),
                        apiKey));
            }
            if (string.Equals(
                    eventType,
                    "response.incomplete",
                    StringComparison.Ordinal))
            {
                JsonElement responseElement = getObject(root, "response");
                JsonElement details = getObject(
                    responseElement,
                    "incomplete_details");
                string reason = getString(details, "reason");
                throw new InvalidOperationException(
                    $"OpenAI response was incomplete: " +
                    (reason.Length == 0 ? "unknown" : reason) +
                    ".");
            }
            if (!string.Equals(
                    eventType,
                    "response.completed",
                    StringComparison.Ordinal))
            {
                continue;
            }

            JsonElement completedResponse = getObject(root, "response");
            if (completedResponse.TryGetProperty(
                    "output",
                    out JsonElement output) &&
                output.ValueKind == JsonValueKind.Array)
            {
                foreach (JsonElement item in output.EnumerateArray())
                {
                    JsonNode? storedItem = JsonNode.Parse(item.GetRawText());
                    if (storedItem is not null)
                        inputItems.Add(storedItem);
                    if (!string.Equals(
                            getString(item, "type"),
                            "function_call",
                            StringComparison.Ordinal))
                    {
                        continue;
                    }
                    yield return AgentProviderEvent.ToolCallReady(
                        new AgentToolCall(
                            getString(item, "call_id"),
                            getString(item, "name"),
                            getString(item, "arguments")));
                }
            }
            completed = true;
        }

        if (!completed)
        {
            throw new InvalidOperationException(
                "OpenAI response stream ended before completion.");
        }
        yield return AgentProviderEvent.Completed();
    }

    public ValueTask DisposeAsync()
    {
        httpClient.Dispose();
        return ValueTask.CompletedTask;
    }

    private void AddInput(IReadOnlyList<AgentInput> input)
    {
        foreach (AgentInput item in input)
        {
            JsonObject inputItem;
            if (item.Role == AgentInputRole.Tool)
            {
                inputItem = new JsonObject
                {
                    ["type"] = "function_call_output",
                    ["call_id"] = item.CallId,
                    ["output"] = item.Content,
                };
            }
            else
            {
                string role = item.Role switch
                {
                    AgentInputRole.Developer => "developer",
                    AgentInputRole.User => "user",
                    AgentInputRole.Assistant => "assistant",
                    _ => throw new InvalidOperationException(
                        $"Unsupported agent input role {item.Role}."),
                };
                inputItem = new JsonObject
                {
                    ["role"] = role,
                    ["content"] = item.Content,
                };
            }
            inputItems.Add(inputItem);
        }
    }

    private JsonObject CreateRequestBody(
        IReadOnlyList<AgentToolDefinition> tools)
    {
        JsonArray serializedInput = [];
        foreach (JsonNode inputItem in inputItems)
            serializedInput.Add(inputItem.DeepClone());

        JsonArray serializedTools = [];
        foreach (AgentToolDefinition tool in tools)
        {
            JsonNode parameters = JsonNode.Parse(tool.ParametersJson) ??
                throw new InvalidDataException(
                    $"Tool schema for {tool.Name} is empty.");
            serializedTools.Add(new JsonObject
            {
                ["type"] = "function",
                ["name"] = tool.Name,
                ["description"] = tool.Description,
                ["parameters"] = parameters,
                ["strict"] = true,
            });
        }

        JsonObject body = new JsonObject
        {
            ["model"] = model,
            ["instructions"] = instructions,
            ["input"] = serializedInput,
            ["stream"] = true,
            ["store"] = false,
            ["parallel_tool_calls"] = false,
        };
        if (serializedTools.Count != 0)
        {
            body["tools"] = serializedTools;
            body["tool_choice"] = "auto";
            body["max_tool_calls"] = 1;
        }
        return body;
    }

    private static Uri CreateResponsesEndpoint(string endpoint)
    {
        Uri baseUri = new Uri(endpoint.TrimEnd('/') + "/", UriKind.Absolute);
        if (baseUri.AbsolutePath.EndsWith(
                "/responses/",
                StringComparison.OrdinalIgnoreCase))
        {
            return new Uri(baseUri.ToString().TrimEnd('/'), UriKind.Absolute);
        }
        return new Uri(baseUri, "responses");
    }

    private static JsonElement getObject(JsonElement value, string propertyName)
    {
        return value.ValueKind == JsonValueKind.Object &&
            value.TryGetProperty(propertyName, out JsonElement property) &&
            property.ValueKind == JsonValueKind.Object
                ? property
                : default;
    }

    private static string getString(JsonElement value, string propertyName)
    {
        return value.ValueKind == JsonValueKind.Object &&
            value.TryGetProperty(propertyName, out JsonElement property) &&
            property.ValueKind == JsonValueKind.String
                ? property.GetString() ?? string.Empty
                : string.Empty;
    }

    private static string getErrorMessage(JsonElement value)
    {
        string message = getString(value, "message");
        if (message.Length != 0)
            return message;
        JsonElement error = getObject(value, "error");
        message = getString(error, "message");
        return message.Length == 0 ? "Unknown provider error." : message;
    }
}
