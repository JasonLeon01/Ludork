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

public partial class MainWindow : Window
{
    private const int MaximumConsoleLineCount = 5000;
    private readonly EditorSettings? editorSettings;
    private readonly ProjectRunnerService? projectRunner;
    private readonly ConsoleLogSession consoleLogSession = new();
    private readonly object consoleOutputSync = new();
    private readonly Queue<string> pendingConsoleLines = new();
    private readonly Queue<string> visibleConsoleLines = new();
    private MainViewModel? viewModel;
    private LayerTabViewModel? draggedLayer;
    private Point dragStart;
    private bool isDraggingLayer;
    private bool closeConfirmed;
    private bool closingPrompt;
    private bool layoutReady;
    private bool layoutSavePending;
    private TileSelectViewModel? tileSelect;
    private AnimationOverviewWindow? animationOverview;
    private TilesetEditorWindow? tilesetEditor;
    private GeneralDataEditorWindow? generalDataEditor;
    private CommonFunctionWindow? commonFunctionWindow;
    private GameVariableManagerWindow? gameVariableManager;
    private PerformanceMonitorWindow? performanceMonitorWindow;
    private PackSelectionDialog? packSelectionDialog;
    private PackLogDialog? packLogDialog;
    private readonly Toast toast;
    private readonly Dictionary<string, CurveWindow> curveWindows = new(StringComparer.Ordinal);
    private readonly Dictionary<string, TextConfigEditorWindow> textConfigWindows = new(StringComparer.Ordinal);
    private readonly Dictionary<string, BlueprintEditorWindow> blueprintWindows = new(StringComparer.Ordinal);
    private readonly Dictionary<string, UiAssetEditorWindow> uiAssetWindows = new(StringComparer.Ordinal);
    private readonly List<string> consoleHistory = new();
    private MapClipboard? mapClipboard;
    private int consoleHistoryIndex;
    private string consoleDraft = string.Empty;
    private bool settingConsoleHistoryText;
    private bool consoleSendPending;
    private bool consoleFlushScheduled;
    private bool consoleFollowsLatest = true;
    private bool consoleViewportUpdatePending;
    private int consoleViewportGeneration;
    private ScrollViewer? consoleScrollViewer;
    private bool updatingActorOutlinerSelection;
    private ActorPreviewService? actorPreviewService;
    private bool actorPreviewFallbackNotified;
    private Task gameInputSendTail = Task.CompletedTask;
    private bool projectLaunchPending;
    private bool projectLaunchCancelled;
    private bool projectRunReachedRunning;
    private bool gameLayoutLocked;
    private bool uiAssetRefreshPending;
    private string? lastActiveBlueprintKey;
    private ProjectWindowMode? activeWindowMode;
    private GridLength previousCenterWidth;
    private double previousCenterMinWidth;
    private double previousCenterMaxWidth;
    private GridLength previousUpperHeight;
    private double previousUpperMinHeight;
    private double previousUpperMaxHeight;
    private ColumnDefinition leftColumn => UpperGrid.ColumnDefinitions[0];
    private ColumnDefinition centerColumn => UpperGrid.ColumnDefinitions[2];
    private ColumnDefinition workspaceColumn => MainLayoutGrid.ColumnDefinitions[0];
    private ColumnDefinition rightColumn => MainLayoutGrid.ColumnDefinitions[2];
    private ColumnDefinition lowerLeftColumn => LowerGrid.ColumnDefinitions[0];
    private RowDefinition upperRow => MainLayoutGrid.RowDefinitions[0];
    private RowDefinition lowerRow => MainLayoutGrid.RowDefinitions[2];

    public string ProjectPath { get; } = string.Empty;

    public MainWindow()
    {
        InitializeComponent();
        installPluginMenus();
        toast = new Toast(this);
        initializeInteraction();
    }

    public MainWindow(EditorSettings settings, string projectPath)
    {
        editorSettings = settings;
        if (!Path.IsPathFullyQualified(projectPath))
            throw new ArgumentException(nameof(projectPath));
        ProjectPath = Path.GetFullPath(projectPath);
        projectRunner = new ProjectRunnerService(ProjectPath);
        InitializeComponent();
        installPluginMenus();
        toast = new Toast(this);
        initializeInteraction();
        Width = Math.Max(MinWidth, settings.Width);
        Height = Math.Max(MinHeight, settings.Height);
        leftColumn.Width = new GridLength(Math.Max(160, settings.UpperLeftWidth));
        rightColumn.Width = new GridLength(Math.Max(320, settings.UpperRightWidth));
        lowerLeftColumn.Width = new GridLength(Math.Max(180, settings.LowerLeftWidth));
    }

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
        ConsoleOutput.Document.UndoStack.SizeLimit = 0;
        ConsoleOutput.TextArea.TextView.Margin = new Thickness(12, 8, 12, 16);
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
        consoleScrollViewer = findConsoleScrollViewer();
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
        Width = Math.Max(MinWidth, editorSettings.Width);
        Height = Math.Max(MinHeight, editorSettings.Height);
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

