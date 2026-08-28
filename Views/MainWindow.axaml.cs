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

}

internal sealed record MapClipboard(string SourceKey, JsonObject Data);
