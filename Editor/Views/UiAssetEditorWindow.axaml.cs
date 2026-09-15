using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Controls.Presenters;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Threading;
using Avalonia.VisualTree;
using Ludork.Controls;
using Ludork.Models;
using Ludork.Services;
using Ludork.Services.UiAssets;
using Ludork.Views.Utils;
using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Views;

public partial class UiAssetEditorWindow : Window, IProjectSaveParticipant
{
    private const string DragPrefix = "ludork-ui-node-name:";
    private const string PaletteDragPrefix = "ludork-ui-control-id:";
    private static readonly HashSet<string> TextStylePropertyIds = new(StringComparer.Ordinal)
    {
        "font",
        "characterSize",
        "bold",
        "italic",
        "underlined",
        "strikeThrough",
        "slantAngle",
        "fillColor",
        "letterSpacing",
        "lineSpacing",
        "lineAlignment",
        "outlineColor",
        "outlineThickness",
        "glowEnabled",
        "glowColor",
        "glowRadius",
        "glowIntensity",
        "gradientEnabled",
        "gradientDirection",
        "gradientCurve",
    };
    private readonly UiAssetEditorDocument document = null!;
    private readonly EditorDocumentBinding documentBinding = null!;
    private Toast? toast;
    private readonly GameDataService gameData = null!;
    private readonly UiControlRegistryService controlRegistry = null!;
    private readonly UiAssetValidationService validationService = null!;
    private readonly ProjectSaveService projectSave = null!;
    private UiAssetPreviewSession previewSession = null!;
    private UiPreviewSurface previewSurface = null!;
    private UiAnimationTimelineEditor timelineEditor = null!;
    private readonly DeferredWindowInitializer initializer = null!;
    private IReadOnlyDictionary<string, UiControlDescriptor> controlLookup =
        new Dictionary<string, UiControlDescriptor>(StringComparer.Ordinal);
    private string? selectedNodeName;
    private Action? pendingFieldCommit;
    private PointerPressedEventArgs? hierarchyDragPress;
    private Point? hierarchyDragStart;
    private string? hierarchyDragText;
    private DragDropEffects hierarchyDragEffects;
    private JsonObject? transformStartSlot;
    private bool startingHierarchyDrag;
    private bool refreshing;
    private bool committingDetails;
    private bool detailsRefreshPending;
    private bool contentInitialized;
    private bool closed;
    private bool refreshPending;
    private string? paletteSignature;
    private string? hierarchySignature;
    private string? detailsSignature;
    private string? animationSignature;
    private string? relatedAssetsRevision;
    private IReadOnlyList<UiControlDescriptor> paletteDescriptors = [];
    private readonly List<Control> textStyleFields = [];

    public UiAssetEditorWindow()
    {
        Content = DeferredWindowInitializer.CreateLoadingContent();
    }

    public UiAssetEditorWindow(
        UiAssetEditorDocument document,
        GameDataService gameData,
        ProjectSaveService projectSave,
        UiControlRegistryService controlRegistry,
        UiAssetValidationService validationService) : this()
    {
        this.document = document;
        this.gameData = gameData;
        this.projectSave = projectSave;
        this.controlRegistry = controlRegistry;
        this.validationService = validationService;
        Title = document.Title + " - " + LocaleService.Get("UI_ASSET_EDITOR");
        Width = 1480;
        Height = 860;
        MinWidth = 1080;
        MinHeight = 640;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        document.Changed += onDocumentChanged;
        controlRegistry.Runtime.Changed += onRegistryChanged;
        gameData.Documents.Changed += onProjectDocumentsChanged;
        Closing += onClosing;
        Closed += onClosed;
        projectSave.RegisterParticipant(this);
        documentBinding = new EditorDocumentBinding(this, gameData, () => document.ResourceDocument,
            () => document.Title + " - " + LocaleService.Get("UI_ASSET_EDITOR"), updateTitle, closeWhenDeleted: true);
        Deactivated += onDeactivated;
        AddHandler(KeyDownEvent, onDocumentKeyDown, RoutingStrategies.Tunnel);
        initializer = new DeferredWindowInitializer(this, async cancellationToken =>
        {
            if (contentInitialized)
                await releasePreviewAsync();
            cancellationToken.ThrowIfCancellationRequested();
            paletteSignature = null;
            hierarchySignature = null;
            detailsSignature = null;
            animationSignature = null;
            InitializeComponent();
            DetailsPanel.LostFocus += onDetailsLostFocus;
            toast = new Toast(this);
            previewSession = new UiAssetPreviewSession(document, gameData, controlRegistry.Runtime);
            previewSurface = new UiPreviewSurface
            {
                HitTestResolver = (generation, x, y) =>
                    previewSession.HitTestAsync(generation, x, y),
            };
            PreviewContainer.Content = previewSurface;
            timelineEditor = new UiAnimationTimelineEditor(document, gameData);
            TimelineContainer.Content = timelineEditor;
            EditorInputs.ApplyEditable(PaletteSearch);
            configureHierarchyDragDrop();
            applyLocale();
            previewSession.StateChanged += onPreviewStateChanged;
            previewSession.FrameReady += onPreviewFrameReady;
            previewSurface.NodeSelected += onPreviewNodeSelected;
            previewSurface.TransformStarted += onPreviewTransformStarted;
            previewSurface.TransformChanged += onPreviewTransformChanged;
            previewSurface.TransformCompleted += onPreviewTransformCompleted;
            previewSurface.TransformCancelled += onPreviewTransformCancelled;
            previewSurface.ZoomChanged += onPreviewZoomChanged;
            timelineEditor.PreviewChanged += onTimelinePreviewChanged;
            ScalingChanged += onScalingChanged;
            contentInitialized = true;
            await EditorUiBatch.YieldAsync(cancellationToken);
            await initializePanelsAsync(cancellationToken);
        });
    }

    public event EventHandler<string>? NestedAssetOpenRequested;

    public UiAssetEditorDocument Document => document;

    public bool Reload()
    {
        flushPendingField();
        if (!document.Reload())
            return false;
        if (initializer.IsInitialized)
            refreshAll();
        return true;
    }

    public bool Rekey(string key)
    {
        if (!document.Rekey(key))
            return false;
        updateTitle();
        requestPreview();
        return true;
    }

    public void FlushPendingChanges()
    {
        flushPendingField();
        document.Flush();
    }

    public void RefreshControls()
    {
        flushPendingField();
        if (initializer.IsInitialized)
            refreshAll();
    }

    private void onClosing(object? sender, WindowClosingEventArgs args)
    {
        FlushPendingChanges();
    }

    private async void onClosed(object? sender, EventArgs args)
    {
        closed = true;
        document.Changed -= onDocumentChanged;
        document.Dispose();
        Deactivated -= onDeactivated;
        controlRegistry.Runtime.Changed -= onRegistryChanged;
        gameData.Documents.Changed -= onProjectDocumentsChanged;
        projectSave.UnregisterParticipant(this);
        Closing -= onClosing;
        Closed -= onClosed;
        await releasePreviewAsync();
    }

    private async Task releasePreviewAsync()
    {
        if (!contentInitialized)
            return;
        contentInitialized = false;
        ScalingChanged -= onScalingChanged;
        timelineEditor.PreviewChanged -= onTimelinePreviewChanged;
        timelineEditor.StopPlayback();
        previewSurface.NodeSelected -= onPreviewNodeSelected;
        previewSurface.TransformStarted -= onPreviewTransformStarted;
        previewSurface.TransformChanged -= onPreviewTransformChanged;
        previewSurface.TransformCompleted -= onPreviewTransformCompleted;
        previewSurface.TransformCancelled -= onPreviewTransformCancelled;
        previewSurface.ZoomChanged -= onPreviewZoomChanged;
        previewSurface.HitTestResolver = null;
        previewSurface.SetUnavailable(string.Empty);
        previewSession.StateChanged -= onPreviewStateChanged;
        previewSession.FrameReady -= onPreviewFrameReady;
        await previewSession.DisposeAsync();
    }

    private void onDeactivated(object? sender, EventArgs args) => FlushPendingChanges();

    private async void onDocumentKeyDown(object? sender, KeyEventArgs args)
    {
        if (!EditorShortcuts.HasPrimaryModifier(args.KeyModifiers) || args.Key is not (Key.S or Key.Z or Key.Y))
            return;
        FlushPendingChanges();
        if (args.Key == Key.S)
            await EditorSaveWorkflow.TrySaveAsync(this, projectSave);
        else if (toast is not null)
            EditorFeedback.ShowHistory(toast, args.Key == Key.Z ? "Undo" : "Redo",
                args.Key == Key.Z ? documentBinding.Undo() : documentBinding.Redo());
        args.Handled = true;
    }

    private void onPreviewZoomChanged(object? sender, EventArgs args)
    {
        updateZoomText();
        requestPreview();
    }

    private void onScalingChanged(object? sender, EventArgs args)
    {
        requestPreview();
    }

