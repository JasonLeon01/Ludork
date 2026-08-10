using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.CompilerServices;
using System.Threading;
using System.Threading.Tasks;
using Ludork.Plugin.Abstractions;
using Ludork.Plugins.OfficialBlueprintAI.Agent;
using Ludork.Plugins.OfficialBlueprintAI.Configuration;
using Ludork.Plugins.OfficialBlueprintAI.Localization;
using Ludork.Plugins.OfficialBlueprintAI.Providers;

namespace Ludork.Plugins.OfficialBlueprintAI;

internal sealed class BlueprintAssistantProvider : IBlueprintAssistantProvider
{
    private readonly IAgentTransportFactory transportFactory;
    private readonly IAgentToolExecutor tools;
    private readonly PluginLocalizer localizer;

    public BlueprintAssistantProvider(PluginLocalizer localizer)
        : this(
            new AgentTransportFactory(),
            new BlueprintAgentTools(),
            localizer)
    {
    }

    internal BlueprintAssistantProvider(
        IAgentTransportFactory transportFactory,
        IAgentToolExecutor tools,
        PluginLocalizer localizer)
    {
        this.transportFactory = transportFactory;
        this.tools = tools;
        this.localizer = localizer;
    }

    public async Task<BlueprintAssistantSettings> LoadSettingsAsync(
        BlueprintAssistantProviderContext context,
        CancellationToken cancellationToken)
    {
        ArgumentNullException.ThrowIfNull(context);
        AiSettingsStore store = new AiSettingsStore(context.PluginDataDirectory);
        AiSettingsFile settings = await store.LoadAsync(cancellationToken);

        AiProviderProfile profile = store.GetActiveProfile(settings);
        string? apiKey = await context.SecretStore.ReadAsync(
            profile.CredentialName,
            cancellationToken);
        return ToPublicSettings(profile, apiKey);
    }

    public async Task<PluginResult> SaveSettingsAsync(
        BlueprintAssistantProviderContext context,
        BlueprintAssistantSettingsUpdate update,
        CancellationToken cancellationToken)
    {
        ArgumentNullException.ThrowIfNull(context);
        ArgumentNullException.ThrowIfNull(update);
        if (update.RemoveApiKey &&
            !string.IsNullOrWhiteSpace(update.NewApiKey))
        {
            return PluginResult.Failed(
                "An API key cannot be replaced and removed in the same update.");
        }
        if (update.NewApiKey is not null &&
            string.IsNullOrWhiteSpace(update.NewApiKey))
        {
            return PluginResult.Failed("The new API key cannot be blank.");
        }

        AiSettingsStore store = new AiSettingsStore(context.PluginDataDirectory);
        AiSettingsFile current = await store.LoadAsync(cancellationToken);
        AiSettingsFile updated;
        try
        {
            updated = store.UpdateActiveProfile(current, update);
        }
        catch (ArgumentException exception)
        {
            return PluginResult.Failed(exception.Message);
        }

        AiProviderProfile profile = store.GetActiveProfile(updated);
        bool secretChanged =
            update.RemoveApiKey || update.NewApiKey is not null;
        string? previousApiKey = secretChanged
            ? await context.SecretStore.ReadAsync(
                profile.CredentialName,
                cancellationToken)
            : null;
        bool settingsSaved = false;
        try
        {
            if (update.RemoveApiKey)
            {
                await context.SecretStore.DeleteAsync(
                    profile.CredentialName,
                    cancellationToken);
            }
            else if (update.NewApiKey is not null)
            {
                await context.SecretStore.WriteAsync(
                    profile.CredentialName,
                    update.NewApiKey,
                    cancellationToken);
            }

            await store.SaveAsync(updated, cancellationToken);
            settingsSaved = true;
        }
        finally
        {
            if (!settingsSaved && secretChanged)
            {
                if (previousApiKey is null)
                {
                    await context.SecretStore.DeleteAsync(
                        profile.CredentialName,
                        CancellationToken.None);
                }
                else
                {
                    await context.SecretStore.WriteAsync(
                        profile.CredentialName,
                        previousApiKey,
                        CancellationToken.None);
                }
            }
        }

        return PluginResult.Completed();
    }

