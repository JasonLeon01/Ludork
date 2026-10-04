using Ludork.Composition;
using Avalonia.Controls;
using Avalonia;
using Avalonia.Controls.Primitives;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Threading;
using Avalonia.VisualTree;
using AvaloniaEdit.Document;
using Ludork.Controls;
using Ludork.Models;
using Ludork.Plugin.Abstractions;
using Ludork.Services;
using Ludork.Services.UiAssets;
using Ludork.Services.BlueprintAssistant;
using Ludork.Services.Plugins;
using Ludork.ViewModels;
using Ludork.Views.Utils;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.Json.Nodes;
using System.Threading.Tasks;

namespace Ludork.Views.Coordination;

internal sealed class DocumentWindowCoordinator : IDisposable
{
    private readonly Window owner;
    private readonly EditorProjectSession projectSession;
    private readonly Action showHelp;
    private MainViewModel viewModel => projectSession.MainViewModel;
    private bool disposed;
    private AnimationOverviewWindow? animationOverview;
    private ParticleOverviewWindow? particleOverview;
    private TilesetEditorWindow? tilesetEditor;
    private GeneralDataEditorWindow? generalDataEditor;
    private CommonFunctionWindow? commonFunctionWindow;
    private GameVariableManagerWindow? gameVariableManager;
    private readonly Dictionary<string, CurveWindow> curveWindows = new(StringComparer.Ordinal);
    private readonly Dictionary<string, TextConfigEditorWindow> textConfigWindows = new(StringComparer.Ordinal);
    private readonly BlueprintEditorWindowCollection blueprintWindows = new();
    private readonly Dictionary<string, UiAssetEditorWindow> uiAssetWindows = new(StringComparer.Ordinal);
    private bool uiAssetRefreshPending;
    private string? lastActiveBlueprintKey;

    public DocumentWindowCoordinator(Window owner, EditorProjectSession session, Action showHelp)
    {
        this.owner = owner;
        projectSession = session;
        this.showHelp = showHelp;
        viewModel.Actions.ActionRequested += onActionRequested;
        viewModel.Actions.DataCreationRequested += onDataCreationRequested;
        viewModel.FileExplorerPanel.DataCreationRequested += onDataCreationRequested;
        viewModel.FileExplorerPanel.ReferenceTreeRequested += onReferenceTreeRequested;
        viewModel.FileExplorerPanel.FilesChanging += onFileChangesStarting;
        viewModel.FileExplorerPanel.FilesChanged += ApplyFileChanges;
        viewModel.GameData.UiAssets.UiAssetsChanged += onUiAssetsChanged;
        viewModel.GameData.Documents.ContentChanged += onDocumentPathsChanged;
        viewModel.FileOpenFailed += onFileOpenFailed;
    }

    public void Dispose()
    {
        if (disposed)
            return;
        disposed = true;
        viewModel.Actions.ActionRequested -= onActionRequested;
        viewModel.Actions.DataCreationRequested -= onDataCreationRequested;
        viewModel.FileExplorerPanel.DataCreationRequested -= onDataCreationRequested;
        viewModel.FileExplorerPanel.ReferenceTreeRequested -= onReferenceTreeRequested;
        viewModel.FileExplorerPanel.FilesChanging -= onFileChangesStarting;
        viewModel.FileExplorerPanel.FilesChanged -= ApplyFileChanges;
        viewModel.GameData.UiAssets.UiAssetsChanged -= onUiAssetsChanged;
        viewModel.GameData.Documents.ContentChanged -= onDocumentPathsChanged;
        viewModel.FileOpenFailed -= onFileOpenFailed;
    }