    private void onTimelinePreviewChanged(object? sender, EventArgs args)
    {
        if (closed || refreshing)
            return;
        previewSurface.TransformEnabled = controlRegistry.IsReady && timelineEditor.CurrentSample is null;
        updateAnchorGuides();
        previewSession.RequestAnimationSample(previewSurface.RenderScale, timelineEditor.CurrentSample);
    }

    private void applyLocale()
    {
        PaletteTitle.Text = LocaleService.Get("UI_PALETTE");
        ToolTip.SetTip(PaletteSearch, LocaleService.Get("SEARCH"));
        HierarchyTitle.Text = LocaleService.Get("UI_HIERARCHY");
        DesignerTitle.Text = LocaleService.Get("UI_DESIGNER");
        TimelineTitle.Text = LocaleService.Get("UI_TIMELINE");
        DetailsTitle.Text = LocaleService.Get("DETAILS");
        ResetViewButton.Content = LocaleService.Get("RESET_VIEW");
        ValidateButton.Content = LocaleService.Get("VALIDATE");
        RefreshPreviewButton.Content = LocaleService.Get("REFRESH_PREVIEW");
        timelineEditor.ApplyLocale();
        updateTitle();
        updatePreviewState();
    }

    private void updateTitle() => documentBinding?.Refresh();

    private void onDocumentChanged(object? sender, EventArgs args)
    {
        if (document.ResourceDocument?.Exists != true)
        {
            Close();
            return;
        }
        updateTitle();
        if (closed || refreshing || !initializer.IsInitialized)
            return;
        if (transformStartSlot is not null)
        {
            requestPreview();
            return;
        }
        if (timelineEditor.IsCommitting)
        {
            animationSignature = hierarchySignature + "\n" + document.Data["animations"]?.ToJsonString();
            requestPreview();
            return;
        }
        refreshAll();
    }

    private void onRegistryChanged(object? sender, EventArgs args)
    {
        Dispatcher.UIThread.Post(() =>
        {
            if (closed || !contentInitialized)
                return;
            flushPendingField();
            if (!controlRegistry.IsReady)
                timelineEditor.StopPlayback();
            paletteSignature = null;
            hierarchySignature = null;
            detailsSignature = null;
            refreshAll();
            updatePreviewState();
            UiAssetValidationResult result = validationService.ValidateAsset(document.AssetKey, document.Data);
            setStatus(result.IsValid ? LocaleService.Get("UI_VALIDATION_SUCCEEDED")
                : result.Errors.FirstOrDefault() ?? LocaleService.Get("UI_VALIDATION_FAILED"));
        });
    }

    private void onProjectDocumentsChanged(object? sender, EventArgs args)
    {
        if (closed || !initializer.IsInitialized || refreshPending)
            return;
        refreshPending = true;
        Dispatcher.UIThread.Post(() =>
        {
            refreshPending = false;
            if (closed)
                return;
            string nextRevision = string.Join("\n", gameData.Documents.All
                .Where(item => item.Section == "UI" && item != document.ResourceDocument)
                .Select(item => item.Id.ToString() + ":" + item.Revision.ToString(CultureInfo.InvariantCulture)));
            if (relatedAssetsRevision != nextRevision)
            {
                relatedAssetsRevision = nextRevision;
                animationSignature = null;
                refreshAll();
            }
            else
                refreshCatalog();
        }, DispatcherPriority.Background);
    }

    private async Task initializePanelsAsync(CancellationToken cancellationToken)
    {
        refreshing = true;
        refreshCatalog();
        await EditorUiBatch.YieldAsync(cancellationToken);
        refreshHierarchy();
        await EditorUiBatch.YieldAsync(cancellationToken);
        refreshDetails();
        await EditorUiBatch.YieldAsync(cancellationToken);
        refreshing = false;
        refreshAll();
    }

    private void refreshCatalog()
    {
        string signature = string.Join("\n", gameData.Documents.All
            .Where(item => item.Section == "UI" && item.Exists)
            .Select(item => item.Key + "\t" + item.InternalData?["palette"]?.ToJsonString()
                + "\t" + item.InternalData?["designSize"]?.ToJsonString()));
        if (paletteSignature == signature)
            return;
        paletteSignature = signature;
        controlLookup = controlRegistry.CreateControlLookup(true);
        paletteDescriptors = controlRegistry.GetDescriptors();
        refreshPalette();
    }

    private void refreshAll(bool immediatePreview = false)
    {
        refreshing = true;
        try
        {
            refreshCatalog();
            refreshHierarchy();
            refreshDetails();
            updateAnchorGuides();
            updateZoomText();
            string nextAnimations = hierarchySignature + "\n" + document.Data["animations"]?.ToJsonString();
            if (animationSignature != nextAnimations)
            {
                animationSignature = nextAnimations;
                timelineEditor.Refresh();
            }
            PaletteSearch.IsEnabled = controlRegistry.IsReady;
            PaletteCategories.IsEnabled = controlRegistry.IsReady;
            HierarchyTree.IsEnabled = controlRegistry.IsReady;
            DetailsPanel.IsEnabled = controlRegistry.IsReady;
            TimelineContainer.IsEnabled = controlRegistry.IsReady;
            previewSurface.TransformEnabled = controlRegistry.IsReady && timelineEditor.CurrentSample is null;
        }
        finally
        {
            refreshing = false;
        }
        requestPreview(immediatePreview);
    }

    private void refreshPalette()
    {
        PaletteCategories.Children.Clear();
        if (!controlRegistry.IsReady)
        {
            PaletteCategories.Children.Add(new TextBlock
            {
                Text = controlRegistry.Runtime.StatusMessage,
                TextWrapping = TextWrapping.Wrap,
                Margin = new Thickness(4),
            });
            return;
        }
        string search = PaletteSearch.Text?.Trim() ?? string.Empty;
        IEnumerable<UiControlDescriptor> descriptors = paletteDescriptors
            .Where(descriptor => search.Length == 0
                || descriptor.DisplayName.Contains(search, StringComparison.CurrentCultureIgnoreCase)
                || descriptor.ControlId.Contains(search, StringComparison.OrdinalIgnoreCase))
            .OrderBy(descriptor => descriptor.Source, StringComparer.Ordinal)
            .ThenBy(descriptor => descriptor.Category, StringComparer.CurrentCulture)
            .ThenBy(descriptor => descriptor.DisplayName, StringComparer.CurrentCulture);
        foreach (IGrouping<string, UiControlDescriptor> group in descriptors
                     .GroupBy(
                         descriptor => getPaletteGroup(descriptor),
                         StringComparer.CurrentCulture))
        {
            StackPanel entries = new()
            {
                Spacing = 2,
            };
            foreach (UiControlDescriptor descriptor in group)
            {
                Button item = new()
                {
                    Content = descriptor.DisplayName,
                    HorizontalContentAlignment = HorizontalAlignment.Left,
                    HorizontalAlignment = HorizontalAlignment.Stretch,
                    Tag = descriptor,
                    Padding = new Thickness(8, 5),
                };
                item.DoubleTapped += onPaletteItemDoubleTapped;
                item.AddHandler(PointerPressedEvent, onPalettePointerPressed, RoutingStrategies.Tunnel);
                item.AddHandler(PointerMovedEvent, onHierarchyPointerMoved, handledEventsToo: true);
                item.AddHandler(PointerReleasedEvent, onHierarchyPointerReleased, handledEventsToo: true);
                entries.Children.Add(item);
            }
            Expander category = new()
            {
                Header = group.Key,
                IsExpanded = true,
                Content = entries,
            };
            PaletteCategories.Children.Add(category);
        }
    }

    private static string getPaletteGroup(UiControlDescriptor descriptor)
    {
        string source = string.Equals(descriptor.Source, "project", StringComparison.Ordinal)
            ? "Project"
            : "System";
        return source + " / " + descriptor.Category;
    }

    private void refreshHierarchy()
    {
        JsonObject? root = document.Data["root"] as JsonObject;
        string nextSignature = getHierarchySignature(root);
        if (hierarchySignature == nextSignature)
        {
            if (HierarchyTree.ItemsSource is IEnumerable<UiHierarchyItem> items
                && items.FirstOrDefault() is UiHierarchyItem existing)
            {
                UiHierarchyItem? existingSelection = findHierarchyItem(existing, selectedNodeName);
                if (existingSelection is not null && !ReferenceEquals(HierarchyTree.SelectedItem, existingSelection))
                    HierarchyTree.SelectedItem = existingSelection;
            }
            previewSurface.SetSelectedNode(selectedNodeName);
            return;
        }
        hierarchySignature = nextSignature;
        if (root is null)
        {
            HierarchyTree.ItemsSource = Array.Empty<UiHierarchyItem>();
            selectedNodeName = null;
            previewSurface.SetSelectedNode(null);
            return;
        }
        UiHierarchyItem rootItem = createHierarchyItem(root);
        HierarchyTree.ItemsSource = new[] { rootItem };
        UiHierarchyItem? selected = findHierarchyItem(rootItem, selectedNodeName);
        if (selected is null)
        {
            selected = rootItem;
            selectedNodeName = rootItem.NodeName;
        }
        HierarchyTree.SelectedItem = selected;
        previewSurface.SetSelectedNode(selectedNodeName);
    }

