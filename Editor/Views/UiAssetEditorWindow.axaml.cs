using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Threading;
using Ludork.Controls;
using Ludork.Models;
using Ludork.Services;
using Ludork.Services.UiAssets;
using Ludork.Views.Utils;
using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Text.Json.Nodes;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Views;

public partial class UiAssetEditorWindow : Window, IProjectSaveParticipant
{
    private readonly UiAssetEditorDocument document = null!;
    private readonly EditorDocumentBinding documentBinding = null!;
    private Toast? toast;
    private readonly ProjectDataStore gameData = null!;
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
    private JsonObject? transformStartSlot;
    private bool refreshing;
    private bool contentInitialized;
    private bool closed;
    private bool refreshPending;
    private string? paletteSignature;
    private string? hierarchySignature;
    private string? animationSignature;
    private string? relatedAssetsRevision;
    private IReadOnlyList<UiControlDescriptor> paletteDescriptors = [];

    public UiAssetEditorWindow()
    {
        Content = DeferredWindowInitializer.CreateLoadingContent();
    }

    public UiAssetEditorWindow(
        UiAssetEditorDocument document,
        ProjectDataStore gameData,
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
        EditorLayoutService.AttachWindow(this, nameof(UiAssetEditorWindow));
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
            bindEditorLayout();
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

    private void bindEditorLayout()
    {
        EditorLayoutService.BindColumns(EditorLayout, PaletteColumnSplitter, "UiAssetEditor.Columns", 0, 2, 4);
        EditorLayoutService.BindColumns(EditorLayout, DetailsColumnSplitter, "UiAssetEditor.Columns", 0, 2, 4);
        EditorLayoutService.BindRows(PaletteLayout, PaletteSplitter, "UiAssetEditor.PaletteRows", 0, 2);
        EditorLayoutService.BindRows(PreviewLayout, TimelineSplitter, "UiAssetEditor.PreviewRows", 1, 3);
        EditorLayoutService.BindExpander(TimelineExpander, "UiAssetEditor.Timeline");
        Grid previewLayout = PreviewLayout;
        GridSplitter timelineSplitter = TimelineSplitter;
        Expander timelineExpander = TimelineExpander;
        GridLength previewHeight = previewLayout.RowDefinitions[1].Height;
        GridLength timelineHeight = previewLayout.RowDefinitions[3].Height;
        void applyTimelineVisibility()
        {
            bool expanded = timelineExpander.IsExpanded;
            if (!expanded)
            {
                previewHeight = previewLayout.RowDefinitions[1].Height;
                timelineHeight = previewLayout.RowDefinitions[3].Height;
            }
            previewLayout.RowDefinitions[1].Height = expanded ? previewHeight : new GridLength(1, GridUnitType.Star);
            previewLayout.RowDefinitions[2].Height = new GridLength(expanded ? 5 : 0);
            previewLayout.RowDefinitions[3].Height = expanded ? timelineHeight : GridLength.Auto;
            timelineSplitter.IsVisible = expanded;
        }
        timelineExpander.PropertyChanged += (_, args) =>
        {
            if (args.Property == Expander.IsExpandedProperty)
                applyTimelineVisibility();
        };
        applyTimelineVisibility();
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
        if (projectSave.GameData.EditOperations.IsBusy)
        {
            args.Handled = true;
            return;
        }
        bool save = EditorShortcuts.HasPrimaryModifier(args.KeyModifiers) && args.Key == Key.S;
        bool undo = EditorShortcuts.IsUndo(args.Key, args.KeyModifiers);
        if (!save && !undo && !EditorShortcuts.IsRedo(args.Key, args.KeyModifiers))
            return;
        FlushPendingChanges();
        await documentBinding.HandleShortcutAsync(args, projectSave, toast);
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
                item.PointerCaptureLost += onHierarchyPointerCaptureLost;
                entries.Children.Add(item);
            }
            Expander category = new()
            {
                Header = group.Key,
                IsExpanded = true,
                Content = entries,
            };
            UiControlDescriptor first = group.First();
            if (!string.Equals(first.Source, "project", StringComparison.Ordinal))
                EditorLayoutService.BindExpander(category, "UiAssetEditor.Palette." + first.Category);
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
        clearHierarchyDropIndicator();
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
        return JsonScalar.String(value?[propertyName], fallback);
    }

    private static string getString(JsonNode? value)
    {
        return JsonScalar.String(value, string.Empty);
    }

    private static bool getBool(JsonNode? value, bool fallback)
    {
        return JsonScalar.Bool(value, fallback);
    }

    private static double getDouble(JsonNode? value, double fallback) => JsonScalar.FiniteNumber(value, fallback);
}
