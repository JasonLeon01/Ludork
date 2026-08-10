using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;
using System.Threading;
using System.Threading.Tasks;
using Ludork.Plugins.OfficialBlueprintAI.Agent;

namespace Ludork.Plugins.OfficialBlueprintAI.Providers;

internal sealed class OpenAICompatibleChatSession : IAgentTransportSession
{
    private readonly HttpClient httpClient;
    private readonly string model;
    private readonly Uri chatEndpoint;
    private readonly string apiKey;
    private readonly List<JsonObject> messages = new List<JsonObject>();

    public OpenAICompatibleChatSession(
        string model,
        string endpoint,
        string apiKey,
        string instructions)
        : this(
            model,
            endpoint,
            apiKey,
            instructions,
            ProviderHttp.CreateDefaultHandler())
    {
    }

    internal OpenAICompatibleChatSession(
        string model,
        string endpoint,
        string apiKey,
        string instructions,
        HttpMessageHandler handler)
    {
        this.model = model;
        chatEndpoint = CreateChatEndpoint(endpoint);
        this.apiKey = apiKey;
        httpClient = new HttpClient(handler)
        {
            Timeout = TimeSpan.FromMinutes(2),
        };
        messages.Add(new JsonObject
        {
            ["role"] = "system",
            ["content"] = instructions,
        });
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
            chatEndpoint);
        request.Headers.Authorization = new AuthenticationHeaderValue(
            "Bearer",
            apiKey);
        request.Headers.Accept.Add(
            new MediaTypeWithQualityHeaderValue("text/event-stream"));
        request.Content = new StringContent(
            body.ToJsonString(),
            Encoding.UTF8,
            "application/json");

        using HttpResponseMessage response = await ProviderHttp.SendAsync(
            httpClient,
            request,
            "AI provider request failed",
            apiKey,
            cancellationToken);
        if (!response.IsSuccessStatusCode)
        {
            string errorBody = await response.Content.ReadAsStringAsync(
                cancellationToken);
            throw new InvalidOperationException(
                $"AI provider returned HTTP {(int)response.StatusCode}: " +
                SensitiveDataSanitizer.Redact(
                    ProviderHttp.TruncateError(errorBody),
                    apiKey));
        }

        await using Stream stream =
            await response.Content.ReadAsStreamAsync(cancellationToken);
        using StreamReader reader = new StreamReader(stream, Encoding.UTF8);
        StringBuilder assistantText = new StringBuilder();
        SortedDictionary<int, CompatibleToolCallBuilder> toolCalls =
            new SortedDictionary<int, CompatibleToolCallBuilder>();

        while (true)
        {
            string? line = await ProviderHttp.ReadLineAsync(
                reader,
                "AI provider stream failed",
                apiKey,
                cancellationToken);
            if (line is null)
            {
                break;
            }
            if (!line.StartsWith("data:", StringComparison.Ordinal))
            {
                continue;
            }

            string payload = line[5..].Trim();
            if (string.Equals(payload, "[DONE]", StringComparison.Ordinal))
            {
                break;
            }
            if (payload.Length == 0)
            {
                continue;
            }

            using JsonDocument document = JsonDocument.Parse(payload);
            JsonElement root = document.RootElement;
            if (root.TryGetProperty("error", out JsonElement error))
            {
                string message = error.TryGetProperty(
                    "message",
                    out JsonElement messageElement)
                    ? messageElement.GetString() ?? "Unknown provider error."
                    : "Unknown provider error.";
                throw new InvalidOperationException(
                    $"AI provider response failed: " +
                    SensitiveDataSanitizer.Redact(message, apiKey));
            }
            if (!root.TryGetProperty("choices", out JsonElement choices) ||
                choices.ValueKind != JsonValueKind.Array ||
                choices.GetArrayLength() == 0)
            {
                continue;
            }

            JsonElement choice = choices[0];
            if (!choice.TryGetProperty("delta", out JsonElement delta) ||
                delta.ValueKind != JsonValueKind.Object)
            {
                continue;
            }
            if (delta.TryGetProperty("content", out JsonElement content) &&
                content.ValueKind == JsonValueKind.String)
            {
                string text = content.GetString() ?? string.Empty;
                if (text.Length > 0)
                {
                    assistantText.Append(text);
                    yield return AgentProviderEvent.TextDelta(text);
                }
            }
            if (delta.TryGetProperty("tool_calls", out JsonElement calls) &&
                calls.ValueKind == JsonValueKind.Array)
            {
                AddToolCallDeltas(calls, toolCalls);
            }
        }

        JsonObject assistantMessage = new JsonObject
        {
            ["role"] = "assistant",
            ["content"] = assistantText.Length == 0
                ? null
                : assistantText.ToString(),
        };
        if (toolCalls.Count > 0)
        {
            JsonArray serializedCalls = new JsonArray();
            foreach (CompatibleToolCallBuilder builder in toolCalls.Values)
            {
                serializedCalls.Add(builder.ToMessageNode());
            }
            assistantMessage["tool_calls"] = serializedCalls;
        }
        messages.Add(assistantMessage);