    private async void onActionRequested(object? sender, EditorActionRequest request)
    {
        if (disposed)
            return;
        if (request.Kind == EditorActionKind.Help)
        {
            showHelp();
            return;
        }
        if (disposed || !viewModel.CanEdit)
            return;
        switch (request.Kind)
        {
            case EditorActionKind.GameConfig:
                await GameConfigWindow.ShowAsync(owner, viewModel.GameConfig);
                break;
            case EditorActionKind.SystemConfig:
                await new ConfigWindow(viewModel.GameData, viewModel.ProjectSave).ShowDialog(owner);
                break;
            case EditorActionKind.Tilesets:
            case EditorActionKind.AutoTiles:
                showTilesetEditor(viewModel.GameData, viewModel.TileSelect,
                    request.Kind == EditorActionKind.AutoTiles, request.ResourceKey);
                break;
            case EditorActionKind.AnimationOverview:
                showAnimationOverview(viewModel.GameData);
                break;
            case EditorActionKind.ParticleOverview:
                showParticleOverview(viewModel.GameData);
                break;
            case EditorActionKind.CommonFunctions:
                await showCommonFunctionsAsync(viewModel, request.ResourceKey);
                break;
            case EditorActionKind.GameVariables:
                showGameVariableManager(viewModel);
                break;
            case EditorActionKind.GeneralData:
                showGeneralDataEditor(viewModel, request.ResourceKey);
                break;
            case EditorActionKind.Blueprint when request.ResourceKey is { } key:
                showBlueprintEditor(viewModel, key);
                break;
            case EditorActionKind.Animation when request.ResourceKey is { } key:
                showAnimation(key, viewModel.GameData);
                break;
            case EditorActionKind.Subtitle when request.ResourceKey is { } key:
                showSubtitle(key, viewModel.GameData);
                break;
            case EditorActionKind.Particle when request.ResourceKey is { } key:
                showParticle(key, viewModel.GameData);
                break;
            case EditorActionKind.Curve when request.ResourceKey is { } key:
                showCurve(key, viewModel.GameData);
                break;
            case EditorActionKind.TextConfig when request.ResourceKey is { } key:
                showTextConfig(key, viewModel.GameData);
                break;
            case EditorActionKind.UiAsset when request.ResourceKey is { } key:
                showUiAssetEditor(viewModel, key);
                break;
            case EditorActionKind.Undo:
                viewModel.UndoChanges();
                break;
            case EditorActionKind.Redo:
                viewModel.RedoChanges();
                break;
        }
    }

    private async void onDataCreationRequested(object? sender, EditorDataCreationRequest request)
    {
        if (disposed || !viewModel.CanEdit)
            return;
        if (request.Kind == EditorDataKind.Blueprint)
            await createBlueprintAsync(viewModel, request);
        else if (request.Kind == EditorDataKind.Animation)
            await createAnimationAsync(viewModel.GameData, request.DestinationPath);
        else if (request.Kind == EditorDataKind.Subtitle)
            await createSubtitleAsync(viewModel.GameData, request.DestinationPath);
        else if (request.Kind == EditorDataKind.Particle)
            await createParticleAsync(viewModel.GameData, request.DestinationPath);
        else if (request.Kind == EditorDataKind.Curve)
            await createCurveAsync(
                viewModel.GameData,
                request.DestinationPath,
                request.DataType);
        else if (request.Kind is EditorDataKind.TextConfig
            or EditorDataKind.PlainTextConfig
            or EditorDataKind.RichTextConfig)
            await createTextConfigAsync(viewModel.GameData, request);
        else if (request.Kind == EditorDataKind.UiAsset)
            await createUiAssetAsync(viewModel, request.DestinationPath);
    }

    private async Task createBlueprintAsync(
        MainViewModel mainViewModel,
        EditorDataCreationRequest request)
    {
        string blueprintsRoot = Path.Combine(mainViewModel.GameData.ProjectPath, "Data", "Blueprints");
        string? selectedPath = await EditorResourceCreation.SelectJsonPathAsync(
            owner, blueprintsRoot, request.DestinationPath, "SELECT_BLUEPRINT_PATH");
        if (selectedPath is null)
            return;

        string parentClass = request.ParentClass?.Trim() ?? string.Empty;
        if (parentClass.Length == 0)
        {
            string? selectedParent = await BlueprintClassSelector.ShowAsync(
                owner,
                mainViewModel.GameData,
                projectSession!.Metadata,
                mainViewModel.BlueprintClasses,
                "Engine.Actor",
                null,
                BlueprintClassSelectorMode.Parent);
            if (selectedParent is null)
                return;
            parentClass = selectedParent;
        }

        BlueprintCreationResult result = projectSession!.BlueprintCreation.Create(selectedPath, parentClass);
        if (!result.Success)
        {
            string message = result.Failure switch
            {
                BlueprintCreationFailure.AlreadyExists => LocaleService.Get("BLUEPRINT_EXISTS"),
                BlueprintCreationFailure.InvalidParent => LocaleService.Get("BLUEPRINT_PARENT_MUST_INHERIT_BPBASE"),
                _ => LocaleService.Get("SELECT_BLUEPRINT_PATH"),
            };
            await AlertDialog.ShowAsync(owner, LocaleService.Get("ERROR"), message);
            return;
        }
        mainViewModel.ActorQueue.PurgeStale();
        await AlertDialog.ShowAsync(owner, LocaleService.Get("HINT"), LocaleService.Get("HINT_CREATE_BP_SUCCESS"));
    }

    private void onReferenceTreeRequested(object? sender, string path)
    {
        if (viewModel is null)
            return;
        string? nodeId = viewModel.ReferenceIndex.GetNodeIdForPath(path);
        if (nodeId is null)
            return;
        new ReferenceTreeWindow(viewModel.ReferenceIndex, nodeId).Show(owner);
    }

