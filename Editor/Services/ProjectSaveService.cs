using System;
using System.Collections.Generic;
using System.Linq;
using Ludork.Models;

namespace Ludork.Services;

public sealed record ProjectSaveAttempt(
    bool Success,
    bool ValidationBlocked,
    IReadOnlyList<BlueprintValidationResult> ValidationResults,
    SaveResult DataResult)
{
    public IReadOnlyList<UiAssetValidationResult> UiValidationResults { get; init; } = [];
    public GameVariableSaveResult GameVariableResult { get; init; } =
        GameVariableSaveResult.Completed(string.Empty);

    public SaveResult Result
    {
        get
        {
            string[] details = [DataResult.Details, GameVariableResult.Detail];
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
    private readonly ProjectConfigService projectConfig;
    private readonly GameVariableService gameVariables;
    private readonly BlueprintValidationService blueprintValidation;
    private readonly UiControlRegistryService uiControlRegistry;
    private readonly UiAssetValidationService uiAssetValidation;
    private readonly List<IProjectSaveParticipant> participants = [];

    public ProjectSaveService(
        GameDataService gameData,
        GameVariableService gameVariables,
        BlueprintValidationService blueprintValidation,
        ProjectConfigService projectConfig)
    {
        this.gameData = gameData;
        this.projectConfig = projectConfig;
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

    public void FlushPendingChanges()
    {
        foreach (IProjectSaveParticipant participant in participants.ToArray())
            participant.FlushPendingChanges();
        SavePreparing?.Invoke(this, EventArgs.Empty);
        gameData.BreakHistoryGesture();
    }

    public ProjectSaveAttempt TrySave(bool allowInvalidBlueprints = false, bool beforeNativeBuild = false)
    {
        FlushPendingChanges();
        GameVariableSaveResult gameVariableResult = GameVariableSaveResult.Completed(string.Empty);
        bool structuralOnly = beforeNativeBuild || !uiControlRegistry.IsReady && !projectConfig.IsStandalone;
        IReadOnlyList<UiAssetValidationResult> uiValidationResults = uiAssetValidation.ValidateAll(structuralOnly);
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
                new SaveResult(false, detail))
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
                new SaveResult(false, string.Empty))
            {
                UiValidationResults = uiValidationResults,
                GameVariableResult = gameVariableResult,
            };
        }

        gameVariableResult = gameVariables.SavePending();
        if (!gameVariableResult.Success)
        {
            return new ProjectSaveAttempt(
                false,
                false,
                [],
                new SaveResult(false, string.Empty))
            {
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
                dataResult)
            {
                UiValidationResults = uiValidationResults,
                GameVariableResult = gameVariableResult,
            };
        }

        SaveResult generationResult = UiAssetGenerationService.Generate(gameData.ProjectPath);
        if (!generationResult.Success)
        {
            return new ProjectSaveAttempt(
                false,
                false,
                validationResults,
                new SaveResult(false, string.Join(Environment.NewLine,
                    new[] { dataResult.Details, generationResult.Details }
                        .Where(value => !string.IsNullOrWhiteSpace(value)))))
            {
                UiValidationResults = uiValidationResults,
                GameVariableResult = gameVariableResult,
            };
        }

        if (structuralOnly && uiValidationResults.Count != 0)
        {
            dataResult = dataResult with
            {
                Details = string.Join(Environment.NewLine,
                    new[] { dataResult.Details, LocaleService.Get("UI_SAVE_NATIVE_VALIDATION_PENDING") }
                        .Where(value => !string.IsNullOrWhiteSpace(value))),
            };
        }
        return new ProjectSaveAttempt(
            true,
            false,
            validationResults,
            dataResult)
        {
            UiValidationResults = uiValidationResults,
            GameVariableResult = gameVariableResult,
        };
    }
}
