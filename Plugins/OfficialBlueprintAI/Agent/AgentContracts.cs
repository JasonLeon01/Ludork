using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;
using System.Threading;
using System.Threading.Tasks;
using Ludork.Plugin.Abstractions;

namespace Ludork.Plugins.OfficialBlueprintAI.Agent;

internal enum AgentInputRole
{
    Developer,
    User,
    Assistant,
    Tool,
}

internal sealed record AgentInput(
    AgentInputRole Role,
    string Content,
    string CallId)
{
    public static AgentInput User(string content)
    {
        return new AgentInput(AgentInputRole.User, content, string.Empty);
    }

    public static AgentInput Developer(string content)
    {
        return new AgentInput(AgentInputRole.Developer, content, string.Empty);
    }

    public static AgentInput Assistant(string content)
    {
        return new AgentInput(AgentInputRole.Assistant, content, string.Empty);
    }

    public static AgentInput Tool(string callId, string content)
    {
        return new AgentInput(AgentInputRole.Tool, content, callId);
    }
}

internal sealed record AgentToolDefinition(
    string Name,
    string Description,
    string ParametersJson);

internal sealed record AgentToolCall(
    string Id,
    string Name,
    string ArgumentsJson);

internal enum AgentProviderEventKind
{
    TextDelta,
    ToolCall,
    Completed,
}

internal sealed record AgentProviderEvent(
    AgentProviderEventKind Kind,
    string Text,
    AgentToolCall? ToolCall)
{
    public static AgentProviderEvent TextDelta(string text)
    {
        return new AgentProviderEvent(
            AgentProviderEventKind.TextDelta,
            text,
            null);
    }

    public static AgentProviderEvent ToolCallReady(AgentToolCall toolCall)
    {
        return new AgentProviderEvent(
            AgentProviderEventKind.ToolCall,
            string.Empty,
            toolCall);
    }

    public static AgentProviderEvent Completed()
    {
        return new AgentProviderEvent(
            AgentProviderEventKind.Completed,
            string.Empty,
            null);
    }
}

internal interface IAgentTransportSession : IAsyncDisposable
{
    IAsyncEnumerable<AgentProviderEvent> StreamAsync(
        IReadOnlyList<AgentInput> input,
        IReadOnlyList<AgentToolDefinition> tools,
        CancellationToken cancellationToken);
}

internal interface IAgentTransportFactory
{
    IAgentTransportSession Create(
        string provider,
        string model,
        string endpoint,
        string organization,
        string project,
        string apiKey,
        string instructions);
}

internal sealed record AgentToolExecution(
    bool Success,
    bool IsReadOnly,
    bool IsFormatError,
    string ModelContent,
    string DisplayText,
    BlueprintAssistantProposal? Proposal);

internal interface IAgentToolExecutor
{
    IReadOnlyList<AgentToolDefinition> Definitions { get; }

    Task<AgentToolExecution> ExecuteAsync(
        AgentToolCall call,
        IBlueprintAssistantWorkspace workspace,
        CancellationToken cancellationToken);
}
