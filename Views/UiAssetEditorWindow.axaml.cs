using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Threading;
using Avalonia.VisualTree;
using Ludork.Controls;
using Ludork.Models;
using Ludork.Services;
using Ludork.Views.Utils;
using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Views;

public partial class UiAssetEditorWindow : Window, IProjectSaveParticipant
{
    private const string DragPrefix = "ludork-ui-node-name:";
    private readonly UiAssetEditorDocument document = null!;
    private readonly GameDataService gameData = null!;
    private readonly UiControlRegistryService controlRegistry = null!;
    private readonly UiAssetValidationService validationService = null!;
    private readonly ProjectSaveService projectSave = null!;
    private UiPreviewClient previewClient = null!;
    private UiPreviewSurface previewSurface = null!;
    private readonly DeferredWindowInitializer initializer = null!;
    private readonly HashSet<string> lockedNodeNames = new(StringComparer.Ordinal);
    private IReadOnlyDictionary<string, UiControlDescriptor> controlLookup =
        new Dictionary<string, UiControlDescriptor>(StringComparer.Ordinal);
    private string? selectedNodeName;
    private Action? pendingFieldCommit;
    private CancellationTokenSource? previewCancellation;
    private PointerPressedEventArgs? hierarchyDragPress;
    private Point? hierarchyDragStart;
    private string? hierarchyDragNodeName;
    private JsonObject? transformStartSlot;
    private bool startingHierarchyDrag;
    private bool refreshing;
    private bool contentInitialized;

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
        Closing += onClosing;
        projectSave.RegisterParticipant(this);
        initializer = new DeferredWindowInitializer(this, () =>
        {
            InitializeComponent();
            contentInitialized = true;
            previewClient = new UiPreviewClient(gameData.ProjectPath);
            previewSurface = new UiPreviewSurface
            {
                HitTestResolver = (generation, x, y) =>
                    previewClient.HitTestAsync(generation, x, y),
            };
            PreviewContainer.Content = previewSurface;
            EditorInputs.ApplyEditable(PaletteSearch);
            configureHierarchyDragDrop();
            applyLocale();
            previewClient.StateChanged += onPreviewStateChanged;
            previewSurface.NodeSelected += onPreviewNodeSelected;
            previewSurface.TransformStarted += onPreviewTransformStarted;
            previewSurface.TransformChanged += onPreviewTransformChanged;
            previewSurface.TransformCompleted += onPreviewTransformCompleted;
            previewSurface.TransformCancelled += onPreviewTransformCancelled;
            previewSurface.ZoomChanged += (_, _) =>
            {
                updateZoomText();
                schedulePreview();
            };
            ScalingChanged += (_, _) => schedulePreview();
            refreshAll();
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

    private async void onClosing(object? sender, WindowClosingEventArgs args)
    {
        FlushPendingChanges();
        document.Changed -= onDocumentChanged;
        projectSave.UnregisterParticipant(this);
        if (!initializer.IsInitialized)
            return;
        previewCancellation?.Cancel();
        await previewClient.DisposeAsync();
    }

    private void applyLocale()
    {
        PaletteTitle.Text = LocaleService.Get("UI_PALETTE");
        ToolTip.SetTip(PaletteSearch, LocaleService.Get("SEARCH"));
        HierarchyTitle.Text = LocaleService.Get("UI_HIERARCHY");
        DesignerTitle.Text = LocaleService.Get("UI_DESIGNER");
        DetailsTitle.Text = LocaleService.Get("DETAILS");
        ResetViewButton.Content = LocaleService.Get("RESET_VIEW");
        ValidateButton.Content = LocaleService.Get("VALIDATE");
        RefreshPreviewButton.Content = LocaleService.Get("REFRESH_PREVIEW");
        updateTitle();
        updatePreviewState();
    }

    private void updateTitle()
    {
        string title = document.Title;
        Title = title + " - " + LocaleService.Get("UI_ASSET_EDITOR");
        if (contentInitialized)
            DocumentTitle.Text = title;
    }

    private void onDocumentChanged(object? sender, EventArgs args)
    {
        if (refreshing || !initializer.IsInitialized)
            return;
        refreshAll();
    }

    private void refreshAll()
    {
        refreshing = true;
        try
        {
            controlLookup = controlRegistry.CreateControlLookup(true);
            refreshPalette();
            refreshHierarchy();
            refreshDetails();
            updateAnchorGuides();
            updateZoomText();
        }
        finally
        {
            refreshing = false;
        }
        schedulePreview();
    }

    private void refreshPalette()
    {
        PaletteCategories.Children.Clear();
        string search = PaletteSearch.Text?.Trim() ?? string.Empty;
        IEnumerable<UiControlDescriptor> descriptors = controlRegistry
            .GetDescriptors()
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
        if (root is null)
        {
            HierarchyTree.ItemsSource = Array.Empty<UiHierarchyItem>();
            lockedNodeNames.Clear();
            selectedNodeName = null;
            previewSurface.SetSelectedNode(null);
            return;
        }
        HashSet<string> nodeNames = UiAssetSchema.EnumerateNodes(document.Data)
            .Select(node => getString(node, "name"))
            .Where(name => name.Length != 0)
            .ToHashSet(StringComparer.Ordinal);
        lockedNodeNames.IntersectWith(nodeNames);
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
            lockedNodeNames.Contains(nodeName),
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

    private void addControl(UiControlDescriptor descriptor)
    {
        JsonObject? parent = getAddParent();
        if (parent is null)
        {
            setStatus(LocaleService.Get("UI_SELECT_CONTAINER"));
            return;
        }
        string parentName = getString(parent, "name");
        if (!canAcceptChild(parent, null))
        {
            setStatus(LocaleService.Get("UI_CONTAINER_REJECTS_CHILD"));
            return;
        }
        if (wouldCreateAssetCycle(descriptor))
        {
            setStatus(LocaleService.Get("UI_ASSET_CYCLE"));
            return;
        }
        JsonObject defaults = new();
        foreach (UiControlPropertyDescriptor property in descriptor.Properties)
        {
            if (!property.EditorOnly && property.Default is not null)
                defaults[property.Id] = property.Default.DeepClone();
        }
        JsonObject slot = createSlot(parent);
        string? nodeName = document.AddNode(
            parentName,
            descriptor.ControlId,
            descriptor.DisplayName,
            defaults,
            slot);
        if (nodeName is null)
            return;
        selectedNodeName = nodeName;
        refreshAll();
    }

    private JsonObject? getAddParent()
    {
        JsonObject? selected = selectedNodeName is null
            ? null
            : document.FindNode(selectedNodeName);
        if (selected is not null && canAcceptChild(selected, null))
            return selected;
        if (selectedNodeName is not null)
        {
            JsonObject? parent = document.FindParent(selectedNodeName);
            if (parent is not null && canAcceptChild(parent, null))
                return parent;
        }
        return document.Data["root"] as JsonObject;
    }

    private bool canAcceptChild(JsonObject parent, string? movingNodeName)
    {
        string controlId = getString(parent, "controlId");
        if (!controlLookup.TryGetValue(controlId, out UiControlDescriptor? descriptor))
            return false;
        if (string.Equals(descriptor.ChildPolicy, "multiple", StringComparison.Ordinal))
            return true;
        if (!string.Equals(descriptor.ChildPolicy, "single", StringComparison.Ordinal))
            return false;
        int childCount = parent["children"] is JsonArray children
            ? children.OfType<JsonObject>()
                .Count(child => !string.Equals(
                    getString(child, "name"),
                    movingNodeName,
                    StringComparison.Ordinal))
            : 0;
        return childCount == 0;
    }

    private JsonObject createSlot(JsonObject parent)
    {
        string controlId = getString(parent, "controlId");
        if (controlLookup.TryGetValue(controlId, out UiControlDescriptor? descriptor)
            && string.Equals(descriptor.SlotType, "canvas", StringComparison.Ordinal))
        {
            return UiAssetEditorDocument.CreateDefaultCanvasSlot();
        }
        return new JsonObject();
    }

    private bool wouldCreateAssetCycle(UiControlDescriptor descriptor)
    {
        if (descriptor.AssetKey is null)
            return false;
        string currentAssetKey = document.AssetKey;
        if (string.Equals(currentAssetKey, descriptor.AssetKey, StringComparison.Ordinal))
            return true;
        return assetReferences(
            descriptor.AssetKey,
            currentAssetKey,
            new HashSet<string>(StringComparer.Ordinal));
    }

    private bool assetReferences(
        string assetKey,
        string targetAssetKey,
        ISet<string> visited)
    {
        if (!visited.Add(assetKey))
            return false;
        string dataKey = UiAssetSchema.ToAssetDataKey(assetKey);
        if (!gameData.UiAssetsData.TryGetValue(dataKey, out JsonObject? asset))
            return false;
        foreach (JsonObject node in UiAssetSchema.EnumerateNodes(asset))
        {
            string controlId = getString(node, "controlId");
            if (!UiAssetSchema.TryGetProjectAssetKey(controlId, out string nestedDataKey))
                continue;
            string nestedAssetKey = nestedDataKey;
            if (string.Equals(nestedAssetKey, targetAssetKey, StringComparison.Ordinal)
                || assetReferences(nestedAssetKey, targetAssetKey, visited))
            {
                return true;
            }
        }
        return false;
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

    private void onHierarchyLockClicked(object? sender, RoutedEventArgs args)
    {
        if (sender is not CheckBox
            {
                DataContext: UiHierarchyItem item,
            } checkBox)
        {
            return;
        }
        if (checkBox.IsChecked == true)
            lockedNodeNames.Add(item.NodeName);
        else
            lockedNodeNames.Remove(item.NodeName);
        refreshHierarchy();
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
        JsonObject? parent = document.FindParent(selectedNodeName);
        if (parent is null || !canAcceptChild(parent, null))
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
        if (!tryGetNodeLocation(
                selectedNodeName,
                out JsonObject? parent,
                out JsonArray? siblings,
                out int index)
            || parent is null
            || siblings is null)
        {
            return;
        }
        int target = index + direction;
        if (target < 0 || target >= siblings.Count)
            return;
        document.MoveNode(selectedNodeName!, getString(parent, "name"), target);
    }

    private void onIndent(object? sender, RoutedEventArgs args)
    {
        if (!tryGetNodeLocation(
                selectedNodeName,
                out JsonObject? parent,
                out JsonArray? siblings,
                out int index)
            || parent is null
            || siblings is null
            || index <= 0
            || siblings[index - 1] is not JsonObject destination
            || !canAcceptChild(destination, selectedNodeName))
        {
            return;
        }
        int childCount = destination["children"] is JsonArray children
            ? children.Count
            : 0;
        document.MoveNode(
            selectedNodeName!,
            getString(destination, "name"),
            childCount,
            createSlot(destination));
    }

    private void onOutdent(object? sender, RoutedEventArgs args)
    {
        if (!tryGetNodeLocation(
                selectedNodeName,
                out JsonObject? parent,
                out _,
                out _)
            || parent is null)
        {
            return;
        }
        string parentName = getString(parent, "name");
        if (!tryGetNodeLocation(
                parentName,
                out JsonObject? grandParent,
                out JsonArray? parentSiblings,
                out int parentIndex)
            || grandParent is null
            || parentSiblings is null
            || !canAcceptChild(grandParent, selectedNodeName))
        {
            return;
        }
        document.MoveNode(
            selectedNodeName!,
            getString(grandParent, "name"),
            parentIndex + 1,
            createSlot(grandParent));
    }

    private bool tryGetNodeLocation(
        string? nodeName,
        out JsonObject? parent,
        out JsonArray? siblings,
        out int index)
    {
        parent = nodeName is null ? null : document.FindParent(nodeName);
        siblings = parent?["children"] as JsonArray;
        index = -1;
        if (siblings is null)
            return false;
        for (int candidateIndex = 0; candidateIndex < siblings.Count; candidateIndex++)
        {
            if (siblings[candidateIndex] is JsonObject child
                && string.Equals(getString(child, "name"), nodeName, StringComparison.Ordinal))
            {
                index = candidateIndex;
                return true;
            }
        }
        return false;
    }

    private void showHierarchyContextMenu(UiHierarchyItem item)
    {
        if (!ReferenceEquals(HierarchyTree.SelectedItem, item))
            HierarchyTree.SelectedItem = item;
        JsonObject? parent = document.FindParent(item.NodeName);
        bool canModify = parent is not null
            && !lockedNodeNames.Contains(item.NodeName);
        MenuItem delete = new()
        {
            Header = LocaleService.Get("DELETE"),
            IsEnabled = canModify,
        };
        delete.Click += onDeleteNode;
        MenuItem duplicate = new()
        {
            Header = LocaleService.Get("DUPLICATE"),
            IsEnabled = canModify
                && parent is not null
                && canAcceptChild(parent, null),
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

    private void refreshDetails()
    {
        pendingFieldCommit = null;
        DetailsPanel.Children.Clear();
        JsonObject? node = selectedNodeName is null
            ? null
            : document.FindNode(selectedNodeName);
        if (node is null)
            return;
        bool isRoot = document.FindParent(selectedNodeName!) is null;
        if (isRoot)
            addAssetDetails();
        addWidgetDetails(node);
        if (!isRoot)
            addSlotDetails(node);
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
            addPropertyField(nodeName, descriptor.ControlId, property, value);
        }
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
            refreshDetails();
            return;
        }
        if (string.Equals(selectedNodeName, nodeName, StringComparison.Ordinal))
            selectedNodeName = nextName;
        if (lockedNodeNames.Remove(nodeName))
            lockedNodeNames.Add(nextName);
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
            case "string" when property.Id is "texture"
                or "lineTexture"
                or "handleTexture":
                addTextureField(
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
                    -2147483648,
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
                Foreground = Brushes.Gray,
                TextWrapping = TextWrapping.Wrap,
            });
            return;
        }
        if (!string.Equals(parentDescriptor.SlotType, "canvas", StringComparison.Ordinal))
            return;
        JsonObject slot = node["slot"] as JsonObject
            ?? UiAssetEditorDocument.CreateDefaultCanvasSlot();
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
            : UiAssetEditorDocument.CreateDefaultCanvasSlot();
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
        Action commitValue = () => commit(box.Text ?? string.Empty);
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
            if (args.Key != Key.Enter)
                return;
            if (!ReferenceEquals(pendingFieldCommit, commitValue))
                return;
            pendingFieldCommit = null;
            commitValue();
            args.Handled = true;
        };
        DetailsPanel.Children.Add(createField(label, box));
    }

    private void addTextureField(
        string label,
        string value,
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
            string assetsRoot = Path.Combine(gameData.ProjectPath, "Assets");
            Directory.CreateDirectory(assetsRoot);
            string? initialFilePath = getTextureInitialFilePath(pathBox.Text ?? string.Empty);
            string? selectedPath = await FileSelectorDialog.ShowAsync(
                this,
                assetsRoot,
                FileSelectorDialog.ImageFilesFilter(),
                initialDirectory: Path.GetDirectoryName(initialFilePath),
                initialFilePath: initialFilePath);
            if (selectedPath is null)
                return;
            string relativePath = Path.GetRelativePath(
                    gameData.ProjectPath,
                    selectedPath)
                .Replace('\\', '/');
            pathBox.Text = relativePath;
            commit(relativePath);
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

    private string? getTextureInitialFilePath(string value)
    {
        if (!value.StartsWith("Assets/", StringComparison.Ordinal))
            return null;
        return Path.Combine(
            gameData.ProjectPath,
            value.Replace('/', Path.DirectorySeparatorChar));
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
                commit((double)number);
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
                commit(next);
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
                    Foreground = Brushes.Gray,
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
        Action? commit = pendingFieldCommit;
        pendingFieldCommit = null;
        commit?.Invoke();
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

    private async void onRefreshPreview(object? sender, RoutedEventArgs args)
    {
        await refreshPreviewAsync();
    }

    private void schedulePreview()
    {
        CancellationTokenSource next = new();
        CancellationTokenSource? previous = previewCancellation;
        previewCancellation = next;
        previous?.Cancel();
        _ = schedulePreviewAsync(next.Token);
    }

    private async Task schedulePreviewAsync(CancellationToken cancellationToken)
    {
        try
        {
            await Task.Delay(140, cancellationToken);
            await refreshPreviewAsync(cancellationToken);
        }
        catch (OperationCanceledException)
        {
        }
    }

    private async Task refreshPreviewAsync(CancellationToken cancellationToken = default)
    {
        IReadOnlyDictionary<string, JsonObject> dependencies = collectDependencies();
        UiPreviewFrame? frame = await previewClient.RenderAsync(
            document.AssetKey,
            document.Data,
            dependencies,
            previewSurface.RenderScale,
            cancellationToken);
        if (frame is null || cancellationToken.IsCancellationRequested)
        {
            updatePreviewState();
            return;
        }
        previewSurface.SetFrame(frame);
        previewSurface.SetSelectedNode(selectedNodeName);
        updateAnchorGuides();
        updatePreviewState();
    }

    private IReadOnlyDictionary<string, JsonObject> collectDependencies()
    {
        Dictionary<string, JsonObject> result = new(StringComparer.Ordinal);
        HashSet<string> visited = new(StringComparer.Ordinal);
        collectDependencies(document.Data, result, visited);
        return result;
    }

    private void collectDependencies(
        JsonObject asset,
        IDictionary<string, JsonObject> result,
        ISet<string> visited)
    {
        foreach (JsonObject node in UiAssetSchema.EnumerateNodes(asset))
        {
            string controlId = getString(node, "controlId");
            if (!UiAssetSchema.TryGetProjectAssetKey(controlId, out string assetKey))
                continue;
            if (!visited.Add(assetKey))
                continue;
            string dataKey = UiAssetSchema.ToAssetDataKey(assetKey);
            if (!gameData.UiAssetsData.TryGetValue(dataKey, out JsonObject? dependency))
                continue;
            result[assetKey] = dependency;
            collectDependencies(dependency, result, visited);
        }
    }

    private void onPreviewStateChanged(object? sender, EventArgs args)
    {
        Dispatcher.UIThread.Post(updatePreviewState);
    }

    private void updatePreviewState()
    {
        PreviewStateText.Text = previewClient.State switch
        {
            UiPreviewClientState.Ready => LocaleService.Get("UI_PREVIEW_READY"),
            UiPreviewClientState.Rendering => LocaleService.Get("UI_PREVIEW_RENDERING"),
            UiPreviewClientState.Starting => LocaleService.Get("UI_PREVIEW_STARTING"),
            UiPreviewClientState.Faulted => LocaleService.Get("UI_PREVIEW_FAILED"),
            _ => LocaleService.Get("UI_PREVIEW_UNAVAILABLE"),
        };
        if (previewClient.State is UiPreviewClientState.Unavailable
            or UiPreviewClientState.Faulted)
        {
            string message = previewClient.StatusMessage.Length == 0
                ? LocaleService.Get("UI_PREVIEW_HOST_REQUIRED")
                : previewClient.StatusMessage;
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
        if (lockedNodeNames.Contains(args.NodeName)
            || !tryGetDesignerCanvasSlot(
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
    }

    private void onPreviewTransformCancelled(object? sender, EventArgs args)
    {
        if (transformStartSlot is null)
            return;
        transformStartSlot = null;
        document.CancelGesture();
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
        if (selectedNodeName is null
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
        PointerPoint point = args.GetCurrentPoint(HierarchyTree);
        UiHierarchyItem? item = getHierarchyItem(args.Source);
        if (!point.Properties.IsLeftButtonPressed
            || item is null
            || document.FindParent(item.NodeName) is null)
        {
            return;
        }
        hierarchyDragPress = args;
        hierarchyDragStart = point.Position;
        hierarchyDragNodeName = item.NodeName;
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
            || hierarchyDragNodeName is null)
        {
            return;
        }
        PointerPoint point = args.GetCurrentPoint(HierarchyTree);
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
        data.Add(DataTransferItem.CreateText(DragPrefix + hierarchyDragNodeName));
        await DragDrop.DoDragDropAsync(
            hierarchyDragPress,
            data,
            DragDropEffects.Move);
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
        string? nodeName = getDraggedNodeName(args);
        UiHierarchyItem? target = getHierarchyItem(args.Source);
        args.DragEffects = nodeName is not null
            && target is not null
            && canDropNode(nodeName, target, args)
                ? DragDropEffects.Move
                : DragDropEffects.None;
        args.Handled = true;
    }

    private void onHierarchyDrop(object? sender, DragEventArgs args)
    {
        string? nodeName = getDraggedNodeName(args);
        UiHierarchyItem? target = getHierarchyItem(args.Source);
        if (nodeName is null || target is null)
            return;
        if (!tryGetDropLocation(
                nodeName,
                target,
                args,
                out string parentName,
                out int index))
        {
            return;
        }
        JsonObject? destination = document.FindNode(parentName);
        JsonObject? sourceParent = document.FindParent(nodeName);
        JsonObject? slot = destination is not null
            && sourceParent is not null
            && !string.Equals(
                getString(destination, "name"),
                getString(sourceParent, "name"),
                StringComparison.Ordinal)
                ? createSlot(destination)
                : null;
        if (document.MoveNode(nodeName, parentName, index, slot))
            selectedNodeName = nodeName;
        args.Handled = true;
    }

    private bool canDropNode(
        string nodeName,
        UiHierarchyItem target,
        DragEventArgs args)
    {
        return tryGetDropLocation(nodeName, target, args, out _, out _);
    }

    private bool tryGetDropLocation(
        string nodeName,
        UiHierarchyItem target,
        DragEventArgs args,
        out string parentName,
        out int index)
    {
        parentName = string.Empty;
        index = 0;
        JsonObject? node = document.FindNode(nodeName);
        JsonObject? targetNode = document.FindNode(target.NodeName);
        if (node is null
            || targetNode is null
            || lockedNodeNames.Contains(nodeName)
            || string.Equals(nodeName, target.NodeName, StringComparison.Ordinal)
            || isDescendant(node, target.NodeName))
        {
            return false;
        }
        TreeViewItem? container = getHierarchyContainer(args.Source);
        double relativeY = container is null
            ? 0.5
            : args.GetPosition(container).Y / Math.Max(1, container.Bounds.Height);
        if (relativeY >= 0.25
            && relativeY <= 0.75
            && canAcceptChild(targetNode, nodeName))
        {
            parentName = target.NodeName;
            index = targetNode["children"] is JsonArray targetChildren
                ? targetChildren.Count
                : 0;
            return normalizeHierarchyDropIndex(
                nodeName,
                parentName,
                ref index);
        }
        if (!tryGetNodeLocation(
                target.NodeName,
                out JsonObject? targetParent,
                out _,
                out int targetIndex)
            || targetParent is null
            || !canAcceptChild(targetParent, nodeName))
        {
            return false;
        }
        parentName = getString(targetParent, "name");
        index = targetIndex + (relativeY > 0.75 ? 1 : 0);
        return normalizeHierarchyDropIndex(
            nodeName,
            parentName,
            ref index);
    }

    private bool normalizeHierarchyDropIndex(
        string nodeName,
        string parentName,
        ref int index)
    {
        if (!tryGetNodeLocation(
                nodeName,
                out JsonObject? sourceParent,
                out _,
                out int sourceIndex)
            || sourceParent is null
            || !string.Equals(
                getString(sourceParent, "name"),
                parentName,
                StringComparison.Ordinal))
        {
            return true;
        }
        if (sourceIndex < index)
            index--;
        return sourceIndex != index;
    }

    private static bool isDescendant(JsonObject node, string nodeName)
    {
        if (node["children"] is not JsonArray children)
            return false;
        foreach (JsonObject child in children.OfType<JsonObject>())
        {
            if (string.Equals(getString(child, "name"), nodeName, StringComparison.Ordinal)
                || isDescendant(child, nodeName))
            {
                return true;
            }
        }
        return false;
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

    private static string? getDraggedNodeName(DragEventArgs args)
    {
        string? text = args.DataTransfer.TryGetText();
        return text is not null && text.StartsWith(DragPrefix, StringComparison.Ordinal)
            ? text[DragPrefix.Length..]
            : null;
    }

    private void clearHierarchyDrag()
    {
        hierarchyDragPress = null;
        hierarchyDragStart = null;
        hierarchyDragNodeName = null;
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