    private static string getHierarchySignature(JsonObject? root)
    {
        StringBuilder result = new();
        void append(JsonObject? node)
        {
            if (node is null)
                return;
            string name = getString(node, "name");
            string control = getString(node, "controlId");
            result.Append(name.Length).Append(':').Append(name)
                .Append(control.Length).Append(':').Append(control)
                .Append(getBool((node["properties"] as JsonObject)?["visible"], true));
            result.Append('[');
            if (node["children"] is JsonArray children)
                foreach (JsonObject child in children.OfType<JsonObject>())
                    append(child);
            result.Append(']');
        }
        append(root);
        return result.ToString();
    }

    private UiHierarchyItem createHierarchyItem(JsonObject node)
    {
        string nodeName = getString(node, "name", "Widget");
        string controlId = getString(node, "controlId");
        string controlLabel = controlLookup.TryGetValue(controlId, out UiControlDescriptor? descriptor)
            ? descriptor.DisplayName
            : controlId;
        bool isNestedAsset = controlId.StartsWith(
            UiAssetSchema.ProjectControlPrefix,
            StringComparison.Ordinal);
        List<UiHierarchyItem> children = [];
        if (!isNestedAsset && node["children"] is JsonArray childData)
        {
            foreach (JsonObject child in childData.OfType<JsonObject>())
                children.Add(createHierarchyItem(child));
        }
        return new UiHierarchyItem(
            nodeName,
            nodeName,
            controlId,
            controlLabel,
            isNestedAsset,
            getBool((node["properties"] as JsonObject)?["visible"], true),
            children);
    }

    private static UiHierarchyItem? findHierarchyItem(
        UiHierarchyItem root,
        string? nodeName)
    {
        if (string.Equals(root.NodeName, nodeName, StringComparison.Ordinal))
            return root;
        foreach (UiHierarchyItem child in root.Children)
        {
            UiHierarchyItem? result = findHierarchyItem(child, nodeName);
            if (result is not null)
                return result;
        }
        return null;
    }

    private void onPaletteSearchChanged(object? sender, TextChangedEventArgs args)
    {
        if (!refreshing)
            refreshPalette();
    }

    private void onPaletteItemDoubleTapped(object? sender, TappedEventArgs args)
    {
        if (sender is Button { Tag: UiControlDescriptor descriptor })
            addControl(descriptor);
    }

    private void addControl(UiControlDescriptor descriptor, string? parentName = null, int? index = null)
    {
        flushPendingField();
        string? nodeName = document.AddControl(
            parentName ?? selectedNodeName,
            descriptor.ControlId,
            out UiAssetEditingService.Failure failure,
            index);
        if (nodeName is null)
        {
            string messageKey = failure switch
            {
                UiAssetEditingService.Failure.SelectContainer => "UI_SELECT_CONTAINER",
                UiAssetEditingService.Failure.AssetCycle => "UI_ASSET_CYCLE",
                UiAssetEditingService.Failure.UnknownControl => "UI_PREVIEW_UNAVAILABLE",
                _ => "UI_CONTAINER_REJECTS_CHILD",
            };
            setStatus(LocaleService.Get(messageKey));
            return;
        }
        selectedNodeName = nodeName;
        refreshAll();
    }

    private void onHierarchySelectionChanged(
        object? sender,
        SelectionChangedEventArgs args)
    {
        if (refreshing || HierarchyTree.SelectedItem is not UiHierarchyItem item)
            return;
        flushPendingField();
        selectedNodeName = item.NodeName;
        previewSurface.SetSelectedNode(selectedNodeName);
        refreshDetails();
    }

    private void onHierarchyDoubleTapped(object? sender, TappedEventArgs args)
    {
        UiHierarchyItem? item = getHierarchyItem(args.Source);
        if (item is null || !item.IsNestedAsset)
            return;
        if (!controlLookup.TryGetValue(item.ControlId, out UiControlDescriptor? descriptor)
            || descriptor.AssetKey is null)
        {
            return;
        }
        NestedAssetOpenRequested?.Invoke(
            this,
            UiAssetSchema.NormalizeAssetKey(descriptor.AssetKey));
        args.Handled = true;
    }

    private void onHierarchyVisibilityClicked(object? sender, RoutedEventArgs args)
    {
        if (sender is not ToggleButton
            {
                DataContext: UiHierarchyItem { CanEditVisibility: true } item,
            } toggle)
        {
            return;
        }
        document.SetNodeProperty(
            item.NodeName,
            "visible",
            JsonValue.Create(toggle.IsChecked == true));
        args.Handled = true;
    }

    private void onDeleteNode(object? sender, RoutedEventArgs args)
    {
        if (selectedNodeName is null)
            return;
        JsonObject? parent = document.FindParent(selectedNodeName);
        if (parent is null)
            return;
        string nextSelection = getString(parent, "name");
        if (document.DeleteNode(selectedNodeName))
        {
            selectedNodeName = nextSelection;
            refreshAll();
        }
    }

    private void onDuplicateNode(object? sender, RoutedEventArgs args)
    {
        if (selectedNodeName is null)
            return;
        string? copyName = document.DuplicateNode(selectedNodeName);
        if (copyName is not null)
        {
            selectedNodeName = copyName;
            refreshAll();
        }
    }

    private void onMoveUp(object? sender, RoutedEventArgs args)
    {
        moveWithinParent(-1);
    }

    private void onMoveDown(object? sender, RoutedEventArgs args)
    {
        moveWithinParent(1);
    }

    private void moveWithinParent(int direction)
    {
        if (selectedNodeName is not null)
            document.MoveWithinParent(selectedNodeName, direction);
    }

    private async void onIndent(object? sender, RoutedEventArgs args)
    {
        flushPendingField();
        if (selectedNodeName is not null
            && document.TryGetIndentLocation(selectedNodeName, out string parentName, out int index))
        {
            await moveNodeAsync(selectedNodeName, parentName, index);
        }
    }

    private async void onOutdent(object? sender, RoutedEventArgs args)
    {
        flushPendingField();
        if (selectedNodeName is not null
            && document.TryGetOutdentLocation(selectedNodeName, out string parentName, out int index))
        {
            await moveNodeAsync(selectedNodeName, parentName, index);
        }
    }

    private async Task moveNodeAsync(string nodeName, string parentName, int index)
    {
        if (await previewSession.MoveNodeAsync(nodeName, parentName, index))
        {
            selectedNodeName = nodeName;
            refreshAll(true);
        }
        else if (!closed && previewSession.StatusMessage.Length != 0)
        {
            setStatus(previewSession.StatusMessage);
        }
    }

    private void showHierarchyContextMenu(UiHierarchyItem item)
    {
        if (!ReferenceEquals(HierarchyTree.SelectedItem, item))
            HierarchyTree.SelectedItem = item;
        JsonObject? parent = document.FindParent(item.NodeName);
        bool canModify = parent is not null;
        MenuItem delete = new()
        {
            Header = LocaleService.Get("DELETE"),
            IsEnabled = canModify,
        };
        delete.Click += onDeleteNode;
        MenuItem duplicate = new()
        {
            Header = LocaleService.Get("DUPLICATE"),
            IsEnabled = document.CanDuplicateNode(item.NodeName),
        };
        duplicate.Click += onDuplicateNode;
        MenuItem moveUp = new()
        {
            Header = LocaleService.Get("MOVE_UP"),
            IsEnabled = canModify,
        };
        moveUp.Click += onMoveUp;
        MenuItem moveDown = new()
        {
            Header = LocaleService.Get("MOVE_DOWN"),
            IsEnabled = canModify,
        };
        moveDown.Click += onMoveDown;
        MenuItem outdent = new()
        {
            Header = LocaleService.Get("OUTDENT"),
            IsEnabled = canModify,
        };
        outdent.Click += onOutdent;
        MenuItem indent = new()
        {
            Header = LocaleService.Get("INDENT"),
            IsEnabled = canModify,
        };
        indent.Click += onIndent;
        ContextMenu menu = new()
        {
            ItemsSource = new object[]
            {
                delete,
                duplicate,
                moveUp,
                moveDown,
                outdent,
                indent,
            },
        };
        menu.Open(HierarchyTree);
    }

    private void refreshDetails(bool force = false)
    {
        JsonObject? node = selectedNodeName is null ? null : document.FindNode(selectedNodeName);
        bool isRoot = node is not null && document.FindParent(selectedNodeName!) is null;
        string signature = selectedNodeName + "\n" + (node is null ? string.Empty : string.Join("\n",
            node.Where(entry => entry.Key is not "children" and not "animations")
                .Select(entry => entry.Key + "=" + entry.Value?.ToJsonString())));
        if (isRoot)
            signature += "\n" + document.AssetKey + "\n" + document.Data["palette"]?.ToJsonString()
                + "\n" + document.Data["designSize"]?.ToJsonString();
        if (!force && signature == detailsSignature)
            return;
        detailsSignature = signature;
        if (committingDetails && !force)
        {
            updateTextStyleFields(node);
            return;
        }
        detailsRefreshPending = false;
        pendingFieldCommit = null;
        textStyleFields.Clear();
        DetailsPanel.Children.Clear();
        if (node is null)
            return;
        if (isRoot)
            addAssetDetails();
        addWidgetDetails(node);
        if (!isRoot)
            addSlotDetails(node);
    }

