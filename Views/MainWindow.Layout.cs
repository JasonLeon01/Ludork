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
    private void installPluginMenus()
    {
        NativeMenu? rootMenu = NativeMenu.GetMenu(this);
        if (Application.Current is not App app
            || rootMenu is null
            || rootMenu.Items.Count < 6
            || rootMenu.Items[0] is not NativeMenuItem { Menu: NativeMenu fileMenu }
            || rootMenu.Items[1] is not NativeMenuItem { Menu: NativeMenu editMenu }
            || rootMenu.Items[2] is not NativeMenuItem { Menu: NativeMenu gameMenu }
            || rootMenu.Items[3] is not NativeMenuItem { Menu: NativeMenu databaseMenu }
            || rootMenu.Items[4] is not NativeMenuItem { Menu: NativeMenu pluginsMenu }
            || rootMenu.Items[5] is not NativeMenuItem { Menu: NativeMenu helpMenu })
        {
            return;
        }
        app.installPluginMenus(
            this,
            ProjectPath.Length == 0 ? null : ProjectPath,
            (PluginMenuLocation.File, fileMenu),
            (PluginMenuLocation.Edit, editMenu),
            (PluginMenuLocation.Game, gameMenu),
            (PluginMenuLocation.Database, databaseMenu),
            (PluginMenuLocation.Help, helpMenu),
            (PluginMenuLocation.Plugins, pluginsMenu));
    }

    private void initializeInteraction()
    {
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
            projectRunner.OutputReceived += onProjectOutputReceived;
            projectRunner.StateChanged += onProjectRunStateChanged;
            projectRunner.CommandAvailabilityChanged += onCommandAvailabilityChanged;
            projectRunner.PerformanceSampleReceived += onPerformanceSampleReceived;
        }
    }

    private void attachViewModel(MainViewModel? next)
    {
        if (viewModel is not null)
        {
            if (actorPreviewService is not null)
                actorPreviewService.StatusChanged -= onActorPreviewStatusChanged;
            viewModel.PropertyChanged -= onViewModelPropertyChanged;
            viewModel.SelectedMapChanged -= onSelectedMapChanged;
            viewModel.LanguageChangeRequested -= onLanguageChangeRequested;
            viewModel.SaveCompleted -= onSaveCompleted;
            viewModel.SaveRequested -= onSaveRequested;
            viewModel.HistoryCompleted -= onHistoryCompleted;
            viewModel.TileSelect.PropertyChanged -= onTileSelectPropertyChanged;
            viewModel.Actions.ActionRequested -= onActionRequested;
            viewModel.Actions.DataCreationRequested -= onDataCreationRequested;
            viewModel.FileExplorerPanel.DataCreationRequested -= onDataCreationRequested;
            viewModel.FileExplorerPanel.ReferenceTreeRequested -= onReferenceTreeRequested;
            viewModel.FileExplorerPanel.FilesChanging -= onFileChangesStarting;
            viewModel.FileExplorerPanel.FilesChanged -= onFileChangesApplied;
            viewModel.GameData.UiAssetsChanged -= onUiAssetsChanged;
            viewModel.FileOpenFailed -= onFileOpenFailed;
            viewModel.ProjectSave.SavePreparing -= onSavePreparing;
            viewModel.NewProjectRequested -= onNewProjectRequested;
            viewModel.OpenProjectRequested -= onOpenProjectRequested;
            viewModel.ExitRequested -= onExitRequested;
            viewModel.PreviewModeRequested -= onPreviewModeRequested;
            viewModel.ActorOutlinerChanged -= onActorOutlinerChanged;
            viewModel.LayerDisplayStateChanged -= onLayerDisplayStateChanged;
        }
        actorPreviewService = null;
        viewModel = next;
        if (viewModel is null)
            return;
        actorPreviewService = viewModel.PreviewService.ActorPreviews;
        actorPreviewFallbackNotified = false;
        actorPreviewService.StatusChanged += onActorPreviewStatusChanged;
        tileSelect = viewModel.TileSelect;
        EditorPanel.configure(viewModel.GameData, viewModel.PreviewService);
        WorldEditorPanel.Configure(viewModel.GameData, viewModel.PreviewService);
        (int gameWidth, int gameHeight) = viewModel.GameData.getGameSize();
        GameAspectPanel.AspectRatio = (double)gameWidth / gameHeight;
        ActorInfoPanel.configure(
            viewModel.GameData,
            viewModel.Metadata,
            viewModel.BlueprintClasses,
            viewModel.GameVariables,
            EditorPanel);
        viewModel.PropertyChanged += onViewModelPropertyChanged;
        viewModel.SelectedMapChanged += onSelectedMapChanged;
        viewModel.ActorQueue.SelectionChanged += onActorQueueSelectionChanged;
        viewModel.ActorQueue.BlueprintOpenRequested += (_, reference) => viewModel.Actions.OpenBlueprint(reference);
        viewModel.ActorQueue.BlueprintLocateRequested += onBlueprintLocateRequested;
        viewModel.LanguageChangeRequested += onLanguageChangeRequested;
        viewModel.SaveCompleted += onSaveCompleted;
        viewModel.SaveRequested += onSaveRequested;
        viewModel.HistoryCompleted += onHistoryCompleted;
        tileSelect.PropertyChanged += onTileSelectPropertyChanged;
        viewModel.Actions.ActionRequested += onActionRequested;
        viewModel.Actions.DataCreationRequested += onDataCreationRequested;
        viewModel.FileExplorerPanel.DataCreationRequested += onDataCreationRequested;
        viewModel.FileExplorerPanel.ReferenceTreeRequested += onReferenceTreeRequested;
        viewModel.FileExplorerPanel.FilesChanging += onFileChangesStarting;
        viewModel.FileExplorerPanel.FilesChanged += onFileChangesApplied;
        viewModel.GameData.UiAssetsChanged += onUiAssetsChanged;
        viewModel.FileOpenFailed += onFileOpenFailed;
        viewModel.ProjectSave.SavePreparing += onSavePreparing;
        viewModel.NewProjectRequested += onNewProjectRequested;
        viewModel.OpenProjectRequested += onOpenProjectRequested;
        viewModel.ExitRequested += onExitRequested;
        viewModel.PreviewModeRequested += onPreviewModeRequested;
        viewModel.ActorOutlinerChanged += onActorOutlinerChanged;
        viewModel.LayerDisplayStateChanged += onLayerDisplayStateChanged;
        Title = viewModel.WindowTitle;
        refreshMapPanel();
        selectPreviewMode(MapEditMode.Tile);
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
        DispatcherTimer.RunOnce(
            () => viewModel?.FileExplorerPanel.NavigateTo(viewModel.FileExplorerPanel.CurrentPath),
            TimeSpan.FromMilliseconds(500)
        );
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
        else if (args.PropertyName == nameof(MainViewModel.SelectedLayerTab))
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
        if (viewModel is not null)
            await EditorSaveWorkflow.TrySaveAsync(this, viewModel.ProjectSave);
    }

    private void onSavePreparing(object? sender, EventArgs args)
    {
        foreach (BlueprintEditorWindow window in blueprintWindows.Values)
            window.FlushPendingChanges();
        foreach (UiAssetEditorWindow window in uiAssetWindows.Values)
            window.FlushPendingChanges();
        generalDataEditor?.FlushBlueprintEditors();
        commonFunctionWindow?.FlushPendingChanges();
    }

    private void onHistoryCompleted(object? sender, HistoryCompletedEventArgs args)
    {
        foreach (UiAssetEditorWindow window in uiAssetWindows.Values.ToArray())
        {
            if (!window.Reload())
                window.Close();
        }
        EditorFeedback.ShowHistory(toast, args.Action, args.Differences);
    }

    private void onMapActorSelectionChanged(object? sender, ActorSelectionChangedEventArgs args)
    {
        ActorInfoPanel.setActor(args.MapKey, args.LayerName, args.Index, args.ActorData);
        if (!string.IsNullOrWhiteSpace(args.BlueprintReference))
            viewModel?.ActorQueue.AddOrPromote(args.BlueprintReference);
        syncActorOutlinerSelection();
    }

    private void onActorDataChanged(object? sender, EventArgs args)
    {
        viewModel?.refreshActorOutliner();
        ActorInfoPanel.refreshActorPosition();
    }

    private void onActorTagChanged(object? sender, ActorSelectionChangedEventArgs args)
    {
        viewModel?.refreshActorOutliner();
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
            ?? (viewModel.SelectedLayerTab is { IsOverview: false } layer ? layer.Name : null);
        ActorOutlinerItemViewModel? selection = null;
        if (layerName is not null)
        {
            ActorOutlinerItemViewModel? layerItem = viewModel.ActorOutlinerItems
                .FirstOrDefault(item => item.LayerName == layerName);
            selection = EditorPanel.SelectedActorIndex is int actorIndex
                ? layerItem?.Children.FirstOrDefault(item => item.ActorIndex == actorIndex)
                : layerItem;
        }
        updatingActorOutlinerSelection = true;
        ActorOutliner.SelectedItem = selection;
        updatingActorOutlinerSelection = false;
    }

    private void onActorOutlinerSelectionChanged(object? sender, SelectionChangedEventArgs args)
    {
        if (updatingActorOutlinerSelection
            || viewModel is null
            || ActorOutliner.SelectedItem is not ActorOutlinerItemViewModel item)
        {
            return;
        }
        LayerTabViewModel? layer = viewModel.LayerTabs
            .FirstOrDefault(tab => !tab.IsOverview && tab.Name == item.LayerName);
        if (layer is null)
            return;
        viewModel.SelectedLayerTab = layer;
        EditorPanel.selectActor(item.LayerName, item.ActorIndex);
    }

    private void onBlueprintLocateRequested(object? sender, string reference)
    {
        string key = reference["Data.Blueprints.".Length..].Replace('.', Path.DirectorySeparatorChar);
        string path = Path.Combine(ProjectPath, "Data", "Blueprints", key + ".json");
        BottomTabs.SelectedIndex = 0;
        FileExplorerPanel.LocatePath(path);
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
