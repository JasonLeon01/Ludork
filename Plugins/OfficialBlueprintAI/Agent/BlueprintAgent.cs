using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;
using System.Text;
using System.Threading;
using Ludork.Plugin.Abstractions;
using Ludork.Plugins.OfficialBlueprintAI.Configuration;
using Ludork.Plugins.OfficialBlueprintAI.Localization;

namespace Ludork.Plugins.OfficialBlueprintAI.Agent;

internal sealed class BlueprintAgent
{
    private const int MaximumIterations = 30;
    private const int MaximumFormatRetries = 5;
    private const int MaximumConsecutiveReadOnlyTools = 8;

    private readonly IAgentTransportFactory transportFactory;
    private readonly IAgentToolExecutor tools;
    private readonly PluginLocalizer localizer;

    public BlueprintAgent(
        IAgentTransportFactory transportFactory,
        IAgentToolExecutor tools,
        PluginLocalizer localizer)
    {
        this.transportFactory = transportFactory;
        this.tools = tools;
        this.localizer = localizer;
    }

    public async IAsyncEnumerable<BlueprintAssistantEvent> RunAsync(
        AiProviderProfile profile,
        string apiKey,
        string instructions,
        IReadOnlyList<AgentInput> initialInput,
        IBlueprintAssistantWorkspace workspace,
        string formatRetryPrompt,
        string researchLimitPrompt,
        [EnumeratorCancellation] CancellationToken cancellationToken)
    {
        await using IAgentTransportSession session = transportFactory.Create(
            profile.Provider,
            profile.Model,
            profile.Endpoint,
            profile.Organization,
            profile.Project,
            apiKey,
            instructions);

        List<AgentInput> nextInput = new List<AgentInput>(initialInput);
        int formatRetries = 0;
        int consecutiveReadOnlyTools = 0;

        for (int iteration = 1; iteration <= MaximumIterations; iteration++)
        {
            cancellationToken.ThrowIfCancellationRequested();
            yield return BlueprintAssistantEvent.Status(
                localizer.Format(
                    "statusIteration",
                    iteration,
                    MaximumIterations));

            List<AgentToolCall> toolCalls = new List<AgentToolCall>();
            StringBuilder responseText = new StringBuilder();
            await foreach (AgentProviderEvent providerEvent
                in session.StreamAsync(
                    nextInput,
                    tools.Definitions,
                    cancellationToken))
            {
                cancellationToken.ThrowIfCancellationRequested();
                if (providerEvent.Kind == AgentProviderEventKind.TextDelta)
                {
                    responseText.Append(providerEvent.Text);
                }
                else if (
                    providerEvent.Kind == AgentProviderEventKind.ToolCall &&
                    providerEvent.ToolCall is not null)
                {
                    toolCalls.Add(providerEvent.ToolCall);
                }
            }

            nextInput = new List<AgentInput>();
            if (toolCalls.Count == 0)
            {
                if (responseText.Length == 0)
                {
                    formatRetries++;
                    if (formatRetries > MaximumFormatRetries)
                    {
                        throw new InvalidOperationException(
                            localizer.Text("errorNoOutput"));
                    }

                    nextInput.Add(AgentInput.Developer(formatRetryPrompt));
                    yield return BlueprintAssistantEvent.Status(
                        localizer.Format(
                            "statusFormatRetry",
                            formatRetries,
                            MaximumFormatRetries));
                    continue;
                }

                yield return BlueprintAssistantEvent.TextDelta(
                    responseText.ToString());
                yield return BlueprintAssistantEvent.Completed();
                yield break;
            }

            bool requestFormatRetry = false;
            bool requestResearchLimit = false;
            foreach (AgentToolCall toolCall in toolCalls)
            {
                cancellationToken.ThrowIfCancellationRequested();
                yield return BlueprintAssistantEvent.ToolStarted(toolCall.Name);

                AgentToolExecution execution;
                if (IsReadOnlyTool(toolCall.Name) &&
                    consecutiveReadOnlyTools >= MaximumConsecutiveReadOnlyTools)
                {
                    execution = new AgentToolExecution(
                        false,
                        true,
                        false,
                        """
                        {"success":false,"error":"The read-only research limit has been reached. Use the information already collected."}
                        """,
                        localizer.Text("statusReadLimit"),
                        null);
                    requestResearchLimit = true;
                }
                else
                {
                    execution = await tools.ExecuteAsync(
                        toolCall,
                        workspace,
                        cancellationToken);
                }

                nextInput.Add(
                    AgentInput.Tool(toolCall.Id, execution.ModelContent));
                string displayText = execution.Success
                    ? execution.Proposal is null
                        ? localizer.Text("toolCompleted")
                        : execution.Proposal.IsValid
                            ? localizer.Text("proposalReady")
                            : localizer.Text("proposalInvalid")
                    : execution.DisplayText;
                yield return BlueprintAssistantEvent.ToolCompleted(
                    toolCall.Name,
                    displayText);

                if (execution.IsFormatError)
                {
                    formatRetries++;
                    if (formatRetries > MaximumFormatRetries)
                    {
                        throw new InvalidOperationException(
                            localizer.Text("errorFormatLimit"));
                    }

                    requestFormatRetry = true;
                    yield return BlueprintAssistantEvent.Status(
                        localizer.Format(
                            "statusFormatRetry",
                            formatRetries,
                            MaximumFormatRetries));
                }
                if (execution.IsReadOnly)
                {
                    consecutiveReadOnlyTools++;
                    if (consecutiveReadOnlyTools ==
                        MaximumConsecutiveReadOnlyTools)
                    {
                        requestResearchLimit = true;
                    }
                }
                else
                {
                    consecutiveReadOnlyTools = 0;
                }

                if (execution.Proposal is not null)
                {
                    if (responseText.Length != 0)
                    {
                        yield return BlueprintAssistantEvent.TextDelta(
                            responseText.ToString());
                    }
                    yield return BlueprintAssistantEvent.ProposalReady(
                        execution.Proposal);
                    yield return BlueprintAssistantEvent.Completed();
                    yield break;
                }
            }
            if (requestFormatRetry)
                nextInput.Add(AgentInput.Developer(formatRetryPrompt));
            if (requestResearchLimit)
                nextInput.Add(AgentInput.Developer(researchLimitPrompt));
        }

        throw new InvalidOperationException(
            localizer.Text("errorIterationLimit"));
    }

    private static bool IsReadOnlyTool(string toolName)
    {
        return toolName is
            "list_blueprints" or
            "read_blueprint" or
            "get_api_catalog" or
            "search_project" or
            "read_file" or
            "validate_blueprint";
    }
}
