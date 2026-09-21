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
using Ludork.Services.BlueprintAssistant;
using Ludork.Services.Plugins;
using Ludork.ViewModels;
using Ludork.Views.Utils;
using Ludork.Views.Coordination;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.Json.Nodes;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Views;

public partial class MainWindow : Window, IProjectOperationInteraction
{
    private readonly EditorSettings? editorSettings;
    private readonly PluginMenuCoordinator pluginMenus;
    private readonly EditorProjectSession? projectSession;
    private DocumentWindowCoordinator? documentWindows;
    private readonly ProjectRunnerService? projectRunner;
    private readonly ProjectOperationCoordinator? projectOperations;
    private readonly ProjectPackCoordinator? projectPack;
    private readonly LiveDebugCoordinator? liveDebug;
    private readonly ConsoleLogSession consoleLogSession = new();
    private readonly object consoleOutputSync = new();
    private readonly LogTextViewController consoleLogView;
    private MainViewModel? viewModel;
    private LayerTabViewModel? draggedLayer;
    private Point dragStart;
    private bool isDraggingLayer;
    private string? lightSelectionMapKey;
    private string? lightSelectionLayerName;
    private bool closeConfirmed;
    private bool closingPrompt;
    private bool layoutReady;
    private bool layoutSavePending;
    private TileSelectViewModel? tileSelect;
    private PerformanceMonitorWindow? performanceMonitorWindow;
    private readonly Toast toast;
    private readonly List<string> consoleHistory = new();
    private int consoleHistoryIndex;
    private string consoleDraft = string.Empty;
    private bool settingConsoleHistoryText;
    private bool consoleSendPending;
    private bool updatingActorOutlinerSelection;
    private ActorPreviewService? actorPreviewService;
    private bool actorPreviewFallbackNotified;
    private Task gameInputSendTail = Task.CompletedTask;
    private bool projectRunReachedRunning;
    private bool gameLayoutLocked;
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

    internal IBlueprintAssistantHost? CreateBlueprintAssistantHost() => documentWindows?.CreateBlueprintAssistantHost();

    public MainWindow()
    {
        InitializeComponent();
        consoleLogView = new LogTextViewController(ConsoleOutput);
        pluginMenus = new PluginMenuCoordinator(this, ProjectPath);
        toast = new Toast(this);
        initializeInteraction();
    }

    public MainWindow(EditorSettings settings, EditorProjectSession session)
    {
        editorSettings = settings;
        projectSession = session;
        ProjectPath = session.ProjectPath;
        projectRunner = session.ProjectRunner;
        projectOperations = new ProjectOperationCoordinator(projectRunner, session.ProjectConfig, this);
        projectOperations.StateChanged += onProjectOperationStateChanged;
        projectPack = new ProjectPackCoordinator(this, session, projectOperations);
        liveDebug = new LiveDebugCoordinator(session.MapWorkspace, session.GameData, projectRunner);
        liveDebug.EditingContextChanged += onLiveDebugEditingContextChanged;
        liveDebug.Started += onLiveDebugStarted;
        liveDebug.Ended += onLiveDebugEnded;
        liveDebug.ContextChanged += onLiveDebugContextChanged;
        liveDebug.StatusChanged += onLiveDebugStatusChanged;
        liveDebug.ErrorReceived += onLiveDebugError;
        InitializeComponent();
        consoleLogView = new LogTextViewController(ConsoleOutput);
        pluginMenus = new PluginMenuCoordinator(this, ProjectPath);
        toast = new Toast(this);
        initializeInteraction();
        documentWindows = new DocumentWindowCoordinator(this, session, showHelp);
        DataContext = session.MainViewModel;
        Width = Math.Max(MinWidth, settings.Width);
        Height = Math.Max(MinHeight, settings.Height);
        leftColumn.Width = new GridLength(Math.Max(160, settings.UpperLeftWidth));
        rightColumn.Width = new GridLength(Math.Max(320, settings.UpperRightWidth));
        lowerLeftColumn.Width = new GridLength(Math.Max(180, settings.LowerLeftWidth));
    }

}
