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

namespace Ludork.Views;

public partial class MainWindow
{
    private async void onActionRequested(object? sender, string action)
    {
        if (action == "Help")
            showHelp();
        else if (action == "GameConfig" && viewModel is not null)
            await GameConfigWindow.ShowAsync(this, viewModel.GameConfig);
        else if (action == "SystemConfig" && viewModel is not null)
            await new ConfigWindow(viewModel.GameData, viewModel.ProjectSave).ShowDialog(this);
        else if (action == "Tilesets" && viewModel is not null)
            showTilesetEditor(viewModel.GameData, viewModel.TileSelect, false, null);
        else if (action.StartsWith("Tilesets:", StringComparison.Ordinal) && viewModel is not null)
            showTilesetEditor(
                viewModel.GameData,
                viewModel.TileSelect,
                false,
                action["Tilesets:".Length..]);
        else if (action == "AutoTiles" && viewModel is not null)
            showTilesetEditor(viewModel.GameData, viewModel.TileSelect, true, null);
        else if (action.StartsWith("AutoTiles:", StringComparison.Ordinal) && viewModel is not null)
            showTilesetEditor(
                viewModel.GameData,
                viewModel.TileSelect,
                true,
                action["AutoTiles:".Length..]);
        else if (action == "NewAnimation" && viewModel is not null)
            await createAnimationAsync(viewModel.GameData);
        else if (action == "NewCurve" && viewModel is not null)
            await createCurveAsync(viewModel.GameData);
        else if (action == "AnimationOverview" && viewModel is not null)
            showAnimationOverview(viewModel.GameData);
        else if (action == "CommonFunctions" && viewModel is not null)
            await showCommonFunctionsAsync(viewModel, null);
        else if (action.StartsWith("CommonFunctions:", StringComparison.Ordinal) && viewModel is not null)
            await showCommonFunctionsAsync(
                viewModel,
                action["CommonFunctions:".Length..]);
        else if (action == "GameVariables" && viewModel is not null)
            showGameVariableManager(viewModel);
        else if (action == "GeneralData" && viewModel is not null)
            showGeneralDataEditor(viewModel, null);
        else if (action.StartsWith("GeneralData:", StringComparison.Ordinal) && viewModel is not null)
            showGeneralDataEditor(viewModel, action["GeneralData:".Length..]);
        else if (action.StartsWith("Blueprint:", StringComparison.Ordinal) && viewModel is not null)
            showBlueprintEditor(viewModel, action["Blueprint:".Length..]);
        else if (action.StartsWith("Animation:", StringComparison.Ordinal) && viewModel is not null)
            showAnimation(action["Animation:".Length..], viewModel.GameData);
        else if (action.StartsWith("Curve:", StringComparison.Ordinal) && viewModel is not null)
            showCurve(action["Curve:".Length..], viewModel.GameData);
        else if (action.StartsWith("TextConfig:", StringComparison.Ordinal) && viewModel is not null)
            showTextConfig(action["TextConfig:".Length..], viewModel.GameData);
        else if (action.StartsWith("UiAsset:", StringComparison.Ordinal) && viewModel is not null)
            showUiAssetEditor(viewModel, action["UiAsset:".Length..]);
    }

    private async void onDataCreationRequested(object? sender, EditorDataCreationRequest request)
    {
        if (viewModel is null)
            return;
        if (request.Kind == EditorDataKind.Blueprint)
            await createBlueprintAsync(viewModel, request);
        else if (request.Kind == EditorDataKind.Animation)
            await createAnimationAsync(viewModel.GameData, request.DestinationPath);
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
        Directory.CreateDirectory(blueprintsRoot);
        string? selectedPath = request.DestinationPath;
        if (string.IsNullOrWhiteSpace(selectedPath))
        {
            selectedPath = await FileSelectorDialog.ShowAsync(
                this,
                blueprintsRoot,
                FileSelectorDialog.FilesFilter("*.json"),
                LocaleService.Get("SELECT_BLUEPRINT_PATH"),
                save: true);
        }
        if (selectedPath is null)
            return;
        selectedPath = Path.GetFullPath(selectedPath);
        if (!Path.HasExtension(selectedPath))
            selectedPath = Path.ChangeExtension(selectedPath, "json");

        string parentClass = request.ParentClass?.Trim() ?? string.Empty;
        if (parentClass.Length == 0)
        {
            string? selectedParent = await BlueprintClassSelector.ShowAsync(
                this,
                mainViewModel.GameData,
                mainViewModel.Metadata,
                mainViewModel.BlueprintClasses,
                "Engine.Actor",
                null,
                BlueprintClassSelectorMode.Parent);
            if (selectedParent is null)
                return;
            parentClass = selectedParent;
        }

        BlueprintCreationResult result = mainViewModel.BlueprintCreation.Create(selectedPath, parentClass);
        if (!result.Success)
        {
            string message = result.Failure switch
            {
                BlueprintCreationFailure.AlreadyExists => LocaleService.Get("BLUEPRINT_EXISTS"),
                BlueprintCreationFailure.InvalidParent => LocaleService.Get("BLUEPRINT_PARENT_MUST_INHERIT_BPBASE"),
                _ => LocaleService.Get("SELECT_BLUEPRINT_PATH"),
            };
            await AlertDialog.ShowAsync(this, LocaleService.Get("ERROR"), message);
            return;
        }
        mainViewModel.ActorQueue.PurgeStale();
        await AlertDialog.ShowAsync(this, LocaleService.Get("HINT"), LocaleService.Get("HINT_CREATE_BP_SUCCESS"));
    }