    private void onDetailsLostFocus(object? sender, RoutedEventArgs args)
    {
        Dispatcher.UIThread.Post(() =>
        {
            if (!closed && detailsRefreshPending && !DetailsPanel.IsKeyboardFocusWithin)
                refreshDetails(true);
        }, DispatcherPriority.Background);
    }

    private void addAssetDetails()
    {
        addSection(LocaleService.Get("UI_ASSET"));
        addReadOnlyTextField(
            LocaleService.Get("ASSET_PATH"),
            document.AssetKey);
        JsonObject? designSize = document.Data["designSize"] as JsonObject;
        double width = getDouble(designSize?["width"], 640);
        double height = getDouble(designSize?["height"], 480);
        addNumericField(
            LocaleService.Get("WIDTH"),
            width,
            1,
            32768,
            1,
            value => setDesignSize(value, getDesignHeight()));
        addNumericField(
            LocaleService.Get("HEIGHT"),
            height,
            1,
            32768,
            1,
            value => setDesignSize(getDesignWidth(), value));
        JsonObject? palette = document.Data["palette"] as JsonObject;
        bool exposed = getBool(palette?["exposed"], true);
        string displayName = getString(palette, "displayName", document.Title);
        string category = getString(palette, "category", "Project");
        addBoolField(
            LocaleService.Get("EXPOSE_TO_PALETTE"),
            exposed,
            value => setPalette(value, getPaletteDisplayName(), getPaletteCategory()));
        addTextField(
            LocaleService.Get("DISPLAY_NAME"),
            displayName,
            value => setPalette(getPaletteExposed(), value, getPaletteCategory()));
        addTextField(
            LocaleService.Get("CATEGORY"),
            category,
            value => setPalette(getPaletteExposed(), getPaletteDisplayName(), value));
    }

    private void setPalette(bool exposed, string displayName, string category)
    {
        if (document.SetPalette(exposed, displayName, category))
            refreshAll();
    }

    private void setDesignSize(double width, double height)
    {
        if (document.SetDesignSize(width, height))
            refreshAll();
    }

    private void addWidgetDetails(JsonObject node)
    {
        addSection(LocaleService.Get("WIDGET"));
        string nodeName = getString(node, "name");
        addTextField(
            LocaleService.Get("NAME"),
            nodeName,
            value => renameNode(nodeName, value));
        string controlId = getString(node, "controlId");
        addReadOnlyTextField(LocaleService.Get("CONTROL"), controlId);
        if (!controlLookup.TryGetValue(controlId, out UiControlDescriptor? descriptor))
            return;
        if (descriptor.AssetKey is not null)
        {
            addReadOnlyTextField(
                LocaleService.Get("UI_SOURCE_ASSET"),
                descriptor.AssetKey ?? string.Empty);
            Button open = new()
            {
                Content = LocaleService.Get("OPEN_SOURCE_ASSET"),
                HorizontalAlignment = HorizontalAlignment.Stretch,
            };
            open.Click += (_, _) =>
            {
                if (descriptor.AssetKey is not null)
                {
                    NestedAssetOpenRequested?.Invoke(
                        this,
                        UiAssetSchema.NormalizeAssetKey(descriptor.AssetKey));
                }
            };
            DetailsPanel.Children.Add(open);
            return;
        }
        JsonObject properties = node["properties"] as JsonObject ?? new JsonObject();
        JsonObject editor = node["editor"] as JsonObject ?? new JsonObject();
        bool rootCanvas = document.FindParent(nodeName) is null
            && string.Equals(
                descriptor.ControlId,
                "Engine.Canvas",
                StringComparison.Ordinal);
        foreach (UiControlPropertyDescriptor property in descriptor.Properties)
        {
            if (rootCanvas && string.Equals(property.Id, "size", StringComparison.Ordinal))
                continue;
            JsonObject source = property.EditorOnly ? editor : properties;
            JsonNode? value = source[property.Id] ?? property.Default;
            int fieldIndex = DetailsPanel.Children.Count;
            addPropertyField(nodeName, descriptor.ControlId, property, value);
            if (TextStylePropertyIds.Contains(property.Id)
                && DetailsPanel.Children.Count > fieldIndex)
            {
                textStyleFields.Add(DetailsPanel.Children[fieldIndex]);
            }
        }
        updateTextStyleFields(node);
    }

    private void updateTextStyleFields(JsonObject? node)
    {
        bool usesTextConfig = !string.IsNullOrEmpty(getString(node?["properties"]?["textConfig"]));
        foreach (Control field in textStyleFields)
            field.IsEnabled = !usesTextConfig;
    }

    private void renameNode(string nodeName, string value)
    {
        if (refreshing)
            return;
        string nextName = value.Trim();
        if (string.Equals(nodeName, nextName, StringComparison.Ordinal))
            return;
        bool renamed;
        bool wasRefreshing = refreshing;
        refreshing = true;
        try
        {
            renamed = document.RenameNode(nodeName, value);
        }
        finally
        {
            refreshing = wasRefreshing;
        }
        if (!renamed)
        {
            setStatus(LocaleService.Get("UI_NAME_MUST_BE_UNIQUE"));
            refreshDetails(true);
            return;
        }
        if (string.Equals(selectedNodeName, nodeName, StringComparison.Ordinal))
            selectedNodeName = nextName;
        refreshDetails(true);
        refreshAll();
    }

    private void addPropertyField(
        string nodeName,
        string controlId,
        UiControlPropertyDescriptor property,
        JsonNode? value)
    {
        Action<JsonNode?> commit = nextValue =>
        {
            if (property.EditorOnly)
                document.SetNodeEditorProperty(nodeName, property.Id, nextValue);
            else
                document.SetNodeProperty(nodeName, property.Id, nextValue);
        };
        switch (property.Type)
        {
            case "string" when property.Id == "particle":
                addChoiceField(
                    property.DisplayName,
                    getString(value),
                    gameData.ParticlesData.Keys.Prepend(string.Empty).OrderBy(key => key, StringComparer.Ordinal).ToArray(),
                    next => commit(JsonValue.Create(next)));
                break;
            case "sf.Text.LineAlignment":
                addChoiceField(
                    property.DisplayName,
                    getString(value),
                    ["default", "left", "center", "right"],
                    next => commit(JsonValue.Create(next)));
                break;
            case "Engine.TextGradientDirection":
                addChoiceField(
                    property.DisplayName,
                    getString(value),
                    ["vertical", "horizontal"],
                    next => commit(JsonValue.Create(next)));
                break;
            case "Engine.ImageDrawAs":
                addChoiceField(
                    property.DisplayName,
                    getString(value),
                    ["Image", "Tile"],
                    next => commit(JsonValue.Create(next)));
                break;
            case "string" when controlId == "Engine.Button" && property.Id == "gamepadButton":
                addChoiceField(
                    property.DisplayName,
                    getString(value),
                    ["", "A", "B", "X", "Y", "LB", "RB", "View", "Menu", "LS", "RS", "XBox", "Share"],
                    next => commit(JsonValue.Create(next)));
                break;
            case "string" when property.Id == "font":
                addFontField(
                    property.DisplayName,
                    getString(value),
                    next => commit(JsonValue.Create(next)));
                break;
            case "string" when property.Id is "texture"
                or "backgroundTexture"
                or "fillTexture"
                or "windowSkin"
                or "lineTexture"
                or "handleTexture":
                addTextureField(
                    property.DisplayName,
                    getString(value),
                    next => commit(JsonValue.Create(next)));
                break;
            case "string" when property.Id == "shader":
                addShaderField(
                    property.DisplayName,
                    getString(value),
                    next => commit(JsonValue.Create(next)));
                break;
            case "bool":
                addBoolField(
                    property.DisplayName,
                    getBool(value, false),
                    next => commit(JsonValue.Create(next)));
                break;
            case "int":
                addNumericField(
                    property.DisplayName,
                    getDouble(value, 0),
                    controlId == "Engine.WrapBox" && property.Id == "count" ? 0 : -2147483648,
                    2147483647,
                    1,
                    next => commit(JsonValue.Create((int)Math.Round(next))));
                break;
            case "float":
                addNumericField(
                    property.DisplayName,
                    getDouble(value, 0),
                    -1000000000,
                    1000000000,
                    0.1,
                    next => commit(JsonValue.Create(next)));
                break;
            case "sf.Vector2f":
                addNumberArrayField(property.DisplayName, value, 2, false, commit);
                break;
            case "sf.Vector2u":
                double? unsignedMaximum =
                    string.Equals(property.Id, "size", StringComparison.Ordinal)
                    && (string.Equals(
                            controlId,
                            "Engine.Canvas",
                            StringComparison.Ordinal)
                        || string.Equals(
                            controlId,
                            "Engine.Window",
                            StringComparison.Ordinal))
                        ? int.MaxValue
                        : null;
                addNumberArrayField(
                    property.DisplayName,
                    value,
                    2,
                    true,
                    commit,
                    false,
                    unsignedMaximum);
                break;
            case "sf.IntRect":
                addNumberArrayField(property.DisplayName, value, 4, false, commit, true);
                break;
            case "sf.Color":
                addColorField(property.DisplayName, value, commit);
                break;
            case "string[]":
                addStringArrayField(property.DisplayName, value, commit);
                break;
            default:
                addTextField(
                    property.DisplayName,
                    getString(value),
                    next => commit(JsonValue.Create(next)));
                break;
        }
    }