    private async void onActionRequested(object? sender, string action)
    {
        if (action == "Help")
            showHelp();
        else if (action == "GameConfig" && viewModel is not null)
            await GameConfigWindow.ShowAsync(this, viewModel.GameConfig);
        else if (action == "SystemConfig" && viewModel is not null)
            await new ConfigWindow(viewModel.GameData, viewModel.ProjectSave).ShowDialog(this);
        else if (action == "Tilesets" && viewModel is not null)
            showTilesetEditor(viewModel.GameData, viewModel.TileSelect);
        else if (action == "NewAnimation" && viewModel is not null)
            await createAnimationAsync(viewModel.GameData);
        else if (action == "NewCurve" && viewModel is not null)
            await createCurveAsync(viewModel.GameData);
        else if (action == "AnimationOverview" && viewModel is not null)
            showAnimationOverview(viewModel.GameData);
        else if (action == "CommonFunctions" && viewModel is not null)
            await showCommonFunctionsAsync(viewModel);
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

    private async Task showCommonFunctionsAsync(MainViewModel mainViewModel)
    {
        if (commonFunctionWindow is not null)
        {
            commonFunctionWindow.Activate();
            return;
        }
        CommonFunctionWindow window = new(
            mainViewModel.GameData,
            mainViewModel.ProjectSave,
            mainViewModel.Metadata,
            mainViewModel.BlueprintClasses);
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

    private void showTilesetEditor(GameDataService gameData, TileSelectViewModel tileSelect)
    {
        if (tilesetEditor is not null)
        {
            tilesetEditor.Show();
            tilesetEditor.Activate();
            return;
        }
        tilesetEditor = new TilesetEditorWindow(gameData, viewModel!.ProjectSave, tileSelect);
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

    private void refreshMapPanel()
    {
        EditorPanel.refreshMap(viewModel?.SelectedMap?.Key, viewModel?.SelectedMapData);
        refreshMapPanelState();
    }

    private void refreshMapPanelState()
    {
        string? layerName = viewModel?.SelectedLayerTab is { IsOverview: false } layer ? layer.Name : null;
        EditorPanel.setSelectedLayer(layerName);
        EditorPanel.setSelectedLayerEditable(viewModel?.IsSelectedLayerEditable == true);
        ActorInfoPanel.setLayerEditable(viewModel?.IsSelectedLayerEditable == true);
        if (tileSelect is not null)
        {
            tileSelect.IsLayerSelected = EditorPanel.EditMode == MapEditMode.Tile && layerName is not null;
        }
        syncTileSelection();
    }

    private void onLightSelectionChanged(object? sender, LightSelectionChangedEventArgs args)
    {
        if (EditorPanel.EditMode == MapEditMode.Light)
            LightInfoPanel.setLight(args.LightData);
    }

    private void onLightDataChanged(object? sender, LightDataChangedEventArgs args)
    {
        if (EditorPanel.EditMode == MapEditMode.Light)
            LightInfoPanel.updateLight(args.LightData);
    }

    private void onLightEdited(object? sender, LightInfoEditedEventArgs args)
    {
        EditorPanel.updateSelectedLight(args.LightData);
    }

    private void onTileSelectPropertyChanged(object? sender, PropertyChangedEventArgs args)
    {
        if (args.PropertyName is nameof(TileSelectViewModel.SelectedTiles) or nameof(TileSelectViewModel.SelectedAutoTile))
            syncTileSelection();
    }

    private void syncTileSelection()
    {
        EditorPanel.setTileSelection(tileSelect?.SelectedTiles, tileSelect?.SelectedAutoTile?.Key);
    }

    private void onTileSelectionPicked(object? sender, TileSelectionChangedEventArgs args)
    {
        if (tileSelect is null)
            return;
        if (!string.IsNullOrWhiteSpace(args.AutoTileKey))
        {
            tileSelect.SelectedAutoTile = tileSelect.AutoTiles.FirstOrDefault(item => item.Key == args.AutoTileKey);
            return;
        }
        if (args.Tiles is { } tiles)
        {
            tileSelect.selectTiles(tiles.OriginTileNumber, tiles.Width, tiles.Height);
            return;
        }
        tileSelect.ClearSelection();
    }

    private void onTileModeClick(object? sender, RoutedEventArgs args) => selectPreviewMode(MapEditMode.Tile);

    private void onLightModeClick(object? sender, RoutedEventArgs args) => selectPreviewMode(MapEditMode.Light);

    private void onActorModeClick(object? sender, RoutedEventArgs args) => selectPreviewMode(MapEditMode.Actor);

    private async void onEditRunModeClick(object? sender, RoutedEventArgs args)
    {
        if (projectLaunchPending)
        {
            projectLaunchCancelled = true;
            setProjectRunState(ProjectRunState.Idle);
            return;
        }
        if (projectRunner is not null && projectRunner.State != ProjectRunState.Idle)
        {
            GamePanel.SetInputEnabled(false);
            long generation = projectRunner.RunGeneration;
            await projectRunner.SetPerformanceMonitoringAsync(false, generation);
            await projectRunner.StopAsync(generation);
            return;
        }
        setProjectRunState(ProjectRunState.Idle);
    }

    private async void onPlayRunModeClick(object? sender, RoutedEventArgs args)
    {
        if (projectLaunchPending
            || projectRunner is null
            || viewModel is null
            || projectRunner.State != ProjectRunState.Idle)
        {
            setProjectRunState(projectRunner?.State ?? ProjectRunState.Idle);
            return;
        }
        if (!await EditorSaveWorkflow.TrySaveAsync(
                this,
                viewModel.ProjectSave,
                false))
        {
            return;
        }

        resetConsoleOutput();
        performanceMonitorWindow?.ClearData();
        BottomTabs.SelectedIndex = 1;
        ProjectWindowMode windowMode = viewModel.IndividualWindow
            ? ProjectWindowMode.Individual
            : ProjectWindowMode.Embedded;
        projectLaunchPending = true;
        projectLaunchCancelled = false;
        setProjectRunState(ProjectRunState.Building);
        nint windowHandle = await prepareGameViewportAsync(windowMode);
        if (projectLaunchCancelled)
        {
            projectLaunchPending = false;
            setProjectRunState(ProjectRunState.Idle);
            return;
        }
        if (windowMode == ProjectWindowMode.Embedded && windowHandle == nint.Zero)
        {
            projectLaunchPending = false;
            setProjectRunState(ProjectRunState.Idle);
            await AlertDialog.ShowAsync(
                this,
                LocaleService.Get("PLAY_ERROR"),
                LocaleService.Get("RUN_EMBED_HANDLE_UNAVAILABLE")
            );
            return;
        }
        ProjectRunOptions options = new(
            viewModel.ProjectConfig.IsStandalone,
            windowMode,
            windowHandle);
        string? logError = consoleLogSession.Start(ProjectPath);
        if (logError is not null)
            appendConsoleLine("[Console] Failed to create the log file: " + logError);
        Task<ProjectRunResult> runTask = projectRunner.StartAsync(options);
        projectLaunchPending = false;
        ProjectRunResult result;
        try
        {
            result = await runTask;
        }
        finally
        {
            string? logStopError = consoleLogSession.Stop();
            if (logStopError is not null)
                appendConsoleLine("[Console] Failed to close the log file: " + logStopError);
        }
        setProjectRunState(projectRunner.State);
        if (result.Success || result.Cancelled)
            return;
        await AlertDialog.ShowAsync(
            this,
            LocaleService.Get("PLAY_ERROR"),
            getProjectRunFailureMessage(result)
        );
    }

    private void onProjectOutputReceived(object? sender, string line)
    {
        appendConsoleLine(line);
    }

    private void onPerformanceSampleReceived(object? sender, PerformanceSample sample)
    {
        performanceMonitorWindow?.AddSample(sample);
    }

    private void onProjectRunStateChanged(object? sender, ProjectRunState state)
    {
        Dispatcher.UIThread.Post(() =>
        {
            setProjectRunState(state);
        });
    }

    private void onCommandAvailabilityChanged(object? sender, bool available)
    {
        Dispatcher.UIThread.Post(() =>
        {
            updateConsoleInputState();
            GamePanel.SetInputEnabled(
                available
                && projectRunner?.State == ProjectRunState.Running
                && activeWindowMode == ProjectWindowMode.Embedded);
            if (!available || projectRunner is null)
                return;
            bool enabled = performanceMonitorWindow is not null;
            if (enabled)
                performanceMonitorWindow?.ClearData();
            _ = projectRunner.SetPerformanceMonitoringAsync(enabled, projectRunner.RunGeneration);
        });
    }

    private void onOpenPerformanceMonitor(object? sender, EventArgs args)
    {
        if (performanceMonitorWindow is not null)
        {
            performanceMonitorWindow.Show();
            performanceMonitorWindow.Activate();
            return;
        }
        PerformanceMonitorWindow window = new();
        performanceMonitorWindow = window;
        window.Closed += onPerformanceMonitorClosed;
        window.Show(this);
        if (projectRunner?.CanSendCommand == true)
            _ = projectRunner.SetPerformanceMonitoringAsync(true, projectRunner.RunGeneration);
    }

    private void onPerformanceMonitorClosed(object? sender, EventArgs args)
    {
        if (sender is PerformanceMonitorWindow window)
            window.Closed -= onPerformanceMonitorClosed;
        performanceMonitorWindow = null;
        if (projectRunner?.CanSendCommand == true)
            _ = projectRunner.SetPerformanceMonitoringAsync(false, projectRunner.RunGeneration);
    }

    private void onGameInputBatchReady(object? sender, GameInputBatchEventArgs args)
    {
        if (projectRunner?.State != ProjectRunState.Running
            || !projectRunner.CanSendCommand
            || activeWindowMode != ProjectWindowMode.Embedded)
        {
            return;
        }
        gameInputSendTail = sendGameInputBatchAsync(gameInputSendTail, args.Events);
    }

    private async Task sendGameInputBatchAsync(
        Task previousBatch,
        IReadOnlyList<RuntimeInputEvent> events)
    {
        await previousBatch;
        if (projectRunner?.State != ProjectRunState.Running
            || !projectRunner.CanSendCommand
            || activeWindowMode != ProjectWindowMode.Embedded)
        {
            return;
        }
        await projectRunner.SendInputBatchAsync(events);
    }

    private async void onConsoleSendClick(object? sender, RoutedEventArgs args)
    {
        await sendConsoleCommandAsync();
    }

    private async void onConsoleInputKeyDown(object? sender, KeyEventArgs args)
    {
        if (args.Key == Key.Enter)
        {
            args.Handled = true;
            await sendConsoleCommandAsync();
        }
        else if (args.Key == Key.Up)
        {
            args.Handled = navigateConsoleHistory(-1);
        }
        else if (args.Key == Key.Down)
        {
            args.Handled = navigateConsoleHistory(1);
        }
    }

    private void onConsoleInputTextChanged()
    {
        if (!settingConsoleHistoryText && consoleHistoryIndex != consoleHistory.Count)
        {
            consoleHistoryIndex = consoleHistory.Count;
            consoleDraft = ConsoleInput.Text ?? string.Empty;
        }
        updateConsoleInputState();
    }

    private async Task sendConsoleCommandAsync()
    {
        if (projectRunner is null || consoleSendPending)
            return;
        string command = ConsoleInput.Text?.Trim() ?? string.Empty;
        if (command.Length == 0)
            return;

        consoleSendPending = true;
        updateConsoleInputState();
        bool sent = await projectRunner.SendCommandAsync(command);
        consoleSendPending = false;
        if (!sent)
        {
            appendConsoleLine(LocaleService.Get("CONSOLE_SEND_FAILED"));
            updateConsoleInputState();
            return;
        }

        appendConsoleLine(">>> " + command);
        consoleHistory.Add(command);
        consoleHistoryIndex = consoleHistory.Count;
        consoleDraft = string.Empty;
        setConsoleInputText(string.Empty);
    }

    private bool navigateConsoleHistory(int direction)
    {
        if (consoleHistory.Count == 0)
            return false;
        if (direction < 0)
        {
            if (consoleHistoryIndex == consoleHistory.Count)
                consoleDraft = ConsoleInput.Text ?? string.Empty;
            if (consoleHistoryIndex > 0)
                consoleHistoryIndex--;
        }
        else
        {
            if (consoleHistoryIndex >= consoleHistory.Count)
                return false;
            consoleHistoryIndex++;
        }

        string text = consoleHistoryIndex < consoleHistory.Count
            ? consoleHistory[consoleHistoryIndex]
            : consoleDraft;
        setConsoleInputText(text);
        return true;
    }

    private void setConsoleInputText(string text)
    {
        settingConsoleHistoryText = true;
        ConsoleInput.Text = text;
        ConsoleInput.CaretIndex = text.Length;
        settingConsoleHistoryText = false;
        updateConsoleInputState();
    }

    private void updateConsoleInputState()
    {
        bool available = projectRunner?.State == ProjectRunState.Running
            && projectRunner.CanSendCommand;
        ConsoleInput.IsEnabled = available;
        ConsoleSendButton.IsEnabled = available
            && !consoleSendPending
            && !string.IsNullOrWhiteSpace(ConsoleInput.Text);
    }

    private void appendConsoleLine(string line)
    {
        bool scheduleFlush = false;
        lock (consoleOutputSync)
        {
            enqueuePendingConsoleText(line);
            string? logError = consoleLogSession.WriteLine(line);
            if (logError is not null)
                enqueuePendingConsoleText("[Console] Failed to write the log file: " + logError);
            if (!consoleFlushScheduled)
            {
                consoleFlushScheduled = true;
                scheduleFlush = true;
            }
        }
        if (scheduleFlush)
        {
            Dispatcher.UIThread.Post(
                flushConsoleLines,
                DispatcherPriority.Background);
        }
    }

    private void enqueuePendingConsoleText(string text)
    {
        string normalized = text.Replace("\r\n", "\n", StringComparison.Ordinal).Replace('\r', '\n');
        string[] lines = normalized.Split('\n');
        foreach (string line in lines)
            pendingConsoleLines.Enqueue(line);
    }

    private void flushConsoleLines()
    {
        List<string> batch = new();
        lock (consoleOutputSync)
        {
            while (pendingConsoleLines.Count > 0)
                batch.Add(pendingConsoleLines.Dequeue());
            consoleFlushScheduled = false;
        }
        if (batch.Count == 0)
            return;

        ScrollViewer? scrollViewer = findConsoleScrollViewer();
        Vector previousOffset = scrollViewer?.Offset ?? default;
        bool followBottom = consoleFollowsLatest;
        TextDocument document = ConsoleOutput.Document;
        StringBuilder appendedText = new();
        if (visibleConsoleLines.Count > 0)
            appendedText.Append(Environment.NewLine);
        appendedText.AppendJoin(Environment.NewLine, batch);
        foreach (string line in batch)
            visibleConsoleLines.Enqueue(line);

        int removeCount = Math.Max(0, visibleConsoleLines.Count - MaximumConsoleLineCount);
        int removedCharacterCount = 0;
        for (int index = 0; index < removeCount; index++)
        {
            string removed = visibleConsoleLines.Dequeue();
            removedCharacterCount += removed.Length + Environment.NewLine.Length;
        }

        consoleViewportUpdatePending = true;
        using (document.RunUpdate())
        {
            document.Insert(document.TextLength, appendedText.ToString());
            if (removedCharacterCount > 0)
                document.Remove(0, removedCharacterCount);
        }

        int generation = ++consoleViewportGeneration;
        Dispatcher.UIThread.Post(
            () => restoreConsoleViewport(
                generation,
                previousOffset,
                followBottom),
            DispatcherPriority.Background);
    }

    private ScrollViewer? findConsoleScrollViewer()
    {
        if (consoleScrollViewer is not null)
            return consoleScrollViewer;
        consoleScrollViewer = ConsoleOutput.GetVisualDescendants()
            .OfType<ScrollViewer>()
            .FirstOrDefault(viewer => viewer.Name == "PART_ScrollViewer")
            ?? ConsoleOutput.GetVisualDescendants()
                .OfType<ScrollViewer>()
                .FirstOrDefault();
        if (consoleScrollViewer is not null)
            consoleScrollViewer.ScrollChanged += onConsoleScrollChanged;
        return consoleScrollViewer;
    }

    private void onConsoleScrollChanged(object? sender, ScrollChangedEventArgs args)
    {
        if (consoleViewportUpdatePending || sender is not ScrollViewer scrollViewer)
            return;
        double maximumY = Math.Max(0, scrollViewer.Extent.Height - scrollViewer.Viewport.Height);
        if (scrollViewer.Offset.Y >= maximumY - 2)
            consoleFollowsLatest = true;
        else if (args.OffsetDelta.Y < 0)
            consoleFollowsLatest = false;
    }

    private void restoreConsoleViewport(int generation, Vector previousOffset, bool followBottom)
    {
        if (generation != consoleViewportGeneration)
            return;
        ScrollViewer? scrollViewer = findConsoleScrollViewer();
        if (scrollViewer is null)
        {
            consoleViewportUpdatePending = false;
            return;
        }
        double maximumX = Math.Max(0, scrollViewer.Extent.Width - scrollViewer.Viewport.Width);
        double maximumY = Math.Max(0, scrollViewer.Extent.Height - scrollViewer.Viewport.Height);
        double targetX = Math.Clamp(previousOffset.X, 0, maximumX);
        double targetY = followBottom
            ? maximumY
            : Math.Clamp(previousOffset.Y, 0, maximumY);
        scrollViewer.Offset = new Vector(targetX, targetY);
        consoleViewportUpdatePending = false;
    }

    private void resetConsoleOutput()
    {
        lock (consoleOutputSync)
        {
            pendingConsoleLines.Clear();
            visibleConsoleLines.Clear();
        }
        consoleViewportGeneration++;
        consoleViewportUpdatePending = true;
        ConsoleOutput.Clear();
        ScrollViewer? scrollViewer = findConsoleScrollViewer();
        if (scrollViewer is not null)
            scrollViewer.Offset = default;
        consoleViewportUpdatePending = false;
        consoleFollowsLatest = true;
    }

    private void setProjectRunState(ProjectRunState state)
    {
        bool active = state != ProjectRunState.Idle;
        if (state == ProjectRunState.Running)
            projectRunReachedRunning = true;
        bool returnedFromRun = !active && projectRunReachedRunning;
        if (returnedFromRun)
            projectRunReachedRunning = false;
        EditRunModeToggle.IsChecked = !active;
        PlayRunModeToggle.IsChecked = active;
        EditRunModeToggle.IsEnabled = true;
        PlayRunModeToggle.IsEnabled = !active;
        EditModeToggles.IsEnabled = !active;
        EditorPanel.IsEnabled = !active;
        MapList.IsEnabled = !active;
        LayerTabs.IsEnabled = !active;
        RightList.IsEnabled = !active;
        LightInfoPanel.IsEnabled = !active;
        ActorInfoPanel.IsEnabled = !active;
        RightModePanel.IsEnabled = !active;
        FileExplorerPanel.IsEnabled = !active;
        if (!active)
        {
            if (returnedFromRun
                && viewModel is not null
                && !viewModel.GameConfig.IsModified)
            {
                viewModel.GameConfig.Reload();
            }
            restoreEditorViewport();
        }
        else
            GamePanel.SetInputEnabled(
                projectRunner?.CanSendCommand == true
                && state == ProjectRunState.Running
                && activeWindowMode == ProjectWindowMode.Embedded);
        updateConsoleInputState();
    }

    private async Task<nint> prepareGameViewportAsync(ProjectWindowMode windowMode)
    {
        activeWindowMode = windowMode;
        EditorPanel.IsVisible = false;
        GameViewport.IsVisible = true;
        GameViewport.InvalidateMeasure();
        await Dispatcher.UIThread.InvokeAsync(
            () => GamePanel.TryUpdateNativeControlPosition(),
            DispatcherPriority.Render);
        lockGameLayout();
        if (windowMode != ProjectWindowMode.Embedded)
            return nint.Zero;
        if (GamePanel.NativeHandle == nint.Zero)
        {
            TaskCompletionSource handleReady = new();
            void onHandleReady(object? sender, EventArgs args) => handleReady.TrySetResult();
            GamePanel.NativeHandleReady += onHandleReady;
            GameViewport.InvalidateMeasure();
            Task completed = await Task.WhenAny(handleReady.Task, Task.Delay(TimeSpan.FromSeconds(1)));
            GamePanel.NativeHandleReady -= onHandleReady;
            if (completed != handleReady.Task)
                return nint.Zero;
        }
        nint handle = GamePanel.NativeHandle;
        await Dispatcher.UIThread.InvokeAsync(
            () => GamePanel.TryUpdateNativeControlPosition(),
            DispatcherPriority.Render);
        return handle != nint.Zero && handle == GamePanel.NativeHandle
            ? handle
            : nint.Zero;
    }

    private void lockGameLayout()
    {
        if (gameLayoutLocked)
            return;
        double width = CenterArea.Bounds.Width;
        double height = UpperGrid.Bounds.Height;
        if (width <= 0 || height <= 0)
            return;
        previousCenterWidth = centerColumn.Width;
        previousCenterMinWidth = centerColumn.MinWidth;
        previousCenterMaxWidth = centerColumn.MaxWidth;
        previousUpperHeight = upperRow.Height;
        previousUpperMinHeight = upperRow.MinHeight;
        previousUpperMaxHeight = upperRow.MaxHeight;
        centerColumn.Width = new GridLength(width);
        centerColumn.MinWidth = width;
        centerColumn.MaxWidth = width;
        upperRow.Height = new GridLength(height);
        upperRow.MinHeight = height;
        upperRow.MaxHeight = height;
        UpperLeftSplitter.IsEnabled = false;
        UpperRightSplitter.IsEnabled = false;
        UpperLowerSplitter.IsEnabled = false;
        gameLayoutLocked = true;
    }

    private void restoreEditorViewport()
    {
        GamePanel.SetInputEnabled(false);
        GameViewport.IsVisible = false;
        EditorPanel.IsVisible = true;
        activeWindowMode = null;
        if (gameLayoutLocked)
        {
            centerColumn.Width = previousCenterWidth;
            centerColumn.MinWidth = previousCenterMinWidth;
            centerColumn.MaxWidth = previousCenterMaxWidth;
            upperRow.Height = previousUpperHeight;
            upperRow.MinHeight = previousUpperMinHeight;
            upperRow.MaxHeight = previousUpperMaxHeight;
            UpperLeftSplitter.IsEnabled = true;
            UpperRightSplitter.IsEnabled = true;
            UpperLowerSplitter.IsEnabled = true;
            gameLayoutLocked = false;
        }
        Dispatcher.UIThread.Post(() =>
        {
            if (!IsActive
                || projectRunner?.State != ProjectRunState.Idle
                || activeWindowMode is not null
                || !EditorPanel.IsVisible
                || !EditorPanel.IsEnabled)
            {
                return;
            }
            EditorPanel.Focus();
        }, DispatcherPriority.Input);
    }

    private static string getProjectRunFailureMessage(ProjectRunResult result)
    {
        string key = result.Failure switch
        {
            ProjectRunFailure.ProjectInvalid => "RUN_PROJECT_INVALID",
            ProjectRunFailure.PluginPreparationFailed => "RUN_PLUGIN_PREPARATION_FAILED",
            ProjectRunFailure.BuildToolMissing => "RUN_BUILD_TOOL_MISSING",
            ProjectRunFailure.BuildFailed => "RUN_BUILD_FAILED",
            ProjectRunFailure.ExecutableMissing => "RUN_EXECUTABLE_MISSING",
            ProjectRunFailure.EmbeddedHandleUnavailable => "RUN_EMBED_HANDLE_UNAVAILABLE",
            ProjectRunFailure.ProtocolMismatch => "RUN_PROTOCOL_MISMATCH",
            ProjectRunFailure.GameFailed => "RUN_GAME_FAILED",
            _ => "RUN_LAUNCH_FAILED",
        };
        string message = LocaleService.Get(key);
        return string.IsNullOrWhiteSpace(result.Detail)
            ? message
            : message + Environment.NewLine + result.Detail;
    }

    private void onPreviewModeRequested(object? sender, int modeIndex)
    {
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
        if (projectRunner is not null && projectRunner.State != ProjectRunState.Idle)
        {
            long generation = projectRunner.RunGeneration;
            await projectRunner.SetPerformanceMonitoringAsync(false, generation);
            await projectRunner.StopAsync(generation);
        }
        closeConfirmed = true;
        Close();
    }

    private async Task<bool> confirmLeaveAsync()
    {
        if (viewModel?.IsModified != true)
            return true;
        UnsavedChangesResult result = await new UnsavedChangesDialog().ShowDialog<UnsavedChangesResult>(this);
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
        bool tileMode = mode == MapEditMode.Tile;
        bool lightMode = mode == MapEditMode.Light;
        bool actorMode = mode == MapEditMode.Actor;
        LayerTabs.IsEnabled = !lightMode;
        if (lightMode && viewModel is not null)
            viewModel.SelectedLayerTab = null;
        TileModeToggle.IsChecked = tileMode;
        LightModeToggle.IsChecked = lightMode;
        ActorModeToggle.IsChecked = actorMode;
        EditorPanel.setEditMode(mode);
        RightList.IsVisible = tileMode;
        LightInfoPanel.IsVisible = false;
        ActorModePanel.IsVisible = actorMode;
        if (lightMode)
        {
            EditorPanel.clearLightSelection();
            LightInfoPanel.setLight(null);
        }
        if (actorMode)
            viewModel?.refreshActorOutliner();
        refreshMapPanelState();
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
            showLayerContextMenu(layer, item as Control ?? LayerTabs);
            args.Handled = true;
            return;
        }
        if (!point.Properties.IsLeftButtonPressed || layer is null || layer.IsOverview)
            return;
        draggedLayer = layer;
        dragStart = args.GetPosition(LayerTabs);
        isDraggingLayer = false;
    }

    private void onLayerVisibilityClick(object? sender, RoutedEventArgs args)
    {
        if (viewModel is null || (sender as Control)?.DataContext is not LayerTabViewModel layer)
            return;
        viewModel.SelectedLayerTab = layer;
        viewModel.setLayerVisible(layer, !layer.LayerVisible);
        args.Handled = true;
    }

    private void onMapListPointerPressed(object? sender, PointerPressedEventArgs args)
    {
        if (viewModel is null || !args.GetCurrentPoint(MapList).Properties.IsRightButtonPressed)
            return;
        ListBoxItem? item = getMapListItem(args.Source);
        MapListItemViewModel? map = item?.DataContext as MapListItemViewModel;
        if (map is not null)
            viewModel.SelectedMap = map;
        showMapContextMenu(map);
        args.Handled = true;
    }

    private async void onMapListDoubleTapped(object? sender, TappedEventArgs args)
    {
        ListBoxItem? item = getMapListItem(args.Source);
        if (item?.DataContext is not MapListItemViewModel map)
            return;
        await editMapAsync(map.Key);
        args.Handled = true;
    }

    private async void onMapListKeyDown(object? sender, KeyEventArgs args)
    {
        if (viewModel is null)
            return;
        bool primary = EditorShortcuts.HasPrimaryModifier(args.KeyModifiers);
        if (primary && args.Key == Key.C && viewModel.SelectedMap is MapListItemViewModel copyMapItem)
            copyMap(copyMapItem.Key);
        else if (primary && args.Key == Key.V)
            pasteMap();
        else if (args.Key == Key.Delete && viewModel.SelectedMap is MapListItemViewModel deleteMapItem)
            await deleteMapAsync(deleteMapItem.Key);
        else if (args.Key is Key.Enter or Key.Return && viewModel.SelectedMap is MapListItemViewModel editMapItem)
            await editMapAsync(editMapItem.Key);
        else
            return;
        args.Handled = true;
    }

    private void showMapContextMenu(MapListItemViewModel? map)
    {
        if (viewModel is null)
            return;
        ContextMenu menu = new();
        if (map is null)
        {
            MenuItem newMap = new() { Header = LocaleService.Get("NEW_MAP") };
            newMap.Click += async (_, _) => await createMapAsync();
            MenuItem pasteItem = new() { Header = LocaleService.Get("PASTE"), IsEnabled = mapClipboard is not null };
            pasteItem.Click += (_, _) => pasteMap();
            menu.Items.Add(newMap);
            menu.Items.Add(pasteItem);
        }
        else
        {
            MenuItem editMap = new() { Header = LocaleService.Get("MAPLIST_EDIT") };
            editMap.Click += async (_, _) => await editMapAsync(map.Key);
            MenuItem copyItem = new() { Header = LocaleService.Get("COPY") };
            copyItem.Click += (_, _) => copyMap(map.Key);
            MenuItem deleteItem = new() { Header = LocaleService.Get("DELETE") };
            deleteItem.Click += async (_, _) => await deleteMapAsync(map.Key);
            menu.Items.Add(editMap);
            menu.Items.Add(copyItem);
            menu.Items.Add(deleteItem);
            if (Application.Current is App app)
            {
                app.appendPluginMapContextMenuCommands(
                    this,
                    menu,
                    map.Key,
                    createMapEditorHost(map.Key));
            }
        }
        menu.Open(MapList);
    }

    private static bool tryGetUiAssetKey(
        string uiRoot,
        string path,
        GameDataService gameData,
        out string key)
    {
        string relative = Path.GetRelativePath(uiRoot, Path.GetFullPath(path));
        if (!isRelativePathInside(relative)
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
            && gameData.UiAssetsData.ContainsKey(UiAssetSchema.ToAssetDataKey(key));
    }

    private IMapEditorHost createMapEditorHost(string mapKey)
    {
        return new MapEditorHostBridge(
            viewModel!.GameData,
            viewModel.PreviewService,
            mapKey,
            refreshPluginMap,
            viewModel.canEditLayer);
    }

    private void refreshPluginMap(string mapKey, string layerName)
    {
        if (viewModel is null)
            return;
        MapListItemViewModel? map = viewModel.Maps.FirstOrDefault(
            item => string.Equals(item.Key, mapKey, StringComparison.Ordinal));
        if (map is null)
            return;
        viewModel.SelectedMap = map;
        LayerTabViewModel? layer = viewModel.LayerTabs.FirstOrDefault(
            item => !item.IsOverview
                && string.Equals(item.Name, layerName, StringComparison.Ordinal));
        if (layer is not null)
            viewModel.SelectedLayerTab = layer;
        selectPreviewMode(MapEditMode.Tile);
        refreshMapPanel();
    }

    private async Task createMapAsync()
    {
        if (viewModel is null)
            return;
        MapInfo initial = new()
        {
            FileName = viewModel.GameData.getNewMapFileName(),
            MapName = LocaleService.Get("NEW_MAP_DEFAULT_NAME"),
            Width = 13,
            Height = 13,
        };
        MapInfo? result = await MapEditWindow.ShowAsync(this, viewModel.GameData, initial, string.Empty, true);
        if (result is not null && viewModel.GameData.CreateMap(result))
            viewModel.refreshMaps(normaliseMapKey(result.FileName));
    }

    private async Task editMapAsync(string key)
    {
        if (viewModel?.GameData.getMapInfo(key) is not MapInfo initial)
            return;
        MapInfo? result = await MapEditWindow.ShowAsync(this, viewModel.GameData, initial, key, false);
        if (result is not null && viewModel.GameData.UpdateMap(key, result))
            viewModel.refreshMaps(normaliseMapKey(result.FileName));
    }

    private void copyMap(string key)
    {
        if (viewModel?.GameData.getMap(key) is JsonObject map)
            mapClipboard = new MapClipboard(key, (JsonObject)map.DeepClone());
    }

    private void pasteMap()
    {
        if (viewModel is null || mapClipboard is null)
            return;
        string? copyKey = viewModel.GameData.PasteMap(mapClipboard.Data, mapClipboard.SourceKey);
        if (copyKey is not null)
            viewModel.refreshMaps(copyKey);
    }

    private async Task deleteMapAsync(string key)
    {
        if (viewModel is null || !await ConfirmationDialog.ShowAsync(this, LocaleService.Get("CONFIRM_DELETE"), LocaleService.Get("DELETE_CONFIRMATION")))
            return;
        if (viewModel.GameData.DeleteMap(key))
            viewModel.refreshMaps();
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
        viewModel.moveLayer(draggedLayer, target);
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
        if (viewModel is null)
            return;
        if (layer is { IsOverview: false })
            viewModel.SelectedLayerTab = layer;
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
            visibilityItem.Click += (_, _) => viewModel.setLayerVisible(layer, !layer.LayerVisible);
            MenuItem addItem = new MenuItem { Header = LocaleService.Get("ADD_LAYER") };
            addItem.Click += async (_, _) => await addLayerAsync(layer.Name);
            MenuItem renameItem = new MenuItem { Header = LocaleService.Get("RENAME_LAYER") };
            renameItem.Click += async (_, _) => await renameLayerAsync(layer.Name);
            MenuItem copyItem = new MenuItem { Header = LocaleService.Get("COPY") };
            copyItem.Click += (_, _) => viewModel.copyLayer(layer.Name);
            MenuItem pasteItem = new MenuItem { Header = LocaleService.Get("PASTE"), IsEnabled = viewModel.CanPasteLayer };
            pasteItem.Click += (_, _) => viewModel.pasteLayer(layer.Name);
            MenuItem selectShaderItem = new MenuItem { Header = LocaleService.Get("SELECT_LAYER_SHADER") };
            selectShaderItem.Click += async (_, _) => await selectLayerShaderAsync(layer.Name);
            MenuItem clearShaderItem = new MenuItem
            {
                Header = LocaleService.Get("CLEAR_LAYER_SHADER"),
                IsEnabled = !string.IsNullOrWhiteSpace(viewModel.getLayerShaderPath(layer.Name)),
            };
            clearShaderItem.Click += (_, _) => viewModel.setLayerShaderPath(layer.Name, string.Empty);
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
            viewModel.LayerTabs.Where(item => !item.IsOverview).Select(item => item.Name));
        if (name is not null)
            viewModel.addLayer(name, insertAfterLayer);
    }

    private async System.Threading.Tasks.Task renameLayerAsync(string oldName)
    {
        if (viewModel is null)
            return;
        string? name = await SingleRowDialog.ShowAsync(
            this,
            LocaleService.Get("RENAME_LAYER"),
            LocaleService.Get("RENAME_MESSAGE"),
            viewModel.LayerTabs.Where(item => !item.IsOverview && item.Name != oldName).Select(item => item.Name),
            oldName);
        if (name is not null
            && !string.Equals(name, oldName, StringComparison.Ordinal)
            && viewModel.renameLayer(oldName, name))
        {
            EditorPanel.refreshSelectedActor();
        }
    }

    private async System.Threading.Tasks.Task selectLayerShaderAsync(string layerName)
    {
        if (viewModel is null)
            return;
        string? shaderPath = await FileSelectorDialog.SelectLayerShaderAsync(this, viewModel.GameData.ProjectPath);
        if (shaderPath is not null)
            viewModel.setLayerShaderPath(layerName, shaderPath);
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
            viewModel.deleteLayer(layerName);
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

    private static ListBoxItem? getMapListItem(object? source)
    {
        if (source is ListBoxItem item)
            return item;
        return (source as Visual)?.GetVisualAncestors().OfType<ListBoxItem>().FirstOrDefault();
    }

    private static string normaliseMapKey(string fileName)
    {
        string key = fileName.Replace('\\', '/').Trim().Trim('/');
        return key.EndsWith(".json", StringComparison.OrdinalIgnoreCase) ? key[..^5] : key;
    }

    private bool containsPoint(TabStripItem item, Point point)
    {
        Point? origin = item.TranslatePoint(new Point(), LayerTabs);
        return origin is not null && new Rect(origin.Value, item.Bounds.Size).Contains(point);
    }

    private async void onClosing(object? sender, WindowClosingEventArgs args)
    {
        foreach (BlueprintEditorWindow window in blueprintWindows.Values.ToArray())
            window.FlushPendingChanges();
        foreach (UiAssetEditorWindow window in uiAssetWindows.Values.ToArray())
            window.FlushPendingChanges();
        generalDataEditor?.FlushBlueprintEditors();
        if (closeConfirmed)
            return;
        if (projectLaunchPending)
            projectLaunchCancelled = true;
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
            UnsavedChangesResult result = await new UnsavedChangesDialog().ShowDialog<UnsavedChangesResult>(this);
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
        consoleLogSession.Dispose();
        if (actorPreviewService is not null)
        {
            actorPreviewService.StatusChanged -= onActorPreviewStatusChanged;
            actorPreviewService = null;
        }
        if (projectRunner is not null)
        {
            projectRunner.OutputReceived -= onProjectOutputReceived;
            projectRunner.StateChanged -= onProjectRunStateChanged;
            projectRunner.CommandAvailabilityChanged -= onCommandAvailabilityChanged;
            projectRunner.PerformanceSampleReceived -= onPerformanceSampleReceived;
            projectRunner.Dispose();
        }
        GamePanel.InputBatchReady -= onGameInputBatchReady;
        viewModel?.Dispose();
        base.OnClosed(args);
    }
}

internal sealed record MapClipboard(string SourceKey, JsonObject Data);