    private void onDocumentPathsChanged(object? sender, EditorDocumentsChangedEventArgs args)
    {
        if (!args.Reset && !args.Changes.Any(change => change.IdentityChanged
                && change.Section is "Blueprints" or "UI"))
            return;
        blueprintWindows.Reindex();
        UiAssetEditorWindow[] assets = uiAssetWindows.Values.Distinct().ToArray();
        uiAssetWindows.Clear();
        foreach (UiAssetEditorWindow window in assets)
            uiAssetWindows[window.Document.DocumentKey] = window;
    }

    internal void ApplyFileChanges(
        object? sender,
        FileExplorerFilesChangedEventArgs args)
    {
        if (viewModel is null)
            return;
        string uiAssetsRoot = Path.Combine(
            viewModel.GameData.ProjectPath,
            "Data",
            "UI",
            "Assets");
        bool uiAssetsChanged = args.Added
                .Concat(args.Deleted)
                .Any(path => EditorPathSandbox.IsSameOrChildPath(uiAssetsRoot, path))
            || args.Moved.Any(move =>
                EditorPathSandbox.IsSameOrChildPath(uiAssetsRoot, move.OldPath)
                || EditorPathSandbox.IsSameOrChildPath(uiAssetsRoot, move.NewPath));
        bool uiAssetsMoved = args.Moved.Any(move =>
            EditorPathSandbox.IsSameOrChildPath(uiAssetsRoot, move.OldPath)
            || EditorPathSandbox.IsSameOrChildPath(uiAssetsRoot, move.NewPath));
        if (uiAssetsChanged && !uiAssetsMoved)
        {
            foreach (UiAssetEditorWindow window in uiAssetWindows.Values
                         .Distinct()
                         .ToArray())
            {
                window.RefreshControls();
            }
        }
        if (args.Moved.Count == 0 && args.Deleted.Count == 0)
            return;
        string blueprintsRoot = Path.Combine(
            viewModel.GameData.ProjectPath,
            "Data",
            "Blueprints");
        foreach (BlueprintEditorWindow window in blueprintWindows.Windows)
        {
            if (window.Document.BlueprintKey is not string key)
                continue;
            string path = Path.Combine(
                blueprintsRoot,
                key.Replace('/', Path.DirectorySeparatorChar)
                    + DataConfig.DataFileExtension);
            if (tryMapMovedPath(path, args.Moved, out string movedPath))
            {
                if (!tryGetBlueprintKey(
                        blueprintsRoot,
                        movedPath,
                        viewModel.GameData,
                        out string movedKey)
                    || !window.RekeyBlueprint(movedKey))
                {
                    window.Close();
                    continue;
                }
                blueprintWindows.Reindex();
                continue;
            }
            if (args.Deleted.Any(deleted => EditorPathSandbox.IsSameOrChildPath(deleted, path)))
                window.Close();
        }
        string uiRoot = Path.Combine(
            viewModel.GameData.ProjectPath,
            "Data",
            "UI",
            "Assets");
        foreach (UiAssetEditorWindow window in uiAssetWindows.Values
                     .Distinct()
                     .ToArray())
        {
            string path = Path.Combine(
                uiRoot,
                window.Document.AssetKey.Replace('/', Path.DirectorySeparatorChar)
                    + DataConfig.DataFileExtension);
            if (tryMapMovedPath(path, args.Moved, out string movedPath))
            {
                string oldDocumentKey = window.Document.DocumentKey;
                if (!tryGetUiAssetKey(
                        uiRoot,
                        movedPath,
                        viewModel.GameData,
                        out string movedKey)
                    || !window.Rekey(movedKey))
                {
                    window.Close();
                    continue;
                }
                uiAssetWindows.Remove(oldDocumentKey);
                uiAssetWindows[window.Document.DocumentKey] = window;
                continue;
            }
            if (args.Deleted.Any(deleted => EditorPathSandbox.IsSameOrChildPath(deleted, path)))
                window.Close();
        }
        if (!uiAssetsMoved)
            return;
        foreach (UiAssetEditorWindow window in uiAssetWindows.Values
                     .Distinct()
                     .ToArray())
        {
            if (!window.Reload())
                window.Close();
        }
    }

    private void onUiAssetsChanged(object? sender, EventArgs args)
    {
        if (uiAssetRefreshPending)
            return;
        uiAssetRefreshPending = true;
        Dispatcher.UIThread.Post(
            () =>
            {
                uiAssetRefreshPending = false;
                if (disposed)
                    return;
                foreach (UiAssetEditorWindow window in uiAssetWindows.Values
                             .Distinct()
                             .ToArray())
                {
                    window.RefreshControls();
                }
            },
            DispatcherPriority.Background);
    }

    private void onFileChangesStarting(object? sender, FileExplorerFilesChangedEventArgs args)
    {
        viewModel?.ProjectSave.FlushPendingChanges();
    }