    private void addSlotDetails(JsonObject node)
    {
        JsonObject? parent = document.FindParent(getString(node, "name"));
        if (parent is null
            || !controlLookup.TryGetValue(
                getString(parent, "controlId"),
                out UiControlDescriptor? parentDescriptor))
        {
            return;
        }
        addSection(LocaleService.Get("SLOT"));
        if (string.Equals(parentDescriptor.SlotType, "list", StringComparison.Ordinal))
        {
            DetailsPanel.Children.Add(new TextBlock
            {
                Text = LocaleService.Get("UI_LIST_SLOT_ORDERED"),
                Foreground = EditorTheme.Brush("TextMuted"),
                TextWrapping = TextWrapping.Wrap,
            });
            return;
        }
        if (!string.Equals(parentDescriptor.SlotType, "canvas", StringComparison.Ordinal))
            return;
        JsonObject slot = node["slot"] as JsonObject
            ?? UiAssetEditingService.CreateDefaultCanvasSlot();
        addCanvasSlotFields(getString(node, "name"), slot);
    }

    private void addCanvasSlotFields(string nodeName, JsonObject slot)
    {
        JsonObject anchors = slot["anchors"] as JsonObject ?? new JsonObject();
        JsonArray min = anchors["min"] as JsonArray ?? new JsonArray(0, 0);
        JsonArray max = anchors["max"] as JsonArray ?? new JsonArray(0, 0);
        JsonObject offsets = slot["offsets"] as JsonObject ?? new JsonObject();
        JsonArray alignment = slot["alignment"] as JsonArray ?? new JsonArray(0, 0);
        double minimumX = getDouble(min[0], 0);
        double minimumY = getDouble(min[1], 0);
        double maximumX = getDouble(max[0], 0);
        double maximumY = getDouble(max[1], 0);
        addAnchorPicker(
            nodeName,
            minimumX,
            minimumY,
            maximumX,
            maximumY);
        bool stretchesX = Math.Abs(maximumX - minimumX) > 0.0001;
        bool stretchesY = Math.Abs(maximumY - minimumY) > 0.0001;
        addSlotOffsetField(
            nodeName,
            stretchesX ? "OFFSET_LEFT" : "POSITION_X",
            "left",
            getDouble(offsets["left"], 0));
        addSlotOffsetField(
            nodeName,
            stretchesY ? "OFFSET_TOP" : "POSITION_Y",
            "top",
            getDouble(offsets["top"], 0));
        addSlotOffsetField(
            nodeName,
            stretchesX ? "OFFSET_RIGHT" : "SIZE_X",
            "right",
            getDouble(offsets["right"], 100));
        addSlotOffsetField(
            nodeName,
            stretchesY ? "OFFSET_BOTTOM" : "SIZE_Y",
            "bottom",
            getDouble(offsets["bottom"], 34));
        addAxisPointField(
            LocaleService.Get("ALIGNMENT"),
            getDouble(alignment[0], 0),
            getDouble(alignment[1], 0),
            0,
            1,
            (x, y) => updateSlotPoint(nodeName, null, "alignment", x, y));
        addBoolField(
            LocaleService.Get("AUTO_SIZE"),
            getBool(slot["autoSize"], false),
            value => updateSlotScalar(nodeName, "autoSize", JsonValue.Create(value)));
        addNumericField(
            LocaleService.Get("Z_ORDER"),
            getDouble(slot["zOrder"], 0),
            int.MinValue,
            int.MaxValue,
            1,
            value => updateSlotScalar(
                nodeName,
                "zOrder",
                JsonValue.Create((int)Math.Round(value))));
    }

    private void addAnchorPicker(
        string nodeName,
        double minimumX,
        double minimumY,
        double maximumX,
        double maximumY)
    {
        CanvasAnchorPresetPicker picker = new(
            minimumX,
            minimumY,
            maximumX,
            maximumY);
        picker.PresetSelected += preset => setAnchorPreset(nodeName, preset);
        DetailsPanel.Children.Add(createField(
            LocaleService.Get("ANCHORS"),
            picker));
    }

    private void addSlotOffsetField(
        string nodeName,
        string localeKey,
        string offsetName,
        double value)
    {
        addNumericField(
            LocaleService.Get(localeKey),
            value,
            -1000000000,
            1000000000,
            1,
            next => updateSlotOffset(nodeName, offsetName, next));
    }

    private void setAnchorPreset(
        string nodeName,
        CanvasAnchorPreset preset)
    {
        JsonObject slot = cloneSlot(nodeName);
        slot["anchors"] = new JsonObject
        {
            ["min"] = new JsonArray(preset.MinimumX, preset.MinimumY),
            ["max"] = new JsonArray(preset.MaximumX, preset.MaximumY),
        };
        slot["alignment"] = new JsonArray(
            preset.AlignmentX,
            preset.AlignmentY);
        document.SetNodeSlot(nodeName, slot);
    }

    private void updateSlotPoint(
        string nodeName,
        string? group,
        string name,
        double x,
        double y)
    {
        JsonObject slot = cloneSlot(nodeName);
        if (group is null)
        {
            slot[name] = new JsonArray(x, y);
        }
        else
        {
            JsonObject groupValue = slot[group] as JsonObject ?? new JsonObject();
            groupValue[name] = new JsonArray(x, y);
            slot[group] = groupValue;
        }
        document.SetNodeSlot(nodeName, slot);
    }

    private void updateSlotOffset(
        string nodeName,
        string name,
        double value)
    {
        JsonObject slot = cloneSlot(nodeName);
        JsonObject offsets = slot["offsets"] as JsonObject ?? new JsonObject();
        offsets[name] = value;
        slot["offsets"] = offsets;
        document.SetNodeSlot(nodeName, slot);
    }

    private void updateSlotScalar(string nodeName, string name, JsonNode value)
    {
        JsonObject slot = cloneSlot(nodeName);
        slot[name] = value;
        document.SetNodeSlot(nodeName, slot);
    }

    private JsonObject cloneSlot(string nodeName)
    {
        JsonObject? node = document.FindNode(nodeName);
        return node?["slot"] is JsonObject slot
            ? (JsonObject)slot.DeepClone()
            : UiAssetEditingService.CreateDefaultCanvasSlot();
    }

    private void addSection(string label)
    {
        if (DetailsPanel.Children.Count != 0)
            DetailsPanel.Children.Add(new Separator { Margin = new Thickness(0, 6) });
        DetailsPanel.Children.Add(new TextBlock
        {
            Text = label,
            FontWeight = FontWeight.SemiBold,
            FontSize = 13,
            Margin = new Thickness(0, 2, 0, 3),
        });
    }

    private void addReadOnlyTextField(string label, string value)
    {
        TextBox box = EditorInputs.CreateReadOnlyTextBox(value);
        DetailsPanel.Children.Add(createField(label, box));
    }

    private void addTextField(
        string label,
        string value,
        Action<string> commit)
    {
        TextBox box = EditorInputs.CreateEditableTextBox(value);
        bindTextField(box, commit);
        DetailsPanel.Children.Add(createField(label, box));
    }

    private void bindTextField(TextBox box, Action<string> commit)
    {
        string displayed = box.Text ?? string.Empty;
        Action commitValue = () =>
        {
            string next = box.Text ?? string.Empty;
            if (string.Equals(displayed, next, StringComparison.Ordinal))
                return;
            displayed = next;
            commitDetailsField(() => commit(next));
        };
        box.GotFocus += (_, _) => pendingFieldCommit = commitValue;
        box.LostFocus += (_, _) =>
        {
            if (!ReferenceEquals(pendingFieldCommit, commitValue))
                return;
            pendingFieldCommit = null;
            commitValue();
        };
        box.KeyDown += (_, args) =>
        {
            if (args.Key != Key.Enter || box.AcceptsReturn)
                return;
            if (!ReferenceEquals(pendingFieldCommit, commitValue))
                return;
            commitValue();
            args.Handled = true;
        };
    }

