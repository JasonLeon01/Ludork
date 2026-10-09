using System;
using System.Collections.Generic;
using System.Linq;
using System.IO;
using System.Threading.Tasks;
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
            string[] details =
            [
                DataResult.Success ? string.Empty : DataResult.Details,
                GameVariableResult.Success ? string.Empty : GameVariableResult.Detail,
            ];
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
    IReadOnlyList<string> PendingInputErrors => [];
    IReadOnlyList<string> PendingInputPaths => [];
}

public sealed class ProjectSaveService
{
    private readonly ProjectDataStore gameData;
    private readonly ProjectConfigService projectConfig;
    private readonly GameVariableService gameVariables;
    private readonly UiControlRegistryService uiControlRegistry;
    private readonly UiAssetValidationService uiAssetValidation;
    private readonly List<IProjectSaveParticipant> participants = [];

    public ProjectSaveService(
        ProjectDataStore gameData,
        GameVariableService gameVariables,
        ProjectConfigService projectConfig)
    {
        this.gameData = gameData;
        this.projectConfig = projectConfig;
        this.gameVariables = gameVariables;
        uiControlRegistry = new UiControlRegistryService(gameData);
        uiAssetValidation = new UiAssetValidationService(gameData, uiControlRegistry);
    }

    public event EventHandler? SavePreparing;
    public event EventHandler? PendingInputsChanged;
    public bool HasPendingInputErrors => participants.Any(participant => participant.PendingInputErrors.Count != 0);
    public IReadOnlyList<string> PendingInputErrors => participants.SelectMany(participant => participant.PendingInputErrors).ToArray();
    public IReadOnlyList<string> PendingInputPaths => participants.SelectMany(participant => participant.PendingInputPaths).Distinct(StringComparer.Ordinal).ToArray();
    public void NotifyPendingInputsChanged() => PendingInputsChanged?.Invoke(this, EventArgs.Empty);
    public UiControlRegistryService UiControlRegistry => uiControlRegistry;
    public UiAssetValidationService UiAssetValidation => uiAssetValidation;
    public GameVariableService GameVariables => gameVariables;
    public ProjectDataStore GameData => gameData;

    public void RegisterParticipant(IProjectSaveParticipant participant)
    {
        if (!participants.Contains(participant))
            participants.Add(participant);
    }

    public void UnregisterParticipant(IProjectSaveParticipant participant)
    {
        participants.Remove(participant);
        NotifyPendingInputsChanged();
    }

    public void FlushPendingChanges()
    {
        foreach (IProjectSaveParticipant participant in participants.ToArray())
            participant.FlushPendingChanges();
        SavePreparing?.Invoke(this, EventArgs.Empty);
        gameData.BreakHistoryGesture();
    }