    private static bool tryMapMovedPath(
        string path,
        IReadOnlyList<(string OldPath, string NewPath)> moved,
        out string mappedPath)
    {
        foreach ((string oldPath, string newPath) in moved)
        {
            string relative = Path.GetRelativePath(
                Path.GetFullPath(oldPath),
                Path.GetFullPath(path));
            if (!EditorPathSandbox.IsSameOrChildPath(oldPath, path))
                continue;
            mappedPath = relative == "."
                ? Path.GetFullPath(newPath)
                : Path.GetFullPath(Path.Combine(newPath, relative));
            return true;
        }
        mappedPath = string.Empty;
        return false;
    }

    private static bool tryGetBlueprintKey(
        string blueprintsRoot,
        string path,
        ProjectDataStore gameData,
        out string key)
    {
        string relative = Path.GetRelativePath(
            Path.GetFullPath(blueprintsRoot),
            Path.GetFullPath(path));
        if (!EditorPathSandbox.IsSameOrChildPath(blueprintsRoot, path)
            || !string.Equals(
                Path.GetExtension(path),
                DataConfig.DataFileExtension,
                StringComparison.OrdinalIgnoreCase))
        {
            key = string.Empty;
            return false;
        }
        key = BlueprintReference.NormalizeKey(relative);
        return gameData.Blueprints.BlueprintsData.ContainsKey(key);
    }


    private async void onFileOpenFailed(object? sender, string message)
    {
        await AlertDialog.ShowAsync(owner, LocaleService.Get("ERROR"), message);
    }

    private async Task showCommonFunctionsAsync(
        MainViewModel mainViewModel,
        string? functionKey)
    {
        if (commonFunctionWindow is not null)
        {
            commonFunctionWindow.SelectFunction(functionKey);
            commonFunctionWindow.Activate();
            return;
        }
        CommonFunctionWindow window = new(
            mainViewModel.GameData,
            mainViewModel.ProjectSave,
            projectSession!.Metadata,
            mainViewModel.BlueprintClasses);
        window.SelectFunction(functionKey);
        commonFunctionWindow = window;
        await window.ShowDialog(owner);
        commonFunctionWindow = null;
    }

    private void showGameVariableManager(MainViewModel mainViewModel)
    {
        if (gameVariableManager is not null)
        {
            gameVariableManager.Show();
            gameVariableManager.Activate();
            return;
        }
        gameVariableManager = new GameVariableManagerWindow(mainViewModel.GameVariables, mainViewModel.ProjectSave, mainViewModel.GameData.Enums);
        gameVariableManager.Closed += (_, _) => gameVariableManager = null;
        gameVariableManager.Show(owner);
    }

    private void showGeneralDataEditor(MainViewModel mainViewModel, string? typeKey)
    {
        if (generalDataEditor is not null)
        {
            if (typeKey is not null)
                generalDataEditor.selectDataType(typeKey);
            generalDataEditor.Show();
            generalDataEditor.Activate();
            return;
        }
        generalDataEditor = new GeneralDataEditorWindow(
            mainViewModel.GameData,
            mainViewModel.ProjectSave,
            projectSession!.Metadata,
            mainViewModel.BlueprintClasses,
            projectSession!.PreviewService);
        generalDataEditor.Closed += (_, _) => generalDataEditor = null;
        if (typeKey is not null)
            generalDataEditor.selectDataType(typeKey);
        generalDataEditor.Show(owner);
    }

    private void showBlueprintEditor(MainViewModel mainViewModel, string reference)
    {
        BlueprintEditorDocument? document = BlueprintEditorDocument.CreateBlueprint(
            mainViewModel.GameData,
            reference);
        blueprintWindows.Open(owner, document, value => createBlueprintWindow(mainViewModel, value));
    }

    private BlueprintEditorWindow createBlueprintWindow(MainViewModel mainViewModel, BlueprintEditorDocument document)
    {
        BlueprintEditorWindow window = new(
            document,
            mainViewModel.GameData,
            mainViewModel.ProjectSave,
            projectSession!.Metadata,
            mainViewModel.BlueprintClasses,
            projectSession!.PreviewService);
        JsonObject actorLibraryState = createActorLibraryState(document);
        EventHandler actorLibraryDocumentChanged = (_, _) =>
        {
            JsonObject nextState = createActorLibraryState(document);
            if (JsonNode.DeepEquals(actorLibraryState, nextState))
                return;
            actorLibraryState = nextState;
            mainViewModel.ActorQueue.PurgeStale();
        };
        document.Changed += actorLibraryDocumentChanged;
        window.Activated += (_, _) =>
        {
            if (window.Document.BlueprintKey is string key)
                lastActiveBlueprintKey = key;
        };
        window.Closed += (_, _) =>
        {
            document.Changed -= actorLibraryDocumentChanged;
        };
        return window;
    }