    private void addChoiceField(
        string label,
        string value,
        IReadOnlyList<string> choices,
        Action<string> commit)
    {
        ComboBox input = new()
        {
            ItemsSource = choices.Select(choice => choice.Length == 0
                ? LocaleService.Get("UI_GAMEPAD_UNBOUND") : choice).ToArray(),
            SelectedIndex = Math.Max(0, choices.ToList().IndexOf(value)),
            HorizontalAlignment = HorizontalAlignment.Stretch,
        };
        input.SelectionChanged += (_, _) =>
        {
            if (refreshing || input.SelectedIndex < 0 || input.SelectedIndex >= choices.Count)
                return;
            commit(choices[input.SelectedIndex]);
        };
        DetailsPanel.Children.Add(createField(label, input));
    }

    private void addStringArrayField(
        string label,
        JsonNode? value,
        Action<JsonNode?> commit)
    {
        IEnumerable<string> items = value is JsonArray array
            ? array.Select(getString)
            : [];
        TextBox box = EditorInputs.CreateEditableTextBox(string.Join(Environment.NewLine, items));
        box.AcceptsReturn = true;
        box.MinHeight = 96;
        box.TextWrapping = TextWrapping.Wrap;
        bindTextField(box, next =>
        {
            JsonArray result = new();
            foreach (string item in next.Length == 0 ? Array.Empty<string>() : next.Split(
                         ["\r\n", "\n", "\r"],
                         StringSplitOptions.None))
            {
                result.Add(item);
            }
            commit(result);
        });
        DetailsPanel.Children.Add(createField(label, box));
    }

    private void addTextureField(
        string label,
        string value,
        Action<string> commit)
    {
        addAssetFileField(
            label,
            value,
            Path.Combine(gameData.ProjectPath, "Assets"),
            FileSelectorDialog.ImageFilesFilter(),
            commit);
    }

    private void addFontField(
        string label,
        string value,
        Action<string> commit)
    {
        addAssetFileField(
            label,
            value,
            Path.Combine(gameData.ProjectPath, "Assets", "Fonts"),
            FileSelectorDialog.FilesFilter("*.ttf", "*.otf"),
            commit);
    }

    private void addShaderField(
        string label,
        string value,
        Action<string> commit)
    {
        addAssetFileField(
            label,
            value,
            Path.Combine(gameData.ProjectPath, "Assets", "Shaders"),
            FileSelectorDialog.FilesFilter("*.vert", "*.frag", "*.geom"),
            commit);
    }

    private void addAssetFileField(
        string label,
        string value,
        string selectorRoot,
        string filter,
        Action<string> commit)
    {
        TextBox pathBox = EditorInputs.CreateReadOnlyTextBox(value);
        Button browse = new()
        {
            Content = "...",
            MinWidth = 36,
            Height = EditorInputs.FieldMinHeight,
        };
        browse.Click += async (_, _) =>
        {
            Directory.CreateDirectory(selectorRoot);
            string? initialFilePath = getAssetInitialFilePath(pathBox.Text ?? string.Empty);
            string? selectedPath = await FileSelectorDialog.ShowAsync(
                this,
                selectorRoot,
                filter,
                initialDirectory: Path.GetDirectoryName(initialFilePath),
                initialFilePath: initialFilePath);
            if (selectedPath is null)
                return;
            if (!GameAssetPath.TryFromProjectFile(
                    gameData.ProjectPath,
                    selectedPath,
                    out string assetPath))
            {
                return;
            }
            pathBox.Text = assetPath;
            commit(assetPath);
        };
        ToolTip.SetTip(browse, LocaleService.Get("BROWSE"));
        Button clear = new()
        {
            Content = LocaleService.Get("CLEAR"),
            Height = EditorInputs.FieldMinHeight,
        };
        clear.Click += (_, _) =>
        {
            if (string.IsNullOrEmpty(pathBox.Text))
                return;
            pathBox.Text = string.Empty;
            commit(string.Empty);
        };
        Grid row = new()
        {
            ColumnDefinitions = new ColumnDefinitions("*,Auto,Auto"),
            ColumnSpacing = 6,
        };
        row.Children.Add(pathBox);
        Grid.SetColumn(browse, 1);
        row.Children.Add(browse);
        Grid.SetColumn(clear, 2);
        row.Children.Add(clear);
        DetailsPanel.Children.Add(createField(label, row));
    }

    private string? getAssetInitialFilePath(string value)
    {
        return GameAssetPath.TryResolveExistingFile(gameData.ProjectPath, value, out string path)
            ? path
            : null;
    }

    private void addNumericField(
        string label,
        double value,
        double minimum,
        double maximum,
        double increment,
        Action<double> commit)
    {
        NumericUpDown box = EditorInputs.CreateNumericUpDown(
            (decimal)Math.Clamp(value, minimum, maximum),
            (decimal)minimum,
            (decimal)maximum,
            (decimal)increment);
        box.ValueChanged += (_, _) =>
        {
            if (!refreshing && box.Value is decimal number)
                commitDetailsField(() => commit((double)number));
        };
        DetailsPanel.Children.Add(createField(label, box));
    }

    private void addBoolField(
        string label,
        bool value,
        Action<bool> commit)
    {
        CheckBox box = new()
        {
            IsChecked = value,
            VerticalAlignment = VerticalAlignment.Center,
            HorizontalAlignment = HorizontalAlignment.Left,
        };
        box.IsCheckedChanged += (_, _) =>
        {
            if (!refreshing)
                commit(box.IsChecked == true);
        };
        DetailsPanel.Children.Add(createField(label, box));
    }

    private void addNumberArrayField(
        string label,
        JsonNode? value,
        int count,
        bool unsigned,
        Action<JsonNode?> commit,
        bool integral = false,
        double? explicitMaximum = null)
    {
        double[] values = readArray(value, count);
        addNumberArrayField(
            label,
            values,
            unsigned,
            integral,
            next =>
            {
                JsonArray result = new();
                foreach (double item in next)
                {
                    if (unsigned)
                        result.Add(JsonValue.Create((long)Math.Round(item)));
                    else if (integral)
                        result.Add(JsonValue.Create((int)Math.Round(item)));
                    else
                        result.Add(JsonValue.Create(item));
                }
                commit(result);
            },
            null,
            explicitMaximum);
    }

    private void addAxisPointField(
        string label,
        double x,
        double y,
        double minimum,
        double maximum,
        Action<double, double> commit)
    {
        double[] values = [x, y];
        addNumberArrayField(
            label,
            values,
            false,
            false,
            next => commit(
                Math.Clamp(next[0], minimum, maximum),
                Math.Clamp(next[1], minimum, maximum)),
            minimum,
            maximum,
            0.01,
            ["X", "Y"]);
    }

    private void addNumberArrayField(
        string label,
        double[] values,
        bool unsigned,
        bool integral,
        Action<double[]> commit,
        double? explicitMinimum = null,
        double? explicitMaximum = null,
        double? explicitIncrement = null,
        IReadOnlyList<string>? componentLabels = null)
    {
        Grid grid = new()
        {
            ColumnDefinitions = new ColumnDefinitions(
                string.Join(',', Enumerable.Repeat("*", values.Length))),
            ColumnSpacing = 4,
        };
        NumericUpDown[] boxes = new NumericUpDown[values.Length];
        double minimum = explicitMinimum ?? (unsigned ? 0 : -1000000000);
        double maximum = explicitMaximum ?? (unsigned ? uint.MaxValue : 1000000000);
        double increment = explicitIncrement ?? (integral || unsigned ? 1 : 0.1);
        for (int index = 0; index < boxes.Length; index++)
        {
            NumericUpDown box = EditorInputs.CreateNumericUpDown(
                (decimal)Math.Clamp(values[index], minimum, maximum),
                (decimal)minimum,
                (decimal)maximum,
                (decimal)increment);
            int valueIndex = index;
            box.ValueChanged += (_, _) =>
            {
                if (refreshing || box.Value is not decimal number)
                    return;
                double[] next = boxes
                    .Select(candidate => (double)(candidate.Value ?? 0))
                    .ToArray();
                next[valueIndex] = (double)number;
                commitDetailsField(() => commit(next));
            };
            boxes[index] = box;
            Control editor = box;
            if (componentLabels is not null
                && index < componentLabels.Count)
            {
                Grid component = new()
                {
                    ColumnDefinitions = new ColumnDefinitions("Auto,*"),
                    ColumnSpacing = 4,
                };
                TextBlock componentLabel = new()
                {
                    Text = componentLabels[index],
                    Foreground = EditorTheme.Brush("TextMuted"),
                    VerticalAlignment = VerticalAlignment.Center,
                };
                Grid.SetColumn(box, 1);
                component.Children.Add(componentLabel);
                component.Children.Add(box);
                editor = component;
            }
            Grid.SetColumn(editor, index);
            grid.Children.Add(editor);
        }
        DetailsPanel.Children.Add(createField(label, grid));
    }

