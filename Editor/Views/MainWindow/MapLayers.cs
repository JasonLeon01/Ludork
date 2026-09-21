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
    private void onPreviewModeRequested(object? sender, int modeIndex)
    {
        if (viewModel?.MapWorkspace.CanUseMapTools != true)
            return;
        MapEditMode mode = modeIndex switch
        {
            1 => MapEditMode.Light,
            2 => MapEditMode.Actor,
            _ => MapEditMode.Tile,
        };
        selectPreviewMode(mode);
    }

    private void onExitRequested(object? sender, EventArgs args) => Close();

    private async void onNewProjectRequested(object? sender, EventArgs args)
    {
        if (editorSettings is null || !await confirmLeaveAsync())
            return;
        string? projectFilePath = await NewProjectWindow.ShowAsync(this, editorSettings);
        if (projectFilePath is null)
            return;
        if (Avalonia.Application.Current is App app)
            app.openProject(this, projectFilePath);
    }

    private async void onOpenProjectRequested(object? sender, EventArgs args)
    {
        if (editorSettings is null)
            return;
        string root = Path.GetFullPath(editorSettings.getLastPathOrHome());
        if (!Directory.Exists(root))
            root = Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
        string? projectFilePath = await FileSelectorDialog.ShowAsync(
            this,
            root,
            FileSelectorDialog.FilesFilter("*.proj"),
            LocaleService.Get("SELECT_PROJ_FILE"));
        if (projectFilePath is null)
            return;
        string fullPath = Path.GetFullPath(projectFilePath);
        if (!fullPath.EndsWith(".proj", StringComparison.OrdinalIgnoreCase) || !File.Exists(fullPath))
        {
            await AlertDialog.ShowAsync(this, LocaleService.Get("HINT"), LocaleService.Get("INVALID_PROJ_FILE"));
            return;
        }
        string? nextProjectPath = Path.GetDirectoryName(fullPath);
        if (string.IsNullOrWhiteSpace(nextProjectPath))
            return;
        if (string.Equals(Path.GetFullPath(nextProjectPath), ProjectPath, StringComparison.OrdinalIgnoreCase))
            return;
        if (!await confirmLeaveAsync())
            return;
        if (Avalonia.Application.Current is App app)
            app.openProject(this, fullPath);
    }

    public async void CloseForProjectSwitch()
    {
        closingPrompt = true;
        Task completion = projectOperations?.Completion ?? Task.CompletedTask;
        projectOperations?.Cancel();
        if (projectRunner is not null && projectRunner.State != ProjectRunState.Idle)
        {
            long generation = projectRunner.RunGeneration;
            await projectRunner.SetPerformanceMonitoringAsync(false, generation);
            await projectRunner.StopAsync(generation);
        }
        await completion;
        closingPrompt = false;
        closeConfirmed = true;
        Close();
    }

    private async Task<bool> confirmLeaveAsync()
    {
        viewModel?.ProjectSave.FlushPendingChanges();
        if (viewModel?.IsModified != true)
            return true;
        UnsavedChangesResult result = await new UnsavedChangesDialog(viewModel.GameData.GetUnsavedDocumentPaths()
                .Concat(viewModel.ProjectSave.PendingInputPaths).Distinct(StringComparer.Ordinal).ToArray(), ProjectPath).ShowDialog<UnsavedChangesResult>(this);
        if (result == UnsavedChangesResult.Cancel)
            return false;
        if (result == UnsavedChangesResult.Save)
        {
            if (!await EditorSaveWorkflow.TrySaveAsync(this, viewModel.ProjectSave))
                return false;
        }
        return true;
    }

    private void selectPreviewMode(MapEditMode mode)
    {
        if (viewModel?.MapWorkspace.IsLiveDebugActive == true && mode == MapEditMode.Light)
            return;
        bool tileMode = mode == MapEditMode.Tile;
        bool lightMode = mode == MapEditMode.Light;
        bool actorMode = mode == MapEditMode.Actor;
        bool wasLightMode = EditorPanel.EditMode == MapEditMode.Light;
        if (lightMode && LightActorSelectionToggle.IsChecked != true)
            suspendLightLayerSelection();
        else if (wasLightMode && !lightMode && viewModel?.MapWorkspace.SelectedLayerTab is null
            || lightMode && !wasLightMode)
            restoreLightLayerSelection();
        TileModeToggle.IsChecked = tileMode;
        LightModeToggle.IsChecked = lightMode;
        ActorModeToggle.IsChecked = actorMode;
        EditorPanel.setEditMode(mode);
        updateLightModeControls();
        updateMapModePanels();
        if (lightMode)
        {
            EditorPanel.clearLightSelection();
            LightInfoPanel.setLight(null);
        }
        if (actorMode)
            viewModel?.MapWorkspace.refreshActorOutliner();
        refreshMapPanelState();
    }

    private void onLightActorSelectionClick(object? sender, RoutedEventArgs args)
    {
        EditorPanel.setLightActorSelectionEnabled(LightActorSelectionToggle.IsChecked == true);
        if (LightActorSelectionToggle.IsChecked == true)
            restoreLightLayerSelection();
        else
            suspendLightLayerSelection();
        updateLightModeControls();
        updateMapModePanels();
        refreshMapPanelState();
    }

    private void suspendLightLayerSelection()
    {
        if (viewModel is null)
            return;
        if (viewModel.MapWorkspace.SelectedLayerTab is { IsOverview: false } layer)
        {
            lightSelectionMapKey = viewModel.MapWorkspace.SelectedMap?.Key;
            lightSelectionLayerName = layer.Name;
        }
        viewModel.MapWorkspace.SelectedLayerTab = null;
    }

    private void restoreLightLayerSelection()
    {
        if (viewModel is null || viewModel.MapWorkspace.SelectedLayerTab is { IsOverview: false })
            return;
        viewModel.MapWorkspace.SelectedLayerTab = viewModel.MapWorkspace.LayerTabs.FirstOrDefault(layer =>
            !layer.IsOverview
            && lightSelectionMapKey == viewModel.MapWorkspace.SelectedMap?.Key && layer.Name == lightSelectionLayerName)
            ?? viewModel.MapWorkspace.LayerTabs.FirstOrDefault(layer => layer.IsOverview);
    }

    private void updateLightModeControls()
    {
        bool lightMode = EditorPanel.EditMode == MapEditMode.Light;
        LightActorSelectionToggle.IsVisible = lightMode && viewModel?.MapWorkspace.SelectedMap is { IsMap: true };
        LightActorSelectionToggle.IsEnabled = viewModel?.CanEdit == true;
        LayerTabs.IsEnabled = viewModel?.MapWorkspace.CanUseMapTools == true
            && (!lightMode || LightActorSelectionToggle.IsChecked == true);
    }

    private void updateMapModePanels()
    {
        bool actorMode = EditorPanel.EditMode == MapEditMode.Actor;
        bool lightActorSelected = EditorPanel.EditMode == MapEditMode.Light
            && EditorPanel.LightActorSelectionEnabled && EditorPanel.SelectedActorIndex is not null;
        RightList.IsVisible = EditorPanel.EditMode == MapEditMode.Tile;
        LightInfoPanel.IsVisible = false;
        ActorModePanel.IsVisible = actorMode || lightActorSelected;
        ActorOutlinerPanel.IsVisible = actorMode;
        ActorOutlinerSplitter.IsVisible = actorMode;
        Grid.SetRow(ActorInfoPanel, actorMode ? 2 : 0);
        Grid.SetRowSpan(ActorInfoPanel, actorMode ? 1 : 3);
    }

    private void onLayerPointerPressed(object? sender, PointerPressedEventArgs args)
    {
        if (!LayerTabs.IsEnabled)
            return;
        if (isLayerActionSource(args.Source))
            return;
        PointerPoint point = args.GetCurrentPoint(LayerTabs);
        TabStripItem? item = getTabStripItem(args.Source);
        LayerTabViewModel? layer = item?.Content as LayerTabViewModel ?? item?.DataContext as LayerTabViewModel;
        if (point.Properties.IsRightButtonPressed)
        {
            if (viewModel?.CanEdit != true)
                return;
            showLayerContextMenu(layer, item as Control ?? LayerTabs);
            args.Handled = true;
            return;
        }
        if (!point.Properties.IsLeftButtonPressed || layer is null || layer.IsOverview)
            return;
        if (viewModel?.CanEdit != true)
            return;
        draggedLayer = layer;
        dragStart = args.GetPosition(LayerTabs);
        isDraggingLayer = false;
    }

    private void onLayerVisibilityClick(object? sender, RoutedEventArgs args)
    {
        if (viewModel?.CanEdit != true)
            return;
        if (viewModel is null || (sender as Control)?.DataContext is not LayerTabViewModel layer)
            return;
        viewModel.MapWorkspace.SelectedLayerTab = layer;
        viewModel.MapWorkspace.setLayerVisible(layer, !layer.LayerVisible);
        args.Handled = true;
    }

    private IMapEditorHost createMapEditorHost(string mapKey)
    {
        return new MapEditorHostBridge(
            viewModel!.GameData,
            projectSession!.PreviewService,
            mapKey,
            refreshPluginMap,
            viewModel.MapWorkspace.canEditLayer);
    }

    private void refreshPluginMap(string mapKey, string layerName)
    {
        if (viewModel is null)
            return;
        MapListItemViewModel? map = viewModel.MapWorkspace.findMapItem(mapKey);
        if (map is null)
            return;
        viewModel.MapWorkspace.SelectedMap = map;
        LayerTabViewModel? layer = viewModel.MapWorkspace.LayerTabs.FirstOrDefault(
            item => !item.IsOverview
                && string.Equals(item.Name, layerName, StringComparison.Ordinal));
        if (layer is not null)
            viewModel.MapWorkspace.SelectedLayerTab = layer;
        selectPreviewMode(MapEditMode.Tile);
        refreshMapPanel();
    }

    private void onLayerPointerMoved(object? sender, PointerEventArgs args)
    {
        if (!LayerTabs.IsEnabled)
            return;
        if (draggedLayer is null || viewModel is null)
            return;
        PointerPoint point = args.GetCurrentPoint(LayerTabs);
        if (!point.Properties.IsLeftButtonPressed)
            return;
        Point position = args.GetPosition(LayerTabs);
        if (!isDraggingLayer && Math.Abs(position.X - dragStart.X) < 4 && Math.Abs(position.Y - dragStart.Y) < 4)
            return;
        if (!isDraggingLayer)
        {
            isDraggingLayer = true;
            args.Pointer.Capture(LayerTabs);
        }
        TabStripItem? targetItem = LayerTabs.GetVisualDescendants()
            .OfType<TabStripItem>()
            .FirstOrDefault(item => containsPoint(item, position));
        LayerTabViewModel? target = targetItem?.Content as LayerTabViewModel ?? targetItem?.DataContext as LayerTabViewModel;
        if (target is null || target.IsOverview || target == draggedLayer)
            return;
        viewModel.MapWorkspace.moveLayer(draggedLayer, target);
        dragStart = position;
    }

    private void onLayerPointerReleased(object? sender, PointerReleasedEventArgs args)
    {
        if (!LayerTabs.IsEnabled)
            return;
        draggedLayer = null;
        if (isDraggingLayer)
            args.Pointer.Capture(null);
        isDraggingLayer = false;
    }

    private void onLayerPointerCaptureLost(object? sender, PointerCaptureLostEventArgs args)
    {
        if (!LayerTabs.IsEnabled)
            return;
        draggedLayer = null;
        isDraggingLayer = false;
    }

    private void showLayerContextMenu(LayerTabViewModel? layer, Control target)
    {
        if (viewModel?.CanEdit != true)
            return;
        if (viewModel is null)
            return;
        if (layer is { IsOverview: false })
            viewModel.MapWorkspace.SelectedLayerTab = layer;
        ContextMenu menu = new ContextMenu();
        if (layer is null || layer.IsOverview)
        {
            MenuItem addItem = new MenuItem { Header = LocaleService.Get("ADD_LAYER") };
            addItem.Click += async (_, _) => await addLayerAsync(null);
            menu.Items.Add(addItem);
        }
        else
        {
            MenuItem visibilityItem = new MenuItem
            {
                Header = LocaleService.Get(layer.LayerVisible ? "HIDE_LAYER" : "SHOW_LAYER"),
            };
            visibilityItem.Click += (_, _) => viewModel.MapWorkspace.setLayerVisible(layer, !layer.LayerVisible);
            MenuItem addItem = new MenuItem { Header = LocaleService.Get("ADD_LAYER") };
            addItem.Click += async (_, _) => await addLayerAsync(layer.Name);
            MenuItem renameItem = new MenuItem { Header = LocaleService.Get("RENAME_LAYER") };
            renameItem.Click += async (_, _) => await renameLayerAsync(layer.Name);
            MenuItem copyItem = new MenuItem { Header = LocaleService.Get("COPY") };
            copyItem.Click += (_, _) => viewModel.MapWorkspace.copyLayer(layer.Name);
            MenuItem pasteItem = new MenuItem { Header = LocaleService.Get("PASTE"), IsEnabled = viewModel.MapWorkspace.CanPasteLayer };
            pasteItem.Click += (_, _) => viewModel.MapWorkspace.pasteLayer(layer.Name);
            MenuItem selectShaderItem = new MenuItem { Header = LocaleService.Get("SELECT_LAYER_SHADER") };
            selectShaderItem.Click += async (_, _) => await selectLayerShaderAsync(layer.Name);
            MenuItem clearShaderItem = new MenuItem
            {
                Header = LocaleService.Get("CLEAR_LAYER_SHADER"),
                IsEnabled = !string.IsNullOrWhiteSpace(viewModel.MapWorkspace.getLayerShaderPath(layer.Name)),
            };
            clearShaderItem.Click += (_, _) => viewModel.MapWorkspace.setLayerShaderPath(layer.Name, string.Empty);
            MenuItem deleteItem = new MenuItem { Header = LocaleService.Get("DELETE") };
            deleteItem.Click += async (_, _) => await deleteLayerAsync(layer.Name);
            menu.Items.Add(visibilityItem);
            menu.Items.Add(new Separator());
            menu.Items.Add(addItem);
            menu.Items.Add(renameItem);
            menu.Items.Add(new Separator());
            menu.Items.Add(copyItem);
            menu.Items.Add(pasteItem);
            menu.Items.Add(new Separator());
            menu.Items.Add(selectShaderItem);
            menu.Items.Add(clearShaderItem);
            menu.Items.Add(new Separator());
            menu.Items.Add(deleteItem);
        }
        menu.Open(target);
    }

    private async System.Threading.Tasks.Task addLayerAsync(string? insertAfterLayer)
    {
        if (viewModel is null)
            return;
        string? name = await SingleRowDialog.ShowAsync(
            this,
            LocaleService.Get("ADD_LAYER"),
            LocaleService.Get("ADD_MESSAGE"),
            viewModel.MapWorkspace.LayerTabs.Where(item => !item.IsOverview).Select(item => item.Name));
        if (name is not null)
            viewModel.MapWorkspace.addLayer(name, insertAfterLayer);
    }

    private async System.Threading.Tasks.Task renameLayerAsync(string oldName)
    {
        if (viewModel is null)
            return;
        string? name = await SingleRowDialog.ShowAsync(
            this,
            LocaleService.Get("RENAME_LAYER"),
            LocaleService.Get("RENAME_MESSAGE"),
            viewModel.MapWorkspace.LayerTabs.Where(item => !item.IsOverview && item.Name != oldName).Select(item => item.Name),
            oldName);
        if (name is not null
            && !string.Equals(name, oldName, StringComparison.Ordinal)
            && viewModel.MapWorkspace.renameLayer(oldName, name))
        {
            EditorPanel.refreshSelectedActor();
        }
    }

    private async System.Threading.Tasks.Task selectLayerShaderAsync(string layerName)
    {
        if (viewModel is null)
            return;
        string? shaderPath = await FileSelectorDialog.SelectLayerShaderAsync(
            this,
            viewModel.GameData.ProjectPath,
            viewModel.MapWorkspace.getLayerShaderPath(layerName));
        if (shaderPath is not null)
            viewModel.MapWorkspace.setLayerShaderPath(layerName, shaderPath);
    }

    private async System.Threading.Tasks.Task deleteLayerAsync(string layerName)
    {
        if (viewModel is null)
            return;
        bool confirmed = await ConfirmationDialog.ShowAsync(
            this,
            LocaleService.Get("CONFIRM_DELETE"),
            LocaleService.Get("CONFIRM_DELETE_LAYER").Replace("{name}", layerName, StringComparison.Ordinal));
        if (confirmed)
            viewModel.MapWorkspace.deleteLayer(layerName);
    }

    private static TabStripItem? getTabStripItem(object? source)
    {
        if (source is TabStripItem item)
            return item;
        return (source as Visual)?.GetVisualAncestors().OfType<TabStripItem>().FirstOrDefault();
    }

    private static bool isLayerActionSource(object? source)
    {
        if (source is Button)
            return true;
        return (source as Visual)?.GetVisualAncestors().Any(visual => visual is Button) == true;
    }

    private static TreeViewItem? getMapListItem(object? source)
    {
        if (source is TreeViewItem item)
            return item;
        return (source as Visual)?.GetVisualAncestors().OfType<TreeViewItem>().FirstOrDefault();
    }

    private bool containsPoint(TabStripItem item, Point point)
    {
        Point? origin = item.TranslatePoint(new Point(), LayerTabs);
        return origin is not null && new Rect(origin.Value, item.Bounds.Size).Contains(point);
    }

    private async void onClosing(object? sender, WindowClosingEventArgs args)
    {
        if (closeConfirmed)
            return;
        if (projectOperations?.IsPending == true)
        {
            args.Cancel = true;
            if (closingPrompt)
                return;
            closingPrompt = true;
            Task completion = projectOperations.Completion;
            projectOperations.Cancel();
            if (projectRunner is not null && projectRunner.State != ProjectRunState.Idle)
            {
                long generation = projectRunner.RunGeneration;
                await projectRunner.SetPerformanceMonitoringAsync(false, generation);
                await projectRunner.StopAsync(generation);
            }
            await completion;
            closingPrompt = false;
            Close();
            return;
        }
        viewModel?.ProjectSave.FlushPendingChanges();
        bool hasRunningProject = projectRunner is not null
            && projectRunner.State != ProjectRunState.Idle;
        if (viewModel?.IsModified != true && !hasRunningProject)
            return;
        args.Cancel = true;
        if (closingPrompt)
            return;
        closingPrompt = true;
        if (viewModel?.IsModified == true)
        {
            UnsavedChangesResult result = await new UnsavedChangesDialog(viewModel.GameData.GetUnsavedDocumentPaths()
                .Concat(viewModel.ProjectSave.PendingInputPaths).Distinct(StringComparer.Ordinal).ToArray(), ProjectPath).ShowDialog<UnsavedChangesResult>(this);
            if (result == UnsavedChangesResult.Cancel)
            {
                closingPrompt = false;
                return;
            }
            if (result == UnsavedChangesResult.Save)
            {
                if (!await EditorSaveWorkflow.TrySaveAsync(this, viewModel.ProjectSave))
                {
                    closingPrompt = false;
                    return;
                }
            }
        }
        if (projectRunner is not null && projectRunner.State != ProjectRunState.Idle)
        {
            long generation = projectRunner.RunGeneration;
            await projectRunner.SetPerformanceMonitoringAsync(false, generation);
            await projectRunner.StopAsync(generation);
        }
        closingPrompt = false;
        closeConfirmed = true;
        Close();
    }

    protected override void OnClosed(EventArgs args)
    {
        saveEditorLayout();
        consoleLogView.Dispose();
        consoleLogSession.Dispose();
        if (actorPreviewService is not null)
        {
            actorPreviewService.StatusChanged -= onActorPreviewStatusChanged;
            actorPreviewService = null;
        }
        if (projectRunner is not null)
        {
            if (viewModel?.ProjectConfig.IsStandalone == false)
                projectRunner.NativeBuildState.Changed -= onNativeBuildStateChanged;
            projectRunner.ExportState.Changed -= onExportStateChanged;
            projectRunner.OutputReceived -= onProjectOutputReceived;
            projectRunner.StateChanged -= onProjectRunStateChanged;
            projectRunner.CommandAvailabilityChanged -= onCommandAvailabilityChanged;
            projectRunner.PerformanceSampleReceived -= onPerformanceSampleReceived;
            projectRunner.TextInputReceived -= onRuntimeTextInputReceived;
        }
        GamePanel.InputBatchReady -= onGameInputBatchReady;
        if (liveDebug is not null)
        {
            liveDebug.EditingContextChanged -= onLiveDebugEditingContextChanged;
            liveDebug.Started -= onLiveDebugStarted;
            liveDebug.Ended -= onLiveDebugEnded;
            liveDebug.ContextChanged -= onLiveDebugContextChanged;
            liveDebug.StatusChanged -= onLiveDebugStatusChanged;
            liveDebug.ErrorReceived -= onLiveDebugError;
            liveDebug.Dispose();
        }
        projectOperations?.Dispose();
        if (projectOperations is not null)
            projectOperations.StateChanged -= onProjectOperationStateChanged;
        documentWindows?.Dispose();
        attachViewModel(null);
        projectSession?.Dispose();
        base.OnClosed(args);
    }
}