    private static JsonObject createActorLibraryState(BlueprintEditorDocument document)
    {
        return new JsonObject
        {
            ["parent"] = document.Data["parent"]?.DeepClone(),
            ["attrs"] = document.Data["attrs"]?.DeepClone(),
        };
    }

    private async Task createUiAssetAsync(
        MainViewModel mainViewModel,
        string? destinationPath)
    {
        await projectSession!.UiControlRegistry.Runtime.RefreshAsync();
        if (!projectSession!.UiControlRegistry.IsReady)
        {
            await AlertDialog.ShowAsync(owner, LocaleService.Get("UI_ASSET_EDITOR"),
                projectSession!.UiControlRegistry.Runtime.StatusMessage);
            return;
        }
        if (!projectSession!.UiControlRegistry.SystemDescriptors.Any(control => control.ControlId == "Engine.Canvas"))
        {
            await AlertDialog.ShowAsync(owner, LocaleService.Get("UI_ASSET_EDITOR"),
                LocaleService.Get("UI_REGISTRY_CANVAS_REQUIRED"));
            return;
        }
        string uiRoot = Path.Combine(
            mainViewModel.GameData.ProjectPath,
            "Data",
            "UI");
        string assetsRoot = Path.Combine(uiRoot, "Assets");
        string? selectedPath = await EditorResourceCreation.SelectJsonPathAsync(
            owner, assetsRoot, destinationPath, "SELECT_UI_ASSET_PATH");
        if (selectedPath is null)
            return;
        bool validPath = EditorResourceCreation.TryGetJsonKey(
            assetsRoot, selectedPath, out string relativeKey, StringComparison.Ordinal);
        string key = UiAssetSchema.NormalizeAssetKey(relativeKey);
        if (!validPath || key.Length == 0
            || File.Exists(selectedPath)
            || !mainViewModel.GameData.UiAssets.CreateUiAsset(key))
        {
            await AlertDialog.ShowAsync(
                owner,
                LocaleService.Get("ERROR"),
                LocaleService.Get("UI_ASSET_EXISTS"));
            return;
        }
        showUiAssetEditor(mainViewModel, key);
        await AlertDialog.ShowAsync(
            owner,
            LocaleService.Get("HINT"),
            LocaleService.Get("HINT_CREATE_UI_ASSET_SUCCESS"));
    }

    private void showUiAssetEditor(
        MainViewModel mainViewModel,
        string key)
    {
        UiAssetEditorDocument? document = UiAssetEditorDocument.Create(
            mainViewModel.GameData,
            projectSession!.UiControlRegistry,
            key);
        if (document is null)
            return;
        if (uiAssetWindows.TryGetValue(
                document.DocumentKey,
                out UiAssetEditorWindow? existing))
        {
            document.Dispose();
            existing.Show();
            existing.Activate();
            return;
        }
        UiAssetEditorWindow window = new(
            document,
            mainViewModel.GameData,
            mainViewModel.ProjectSave,
            projectSession!.UiControlRegistry,
            projectSession!.UiAssetValidation);
        uiAssetWindows[document.DocumentKey] = window;
        window.NestedAssetOpenRequested += (_, nestedKey) =>
            showUiAssetEditor(mainViewModel, nestedKey);
        window.Closed += (_, _) =>
            uiAssetWindows.Remove(window.Document.DocumentKey);
        window.Show(owner);
    }

    internal IBlueprintAssistantHost? CreateBlueprintAssistantHost()
    {
        if (viewModel is null)
            return null;
        return new BlueprintAssistantHostBridge(
            viewModel.GameData,
            projectSession!.Metadata,
            viewModel.BlueprintClasses,
            projectSession!.BlueprintValidation,
            getBlueprintAssistantTarget,
            FlushBlueprintAssistantTarget,
            RefreshBlueprintAssistantTarget);
    }

    private string? getBlueprintAssistantTarget()
    {
        if (lastActiveBlueprintKey is null)
            return null;
        string documentKey = "Blueprint:" + lastActiveBlueprintKey;
        return blueprintWindows.Find(documentKey) is not null
            ? lastActiveBlueprintKey
            : null;
    }

    internal void FlushBlueprintAssistantTarget(string blueprintKey)
    {
        string documentKey = "Blueprint:" + blueprintKey;
        if (blueprintWindows.Find(documentKey) is BlueprintEditorWindow window)
        {
            window.FlushPendingChanges();
        }
    }

    internal void RefreshBlueprintAssistantTarget(string blueprintKey)
    {
        string documentKey = "Blueprint:" + blueprintKey;
        if (blueprintWindows.Find(documentKey) is BlueprintEditorWindow window)
        {
            window.Reload();
        }
    }