    public async Task<PluginResult> TestConnectionAsync(
        BlueprintAssistantProviderContext context,
        CancellationToken cancellationToken)
    {
        ArgumentNullException.ThrowIfNull(context);
        AiSettingsStore store = new AiSettingsStore(context.PluginDataDirectory);
        AiSettingsFile settings = await store.LoadAsync(cancellationToken);
        AiProviderProfile profile = store.GetActiveProfile(settings);
        string? apiKey = await context.SecretStore.ReadAsync(
            profile.CredentialName,
            cancellationToken);
        if (string.IsNullOrWhiteSpace(apiKey))
        {
            return PluginResult.Failed(
                localizer.Text("errorMissingApiKey"));
        }

        try
        {
            await using IAgentTransportSession session = transportFactory.Create(
                profile.Provider,
                profile.Model,
                profile.Endpoint,
                profile.Organization,
                profile.Project,
                apiKey,
                "Return exactly OK.");
            IReadOnlyList<AgentInput> input = new List<AgentInput>
            {
                AgentInput.User("Connection test"),
            };
            bool receivedOutput = false;
            await foreach (AgentProviderEvent providerEvent
                in session.StreamAsync(
                    input,
                    Array.Empty<AgentToolDefinition>(),
                    cancellationToken))
            {
                if (providerEvent.Kind == AgentProviderEventKind.TextDelta &&
                    !string.IsNullOrWhiteSpace(providerEvent.Text))
                {
                    receivedOutput = true;
                }
            }

            return receivedOutput
                ? PluginResult.Completed()
                : PluginResult.Failed(
                    "The provider completed without returning text.");
        }
        catch (OperationCanceledException)
        {
            throw;
        }
        catch (Exception exception)
        {
            return PluginResult.Failed(
                SensitiveDataSanitizer.Redact(exception.Message, apiKey));
        }
    }

    public async IAsyncEnumerable<BlueprintAssistantEvent> RunTurnAsync(
        BlueprintAssistantTurnContext context)
    {
        ArgumentNullException.ThrowIfNull(context);
        CancellationToken cancellationToken = context.CancellationToken;
        AiSettingsStore settingsStore = new AiSettingsStore(
            context.ProviderContext.PluginDataDirectory);
        AiSettingsFile settings = await settingsStore.LoadAsync(
            cancellationToken);
        AiProviderProfile profile = settingsStore.GetActiveProfile(settings);
        string? apiKey = await context.ProviderContext.SecretStore.ReadAsync(
            profile.CredentialName,
            cancellationToken);
        if (string.IsNullOrWhiteSpace(apiKey))
        {
            yield return BlueprintAssistantEvent.Status(
                localizer.Text("errorMissingApiKey"));
            yield return BlueprintAssistantEvent.Completed();
            yield break;
        }

        string promptDirectory = Path.Combine(
            context.ProviderContext.PluginDirectory,
            "prompts");
        string systemPrompt = await File.ReadAllTextAsync(
            Path.Combine(promptDirectory, "System.md"),
            cancellationToken);
        string formatRetryPrompt = await File.ReadAllTextAsync(
            Path.Combine(promptDirectory, "FormatRetry.md"),
            cancellationToken);
        string researchLimitPrompt = await File.ReadAllTextAsync(
            Path.Combine(promptDirectory, "ResearchLimit.md"),
            cancellationToken);
        string projectName = Path.GetFileName(
            context.ProjectPath.TrimEnd(
                Path.DirectorySeparatorChar,
                Path.AltDirectorySeparatorChar));
        string instructions =
            $"{systemPrompt}\n\n" +
            "Bound conversation context:\n" +
            $"- Project: {projectName}\n" +
            $"- Target Blueprint: {context.BlueprintKey}\n" +
            $"- Base revision: {context.BaseRevision}\n" +
            "Only proposal tools for this bound Blueprint are writable.";

        ConversationContextStore conversationStore =
            new ConversationContextStore(
                context.ProviderContext.PluginDataDirectory);
        IReadOnlyList<AgentInput> modelInput =
            await conversationStore.BuildAsync(
                context.ConversationId,
                context.ProjectPath,
                context.BlueprintKey,
                context.Messages,
                cancellationToken);
        BlueprintAgent agent = new BlueprintAgent(
            transportFactory,
            tools,
            localizer);

        await foreach (BlueprintAssistantEvent assistantEvent
            in agent.RunAsync(
                profile,
                apiKey,
                instructions,
                modelInput,
                context.Workspace,
                formatRetryPrompt,
                researchLimitPrompt,
                cancellationToken))
        {
            yield return assistantEvent;
        }
    }

    private static BlueprintAssistantSettings ToPublicSettings(
        AiProviderProfile profile,
        string? apiKey)
    {
        bool hasApiKey = !string.IsNullOrWhiteSpace(apiKey);
        return new BlueprintAssistantSettings(
            profile.Provider,
            profile.Model,
            profile.Endpoint,
            profile.Organization,
            profile.Project,
            hasApiKey,
            hasApiKey ? MaskApiKey(apiKey!) : string.Empty);
    }

    private static string MaskApiKey(string apiKey)
    {
        if (apiKey.Length <= 6)
        {
            return new string('*', 11);
        }

        return apiKey[..3] + new string('*', 11) + apiKey[^3..];
    }
}
