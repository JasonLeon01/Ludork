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
    public GameVariableSaveResult GameVariableResult { get; init; } =
        GameVariableSaveResult.Completed(string.Empty);

    public SaveResult Result
    {
        get
        {
            string[] details = [DataResult.Details, GameConfigResult.Detail, GameVariableResult.Detail];
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
    private readonly GameVariableService gameVariables;
    private readonly BlueprintValidationService blueprintValidation;
    private readonly UiControlRegistryService uiControlRegistry;
    private readonly UiAssetValidationService uiAssetValidation;
    private readonly List<IProjectSaveParticipant> participants = [];

    public ProjectSaveService(
        GameDataService gameData,
        GameConfigService gameConfig,
        GameVariableService gameVariables,
        BlueprintValidationService blueprintValidation)
    {
        this.gameData = gameData;
        this.gameConfig = gameConfig;
        this.gameVariables = gameVariables;
        this.blueprintValidation = blueprintValidation;
        uiControlRegistry = new UiControlRegistryService(gameData);
        uiAssetValidation = new UiAssetValidationService(gameData, uiControlRegistry);
    }

    public event EventHandler? SavePreparing;
    public UiControlRegistryService UiControlRegistry => uiControlRegistry;
    public UiAssetValidationService UiAssetValidation => uiAssetValidation;
    public GameVariableService GameVariables => gameVariables;

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
        GameVariableSaveResult gameVariableResult = gameVariables.SavePending();
        if (!gameVariableResult.Success)
        {
            return new ProjectSaveAttempt(
                false,
                false,
                [],
                new SaveResult(false, string.Empty),
                GameConfigSaveResult.Completed(string.Empty))
            {
                GameVariableResult = gameVariableResult,
            };
        }
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
                GameVariableResult = gameVariableResult,
            };
        }
        IReadOnlyList<BlueprintValidationResult> blueprintValidationResults =
            blueprintValidation.ValidateBlueprints(gameData.GetModifiedBlueprintKeys());
        IReadOnlyList<BlueprintValidationResult> generalDataValidationResults =
            blueprintValidation.ValidateGeneralDataGraphs();
        BlueprintValidationResult[] validationResults = blueprintValidationResults
            .Concat(generalDataValidationResults)
            .ToArray();
        bool hasBlueprintValidationErrors = blueprintValidationResults.Any(result => !result.IsValid);
        bool hasGeneralDataValidationErrors = generalDataValidationResults.Any(result => !result.IsValid);
        if (hasGeneralDataValidationErrors || hasBlueprintValidationErrors && !allowInvalidBlueprints)
        {
            return new ProjectSaveAttempt(
                false,
                true,
                validationResults,
                new SaveResult(false, string.Empty),
                GameConfigSaveResult.Completed(string.Empty))
            {
                UiValidationResults = uiValidationResults,
                GameVariableResult = gameVariableResult,
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
                GameVariableResult = gameVariableResult,
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
            GameVariableResult = gameVariableResult,
        };
    }
}