    public async Task<ProjectSaveAttempt> TrySaveAsync(
        bool allowInvalidBlueprints = false,
        bool beforeNativeBuild = false,
        IProgress<EditorOperationProgress>? progress = null)
    {
        progress?.Report(new EditorOperationProgress("EDIT_OPERATION_PREPARING"));
        FlushPendingChanges();
        IReadOnlyList<string> inputErrors = PendingInputErrors;
        if (inputErrors.Count != 0)
        {
            return new ProjectSaveAttempt(false, false, [],
                new SaveResult(false, LocaleService.Get("BLUEPRINT_TEXT_SAVE_BLOCKED") + Environment.NewLine + string.Join(Environment.NewLine, inputErrors)));
        }
        ProjectOperationSnapshot snapshot = gameData.CaptureOperationSnapshot();
        EditorDocument[] modified = gameData.Documents.ModifiedDocuments
            .Where(document => document.Section != "GameVariables").ToArray();
        Func<GameVariableSaveResult>? saveVariables = gameVariables.CapturePendingSave();
        EditorDocumentState variableState = gameVariables.Document.CaptureState();
        UiControlDescriptor[] controls = uiControlRegistry.SystemDescriptors.ToArray();
        bool registryReady = uiControlRegistry.IsReady;
        string registryStatus = uiControlRegistry.Runtime.StatusMessage;
        bool structuralOnly = beforeNativeBuild || !registryReady && !projectConfig.IsStandalone;
        string[] inputPaths = snapshot.Documents.SelectMany(document => new[] { document.Current.Path, document.Saved.Path })
            .Concat(snapshot.MapCatalog.Select(entry => Path.Combine(snapshot.ProjectPath, "Data", "Maps",
                entry.Key.Replace('/', Path.DirectorySeparatorChar) + (entry.Kind == MapCatalogEntryKind.WorldMap ? "/_world.json" : ".json"))))
            .Concat(new[] { gameVariables.RuntimePath, gameVariables.MetadataPath }).Distinct(StringComparer.Ordinal).ToArray();
        ReferenceInputFiles files = await Task.Run(() => ReferenceInputFiles.Capture(snapshot.ProjectPath, inputPaths));
        Dictionary<string, ReferenceInputFiles.Stamp> dataFiles = await Task.Run(() => inputPaths
            .Where(path => !path.EndsWith(".lua", StringComparison.Ordinal))
            .ToDictionary(path => path, ReferenceInputFiles.Read, StringComparer.Ordinal));
        using ProjectDataStore captured = await Task.Run(snapshot.CreateStore);
        ProjectSaveAttempt attempt = await Task.Run(() =>
        {
            progress?.Report(new EditorOperationProgress("EDIT_OPERATION_VALIDATING"));
            LuaMetadataService metadata = new(captured.ProjectPath, enums: captured.Enums);
            using BlueprintClassResolver resolver = new(captured, metadata);
            BlueprintValidationService validation = new(captured, metadata, resolver);
            UiAssetValidationService uiValidation = new(captured, controls, registryReady, registryStatus);
            return validateCaptured(captured, validation, uiValidation, structuralOnly, allowInvalidBlueprints, progress);
        });
        if (!attempt.Success)
            return attempt;
        if (gameData.Documents.Revision != snapshot.Revision)
            return new ProjectSaveAttempt(false, false, [], new SaveResult(false, LocaleService.Get("EDIT_OPERATION_INPUTS_CHANGED")));
        bool filesCurrent = await Task.Run(files.IsCurrent);
        if (!filesCurrent || gameData.Documents.Revision != snapshot.Revision)
            return new ProjectSaveAttempt(false, false, [], new SaveResult(false, LocaleService.Get("EDIT_OPERATION_INPUTS_CHANGED")));
        GameVariableSaveResult variableResult = await Task.Run(() =>
        {
            progress?.Report(new EditorOperationProgress("EDIT_OPERATION_GENERATING"));
            return saveVariables?.Invoke() ?? GameVariableSaveResult.Completed(string.Empty);
        });
        if (saveVariables is not null && variableResult.Success)
            gameVariables.CompletePendingSave(variableState);
        if (!variableResult.Success)
            return attempt with { Success = false, GameVariableResult = variableResult };
        SaveResult dataResult = await Task.Run(() => captured.SaveAllModified(true, progress,
            () => dataFiles.All(pair => ReferenceInputFiles.Read(pair.Key) == pair.Value)));
        attempt = attempt with { Success = dataResult.Success, DataResult = dataResult, GameVariableResult = variableResult };
        if (attempt.Success)
            await gameData.CompleteOperationSaveAsync(captured, modified, progress);
        if (attempt.Success && structuralOnly && attempt.UiValidationResults.Count != 0)
        {
            attempt = attempt with
            {
                DataResult = attempt.DataResult with
                {
                    Details = string.Join(Environment.NewLine,
                        new[] { attempt.DataResult.Details, LocaleService.Get("UI_SAVE_NATIVE_VALIDATION_PENDING") }
                            .Where(value => !string.IsNullOrWhiteSpace(value))),
                },
            };
        }
        return attempt;
    }

    private static ProjectSaveAttempt validateCaptured(
        ProjectDataStore captured,
        BlueprintValidationService validation,
        UiAssetValidationService uiValidation,
        bool structuralOnly,
        bool allowInvalidBlueprints,
        IProgress<EditorOperationProgress>? progress)
    {
        IReadOnlyList<string> blueprintSchemaErrors = captured.ValidateBlueprintSchemas();
        if (blueprintSchemaErrors.Count != 0)
        {
            return new ProjectSaveAttempt(false, false, [],
                new SaveResult(false, string.Join(Environment.NewLine, blueprintSchemaErrors)));
        }
        IReadOnlyList<UiAssetValidationResult> uiValidationResults = uiValidation.ValidateAll(structuralOnly);
        if (uiValidationResults.Any(result => !result.IsValid))
        {
            string detail = string.Join(Environment.NewLine, uiValidationResults
                .Where(result => !result.IsValid)
                .SelectMany(result => result.Errors.Select(error => $"UI/{result.AssetKey}: {error}")));
            return new ProjectSaveAttempt(false, true, [], new SaveResult(false, detail))
            {
                UiValidationResults = uiValidationResults,
            };
        }
        IReadOnlyList<BlueprintValidationResult> blueprintResults =
            validation.ValidateBlueprints(captured.Blueprints.GetModifiedBlueprintKeys(), progress);
        IReadOnlyList<BlueprintValidationResult> generalResults =
            validation.ValidateGeneralDataGraphs(captured.GeneralSaveData, progress);
        BlueprintValidationResult[] results = blueprintResults.Concat(generalResults).ToArray();
        if (generalResults.Any(result => !result.IsValid)
            || !allowInvalidBlueprints && blueprintResults.Any(result => !result.IsValid))
        {
            return new ProjectSaveAttempt(false, true, results, new SaveResult(false, string.Empty))
            {
                UiValidationResults = uiValidationResults,
            };
        }
        return new ProjectSaveAttempt(true, false, results, new SaveResult(true, string.Empty))
        {
            UiValidationResults = uiValidationResults,
        };
    }
}