    private async Task createAnimationAsync(ProjectDataStore gameData, string? destinationPath = null)
    {
        string animationsRoot = Path.Combine(gameData.ProjectPath, "Data", "Animations");
        string? selectedPath = await EditorResourceCreation.SelectJsonPathAsync(
            owner, animationsRoot, destinationPath, "SELECT_ANIMATION_PATH");
        if (selectedPath is null)
            return;
        if (DataConfig.isAnimationCache(selectedPath)
            || !string.Equals(Path.GetExtension(selectedPath), ".json", StringComparison.OrdinalIgnoreCase))
        {
            await AlertDialog.ShowAsync(owner, LocaleService.Get("ERROR"), LocaleService.Get("SELECT_ANIMATION_PATH"));
            return;
        }
        if (File.Exists(selectedPath))
        {
            await AlertDialog.ShowAsync(owner, LocaleService.Get("ERROR"), LocaleService.Get("ANIMATION_EXISTS"));
            return;
        }
        if (!EditorResourceCreation.TryGetJsonKey(animationsRoot, selectedPath, out string key))
        {
            await AlertDialog.ShowAsync(owner, LocaleService.Get("ERROR"), LocaleService.Get("SELECT_ANIMATION_PATH"));
            return;
        }
        if (!gameData.Assets.CreateAnimation(key, Path.GetFileNameWithoutExtension(key)))
        {
            await AlertDialog.ShowAsync(owner, LocaleService.Get("ERROR"), LocaleService.Get("ANIMATION_EXISTS"));
            return;
        }
        await AlertDialog.ShowAsync(owner, LocaleService.Get("HINT"), LocaleService.Get("HINT_CREATE_ANIM_SUCCESS"));
    }

    private void showAnimationOverview(ProjectDataStore gameData)
    {
        if (animationOverview is not null)
        {
            animationOverview.refresh();
            animationOverview.Show();
            animationOverview.Activate();
            return;
        }
        animationOverview = new AnimationOverviewWindow(gameData, viewModel!.ProjectSave);
        animationOverview.Closed += (_, _) => animationOverview = null;
        animationOverview.Show(owner);
    }

    private void showTilesetEditor(
        ProjectDataStore gameData,
        TileSelectViewModel tileSelect,
        bool isAutoTile,
        string? key)
    {
        if (tilesetEditor is not null)
        {
            tilesetEditor.NavigateTo(isAutoTile, key);
            tilesetEditor.Show();
            tilesetEditor.Activate();
            return;
        }
        tilesetEditor = new TilesetEditorWindow(gameData, viewModel!.ProjectSave, tileSelect);
        tilesetEditor.NavigateTo(isAutoTile, key);
        tilesetEditor.Closed += (_, _) => tilesetEditor = null;
        tilesetEditor.Show(owner);
    }

    private async Task createCurveAsync(
        ProjectDataStore gameData,
        string? destinationPath = null,
        string? curveType = null)
    {
        string? selectedCurveType = curveType;
        if (selectedCurveType is null)
        {
            string floatLabel = LocaleService.Get("CURVE_TYPE_FLOAT");
            string vector2Label = LocaleService.Get("CURVE_TYPE_VECTOR2");
            string vector3Label = LocaleService.Get("CURVE_TYPE_VECTOR3");
            string vector4Label = LocaleService.Get("CURVE_TYPE_VECTOR4");
            string? selectedLabel = await ItemSelectorDialog.ShowAsync(
                owner,
                LocaleService.Get("NEW_CURVE"),
                LocaleService.Get("CURVE_TYPE"),
                [floatLabel, vector2Label, vector3Label, vector4Label],
                floatLabel);
            selectedCurveType = selectedLabel switch
            {
                string value when value == vector2Label => "vector2Curve",
                string value when value == vector3Label => "vector3Curve",
                string value when value == vector4Label => "vector4Curve",
                string value when value == floatLabel => "curve",
                _ => null,
            };
            if (selectedCurveType is null)
                return;
        }
        string curvesRoot = Path.Combine(gameData.ProjectPath, "Data", "Curves");
        string? selectedPath = await EditorResourceCreation.SelectJsonPathAsync(
            owner, curvesRoot, destinationPath, "SELECT_CURVE_PATH");
        if (selectedPath is null)
            return;
        if (!string.Equals(Path.GetExtension(selectedPath), ".json", StringComparison.OrdinalIgnoreCase))
        {
            await AlertDialog.ShowAsync(owner, LocaleService.Get("ERROR"), LocaleService.Get("SELECT_CURVE_PATH"));
            return;
        }
        if (File.Exists(selectedPath))
        {
            await AlertDialog.ShowAsync(owner, LocaleService.Get("ERROR"), LocaleService.Get("CURVE_EXISTS"));
            return;
        }
        if (!EditorResourceCreation.TryGetJsonKey(curvesRoot, selectedPath, out string key))
        {
            await AlertDialog.ShowAsync(owner, LocaleService.Get("ERROR"), LocaleService.Get("SELECT_CURVE_PATH"));
            return;
        }
        if (!gameData.Assets.CreateCurve(
                key,
                Path.GetFileNameWithoutExtension(key),
                selectedCurveType))
        {
            await AlertDialog.ShowAsync(owner, LocaleService.Get("ERROR"), LocaleService.Get("CURVE_EXISTS"));
            return;
        }
        showCurve(key, gameData);
        await AlertDialog.ShowAsync(owner, LocaleService.Get("HINT"), LocaleService.Get("HINT_CREATE_CURVE_SUCCESS"));
    }