        foreach (CompatibleToolCallBuilder builder in toolCalls.Values)
        {
            yield return AgentProviderEvent.ToolCallReady(builder.Build());
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
            JsonObject message;
            if (item.Role == AgentInputRole.Tool)
            {
                message = new JsonObject
                {
                    ["role"] = "tool",
                    ["tool_call_id"] = item.CallId,
                    ["content"] = item.Content,
                };
            }
            else
            {
                string role = item.Role switch
                {
                    AgentInputRole.Developer => "system",
                    AgentInputRole.User => "user",
                    AgentInputRole.Assistant => "assistant",
                    _ => throw new InvalidOperationException(
                        $"Unsupported agent input role {item.Role}."),
                };
                message = new JsonObject
                {
                    ["role"] = role,
                    ["content"] = item.Content,
                };
            }

            messages.Add(message);
        }
    }

    private JsonObject CreateRequestBody(
        IReadOnlyList<AgentToolDefinition> tools)
    {
        JsonArray serializedMessages = new JsonArray();
        foreach (JsonObject message in messages)
        {
            serializedMessages.Add(message.DeepClone());
        }

        JsonArray serializedTools = new JsonArray();
        foreach (AgentToolDefinition tool in tools)
        {
            JsonNode parameters = JsonNode.Parse(tool.ParametersJson) ??
                throw new InvalidDataException(
                    $"Tool schema for {tool.Name} is empty.");
            serializedTools.Add(new JsonObject
            {
                ["type"] = "function",
                ["function"] = new JsonObject
                {
                    ["name"] = tool.Name,
                    ["description"] = tool.Description,
                    ["parameters"] = parameters,
                    ["strict"] = true,
                },
            });
        }

        JsonObject body = new JsonObject
        {
            ["model"] = model,
            ["messages"] = serializedMessages,
            ["stream"] = true,
        };
        if (serializedTools.Count > 0)
        {
            body["tools"] = serializedTools;
            body["tool_choice"] = "auto";
            body["parallel_tool_calls"] = false;
        }

        return body;
    }

    private static void AddToolCallDeltas(
        JsonElement calls,
        IDictionary<int, CompatibleToolCallBuilder> builders)
    {
        foreach (JsonElement call in calls.EnumerateArray())
        {
            if (!call.TryGetProperty("index", out JsonElement indexElement) ||
                !indexElement.TryGetInt32(out int index))
            {
                continue;
            }
            if (!builders.TryGetValue(index, out CompatibleToolCallBuilder? builder))
            {
                builder = new CompatibleToolCallBuilder(index);
                builders[index] = builder;
            }

            builder.Add(call);
        }
    }

    private static Uri CreateChatEndpoint(string endpoint)
    {
        Uri baseUri = new Uri(endpoint.TrimEnd('/') + "/", UriKind.Absolute);
        if (baseUri.AbsolutePath.EndsWith(
            "/chat/completions/",
            StringComparison.OrdinalIgnoreCase))
        {
            return new Uri(baseUri.ToString().TrimEnd('/'), UriKind.Absolute);
        }

        return new Uri(baseUri, "chat/completions");
    }

    private sealed class CompatibleToolCallBuilder
    {
        private readonly int index;
        private readonly StringBuilder arguments = new StringBuilder();
        private string id = string.Empty;
        private string name = string.Empty;

        public CompatibleToolCallBuilder(int index)
        {
            this.index = index;
        }

        public void Add(JsonElement call)
        {
            if (call.TryGetProperty("id", out JsonElement id) &&
                id.ValueKind == JsonValueKind.String &&
                !string.IsNullOrEmpty(id.GetString()))
            {
                this.id = id.GetString() ?? string.Empty;
            }
            if (!call.TryGetProperty("function", out JsonElement function) ||
                function.ValueKind != JsonValueKind.Object)
            {
                return;
            }
            if (function.TryGetProperty("name", out JsonElement name) &&
                name.ValueKind == JsonValueKind.String &&
                !string.IsNullOrEmpty(name.GetString()))
            {
                this.name = name.GetString() ?? string.Empty;
            }
            if (function.TryGetProperty("arguments", out JsonElement arguments) &&
                arguments.ValueKind == JsonValueKind.String)
            {
                this.arguments.Append(arguments.GetString());
            }
        }

        public AgentToolCall Build()
        {
            string callId = string.IsNullOrWhiteSpace(id)
                ? $"tool-call-{index}"
                : id;
            return new AgentToolCall(
                callId,
                name,
                arguments.ToString());
        }

        public JsonObject ToMessageNode()
        {
            AgentToolCall call = Build();
            return new JsonObject
            {
                ["id"] = call.Id,
                ["type"] = "function",
                ["function"] = new JsonObject
                {
                    ["name"] = call.Name,
                    ["arguments"] = call.ArgumentsJson,
                },
            };
        }
    }
}
