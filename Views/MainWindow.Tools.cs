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
    private void showHelp()
    {
        string? docsRoot = EditorRuntimePaths.FindDirectory("docs");
        if (docsRoot is null)
            return;
        string path = Path.Combine(docsRoot, LocaleService.CurrentLanguage);
        string imageRoot = Path.Combine(docsRoot, "_images");
        if (!Directory.Exists(path) || !Directory.Exists(imageRoot))
            return;
        List<HintedTextPresenter> mapHints = MapList
            .GetVisualDescendants()
            .OfType<HintedTextPresenter>()
            .ToList();
        foreach (HintedTextPresenter hint in mapHints)
        {
            ToolTip.SetIsOpen(hint, false);
            ToolTip.SetServiceEnabled(hint, false);
        }
        MarkdownPreviewWindow window = new MarkdownPreviewWindow(
            path,
            LocaleService.Get("HELP_EXPLANATION"),
            imageRoot);
        window.Closed += (_, _) =>
        {
            foreach (HintedTextPresenter hint in mapHints)
                ToolTip.SetServiceEnabled(hint, true);
        };
        _ = window.ShowDialog(this);
    }

    private void onIndividualWindowClicked(object? sender, EventArgs args)
    {
        if (viewModel is not null)
            viewModel.IndividualWindow = !viewModel.IndividualWindow;
    }

    private void onOpenAbout(object? sender, EventArgs args)
    {
        if (Application.Current is App app)
            app.showAbout(this);
    }

    private async void onImportPlugin(object? sender, EventArgs args)
    {
        if (Application.Current is App app)
            await app.importPluginAsync(this);
    }

    private async void onManagePlugins(object? sender, EventArgs args)
    {
        if (Application.Current is App app)
            await app.showPluginManagerAsync(this);
    }

    private async void onPackProject(object? sender, EventArgs args)
    {
        if (packLogDialog is not null)
        {
            packLogDialog.Activate();
            return;
        }
        if (packSelectionDialog is not null)
        {
            packSelectionDialog.Activate();
            return;
        }
        if (!Directory.Exists(ProjectPath))
        {
            await AlertDialog.ShowAsync(this, LocaleService.Get("PACK_TITLE"), LocaleService.Get("PACK_NO_PROJECT"));
            return;
        }
        string projectFilePath = Path.Combine(ProjectPath, "Main.proj");
        if (!File.Exists(projectFilePath))
        {
            await AlertDialog.ShowAsync(this, LocaleService.Get("PACK_TITLE"), LocaleService.Get("PACK_ENTRY_MISSING"));
            return;
        }
        if (viewModel is null
            || !await EditorSaveWorkflow.TrySaveAsync(
                this,
                viewModel.ProjectSave,
                false))
        {
            return;
        }
        PackSelectionDialog selectionDialog = new(viewModel.ProjectConfig.IsStandalone);
        packSelectionDialog = selectionDialog;
        ProjectPackOptions? options = await selectionDialog.ShowDialog<ProjectPackOptions?>(this);
        packSelectionDialog = null;
        if (options is null)
            return;
        string? scriptName = ProjectPackService.GetScriptName(options.Platform);
        if (scriptName is null || EditorRuntimePaths.FindFile("tools", scriptName) is null)
        {
            await AlertDialog.ShowAsync(this, LocaleService.Get("PACK_TITLE"), LocaleService.Get("PACK_SCRIPT_MISSING"));
            return;
        }

        ProjectPackService packService = new(ProjectPath);
        PackLogDialog logDialog = new();
        packLogDialog = logDialog;
        packService.OutputReceived += (_, text) => logDialog.AppendLog(text);
        Task closed = logDialog.ShowDialog(this);
        ProjectPackResult result = await packService.PackAsync(options);
        logDialog.Finish(result);
        await closed;
        packLogDialog = null;
    }

    private async Task createAnimationAsync(GameDataService gameData, string? destinationPath = null)
    {
        string animationsRoot = Path.Combine(gameData.ProjectPath, "Data", "Animations");
        Directory.CreateDirectory(animationsRoot);
        string? selectedPath = destinationPath;
        if (string.IsNullOrWhiteSpace(selectedPath))
        {
            selectedPath = await FileSelectorDialog.ShowAsync(this, animationsRoot,
                FileSelectorDialog.FilesFilter("*.json"), LocaleService.Get("SELECT_ANIMATION_PATH"), save: true);
        }
        if (selectedPath is null)
            return;
        selectedPath = Path.GetFullPath(selectedPath);
        if (!Path.HasExtension(selectedPath))
            selectedPath = Path.ChangeExtension(selectedPath, "json");
        if (DataConfig.isAnimationCache(selectedPath)
            || !string.Equals(Path.GetExtension(selectedPath), ".json", StringComparison.OrdinalIgnoreCase))
        {
            await AlertDialog.ShowAsync(this, LocaleService.Get("ERROR"), LocaleService.Get("SELECT_ANIMATION_PATH"));
            return;
        }
        if (File.Exists(selectedPath))
        {
            await AlertDialog.ShowAsync(this, LocaleService.Get("ERROR"), LocaleService.Get("ANIMATION_EXISTS"));
            return;
        }
        string relativePath = Path.GetRelativePath(animationsRoot, selectedPath);
        if (relativePath.StartsWith(".." + Path.DirectorySeparatorChar, StringComparison.Ordinal) || Path.IsPathRooted(relativePath))
        {
            await AlertDialog.ShowAsync(this, LocaleService.Get("ERROR"), LocaleService.Get("SELECT_ANIMATION_PATH"));
            return;
        }
        string key = Path.ChangeExtension(relativePath, null)!.Replace('\\', '/');
        if (!gameData.CreateAnimation(key, Path.GetFileNameWithoutExtension(key)))
        {
            await AlertDialog.ShowAsync(this, LocaleService.Get("ERROR"), LocaleService.Get("ANIMATION_EXISTS"));
            return;
        }
        await AlertDialog.ShowAsync(this, LocaleService.Get("HINT"), LocaleService.Get("HINT_CREATE_ANIM_SUCCESS"));
    }

    private void showAnimationOverview(GameDataService gameData)
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
        animationOverview.Show(this);
    }

    private void showTilesetEditor(
        GameDataService gameData,
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
        tilesetEditor.Show(this);
    }

    private async Task createCurveAsync(
        GameDataService gameData,
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
                this,
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
        Directory.CreateDirectory(curvesRoot);
        string? selectedPath = destinationPath;
        if (string.IsNullOrWhiteSpace(selectedPath))
        {
            selectedPath = await FileSelectorDialog.ShowAsync(this, curvesRoot,
                FileSelectorDialog.FilesFilter("*.json"), LocaleService.Get("SELECT_CURVE_PATH"), save: true);
        }
        if (selectedPath is null)
            return;
        selectedPath = Path.GetFullPath(selectedPath);
        if (!Path.HasExtension(selectedPath))
            selectedPath = Path.ChangeExtension(selectedPath, "json");
        if (!string.Equals(Path.GetExtension(selectedPath), ".json", StringComparison.OrdinalIgnoreCase))
        {
            await AlertDialog.ShowAsync(this, LocaleService.Get("ERROR"), LocaleService.Get("SELECT_CURVE_PATH"));
            return;
        }
        if (File.Exists(selectedPath))
        {
            await AlertDialog.ShowAsync(this, LocaleService.Get("ERROR"), LocaleService.Get("CURVE_EXISTS"));
            return;
        }
        string relativePath = Path.GetRelativePath(curvesRoot, selectedPath);
        if (relativePath.StartsWith(".." + Path.DirectorySeparatorChar, StringComparison.Ordinal) || Path.IsPathRooted(relativePath))
        {
            await AlertDialog.ShowAsync(this, LocaleService.Get("ERROR"), LocaleService.Get("SELECT_CURVE_PATH"));
            return;
        }
        string key = Path.ChangeExtension(relativePath, null)!.Replace('\\', '/');
        if (!gameData.CreateCurve(
                key,
                Path.GetFileNameWithoutExtension(key),
                selectedCurveType))
        {
            await AlertDialog.ShowAsync(this, LocaleService.Get("ERROR"), LocaleService.Get("CURVE_EXISTS"));
            return;
        }
        showCurve(key, gameData);
        await AlertDialog.ShowAsync(this, LocaleService.Get("HINT"), LocaleService.Get("HINT_CREATE_CURVE_SUCCESS"));
    }

    private void showAnimation(string key, GameDataService gameData)
    {
        if (!gameData.AnimationsData.TryGetValue(key, out JsonObject? data))
            return;
        new AnimationWindow(gameData, viewModel!.ProjectSave, key, data).Show(this);
    }

    private void showCurve(string key, GameDataService gameData)
    {
        if (!gameData.CurvesData.TryGetValue(key, out JsonObject? data))
            return;
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
        window.Show(this);
    }

    private async Task createTextConfigAsync(
        GameDataService gameData,
        EditorDataCreationRequest request)
    {
        string root = Path.Combine(gameData.ProjectPath, "Data", "TextConfigs");
        Directory.CreateDirectory(root);
        string? selectedPath;
        string type;
        if (request.Kind == EditorDataKind.TextConfig)
        {
            TextConfigCreationResult? creation = await TextConfigCreationDialog.ShowAsync(
                this,
                root,
                request.InitialDirectory);
            if (creation is null)
                return;
            selectedPath = creation.Path;
            type = creation.Type;
        }
        else
        {
            selectedPath = request.DestinationPath;
            if (string.IsNullOrWhiteSpace(selectedPath))
            {
                selectedPath = await FileSelectorDialog.ShowAsync(
                    this,
                    root,
                    FileSelectorDialog.FilesFilter("*.json"),
                    LocaleService.Get("SELECT_TEXT_CONFIG_PATH"),
                    save: true);
            }
            type = request.Kind == EditorDataKind.PlainTextConfig
                ? "plainTextConfig"
                : "richTextConfig";
        }
        if (selectedPath is null)
            return;
        selectedPath = Path.GetFullPath(selectedPath);
        if (!Path.HasExtension(selectedPath))
            selectedPath = Path.ChangeExtension(selectedPath, "json");
        if (!string.Equals(Path.GetExtension(selectedPath), ".json", StringComparison.OrdinalIgnoreCase))
        {
            await AlertDialog.ShowAsync(this, LocaleService.Get("ERROR"), LocaleService.Get("SELECT_TEXT_CONFIG_PATH"));
            return;
        }
        string relativePath = Path.GetRelativePath(root, selectedPath);
        if (File.Exists(selectedPath)
            || relativePath.StartsWith(".." + Path.DirectorySeparatorChar, StringComparison.Ordinal)
            || Path.IsPathRooted(relativePath))
        {
            await AlertDialog.ShowAsync(this, LocaleService.Get("ERROR"), LocaleService.Get("TEXT_CONFIG_EXISTS"));
            return;
        }
        string key = Path.ChangeExtension(relativePath, null)!.Replace('\\', '/');
        if (!gameData.CreateTextConfig(key, type, Path.GetFileNameWithoutExtension(key)))
        {
            await AlertDialog.ShowAsync(this, LocaleService.Get("ERROR"), LocaleService.Get("TEXT_CONFIG_EXISTS"));
            return;
        }
        showTextConfig(key, gameData);
        await AlertDialog.ShowAsync(this, LocaleService.Get("HINT"), LocaleService.Get("HINT_CREATE_TEXT_CONFIG_SUCCESS"));
    }

    private void showTextConfig(string key, GameDataService gameData)
    {
        if (!gameData.TextConfigsData.TryGetValue(key, out JsonObject? data))
            return;
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
        window.Show(this);
    }

}