    private void showAnimation(string key, ProjectDataStore gameData)
    {
        if (!gameData.Assets.AnimationsData.TryGetValue(key, out AnimationSnapshot? data))
            return;
        new AnimationWindow(gameData, viewModel!.ProjectSave, key, data.ToJson()).Show(owner);
    }

    private void showCurve(string key, ProjectDataStore gameData)
    {
        if (!gameData.Assets.CurvesData.TryGetValue(key, out CurveSnapshot? snapshot))
            return;
        JsonObject data = snapshot.ToJson();
        if (curveWindows.TryGetValue(key, out CurveWindow? window))
        {
            window.Reload(data);
            window.Show();
            window.Activate();
            return;
        }
        window = new CurveWindow(gameData, viewModel!.ProjectSave, key, data);
        curveWindows[key] = window;
        window.Closed += (_, _) => curveWindows.Remove(key);
        window.Show(owner);
    }

    private async Task createTextConfigAsync(
        ProjectDataStore gameData,
        EditorDataCreationRequest request)
    {
        string root = Path.Combine(gameData.ProjectPath, "Data", "TextConfigs");
        Directory.CreateDirectory(root);
        string? selectedPath;
        string type;
        if (request.Kind == EditorDataKind.TextConfig)
        {
            TextConfigCreationResult? creation = await TextConfigCreationDialog.ShowAsync(
                owner,
                root,
                request.InitialDirectory);
            if (creation is null)
                return;
            selectedPath = creation.Path;
            type = creation.Type;
        }
        else
        {
            selectedPath = await EditorResourceCreation.SelectJsonPathAsync(
                owner, root, request.DestinationPath, "SELECT_TEXT_CONFIG_PATH");
            type = request.Kind == EditorDataKind.PlainTextConfig
                ? "plainTextConfig"
                : "richTextConfig";
        }
        if (selectedPath is null)
            return;
        selectedPath = EditorResourceCreation.NormalizePath(selectedPath);
        if (!string.Equals(Path.GetExtension(selectedPath), ".json", StringComparison.OrdinalIgnoreCase))
        {
            await AlertDialog.ShowAsync(owner, LocaleService.Get("ERROR"), LocaleService.Get("SELECT_TEXT_CONFIG_PATH"));
            return;
        }
        if (!EditorResourceCreation.TryGetJsonKey(root, selectedPath, out string key)
            || File.Exists(selectedPath))
        {
            await AlertDialog.ShowAsync(owner, LocaleService.Get("ERROR"), LocaleService.Get("TEXT_CONFIG_EXISTS"));
            return;
        }
        if (!gameData.Assets.CreateTextConfig(key, type, Path.GetFileNameWithoutExtension(key)))
        {
            await AlertDialog.ShowAsync(owner, LocaleService.Get("ERROR"), LocaleService.Get("TEXT_CONFIG_EXISTS"));
            return;
        }
        showTextConfig(key, gameData);
        await AlertDialog.ShowAsync(owner, LocaleService.Get("HINT"), LocaleService.Get("HINT_CREATE_TEXT_CONFIG_SUCCESS"));
    }

    private void showTextConfig(string key, ProjectDataStore gameData)
    {
        if (!gameData.Assets.TextConfigsData.TryGetValue(key, out TextConfigSnapshot? snapshot))
            return;
        JsonObject data = snapshot.ToJson();
        if (textConfigWindows.TryGetValue(key, out TextConfigEditorWindow? window))
        {
            window.Reload(data);
            window.Show();
            window.Activate();
            return;
        }
        window = new TextConfigEditorWindow(gameData, viewModel!.ProjectSave, key, data);
        textConfigWindows[key] = window;
        window.Closed += (_, _) => textConfigWindows.Remove(key);
        window.Show(owner);
    }