    private void addColorField(
        string label,
        JsonNode? value,
        Action<JsonNode?> commit)
    {
        double[] channels = readArray(value, 4);
        BlueprintColourSwatch swatch = new(Color.FromArgb(
            toByte(channels[3], 255),
            toByte(channels[0], 255),
            toByte(channels[1], 255),
            toByte(channels[2], 255)));
        swatch.Click += async (_, _) =>
        {
            Color? result = await ColourPickerWindow.ShowAsync(this, swatch.Colour);
            if (result is not Color colour)
                return;
            swatch.Colour = colour;
            commit(new JsonArray(
                (int)colour.R,
                (int)colour.G,
                (int)colour.B,
                (int)colour.A));
        };
        DetailsPanel.Children.Add(createField(label, swatch));
    }

    private static Grid createField(string label, Control editor)
    {
        Grid result = new()
        {
            ColumnDefinitions = new ColumnDefinitions("118,*"),
            ColumnSpacing = 8,
        };
        TextBlock caption = new()
        {
            Text = label,
            TextWrapping = TextWrapping.Wrap,
            VerticalAlignment = VerticalAlignment.Center,
        };
        Grid.SetColumn(editor, 1);
        result.Children.Add(caption);
        result.Children.Add(editor);
        return result;
    }

    private void flushPendingField()
    {
        pendingFieldCommit?.Invoke();
        if (contentInitialized)
            timelineEditor.FlushPendingChanges();
    }

    private void commitDetailsField(Action commit)
    {
        bool wasCommitting = committingDetails;
        committingDetails = true;
        detailsRefreshPending = true;
        try
        {
            commit();
        }
        finally
        {
            committingDetails = wasCommitting;
        }
    }

    private void onResetView(object? sender, RoutedEventArgs args)
    {
        previewSurface.ResetView();
        updateZoomText();
    }

    private async void onValidate(object? sender, RoutedEventArgs args)
    {
        FlushPendingChanges();
        UiAssetValidationResult result = validationService.ValidateAsset(
            document.AssetKey,
            document.Data);
        if (result.IsValid)
        {
            setStatus(LocaleService.Get("UI_VALIDATION_SUCCEEDED"));
            return;
        }
        string message = string.Join(Environment.NewLine, result.Errors);
        setStatus(result.Errors.FirstOrDefault() ?? LocaleService.Get("UI_VALIDATION_FAILED"));
        await AlertDialog.ShowAsync(
            this,
            LocaleService.Get("UI_VALIDATION_FAILED"),
            message);
    }

    private void onRefreshPreview(object? sender, RoutedEventArgs args)
    {
        flushPendingField();
        requestPreview(true);
    }

    private void requestPreview(bool immediate = false)
    {
        if (!closed && contentInitialized)
            previewSession.RequestRefresh(previewSurface.RenderScale, timelineEditor.CurrentSample,
                immediate, transformStartSlot is not null);
    }

    private void onPreviewFrameReady(object? sender, UiPreviewFrame frame)
    {
        if (closed)
            return;
        previewSurface.SetFrame(frame);
        previewSurface.SetSelectedNode(selectedNodeName);
        updateAnchorGuides();
        updatePreviewState();
    }

    private void onPreviewStateChanged(object? sender, EventArgs args)
    {
        if (!closed)
            updatePreviewState();
    }

    private void updatePreviewState()
    {
        if (!controlRegistry.IsReady)
        {
            PreviewStateText.Text = LocaleService.Get("UI_PREVIEW_UNAVAILABLE");
            previewSurface.SetUnavailable(controlRegistry.Runtime.StatusMessage);
            return;
        }
        PreviewStateText.Text = previewSession.State switch
        {
            UiPreviewClientState.Ready => LocaleService.Get("UI_PREVIEW_READY"),
            UiPreviewClientState.Rendering => LocaleService.Get("UI_PREVIEW_RENDERING"),
            UiPreviewClientState.Starting => LocaleService.Get("UI_PREVIEW_STARTING"),
            UiPreviewClientState.Faulted => LocaleService.Get("UI_PREVIEW_FAILED"),
            _ => LocaleService.Get("UI_PREVIEW_UNAVAILABLE"),
        };
        if (previewSession.State is UiPreviewClientState.Unavailable
            or UiPreviewClientState.Faulted)
        {
            string message = previewSession.StatusMessage.Length == 0
                ? LocaleService.Get("UI_PREVIEW_COMPILE_REQUIRED")
                : previewSession.StatusMessage;
            previewSurface.SetUnavailable(message);
        }
    }

    private void onPreviewNodeSelected(object? sender, UiPreviewNodeEventArgs args)
    {
        flushPendingField();
        selectedNodeName = args.NodeName;
        refreshHierarchy();
        refreshDetails();
    }

    private void onPreviewTransformStarted(
        object? sender,
        UiPreviewTransformEventArgs args)
    {
        if (!tryGetDesignerCanvasSlot(
                args.NodeName,
                out JsonObject slot))
        {
            return;
        }
        transformStartSlot = (JsonObject)slot.DeepClone();
        document.BeginGesture();
    }

    private void onPreviewTransformChanged(
        object? sender,
        UiPreviewTransformEventArgs args)
    {
        applyPreviewTransform(args);
    }

    private void onPreviewTransformCompleted(
        object? sender,
        UiPreviewTransformEventArgs args)
    {
        if (transformStartSlot is null)
            return;
        applyPreviewTransform(args);
        transformStartSlot = null;
        document.CommitGesture();
        refreshAll(true);
    }

    private void onPreviewTransformCancelled(object? sender, EventArgs args)
    {
        if (transformStartSlot is null)
            return;
        transformStartSlot = null;
        document.CancelGesture();
        requestPreview(true);
    }

    private void applyPreviewTransform(UiPreviewTransformEventArgs args)
    {
        if (transformStartSlot is null)
            return;
        JsonObject slot = (JsonObject)transformStartSlot.DeepClone();
        JsonObject anchors = slot["anchors"] as JsonObject ?? new JsonObject();
        JsonArray min = anchors["min"] as JsonArray ?? new JsonArray(0, 0);
        JsonArray max = anchors["max"] as JsonArray ?? new JsonArray(0, 0);
        JsonObject offsets = slot["offsets"] as JsonObject ?? new JsonObject();
        double left = getDouble(offsets["left"], 0);
        double top = getDouble(offsets["top"], 0);
        double right = getDouble(offsets["right"], 100);
        double bottom = getDouble(offsets["bottom"], 34);
        bool stretchX = Math.Abs(getDouble(max[0], 0) - getDouble(min[0], 0)) > 0.000001;
        bool stretchY = Math.Abs(getDouble(max[1], 0) - getDouble(min[1], 0)) > 0.000001;
        if (args.Resize)
        {
            right += stretchX ? -args.DeltaX : args.DeltaX;
            bottom += stretchY ? -args.DeltaY : args.DeltaY;
        }
        else
        {
            left += args.DeltaX;
            top += args.DeltaY;
            if (stretchX)
                right -= args.DeltaX;
            if (stretchY)
                bottom -= args.DeltaY;
        }
        slot["offsets"] = new JsonObject
        {
            ["left"] = left,
            ["top"] = top,
            ["right"] = right,
            ["bottom"] = bottom,
        };
        document.SetNodeSlot(args.NodeName, slot);
    }

    private bool tryGetDesignerCanvasSlot(
        string nodeName,
        out JsonObject slot)
    {
        slot = null!;
        JsonObject? node = document.FindNode(nodeName);
        JsonObject? parent = document.FindParent(nodeName);
        if (node?["slot"] is not JsonObject currentSlot
            || parent is null
            || !controlLookup.TryGetValue(
                getString(parent, "controlId"),
                out UiControlDescriptor? descriptor)
            || !string.Equals(
                descriptor.SlotType,
                "canvas",
                StringComparison.Ordinal))
        {
            return false;
        }
        slot = currentSlot;
        return true;
    }

    private void updateZoomText()
    {
        ZoomText.Text = Math.Round(previewSurface.Zoom * 100)
            .ToString(CultureInfo.InvariantCulture) + "%";
    }

    private void updateAnchorGuides()
    {
        if (timelineEditor.CurrentSample is not null
            || selectedNodeName is null
            || !tryGetDesignerCanvasSlot(
                selectedNodeName,
                out JsonObject slot)
            || slot["anchors"] is not JsonObject anchors
            || anchors["min"] is not JsonArray minimum
            || anchors["max"] is not JsonArray maximum)
        {
            previewSurface.HideAnchorGuides();
            return;
        }
        previewSurface.SetAnchorGuides(
            getDesignWidth(),
            getDesignHeight(),
            getDouble(minimum[0], 0),
            getDouble(minimum[1], 0),
            getDouble(maximum[0], 0),
            getDouble(maximum[1], 0));
    }

    private void configureHierarchyDragDrop()
    {
        HierarchyTree.AddHandler(
            PointerPressedEvent,
            onHierarchyPointerPressed,
            RoutingStrategies.Tunnel);
        HierarchyTree.AddHandler(
            InputElement.ContextRequestedEvent,
            onHierarchyContextRequested,
            RoutingStrategies.Tunnel);
        HierarchyTree.PointerMoved += onHierarchyPointerMoved;
        HierarchyTree.PointerReleased += onHierarchyPointerReleased;
        DragDrop.SetAllowDrop(HierarchyTree, true);
        HierarchyTree.AddHandler(DragDrop.DragOverEvent, onHierarchyDragOver);
        HierarchyTree.AddHandler(DragDrop.DropEvent, onHierarchyDrop);
    }