    private void onReferenceTreeRequested(object? sender, string path)
    {
        if (viewModel is null)
            return;
        string? nodeId = viewModel.ReferenceIndex.GetNodeIdForPath(path);
        if (nodeId is null)
            return;
        new ReferenceTreeWindow(viewModel.ReferenceIndex, nodeId).Show(this);
    }

    private void onFileChangesApplied(
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
                .Any(path => isSameOrChildPath(uiAssetsRoot, path))
            || args.Moved.Any(move =>
                isSameOrChildPath(uiAssetsRoot, move.OldPath)
                || isSameOrChildPath(uiAssetsRoot, move.NewPath));
        bool uiAssetsMoved = args.Moved.Any(move =>
            isSameOrChildPath(uiAssetsRoot, move.OldPath)
            || isSameOrChildPath(uiAssetsRoot, move.NewPath));
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
        foreach (BlueprintEditorWindow window in blueprintWindows.Values
                     .Distinct()
                     .ToArray())
        {
            if (window.Document.BlueprintKey is not string key)
                continue;
            string path = Path.Combine(
                blueprintsRoot,
                key.Replace('/', Path.DirectorySeparatorChar)
                    + DataConfig.DataFileExtension);
            if (tryMapMovedPath(path, args.Moved, out string movedPath))
            {
                string oldDocumentKey = window.Document.DocumentKey;
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
                blueprintWindows.Remove(oldDocumentKey);
                blueprintWindows[window.Document.DocumentKey] = window;
                continue;
            }
            if (args.Deleted.Any(deleted => isSameOrChildPath(deleted, path)))
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
            if (args.Deleted.Any(deleted => isSameOrChildPath(deleted, path)))
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
                foreach (UiAssetEditorWindow window in uiAssetWindows.Values
                             .Distinct()
                             .ToArray())
                {
                    window.RefreshControls();
                }
            },
            DispatcherPriority.Background);
    }

    private void onFileChangesStarting(
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
        if (!args.Moved.Any(move =>
                isSameOrChildPath(uiAssetsRoot, move.OldPath)
                || isSameOrChildPath(uiAssetsRoot, move.NewPath)))
        {
            return;
        }
        foreach (UiAssetEditorWindow window in uiAssetWindows.Values
                     .Distinct()
                     .ToArray())
        {
            window.FlushPendingChanges();
        }
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
            if (!isRelativePathInside(relative))
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
        GameDataService gameData,
        out string key)
    {
        string relative = Path.GetRelativePath(
            Path.GetFullPath(blueprintsRoot),
            Path.GetFullPath(path));
        if (!isRelativePathInside(relative)
            || !string.Equals(
                Path.GetExtension(path),
                DataConfig.DataFileExtension,
                StringComparison.OrdinalIgnoreCase))
        {
            key = string.Empty;
            return false;
        }
        key = Path.ChangeExtension(relative, null)!
            .Replace('\\', '/');
        return gameData.BlueprintsData.ContainsKey(key);
    }

    private static bool isSameOrChildPath(string root, string path)
    {
        string relative = Path.GetRelativePath(
            Path.GetFullPath(root),
            Path.GetFullPath(path));
        return isRelativePathInside(relative);
    }

    private static bool isRelativePathInside(string relative)
    {
        return relative == "."
            || (!Path.IsPathRooted(relative)
                && relative != ".."
                && !relative.StartsWith(
                    ".." + Path.DirectorySeparatorChar,
                    StringComparison.Ordinal));
    }

    private async void onFileOpenFailed(object? sender, string message)
    {
        await AlertDialog.ShowAsync(this, LocaleService.Get("ERROR"), message);
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
            mainViewModel.Metadata,
            mainViewModel.BlueprintClasses);
        window.SelectFunction(functionKey);
        commonFunctionWindow = window;
        await window.ShowDialog(this);
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
        gameVariableManager = new GameVariableManagerWindow(mainViewModel.GameVariables);
        gameVariableManager.Closed += (_, _) => gameVariableManager = null;
        gameVariableManager.Show(this);
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
            mainViewModel.Metadata,
            mainViewModel.BlueprintClasses,
            mainViewModel.PreviewService);
        generalDataEditor.Closed += (_, _) => generalDataEditor = null;
        if (typeKey is not null)
            generalDataEditor.selectDataType(typeKey);
        generalDataEditor.Show(this);
    }

    private void showBlueprintEditor(MainViewModel mainViewModel, string reference)
    {
        BlueprintEditorDocument? document = BlueprintEditorDocument.CreateBlueprint(
            mainViewModel.GameData,
            reference);
        if (document is null)
            return;
        if (blueprintWindows.TryGetValue(document.DocumentKey, out BlueprintEditorWindow? existing))
        {
            if (!existing.Reload())
                return;
            existing.Show();
            existing.Activate();
            return;
        }
        BlueprintEditorWindow window = new(
            document,
            mainViewModel.GameData,
            mainViewModel.ProjectSave,
            mainViewModel.Metadata,
            mainViewModel.BlueprintClasses,
            mainViewModel.PreviewService);
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
        blueprintWindows[document.DocumentKey] = window;
        window.Activated += (_, _) =>
        {
            if (window.Document.BlueprintKey is string key)
                lastActiveBlueprintKey = key;
        };
        window.Closed += (_, _) =>
        {
            document.Changed -= actorLibraryDocumentChanged;
            blueprintWindows.Remove(document.DocumentKey);
        };
        window.Show(this);
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
        string uiRoot = Path.Combine(
            mainViewModel.GameData.ProjectPath,
            "Data",
            "UI");
        string assetsRoot = Path.Combine(uiRoot, "Assets");
        Directory.CreateDirectory(assetsRoot);
        string? selectedPath = destinationPath;
        if (string.IsNullOrWhiteSpace(selectedPath))
        {
            selectedPath = await FileSelectorDialog.ShowAsync(
                this,
                assetsRoot,
                FileSelectorDialog.FilesFilter("*.json"),
                LocaleService.Get("SELECT_UI_ASSET_PATH"),
                save: true);
        }
        if (selectedPath is null)
            return;
        selectedPath = Path.GetFullPath(selectedPath);
        if (!Path.HasExtension(selectedPath))
            selectedPath = Path.ChangeExtension(selectedPath, "json");
        string relativePath = Path.GetRelativePath(assetsRoot, selectedPath);
        string key = UiAssetSchema.NormalizeAssetKey(
            Path.ChangeExtension(relativePath, null)!.Replace('\\', '/'));
        if (key.Length == 0
            || Path.IsPathRooted(relativePath)
            || !isRelativePathInside(relativePath)
            || !string.Equals(
                Path.GetExtension(selectedPath),
                DataConfig.DataFileExtension,
                StringComparison.Ordinal)
            || File.Exists(selectedPath)
            || !mainViewModel.GameData.CreateUiAsset(key))
        {
            await AlertDialog.ShowAsync(
                this,
                LocaleService.Get("ERROR"),
                LocaleService.Get("UI_ASSET_EXISTS"));
            return;
        }
        showUiAssetEditor(mainViewModel, key);
        await AlertDialog.ShowAsync(
            this,
            LocaleService.Get("HINT"),
            LocaleService.Get("HINT_CREATE_UI_ASSET_SUCCESS"));
    }

    private void showUiAssetEditor(
        MainViewModel mainViewModel,
        string key)
    {
        UiAssetEditorDocument? document = UiAssetEditorDocument.Create(
            mainViewModel.GameData,
            key);
        if (document is null)
            return;
        if (uiAssetWindows.TryGetValue(
                document.DocumentKey,
                out UiAssetEditorWindow? existing))
        {
            existing.Show();
            existing.Activate();
            return;
        }
        UiAssetEditorWindow window = new(
            document,
            mainViewModel.GameData,
            mainViewModel.ProjectSave,
            mainViewModel.UiControlRegistry,
            mainViewModel.UiAssetValidation);
        uiAssetWindows[document.DocumentKey] = window;
        window.NestedAssetOpenRequested += (_, nestedKey) =>
            showUiAssetEditor(mainViewModel, nestedKey);
        window.Closed += (_, _) =>
            uiAssetWindows.Remove(window.Document.DocumentKey);
        window.Show(this);
    }

    internal IBlueprintAssistantHost? CreateBlueprintAssistantHost()
    {
        if (viewModel is null)
            return null;
        return new BlueprintAssistantHostBridge(
            viewModel.GameData,
            viewModel.Metadata,
            viewModel.BlueprintClasses,
            viewModel.BlueprintValidation,
            getBlueprintAssistantTarget,
            FlushBlueprintAssistantTarget,
            RefreshBlueprintAssistantTarget);
    }

    private string? getBlueprintAssistantTarget()
    {
        if (lastActiveBlueprintKey is null)
            return null;
        string documentKey = "Blueprint:" + lastActiveBlueprintKey;
        return blueprintWindows.ContainsKey(documentKey)
            ? lastActiveBlueprintKey
            : null;
    }

    internal void FlushBlueprintAssistantTarget(string blueprintKey)
    {
        string documentKey = "Blueprint:" + blueprintKey;
        if (blueprintWindows.TryGetValue(
                documentKey,
                out BlueprintEditorWindow? window))
        {
            window.FlushPendingChanges();
        }
    }

    internal void RefreshBlueprintAssistantTarget(string blueprintKey)
    {
        string documentKey = "Blueprint:" + blueprintKey;
        if (blueprintWindows.TryGetValue(
                documentKey,
                out BlueprintEditorWindow? window))
        {
            window.Reload();
        }
    }

}