    private async Task createSubtitleAsync(ProjectDataStore gameData, string? destinationPath = null)
    {
        string root = Path.Combine(gameData.ProjectPath, "Data", "Subtitles");
        string? path = await EditorResourceCreation.SelectJsonPathAsync(
            owner, root, destinationPath, "SELECT_SUBTITLE_PATH", selectWhenWhitespace: false);
        if (path is null)
            return;
        if (!EditorResourceCreation.TryGetJsonKey(root, path, out string key, StringComparison.OrdinalIgnoreCase))
        {
            await AlertDialog.ShowAsync(owner, LocaleService.Get("ERROR"), LocaleService.Get("SELECT_SUBTITLE_PATH"));
            return;
        }
        if (!gameData.Subtitles.CreateSubtitle(key))
        {
            await AlertDialog.ShowAsync(owner, LocaleService.Get("ERROR"), LocaleService.Get("SUBTITLE_EXISTS"));
            return;
        }
        showSubtitle(key, gameData);
    }

    private void showSubtitle(string key, ProjectDataStore gameData)
    {
        SubtitleWindow? existing = owner.OwnedWindows.OfType<SubtitleWindow>().FirstOrDefault(window => window.Key == key);
        if (existing is not null)
        {
            existing.Activate();
            return;
        }
        if (gameData.Subtitles.Contains(key))
            new SubtitleWindow(gameData, viewModel!.ProjectSave, key, projectSession!.UiControlRegistry.Runtime).Show(owner);
    }

    private async Task createParticleAsync(ProjectDataStore gameData, string? destinationPath = null)
    {
        string root = Path.Combine(gameData.ProjectPath, "Data", "Particles");
        string? path = await EditorResourceCreation.SelectJsonPathAsync(
            owner, root, destinationPath, "SELECT_PARTICLE_PATH", selectWhenWhitespace: false);
        if (path is null)
            return;
        if (!EditorResourceCreation.TryGetJsonKey(root, path, out string key, StringComparison.Ordinal))
        {
            await AlertDialog.ShowAsync(owner, LocaleService.Get("ERROR"), LocaleService.Get("SELECT_PARTICLE_PATH"));
            return;
        }
        if (!gameData.Assets.CreateParticle(key, Path.GetFileNameWithoutExtension(key)))
        {
            await AlertDialog.ShowAsync(owner, LocaleService.Get("ERROR"), LocaleService.Get("PARTICLE_EXISTS"));
            return;
        }
        showParticle(key, gameData);
    }

    private void showParticleOverview(ProjectDataStore gameData)
    {
        if (particleOverview is not null)
        {
            particleOverview.Activate();
            return;
        }
        particleOverview = new ParticleOverviewWindow(gameData, viewModel!.ProjectSave,
            projectSession!.UiControlRegistry.Runtime, () => createParticleAsync(gameData));
        particleOverview.Closed += (_, _) => particleOverview = null;
        particleOverview.Show(owner);
    }

    private void showParticle(string key, ProjectDataStore gameData)
    {
        ParticleWindow? existing = owner.OwnedWindows.OfType<ParticleWindow>().FirstOrDefault(window => window.Key == key);
        if (existing is not null)
        {
            existing.Activate();
            return;
        }
        if (gameData.Assets.ParticlesData.ContainsKey(key))
            new ParticleWindow(gameData, viewModel!.ProjectSave, projectSession!.UiControlRegistry.Runtime, key).Show(owner);
    }
    private static bool tryGetUiAssetKey(
        string uiRoot,
        string path,
        ProjectDataStore gameData,
        out string key)
    {
        string relative = Path.GetRelativePath(uiRoot, Path.GetFullPath(path));
        if (!EditorPathSandbox.IsSameOrChildPath(uiRoot, path)
            || !string.Equals(
                Path.GetExtension(relative),
                DataConfig.DataFileExtension,
                StringComparison.Ordinal))
        {
            key = string.Empty;
            return false;
        }
        key = UiAssetSchema.NormalizeAssetKey(
            Path.ChangeExtension(relative, null)!.Replace('\\', '/'));
        return key.Length != 0
            && gameData.UiAssets.UiAssetsData.ContainsKey(UiAssetSchema.ToAssetDataKey(key));
    }

    public void SetEnabled(bool enabled)
    {
        foreach (Window window in owner.OwnedWindows)
        {
            if (!enabled && window is SubtitleWindow subtitle)
                subtitle.PausePreview();
            if (!enabled && window is ParticleWindow particle)
                particle.PausePreview();
            if (!enabled && window is ParticleOverviewWindow particles)
                particles.PausePreview();
            if (window is SubtitleWindow or AnimationOverviewWindow or AnimationWindow or ParticleOverviewWindow or ParticleWindow or TilesetEditorWindow
                or GeneralDataEditorWindow or CommonFunctionWindow or GameVariableManagerWindow
                or CurveWindow or TextConfigEditorWindow or BlueprintEditorWindow or UiAssetEditorWindow)
                window.IsEnabled = enabled;
        }
    }

}
