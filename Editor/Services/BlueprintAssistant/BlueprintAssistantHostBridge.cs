using Ludork.Plugin.Abstractions;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Services.BlueprintAssistant;

public sealed class BlueprintAssistantHostBridge : IBlueprintAssistantHost
{
    private readonly GameDataService gameData;
    private readonly LuaMetadataService metadataService;
    private readonly BlueprintClassResolver classResolver;
    private readonly BlueprintValidationService validationService;
    private readonly Func<string?> getSuggestedBlueprintKey;
    private readonly Action<string> flushBlueprint;
    private readonly Action<string> refreshBlueprint;

    public BlueprintAssistantHostBridge(
        GameDataService gameData,
        LuaMetadataService metadataService,
        BlueprintClassResolver classResolver,
        BlueprintValidationService validationService,
        Func<string?> getSuggestedBlueprintKey,
        Action<string> flushBlueprint,
        Action<string> refreshBlueprint)
    {
        this.gameData = gameData;
        this.metadataService = metadataService;
        this.classResolver = classResolver;
        this.validationService = validationService;
        this.getSuggestedBlueprintKey = getSuggestedBlueprintKey;
        this.flushBlueprint = flushBlueprint;
        this.refreshBlueprint = refreshBlueprint;
    }

    public string ProjectPath => gameData.ProjectPath;

    public string? SuggestedBlueprintKey
    {
        get
        {
            string? suggested = getSuggestedBlueprintKey();
            return suggested is not null && gameData.BlueprintsData.ContainsKey(suggested)
                ? suggested
                : null;
        }
    }

    public IReadOnlyList<string> ListBlueprints()
    {
        return gameData.BlueprintsData.Keys
            .OrderBy(key => key, StringComparer.Ordinal)
            .ToArray();
    }

    public IBlueprintAssistantSession CreateSession(string blueprintKey)
    {
        flushBlueprint(blueprintKey);
        BlueprintAssistantWorkspace workspace = new(
            gameData,
            metadataService,
            classResolver,
            validationService,
            blueprintKey,
            flushBlueprint,
            refreshBlueprint);
        return new BlueprintAssistantSession(workspace);
    }

    private sealed class BlueprintAssistantSession : IBlueprintAssistantSession
    {
        private readonly BlueprintAssistantWorkspace workspace;

        public BlueprintAssistantSession(BlueprintAssistantWorkspace workspace)
        {
            this.workspace = workspace;
        }

        public string BlueprintKey => workspace.BlueprintKey;
        public string BaseRevision => workspace.BaseRevision;
        public IBlueprintAssistantWorkspace Workspace => workspace;

        public Task<BlueprintAssistantApplyResult> ApplyProposalAsync(
            string proposalId,
            CancellationToken cancellationToken)
        {
            cancellationToken.ThrowIfCancellationRequested();
            return Task.FromResult(workspace.ApplyProposal(proposalId));
        }

        public Task<BlueprintAssistantApplyResult> ApplyCandidateAsync(
            string baseRevision,
            string candidateJson,
            CancellationToken cancellationToken)
        {
            cancellationToken.ThrowIfCancellationRequested();
            return Task.FromResult(workspace.ApplyCandidate(
                workspace.BlueprintKey,
                baseRevision,
                candidateJson));
        }

        public Task<BlueprintAssistantCandidateResult> GetProposalCandidateAsync(
            string proposalId,
            CancellationToken cancellationToken)
        {
            cancellationToken.ThrowIfCancellationRequested();
            string? candidate = workspace.GetProposalCandidate(proposalId);
            if (candidate is null)
            {
                return Task.FromResult(new BlueprintAssistantCandidateResult(
                    false,
                    "The proposal is no longer available.",
                    string.Empty));
            }
            return Task.FromResult(new BlueprintAssistantCandidateResult(
                true,
                string.Empty,
                candidate));
        }

        public Task<PluginResult> DiscardProposalAsync(
            string proposalId,
            CancellationToken cancellationToken)
        {
            cancellationToken.ThrowIfCancellationRequested();
            return Task.FromResult(workspace.DiscardProposal(proposalId)
                ? PluginResult.Completed()
                : PluginResult.Failed("The proposal is no longer available."));
        }
    }
}
