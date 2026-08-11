using System;
using System.Collections.Generic;
using System.Linq;
using Ludork.Models;

namespace Ludork.Services;

public sealed record ProjectSaveAttempt(
    bool Success,
    bool ValidationBlocked,
    IReadOnlyList<BlueprintValidationResult> ValidationResults,
    SaveResult DataResult,
    GameConfigSaveResult GameConfigResult)
{
    public IReadOnlyList<UiAssetValidationResult> UiValidationResults { get; init; } = [];

    public SaveResult Result
    {
        get
        {
            string[] details = [DataResult.Details, GameConfigResult.Detail];
            string detail = string.Join(
                Environment.NewLine,
                details.Where(value => !string.IsNullOrWhiteSpace(value)));
            return new SaveResult(Success, detail);
        }
    }
}

public interface IProjectSaveParticipant
{
    void FlushPendingChanges();
}

public sealed class ProjectSaveService
{
    private readonly GameDataService gameData;
    private readonly GameConfigService gameConfig;
    private readonly BlueprintValidationService blueprintValidation;
    private readonly UiControlRegistryService uiControlRegistry;
    private readonly UiAssetValidationService uiAssetValidation;
    private readonly List<IProjectSaveParticipant> participants = [];

    public ProjectSaveService(
        GameDataService gameData,
        GameConfigService gameConfig,
        BlueprintValidationService blueprintValidation)
    {
        this.gameData = gameData;
        this.gameConfig = gameConfig;
        this.blueprintValidation = blueprintValidation;
        uiControlRegistry = new UiControlRegistryService(gameData);
        uiAssetValidation = new UiAssetValidationService(gameData, uiControlRegistry);
    }

    public event EventHandler? SavePreparing;
    public UiControlRegistryService UiControlRegistry => uiControlRegistry;
    public UiAssetValidationService UiAssetValidation => uiAssetValidation;

    public void RegisterParticipant(IProjectSaveParticipant participant)
    {
        if (!participants.Contains(participant))
            participants.Add(participant);
    }

    public void UnregisterParticipant(IProjectSaveParticipant participant)
    {
        participants.Remove(participant);
    }

    public ProjectSaveAttempt TrySave(bool allowInvalidBlueprints = false)
    {
        foreach (IProjectSaveParticipant participant in participants.ToArray())
            participant.FlushPendingChanges();
        SavePreparing?.Invoke(this, EventArgs.Empty);
        gameData.BreakHistoryGesture();
        IReadOnlyList<UiAssetValidationResult> uiValidationResults = uiAssetValidation.ValidateAll();
        bool hasUiValidationErrors = uiValidationResults.Any(result => !result.IsValid);
        if (hasUiValidationErrors)
        {
            string detail = string.Join(
                Environment.NewLine,
                uiValidationResults
                    .Where(result => !result.IsValid)
                    .SelectMany(result => result.Errors.Select(error => $"UI/{result.AssetKey}: {error}")));
            return new ProjectSaveAttempt(
                false,
                true,
                [],
                new SaveResult(false, detail),
                GameConfigSaveResult.Completed(string.Empty))
            {
                UiValidationResults = uiValidationResults,
            };
        }
        IReadOnlyList<BlueprintValidationResult> validationResults =
            blueprintValidation.ValidateBlueprints(gameData.GetModifiedBlueprintKeys());
        bool hasValidationErrors = validationResults.Any(result => !result.IsValid);
        if (hasValidationErrors && !allowInvalidBlueprints)
        {
            return new ProjectSaveAttempt(
                false,
                true,
                validationResults,
                new SaveResult(false, string.Empty),
                GameConfigSaveResult.Completed(string.Empty))
            {
                UiValidationResults = uiValidationResults,
            };
        }

        SaveResult dataResult = gameData.SaveAllModified();
        if (!dataResult.Success)
        {
            return new ProjectSaveAttempt(
                false,
                false,
                validationResults,
                dataResult,
                GameConfigSaveResult.Completed(string.Empty))
            {
                UiValidationResults = uiValidationResults,
            };
        }

        GameConfigSaveResult configResult = gameConfig.SavePending();
        return new ProjectSaveAttempt(
            configResult.Success,
            false,
            validationResults,
            dataResult,
            configResult)
        {
            UiValidationResults = uiValidationResults,
        };
    }
}
