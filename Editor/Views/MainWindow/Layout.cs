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
    private void initializeInteraction()
    {
        LightActorSelectionToggle.Content = LocaleService.Get("LIGHT_SELECT_ACTORS");
        ToolTip.SetTip(LightActorSelectionToggle, LocaleService.Get("LIGHT_SELECT_ACTORS_HINT"));
        EditorInputs.ApplyEditable(ConsoleInput);
        ConsoleInput.PlaceholderText = LocaleService.Get("SEND_HINT");
        ConsoleSendButton.Content = LocaleService.Get("SEND");
        ConsoleInput.KeyDown += onConsoleInputKeyDown;
        ConsoleInput.TextChanged += (_, _) => onConsoleInputTextChanged();
        ConsoleSendButton.Click += onConsoleSendClick;
        updateConsoleInputState();
        DataContextChanged += (_, _) => attachViewModel(DataContext as MainViewModel);
        MapList.AddHandler(PointerPressedEvent, onMapListPointerPressed, RoutingStrategies.Tunnel);
        LayerTabs.AddHandler(PointerPressedEvent, onLayerPointerPressed, RoutingStrategies.Tunnel);
        LayerTabs.AddHandler(PointerMovedEvent, onLayerPointerMoved, RoutingStrategies.Tunnel);
        LayerTabs.AddHandler(PointerReleasedEvent, onLayerPointerReleased, RoutingStrategies.Tunnel);
        LayerTabs.AddHandler(PointerCaptureLostEvent, onLayerPointerCaptureLost, RoutingStrategies.Tunnel);
        AddHandler(GotFocusEvent, onHistoryContextFocus, RoutingStrategies.Bubble);
        AddHandler(PointerPressedEvent, onHistoryContextPointer, RoutingStrategies.Tunnel);
        Deactivated += (_, _) =>
        {
            EditorPanel.CancelInteractions();
            viewModel?.GameData.BreakHistoryGesture();
        };
        Activated += onMainWindowActivated;
        Closing += onClosing;
        Opened += onOpened;
        SizeChanged += (_, _) => onWindowSizeChanged();
        MainLayoutGrid.SizeChanged += (_, _) => clampHorizontalPanelWidths();
        UpperLeftSplitter.DragDelta += (_, _) => onHorizontalSplitterChanged();
        UpperLeftSplitter.DragCompleted += (_, _) => onHorizontalSplitterChanged();
        UpperRightSplitter.DragDelta += (_, _) => onHorizontalSplitterChanged();
        UpperRightSplitter.DragCompleted += (_, _) => onHorizontalSplitterChanged();
        LowerLeftSplitter.DragDelta += (_, _) => onLowerLeftSplitterChanged();
        LowerLeftSplitter.DragCompleted += (_, _) => onLowerLeftSplitterChanged();
        UpperLowerSplitter.DragDelta += (_, _) => saveEditorLayout();
        UpperLowerSplitter.DragCompleted += (_, _) => saveEditorLayout();
        EditorPanel.TileSelectionPicked += onTileSelectionPicked;
        EditorPanel.ActorSelectionChanged += onMapActorSelectionChanged;
        EditorPanel.ActorDataChanged += onActorDataChanged;
        EditorPanel.ActorPropertiesChanged += (_, _) => ActorInfoPanel.refreshActorProperties();
        EditorPanel.LightSelectionChanged += onLightSelectionChanged;
        EditorPanel.LightDataChanged += onLightDataChanged;
        EditorPanel.EditFeedbackRequested += (_, message) => toast.ShowMessage(message, 3000);
        WorldEditorPanel.PlacementChanged += onWorldPlacementChanged;
        WorldEditorPanel.PlacementRemoved += onWorldPlacementRemoved;
        WorldEditorPanel.ChildMapOpenRequested += onWorldChildMapOpenRequested;
        GamePanel.InputBatchReady += onGameInputBatchReady;
        Deactivated += (_, _) => GamePanel.NotifyHostFocusLost();
        LightInfoPanel.LightEdited += onLightEdited;
        ActorInfoPanel.ActorTagChanged += onActorTagChanged;
        ActorInfoPanel.BlueprintOpenRequested += (_, reference) => viewModel?.Actions.OpenBlueprint(reference);
        ActorInfoPanel.BlueprintLocateRequested += onBlueprintLocateRequested;
        if (projectRunner is not null)
        {
            projectRunner.ExportState.Changed += onExportStateChanged;
            projectRunner.OutputReceived += onProjectOutputReceived;
            projectRunner.StateChanged += onProjectRunStateChanged;
            projectRunner.CommandAvailabilityChanged += onCommandAvailabilityChanged;
            projectRunner.PerformanceSampleReceived += onPerformanceSampleReceived;
            projectRunner.TextInputReceived += onRuntimeTextInputReceived;
        }
    }

    private void onActorQueueBlueprintOpenRequested(object? sender, string reference)
    {
        viewModel?.Actions.OpenBlueprint(reference);
    }

    private void attachViewModel(MainViewModel? next)
    {
        if (viewModel is not null)
        {
            if (!viewModel.ProjectConfig.IsStandalone && projectRunner is not null)
                projectRunner.NativeBuildState.Changed -= onNativeBuildStateChanged;
            if (actorPreviewService is not null)
                actorPreviewService.StatusChanged -= onActorPreviewStatusChanged;
            viewModel.PropertyChanged -= onViewModelPropertyChanged;
            viewModel.MapWorkspace.PropertyChanged -= onViewModelPropertyChanged;
            viewModel.MapWorkspace.SelectedMapChanged -= onSelectedMapChanged;
            viewModel.ActorQueue.SelectionChanged -= onActorQueueSelectionChanged;
            viewModel.ActorQueue.BlueprintOpenRequested -= onActorQueueBlueprintOpenRequested;
            viewModel.ActorQueue.BlueprintLocateRequested -= onBlueprintLocateRequested;
            viewModel.LanguageChangeRequested -= onLanguageChangeRequested;
            viewModel.SaveCompleted -= onSaveCompleted;
            viewModel.SaveRequested -= onSaveRequested;
            viewModel.HistoryCompleted -= onHistoryCompleted;
            viewModel.TileSelect.PropertyChanged -= onTileSelectPropertyChanged;

            viewModel.NewProjectRequested -= onNewProjectRequested;
            viewModel.OpenProjectRequested -= onOpenProjectRequested;
            viewModel.ExitRequested -= onExitRequested;
            viewModel.PreviewModeRequested -= onPreviewModeRequested;
            viewModel.MapWorkspace.ActorOutlinerChanged -= onActorOutlinerChanged;
            viewModel.MapWorkspace.LayerDisplayStateChanged -= onLayerDisplayStateChanged;
        }
        actorPreviewService = null;
        viewModel = next;
        if (viewModel is null)
            return;
        actorPreviewService = projectSession!.PreviewService.ActorPreviews;
        actorPreviewFallbackNotified = false;
        actorPreviewService.StatusChanged += onActorPreviewStatusChanged;
        tileSelect = viewModel.TileSelect;
        EditorPanel.configure(viewModel.GameData, projectSession!.PreviewService);
        WorldEditorPanel.Configure(viewModel.GameData, projectSession!.PreviewService);
        (int gameWidth, int gameHeight) = viewModel.GameData.Configs.getGameSize();
        GameAspectPanel.AspectRatio = (double)gameWidth / gameHeight;
        ActorInfoPanel.configure(
            viewModel.GameData,
            projectSession!.Metadata,
            viewModel.BlueprintClasses,
            viewModel.GameVariables,
            EditorPanel);
        viewModel.PropertyChanged += onViewModelPropertyChanged;
        viewModel.MapWorkspace.PropertyChanged += onViewModelPropertyChanged;
        viewModel.MapWorkspace.SelectedMapChanged += onSelectedMapChanged;
        viewModel.ActorQueue.SelectionChanged += onActorQueueSelectionChanged;
        viewModel.ActorQueue.BlueprintOpenRequested += onActorQueueBlueprintOpenRequested;
        viewModel.ActorQueue.BlueprintLocateRequested += onBlueprintLocateRequested;
        viewModel.LanguageChangeRequested += onLanguageChangeRequested;
        viewModel.SaveCompleted += onSaveCompleted;
        viewModel.SaveRequested += onSaveRequested;
        viewModel.HistoryCompleted += onHistoryCompleted;
        tileSelect.PropertyChanged += onTileSelectPropertyChanged;

        viewModel.NewProjectRequested += onNewProjectRequested;
        viewModel.OpenProjectRequested += onOpenProjectRequested;
        viewModel.ExitRequested += onExitRequested;
        viewModel.PreviewModeRequested += onPreviewModeRequested;
        viewModel.MapWorkspace.ActorOutlinerChanged += onActorOutlinerChanged;
        viewModel.MapWorkspace.LayerDisplayStateChanged += onLayerDisplayStateChanged;
        if (!viewModel.ProjectConfig.IsStandalone && projectRunner is not null)
        {
            projectRunner.NativeBuildState.Changed += onNativeBuildStateChanged;
            onMainWindowActivated(this, EventArgs.Empty);
        }
        Title = viewModel.WindowTitle;
        refreshMapPanel();
        selectPreviewMode(MapEditMode.Tile);
        setProjectRunState(projectRunner?.State ?? ProjectRunState.Idle);
    }

    private void onActorPreviewStatusChanged(object? sender, EventArgs args)
    {
        if (!Dispatcher.UIThread.CheckAccess())
        {
            Dispatcher.UIThread.Post(() => onActorPreviewStatusChanged(sender, args));
            return;
        }
        if (actorPreviewService is null)
            return;
        if (actorPreviewService.IsAvailable)
        {
            actorPreviewFallbackNotified = false;
            return;
        }
        string message = actorPreviewService.StatusMessage;
        if (actorPreviewFallbackNotified || string.IsNullOrWhiteSpace(message))
            return;
        actorPreviewFallbackNotified = true;
        toast.ShowMessage(
            string.Format(LocaleService.Get("ACTOR_PREVIEW_HOST_UNAVAILABLE"), message),
            5000);
    }

    private async void onOpened(object? sender, EventArgs args)
    {
        applyEditorLayout();
        clampHorizontalPanelWidths();
        layoutReady = true;
        saveEditorLayout();
        if (viewModel is null || viewModel.GameData.InvalidLoadPaths.Count == 0)
            return;
        string paths = string.Join(Environment.NewLine, viewModel.GameData.InvalidLoadPaths);
        await AlertDialog.ShowAsync(
            this,
            LocaleService.Get("INVALID_DATA_FILE"),
            string.Format(LocaleService.Get("INVALID_JSON_FILE_MESSAGE"), paths)
        );
    }

    private void applyEditorLayout()
    {
        if (editorSettings is null)
            return;
        leftColumn.Width = new GridLength(Math.Max(160, editorSettings.UpperLeftWidth));
        rightColumn.Width = new GridLength(Math.Max(320, editorSettings.UpperRightWidth));
        lowerLeftColumn.Width = new GridLength(Math.Max(180, editorSettings.LowerLeftWidth));
        lowerRow.Height = new GridLength(Math.Max(lowerRow.MinHeight, editorSettings.LowerAreaHeight));
        clampLowerLeftPanelWidth();
        clampLowerAreaHeight();
    }

    private void onWindowSizeChanged()
    {
        clampHorizontalPanelWidths();
        clampLowerLeftPanelWidth();
        clampLowerAreaHeight();
        scheduleEditorLayoutSave();
    }

    private void scheduleEditorLayoutSave()
    {
        if (layoutSavePending)
            return;
        layoutSavePending = true;
        Dispatcher.UIThread.Post(() =>
        {
            layoutSavePending = false;
            saveEditorLayout();
        }, DispatcherPriority.Background);
    }

    private void onHorizontalSplitterChanged()
    {
        clampHorizontalPanelWidths();
        saveEditorLayout();
    }

    private void onLowerLeftSplitterChanged()
    {
        clampLowerLeftPanelWidth();
        saveEditorLayout();
    }

    private void clampHorizontalPanelWidths()
    {
        if (gameLayoutLocked)
            return;
        double layoutWidth = MainLayoutGrid.Bounds.Width;
        if (layoutWidth <= 0)
            return;
        double innerSplitterWidth = getColumnPixelWidth(UpperGrid.ColumnDefinitions[1]);
        double outerSplitterWidth = getColumnPixelWidth(MainLayoutGrid.ColumnDefinitions[1]);
        double centerMinimumWidth = Math.Max(centerColumn.MinWidth, EditorScroll.MinWidth);
        double availableSideWidth = layoutWidth
            - innerSplitterWidth
            - outerSplitterWidth
            - centerMinimumWidth;
        if (availableSideWidth <= 0)
            return;
        double maximumLeftWidth = Math.Max(
            leftColumn.MinWidth,
            availableSideWidth - rightColumn.MinWidth);
        double leftWidth = Math.Clamp(
            getColumnPixelWidth(leftColumn),
            leftColumn.MinWidth,
            maximumLeftWidth);
        leftColumn.Width = new GridLength(leftWidth);
        workspaceColumn.MinWidth = leftWidth + innerSplitterWidth + centerMinimumWidth;
        double maximumRightWidth = Math.Max(
            rightColumn.MinWidth,
            layoutWidth - outerSplitterWidth - workspaceColumn.MinWidth);
        rightColumn.MaxWidth = maximumRightWidth;
        rightColumn.Width = new GridLength(Math.Clamp(
            getColumnPixelWidth(rightColumn),
            rightColumn.MinWidth,
            maximumRightWidth));
    }

    private void clampLowerAreaHeight()
    {
        double layoutHeight = MainLayoutGrid.Bounds.Height;
        if (layoutHeight <= 0)
            return;
        double splitterHeight = MainLayoutGrid.RowDefinitions[1].ActualHeight;
        if (splitterHeight <= 0)
            splitterHeight = 4;
        double maximum = Math.Max(
            lowerRow.MinHeight,
            layoutHeight - upperRow.MinHeight - splitterHeight);
        double current = getRowPixelHeight(lowerRow);
        if (current > maximum)
            lowerRow.Height = new GridLength(maximum);
    }

    private void clampLowerLeftPanelWidth()
    {
        double layoutWidth = LowerGrid.Bounds.Width;
        if (layoutWidth <= 0)
            return;
        double splitterWidth = getColumnPixelWidth(LowerGrid.ColumnDefinitions[1]);
        double maximum = Math.Max(lowerLeftColumn.MinWidth, layoutWidth - splitterWidth);
        lowerLeftColumn.Width = new GridLength(Math.Clamp(
            getColumnPixelWidth(lowerLeftColumn),
            lowerLeftColumn.MinWidth,
            maximum));
    }

    private void saveEditorLayout()
    {
        if (editorSettings is null || !layoutReady)
            return;
        if (WindowState == WindowState.Normal)
        {
            editorSettings.Width = Math.Max((int)MinWidth, (int)Math.Round(Width));
            editorSettings.Height = Math.Max((int)MinHeight, (int)Math.Round(Height));
        }
        editorSettings.UpperLeftWidth = Math.Max(160, getColumnPixelWidth(leftColumn));
        editorSettings.UpperRightWidth = Math.Max(320, getColumnPixelWidth(rightColumn));
        editorSettings.LowerLeftWidth = Math.Max(180, getColumnPixelWidth(lowerLeftColumn));
        editorSettings.LowerAreaHeight = Math.Max(160, getRowPixelHeight(lowerRow));
        editorSettings.Language = LocaleService.CurrentLanguage;
        editorSettings.Save();
    }

    private static int getColumnPixelWidth(ColumnDefinition column)
    {
        if (column.Width.IsAbsolute)
            return Math.Max(0, (int)Math.Round(column.Width.Value));
        return Math.Max(0, (int)Math.Round(column.ActualWidth));
    }

    private static int getRowPixelHeight(RowDefinition row)
    {
        if (row.Height.IsAbsolute)
            return Math.Max(0, (int)Math.Round(row.Height.Value));
        return Math.Max(0, (int)Math.Round(row.ActualHeight));
    }

    private void onViewModelPropertyChanged(object? sender, PropertyChangedEventArgs args)
    {
        if (args.PropertyName == nameof(MainViewModel.WindowTitle))
            Title = viewModel?.WindowTitle ?? "Ludork";
        else if (args.PropertyName == nameof(MapWorkspaceViewModel.SelectedLayerTab))
        {
            refreshMapPanelState();
            syncActorOutlinerSelection();
        }
    }

    private void onActorQueueSelectionChanged(object? sender, string? reference)
    {
        EditorPanel.setPendingActor(reference);
    }

    private async void onSaveCompleted(object? sender, SaveResult result)
    {
        await EditorFeedback.ShowSaveResultAsync(this, result);
    }

    private async void onSaveRequested(object? sender, EventArgs args)
    {
        if (viewModel?.CanEdit == true)
            await EditorSaveWorkflow.TrySaveAsync(this, viewModel.ProjectSave);
    }

    private void onHistoryCompleted(object? sender, HistoryCompletedEventArgs args)
    {
        EditorFeedback.ShowHistory(toast, args.Action, args.Result);
    }

    private void onMapActorSelectionChanged(object? sender, ActorSelectionChangedEventArgs args)
    {
        if (EditorPanel.IsRuntimeEditing && args.LayerName is not null && viewModel is not null
            && viewModel.MapWorkspace.SelectedLayerTab?.Name != args.LayerName)
            viewModel.MapWorkspace.SelectedLayerTab = viewModel.MapWorkspace.LayerTabs.FirstOrDefault(layer => layer.Name == args.LayerName);
        ActorInfoPanel.setActor(args.MapKey, args.LayerName, args.Index, args.ActorData);
        updateMapModePanels();
        liveDebug?.SelectActor(args.ActorData?["runtimeId"]?.GetValue<string>());
        if (viewModel?.CanEdit == true && !string.IsNullOrWhiteSpace(args.BlueprintReference))
            viewModel?.ActorQueue.AddOrPromote(args.BlueprintReference);
        syncActorOutlinerSelection();
    }

    private void onActorDataChanged(object? sender, EventArgs args)
    {
        viewModel?.MapWorkspace.refreshActorOutliner();
        ActorInfoPanel.refreshActorPosition();
    }

    private void onActorTagChanged(object? sender, ActorSelectionChangedEventArgs args)
    {
        viewModel?.MapWorkspace.refreshActorOutliner();
    }

    private void onLayerDisplayStateChanged(object? sender, EventArgs args)
    {
        refreshMapPanelState();
    }

    private void onActorOutlinerChanged(object? sender, EventArgs args)
    {
        syncActorOutlinerSelection();
    }

    private void syncActorOutlinerSelection()
    {
        if (viewModel is null)
            return;
        string? layerName = EditorPanel.SelectedActorLayer
            ?? (viewModel.MapWorkspace.SelectedLayerTab is { IsOverview: false } layer ? layer.Name : null);
        ActorOutlinerItemViewModel? selection = null;
        if (layerName is not null)
        {
            ActorOutlinerItemViewModel? layerItem = viewModel.MapWorkspace.ActorOutlinerItems
                .FirstOrDefault(item => item.LayerName == layerName);
            selection = EditorPanel.IsRuntimeEditing && EditorPanel.SelectedRuntimeActorId is string runtimeId
                ? viewModel.MapWorkspace.ActorOutlinerItems.SelectMany(item => item.EnumerateDescendants())
                    .FirstOrDefault(item => item.RuntimeId == runtimeId)
                : EditorPanel.SelectedActorIndex is int actorIndex
                    ? layerItem?.Children.FirstOrDefault(item => item.ActorIndex == actorIndex) : layerItem;
        }
        if (ReferenceEquals(ActorOutliner.SelectedItem, selection))
            return;
        updatingActorOutlinerSelection = true;
        ActorOutliner.SelectedItem = selection;
        updatingActorOutlinerSelection = false;
    }

    private void onActorOutlinerSelectionChanged(object? sender, SelectionChangedEventArgs args)
    {
        if (updatingActorOutlinerSelection
            || viewModel is null
            || viewModel.MapWorkspace.IsRefreshingActorOutliner
            || ActorOutliner.SelectedItem is not ActorOutlinerItemViewModel item)
        {
            return;
        }
        LayerTabViewModel? layer = viewModel.MapWorkspace.LayerTabs
            .FirstOrDefault(tab => !tab.IsOverview && tab.Name == item.LayerName);
        if (layer is null)
            return;
        viewModel.MapWorkspace.SelectedLayerTab = layer;
        EditorPanel.selectActor(item.LayerName, item.ActorIndex);
    }

    private async void onBlueprintLocateRequested(object? sender, string reference)
    {
        if (!reference.StartsWith("Data.Blueprints.", StringComparison.Ordinal))
            return;
        string key = reference["Data.Blueprints.".Length..].Replace('.', Path.DirectorySeparatorChar);
        string path = Path.Combine(ProjectPath, "Data", "Blueprints", key + ".json");
        BottomTabs.SelectedIndex = 0;
        await FileExplorerPanel.LocatePathAsync(path);
    }

    private void onLanguageChangeRequested(object? sender, EventArgs args)
    {
        if (editorSettings is not null && viewModel is not null)
        {
            editorSettings.Language = viewModel.SelectedLanguage;
            editorSettings.Save();
        }
        DispatcherTimer.RunOnce(() => _ = showLanguageChangeHint(), TimeSpan.Zero);
    }

    private async Task showLanguageChangeHint()
    {
        await AlertDialog.ShowAsync(this, LocaleService.Get("HINT"), LocaleService.Get("LANGUAGE_CHANGE_RESTART"));
    }

    private void onSelectedMapChanged(object? sender, EventArgs args)
    {
        refreshMapPanel();
    }

}