    private void onHierarchyPointerPressed(
        object? sender,
        PointerPressedEventArgs args)
    {
        clearHierarchyDrag();
        PointerPoint point = args.GetCurrentPoint(this);
        UiHierarchyItem? item = getHierarchyItem(args.Source);
        if (!point.Properties.IsLeftButtonPressed
            || item is null
            || document.FindParent(item.NodeName) is null)
        {
            return;
        }
        hierarchyDragPress = args;
        hierarchyDragStart = point.Position;
        hierarchyDragText = DragPrefix + item.NodeName;
        hierarchyDragEffects = DragDropEffects.Move;
    }

    private void onPalettePointerPressed(object? sender, PointerPressedEventArgs args)
    {
        clearHierarchyDrag();
        PointerPoint point = args.GetCurrentPoint(this);
        if (!point.Properties.IsLeftButtonPressed || args.ClickCount != 1
            || sender is not Button { Tag: UiControlDescriptor descriptor })
        {
            return;
        }
        hierarchyDragPress = args;
        hierarchyDragStart = point.Position;
        hierarchyDragText = PaletteDragPrefix + descriptor.ControlId;
        hierarchyDragEffects = DragDropEffects.Copy;
    }

    private void onHierarchyContextRequested(
        object? sender,
        ContextRequestedEventArgs args)
    {
        if (!args.TryGetPosition(HierarchyTree, out _))
            return;
        UiHierarchyItem? item = getHierarchyItem(args.Source);
        if (item is null)
            return;
        showHierarchyContextMenu(item);
        args.Handled = true;
    }

    private async void onHierarchyPointerMoved(
        object? sender,
        PointerEventArgs args)
    {
        if (startingHierarchyDrag
            || hierarchyDragStart is not Point start
            || hierarchyDragPress is null
            || hierarchyDragText is null)
        {
            return;
        }
        PointerPoint point = args.GetCurrentPoint(this);
        if (!point.Properties.IsLeftButtonPressed)
            return;
        Point current = point.Position;
        if (Math.Abs(current.X - start.X) < 4
            && Math.Abs(current.Y - start.Y) < 4)
        {
            return;
        }
        startingHierarchyDrag = true;
        DataTransfer data = new();
        data.Add(DataTransferItem.CreateText(hierarchyDragText));
        await DragDrop.DoDragDropAsync(
            hierarchyDragPress,
            data,
            hierarchyDragEffects);
        startingHierarchyDrag = false;
        clearHierarchyDrag();
    }

    private void onHierarchyPointerReleased(
        object? sender,
        PointerReleasedEventArgs args)
    {
        clearHierarchyDrag();
    }

    private void onHierarchyDragOver(object? sender, DragEventArgs args)
    {
        string? nodeName = getDraggedValue(args, DragPrefix);
        string? controlId = getDraggedValue(args, PaletteDragPrefix);
        UiHierarchyItem? target = getHierarchyDropTarget(args.Source);
        UiAssetEditingService.DropPosition position = getDropPosition(args);
        args.DragEffects = DragDropEffects.None;
        if (target is not null)
        {
            if (nodeName is not null
                && document.TryGetDropLocation(nodeName, target.NodeName, position, out _, out _))
            {
                args.DragEffects = DragDropEffects.Move;
            }
            else if (controlId is not null
                && document.TryGetControlDropLocation(controlId, target.NodeName, position, out _, out _, out _))
            {
                args.DragEffects = DragDropEffects.Copy;
            }
        }
        args.Handled = true;
    }

    private async void onHierarchyDrop(object? sender, DragEventArgs args)
    {
        string? nodeName = getDraggedValue(args, DragPrefix);
        string? controlId = getDraggedValue(args, PaletteDragPrefix);
        UiHierarchyItem? target = getHierarchyDropTarget(args.Source);
        if (target is null)
            return;
        args.Handled = true;
        args.DragEffects = DragDropEffects.None;
        flushPendingField();
        UiAssetEditingService.DropPosition position = getDropPosition(args);
        if (controlId is not null
            && controlLookup.TryGetValue(controlId, out UiControlDescriptor? descriptor)
            && document.TryGetControlDropLocation(controlId, target.NodeName, position,
                out string addParent, out int addIndex, out _))
        {
            addControl(descriptor, addParent, addIndex);
            args.DragEffects = DragDropEffects.Copy;
        }
        else if (nodeName is not null
            && document.TryGetDropLocation(nodeName, target.NodeName, position,
                out string parentName, out int index))
        {
            args.DragEffects = DragDropEffects.Move;
            await moveNodeAsync(nodeName, parentName, index);
        }
    }

    private static UiAssetEditingService.DropPosition getDropPosition(DragEventArgs args)
    {
        TreeViewItem? container = getHierarchyContainer(args.Source);
        Control? header = container?.GetVisualDescendants().OfType<ContentPresenter>()
            .FirstOrDefault(presenter => presenter.Name == "PART_HeaderPresenter"
                && ReferenceEquals(presenter.TemplatedParent, container));
        double relativeY = header is null
            ? 0.5
            : args.GetPosition(header).Y / Math.Max(1, header.Bounds.Height);
        return relativeY < 0.25
            ? UiAssetEditingService.DropPosition.Before
            : relativeY > 0.75
                ? UiAssetEditingService.DropPosition.After
                : UiAssetEditingService.DropPosition.Inside;
    }

    private UiHierarchyItem? getHierarchyDropTarget(object? source)
    {
        return getHierarchyItem(source)
            ?? (HierarchyTree.ItemsSource as IEnumerable<UiHierarchyItem>)?.FirstOrDefault();
    }

    private static UiHierarchyItem? getHierarchyItem(object? source)
    {
        if (source is Control { DataContext: UiHierarchyItem item })
            return item;
        return (source as Visual)?
            .GetVisualAncestors()
            .OfType<Control>()
            .Select(control => control.DataContext)
            .OfType<UiHierarchyItem>()
            .FirstOrDefault();
    }

    private static TreeViewItem? getHierarchyContainer(object? source)
    {
        if (source is TreeViewItem item)
            return item;
        return (source as Visual)?
            .GetVisualAncestors()
            .OfType<TreeViewItem>()
            .FirstOrDefault();
    }

    private static string? getDraggedValue(DragEventArgs args, string prefix)
    {
        string? text = args.DataTransfer.TryGetText();
        return text is not null && text.StartsWith(prefix, StringComparison.Ordinal)
            ? text[prefix.Length..]
            : null;
    }

    private void clearHierarchyDrag()
    {
        hierarchyDragPress = null;
        hierarchyDragStart = null;
        hierarchyDragText = null;
    }

    private double getDesignWidth()
    {
        return getDouble((document.Data["designSize"] as JsonObject)?["width"], 640);
    }

    private double getDesignHeight()
    {
        return getDouble((document.Data["designSize"] as JsonObject)?["height"], 480);
    }

    private bool getPaletteExposed()
    {
        return getBool((document.Data["palette"] as JsonObject)?["exposed"], true);
    }

    private string getPaletteDisplayName()
    {
        return getString(document.Data["palette"] as JsonObject, "displayName", document.Title);
    }

    private string getPaletteCategory()
    {
        return getString(document.Data["palette"] as JsonObject, "category", "Project");
    }

    private void setStatus(string message)
    {
        StatusText.Text = message;
    }

    private static double[] readArray(JsonNode? value, int count)
    {
        double[] result = new double[count];
        if (value is not JsonArray array)
            return result;
        for (int index = 0; index < result.Length && index < array.Count; index++)
            result[index] = getDouble(array[index], 0);
        return result;
    }

    private static byte toByte(double value, byte fallback)
    {
        return double.IsFinite(value)
            ? (byte)Math.Clamp(Math.Round(value), 0, 255)
            : fallback;
    }

    private static string getString(
        JsonObject? value,
        string propertyName,
        string fallback = "")
    {
        return value?[propertyName] is JsonValue scalar
            && scalar.TryGetValue(out string? text)
                ? text ?? fallback
                : fallback;
    }

    private static string getString(JsonNode? value)
    {
        return value is JsonValue scalar
            && scalar.TryGetValue(out string? text)
                ? text ?? string.Empty
                : string.Empty;
    }

    private static bool getBool(JsonNode? value, bool fallback)
    {
        return value is JsonValue scalar
            && scalar.TryGetValue(out bool result)
                ? result
                : fallback;
    }

    private static double getDouble(JsonNode? value, double fallback)
    {
        if (value is JsonValue scalar)
        {
            if (scalar.TryGetValue(out double result) && double.IsFinite(result))
                return result;
            if (scalar.TryGetValue(out int integer))
                return integer;
            if (scalar.TryGetValue(out long longInteger))
                return longInteger;
        }
        return fallback;
    }
}
