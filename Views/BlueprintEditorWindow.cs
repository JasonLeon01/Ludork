using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Templates;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using Avalonia.Threading;
using Avalonia.VisualTree;
using Ludork.Controls;
using Ludork.Models;
using Ludork.Services;
using Ludork.Views.Utils;
using Ludork.Views.Utils.BlueprintGraph;
using MoonSharp.Interpreter;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;
using System.Threading.Tasks;

namespace Ludork.Views;

public sealed class BlueprintEditorWindow : Window
{
    private readonly BlueprintEditorDocument document;
    private readonly GameDataService gameData;
    private readonly ProjectSaveService projectSave;
    private readonly LuaMetadataService metadataService;
    private readonly BlueprintClassResolver classResolver;
    private readonly BlueprintPreviewService previewService;
    private readonly BlueprintValidationService validationService;
    private readonly BlueprintVariableFieldBuilder fieldBuilder;
    private readonly BlueprintNodeParameterEditorFactory nodeParameterEditorFactory;
    private readonly BlueprintNodeDefinitionCatalog nodeDefinitionCatalog;
    private readonly BlueprintVariableForm variableForm;
    private readonly TextBox parentField;
    private readonly ListBox graphList;
    private readonly Grid contentHost;
    private readonly Grid previewPanel;
    private readonly Image previewImage;
    private readonly TextBlock previewPlaceholder;
    private readonly Dictionary<string, Control> graphViews = new(StringComparer.Ordinal);
    private readonly Dictionary<string, (Button Button, JsonNode? ParentValue)> revertActions = new(StringComparer.Ordinal);
    private readonly Toast toast;
    private ActorPreviewLease? previewLease;
    private Bitmap? previewBitmap;
    private ResolvedBlueprintClass? resolvedParent;
    private ResolvedBlueprintClass? resolvedClass;
    private bool refreshing;

    public BlueprintEditorWindow(
        BlueprintEditorDocument document,
        GameDataService gameData,
        ProjectSaveService projectSave,
        LuaMetadataService metadataService,
        BlueprintClassResolver classResolver,
        BlueprintPreviewService previewService)
    {
        this.document = document;
        this.gameData = gameData;
        this.projectSave = projectSave;
        this.metadataService = metadataService;
        this.classResolver = classResolver;
        this.previewService = previewService;
        validationService = new BlueprintValidationService(gameData, metadataService, classResolver);
        fieldBuilder = new BlueprintVariableFieldBuilder(gameData, metadataService);
        nodeParameterEditorFactory = new BlueprintNodeParameterEditorFactory(
            gameData,
            metadataService,
            classResolver);
        nodeDefinitionCatalog = new BlueprintNodeDefinitionCatalog(
            metadataService,
            classResolver,
            document);

        Title = document.Title;
        Width = 1200;
        Height = 600;
        MaxHeight = 600;
        MinWidth = 700;
        MinHeight = 420;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        Background = new SolidColorBrush(Color.Parse("#1e1e1e"));
        FontFamily = FontFamily.Parse("avares://Ludork/Assets/HarmonyOS_Sans_SC_Regular.ttf#HarmonyOS Sans SC");
        EditorWindowIcon.Apply(this);

        parentField = EditorInputs.CreateReadOnlyTextBox();
        variableForm = new BlueprintVariableForm
        {
            AssetsDirectory = Path.Combine(gameData.ProjectPath, "Assets"),
            ProjectDirectory = gameData.ProjectPath,
            CellSize = gameData.getCellSize(),
            IsReadOnly = !document.CanEditAttributes,
            FieldActionFactory = createAttributeAction,
            CanRemoveComponent = field => document.Data["attrs"] is JsonObject attrs
                && attrs.ContainsKey(field.Name),
        };
        variableForm.ValueChanged += onVariableChanged;
        variableForm.ComponentAddRequested += onComponentAddRequested;
        variableForm.ComponentRemoveRequested += onComponentRemoveRequested;

        WidthConstrainedScrollViewer leftScroll = new()
        {
            MinWidth = 320,
            HorizontalAlignment = HorizontalAlignment.Stretch,
            VerticalAlignment = VerticalAlignment.Stretch,
            HorizontalContentAlignment = HorizontalAlignment.Stretch,
            HorizontalScrollBarVisibility = Avalonia.Controls.Primitives.ScrollBarVisibility.Auto,
            VerticalScrollBarVisibility = Avalonia.Controls.Primitives.ScrollBarVisibility.Auto,
            Content = createAttributePanel(),
        };

        graphList = new ListBox
        {
            Height = 50,
            Background = new SolidColorBrush(Color.Parse("#282828")),
            ClipToBounds = true,
            HorizontalAlignment = HorizontalAlignment.Stretch,
            VerticalAlignment = VerticalAlignment.Stretch,
            SelectionMode = SelectionMode.Single,
            ItemsPanel = new FuncTemplate<Panel?>(() => new StackPanel
            {
                Orientation = Orientation.Horizontal,
            }),
        };
        graphList.SetValue(
            ScrollViewer.HorizontalScrollBarVisibilityProperty,
            Avalonia.Controls.Primitives.ScrollBarVisibility.Auto);
        graphList.SetValue(
            ScrollViewer.VerticalScrollBarVisibilityProperty,
            Avalonia.Controls.Primitives.ScrollBarVisibility.Disabled);
        graphList.SelectionChanged += onGraphSelectionChanged;
        graphList.AddHandler(PointerPressedEvent, onGraphListPointerPressed, RoutingStrategies.Bubble);

        Grid tabBar = new()
        {
            Height = 50,
            ColumnDefinitions = new ColumnDefinitions("*,Auto"),
        };
        Grid.SetColumn(graphList, 0);
        tabBar.Children.Add(graphList);
        if (document.Kind == BlueprintEditorDocumentKind.Blueprint)
        {
            Button validateButton = new()
            {
                Content = LocaleService.Get("VALIDATE_BLUEPRINT"),
                Height = 50,
                Padding = new Thickness(16, 0),
            };
            validateButton.Click += onValidateBlueprint;
            Grid.SetColumn(validateButton, 1);
            tabBar.Children.Add(validateButton);
        }

        previewImage = new Image
        {
            Stretch = Stretch.Uniform,
            HorizontalAlignment = HorizontalAlignment.Center,
            VerticalAlignment = VerticalAlignment.Center,
            Margin = new Thickness(24),
        };
        previewPlaceholder = new TextBlock
        {
            Text = LocaleService.Get("PREVIEW"),
            Foreground = new SolidColorBrush(Color.Parse("#777777")),
            HorizontalAlignment = HorizontalAlignment.Center,
            VerticalAlignment = VerticalAlignment.Center,
        };
        previewPanel = new Grid
        {
            Background = new SolidColorBrush(Color.Parse("#1c1c1c")),
            Children =
            {
                previewImage,
                previewPlaceholder,
            },
        };
        contentHost = new Grid
        {
            Children =
            {
                previewPanel,
            },
        };
        previewPanel.IsVisible = false;

        Grid rightPanel = new()
        {
            RowDefinitions = new RowDefinitions("50,*"),
        };
        rightPanel.Children.Add(tabBar);
        Grid.SetRow(contentHost, 1);
        rightPanel.Children.Add(contentHost);

        GridSplitter splitter = new()
        {
            Width = 4,
            Background = new SolidColorBrush(Color.Parse("#323232")),
            VerticalAlignment = VerticalAlignment.Stretch,
            ResizeDirection = GridResizeDirection.Columns,
        };
        Grid root = new()
        {
            ColumnDefinitions = new ColumnDefinitions("5*,4,6*"),
        };
        root.Children.Add(leftScroll);
        Grid.SetColumn(splitter, 1);
        root.Children.Add(splitter);
        Grid.SetColumn(rightPanel, 2);
        root.Children.Add(rightPanel);
        Content = root;

        toast = new Toast(this);
        gameData.DataRestored += onDataRestored;
        gameData.DataReloaded += onDataReloaded;
        Closed += onClosed;
        Deactivated += (_, _) => flushGraphViews();
        Opened += (_, _) => showSelectedContent();
        AddHandler(KeyDownEvent, onKeyDown, RoutingStrategies.Tunnel);
        refreshAll();
    }

    public event EventHandler<BlueprintGraphRequestedEventArgs>? GraphRequested;
    public event EventHandler<BlueprintGraphOrganizeRequestedEventArgs>? GraphOrganizeRequested;

    public BlueprintEditorDocument Document => document;

    public bool Reload()
    {
        return reload(false);
    }

    public void FlushPendingChanges()
    {
        flushGraphViews();
    }

    public bool RekeyBlueprint(string key)
    {
        if (!document.RekeyBlueprint(key))
            return false;
        FlushPendingChanges();
        Title = document.Title;
        refreshAll();
        return true;
    }

    private async void onValidateBlueprint(object? sender, RoutedEventArgs args)
    {
        FlushPendingChanges();
        if (document.BlueprintKey is not string key)
            return;
        BlueprintValidationResult result = validationService.ValidateBlueprint(key);
        if (result.IsValid)
        {
            toast.ShowMessage(LocaleService.Get("BLUEPRINT_VALIDATION_SUCCESS"));
            return;
        }
        await BlueprintValidationDialog.ShowResultsAsync(this, [result]);
    }

    private bool reload(bool discardPendingChanges)
    {
        clearGraphViews(discardPendingChanges);
        if (!document.Reload())
        {
            Close();
            return false;
        }
        refreshAll();
        return true;
    }

    public void SetGraphContent(string eventName, Control content)
    {
        if (graphViews.TryGetValue(eventName, out Control? previous))
        {
            contentHost.Children.Remove(previous);
            if (previous is IDisposable disposable)
                disposable.Dispose();
        }
        graphViews[eventName] = content;
        content.IsVisible = false;
        contentHost.Children.Add(content);
        if (graphList.SelectedItem is BlueprintEditorTabItem selected
            && !selected.IsPreview
            && string.Equals(selected.EventName, eventName, StringComparison.Ordinal))
        {
            showSelectedContent();
        }
    }

    private Control createAttributePanel()
    {
        Grid parentRow = new()
        {
            ColumnDefinitions = new ColumnDefinitions("Auto,8,*,4,24"),
        };
        TextBlock parentLabel = new()
        {
            Text = LocaleService.Get("PARENT"),
            VerticalAlignment = VerticalAlignment.Center,
        };
        parentRow.Children.Add(parentLabel);
        Grid.SetColumn(parentField, 2);
        parentRow.Children.Add(parentField);
        Button parentPicker = new()
        {
            Content = "...",
            Width = 24,
            Height = 34,
            Padding = new Thickness(0),
            IsVisible = document.CanEditAttributes,
            IsEnabled = document.CanEditAttributes,
        };
        parentPicker.Click += async (_, _) => await selectParentAsync();
        Grid.SetColumn(parentPicker, 4);
        parentRow.Children.Add(parentPicker);

        StackPanel panel = new()
        {
            MinWidth = 320,
            Margin = new Thickness(8),
            Spacing = 8,
            Children =
            {
                parentRow,
                new Border
                {
                    Height = 3,
                    Background = new SolidColorBrush(Color.Parse("#464646")),
                },
                variableForm,
            },
        };
        if (document.CanEditAttributes)
        {
            Button addAttribute = new()
            {
                Content = "+",
                Height = 34,
                HorizontalAlignment = HorizontalAlignment.Stretch,
            };
            addAttribute.Click += async (_, _) => await addAttributeAsync();
            panel.Children.Add(addAttribute);
        }
        return panel;
    }

    private async Task selectParentAsync()
    {
        string current = document.Data["parent"]?.GetValue<string>() ?? string.Empty;
        string? selected = await BlueprintClassSelector.ShowAsync(
            this,
            gameData,
            metadataService,
            classResolver,
            current,
            document.BlueprintKey,
            BlueprintClassSelectorMode.Parent);
        if (string.IsNullOrWhiteSpace(selected)
            || string.Equals(selected, current, StringComparison.Ordinal))
        {
            return;
        }
        JsonObject prospective = (JsonObject)document.Data.DeepClone();
        prospective["parent"] = selected;
        ResolvedBlueprintClass prospectiveClass = classResolver.ResolveBlueprint(
            prospective,
            document.BlueprintKey);
        if (prospectiveClass.HasBlueprintParent
            && prospective["attrs"] is JsonObject attrs
            && tryGetBoolean(attrs["scriptMixin"], out bool localMode)
            && localMode != prospectiveClass.ParentScriptMixin)
        {
            await AlertDialog.ShowAsync(
                this,
                LocaleService.Get("ERROR"),
                LocaleService.Get("SCRIPT_MIXIN_INHERITANCE_CONFLICT"));
            return;
        }
        flushGraphViews();
        clearGraphViews();
        if (document.CommitParent(selected))
            refreshAll();
    }

    private async Task commitScriptPathAsync(JsonNode? value)
    {
        string candidate = getString(value);
        string normalized;
        try
        {
            normalized = ScriptMixinPaths.Normalize(candidate);
            string fullPath = ScriptMixinPaths.GetScriptPath(gameData.ProjectPath, normalized);
            if (string.IsNullOrEmpty(normalized) || !File.Exists(fullPath))
                throw new FileNotFoundException($"Mixin script '{normalized}' was not found", fullPath);
            metadataService.LoadScriptMixinMetadata(normalized);
        }
        catch (InterpreterException exception)
        {
            await showScriptMixinErrorAsync(exception.DecoratedMessage ?? exception.Message);
            refreshAttributes();
            return;
        }
        catch (InvalidDataException exception)
        {
            await showScriptMixinErrorAsync(exception.Message);
            refreshAttributes();
            return;
        }
        catch (IOException exception)
        {
            await showScriptMixinErrorAsync(exception.Message);
            refreshAttributes();
            return;
        }
        catch (UnauthorizedAccessException exception)
        {
            await showScriptMixinErrorAsync(exception.Message);
            refreshAttributes();
            return;
        }

        ResolvedBlueprintClass previous = resolvedClass
            ?? classResolver.ResolveBlueprint(document.Data, document.BlueprintKey);
        JsonObject prospective = (JsonObject)document.Data.DeepClone();
        JsonObject prospectiveAttrs = prospective["attrs"] as JsonObject ?? [];
        prospective["attrs"] = prospectiveAttrs;
        prospectiveAttrs["scriptPath"] = normalized;
        ResolvedBlueprintClass next = classResolver.ResolveBlueprint(
            prospective,
            document.BlueprintKey);
        HashSet<string> nextSchema = new(next.DeclaredFieldNames, StringComparer.Ordinal);
        JsonObject localAttrs = document.Data["attrs"] as JsonObject ?? [];
        List<string> staleFields = previous.LocalMixinFieldNames
            .Where(name => localAttrs.ContainsKey(name) && !nextSchema.Contains(name))
            .Distinct(StringComparer.Ordinal)
            .ToList();
        if (staleFields.Count != 0)
        {
            string message = LocaleService.Get("SCRIPT_MIXIN_REMOVE_FIELDS")
                .Replace("{fields}", string.Join("\n", staleFields), StringComparison.Ordinal);
            bool confirmed = await ConfirmationDialog.ShowAsync(
                this,
                LocaleService.Get("SCRIPT_MIXIN_TITLE"),
                message);
            if (!confirmed)
            {
                refreshAttributes();
                return;
            }
        }

        flushGraphViews();
        Dictionary<string, JsonNode?> updates = new(StringComparer.Ordinal)
        {
            ["scriptPath"] = JsonValue.Create(normalized),
        };
        if (document.CommitAttributes(updates, staleFields))
        {
            refreshAttributes();
            refreshPreview();
        }
    }

    private Task showScriptMixinErrorAsync(string error)
    {
        string message = LocaleService.Get("SCRIPT_MIXIN_METADATA_INVALID")
            .Replace("{error}", error, StringComparison.Ordinal);
        return AlertDialog.ShowAsync(this, LocaleService.Get("ERROR"), message);
    }

    private async Task addAttributeAsync()
    {
        ResolvedBlueprintClass resolved = classResolver.ResolveBlueprint(
            document.Data,
            document.BlueprintKey);
        resolvedClass = resolved;
        string? name = await SingleRowDialog.ShowAsync(
            this,
            LocaleService.Get("ADD_ATTR"),
            LocaleService.Get("ATTR_NAME"),
            resolved.Fields.Select(field => field.Name));
        if (string.IsNullOrWhiteSpace(name))
            return;
        string attributeName = name.Trim();
        if (char.IsDigit(attributeName[0]))
        {
            await AlertDialog.ShowAsync(
                this,
                LocaleService.Get("ERROR"),
                LocaleService.Get("ATTR_NAME_CANNOT_START_WITH_DIGIT"));
            return;
        }
        if (resolved.InvalidVars.Contains(attributeName, StringComparer.Ordinal))
        {
            await AlertDialog.ShowAsync(
                this,
                LocaleService.Get("ERROR"),
                LocaleService.Get("INVALID_NAME"));
            return;
        }
        if (document.CommitAttribute(attributeName, JsonValue.Create(string.Empty)))
            refreshAttributes();
    }

    private Control? createAttributeAction(BlueprintVariableField field)
    {
        if (!document.CanEditAttributes || field.IsComponent || field.IsReadOnly)
            return null;
        if (field.Name == "scriptPath" && resolvedClass?.HasBlueprintParent != true)
            return null;
        bool hasLocalValue = document.Data["attrs"] is JsonObject attrs && attrs.ContainsKey(field.Name);
        ResolvedBlueprintField? parentField = resolvedParent?.GetField(field.Name);
        if (parentField is not null)
        {
            Button revert = new()
            {
                Content = "↶",
                Width = 20,
                Height = 20,
                Padding = new Thickness(0),
                Background = Brushes.Transparent,
                BorderThickness = new Thickness(0),
                VerticalAlignment = VerticalAlignment.Center,
                IsEnabled = hasLocalValue && !JsonNode.DeepEquals(field.Value, parentField.Value),
            };
            JsonNode? parentValue = parentField.Value?.DeepClone();
            revertActions[field.Name] = (revert, parentValue?.DeepClone());
            revert.Click += async (_, _) =>
            {
                if (field.Name == "scriptPath")
                {
                    await removeLocalScriptPathAsync();
                    return;
                }
                if (!document.CommitAttribute(field.Name, parentValue))
                    return;
                refreshAttributes();
                refreshPreview();
            };
            return revert;
        }
        Button remove = new()
        {
            Content = "-",
            Width = 24,
            Height = 34,
            Padding = new Thickness(0),
            IsEnabled = hasLocalValue,
        };
        remove.Click += (_, _) =>
        {
            if (!document.RemoveAttribute(field.Name))
                return;
            refreshAttributes();
            refreshPreview();
        };
        return remove;
    }

    private async Task removeLocalScriptPathAsync()
    {
        if (document.Data["attrs"] is not JsonObject localAttrs
            || !localAttrs.ContainsKey("scriptPath"))
        {
            return;
        }
        ResolvedBlueprintClass previous = resolvedClass
            ?? classResolver.ResolveBlueprint(document.Data, document.BlueprintKey);
        JsonObject prospective = (JsonObject)document.Data.DeepClone();
        if (prospective["attrs"] is JsonObject prospectiveAttrs)
            prospectiveAttrs.Remove("scriptPath");
        ResolvedBlueprintClass next = classResolver.ResolveBlueprint(
            prospective,
            document.BlueprintKey);
        HashSet<string> nextSchema = new(next.DeclaredFieldNames, StringComparer.Ordinal);
        List<string> staleFields = previous.LocalMixinFieldNames
            .Where(name => localAttrs.ContainsKey(name) && !nextSchema.Contains(name))
            .Distinct(StringComparer.Ordinal)
            .ToList();
        if (staleFields.Count != 0)
        {
            string message = LocaleService.Get("SCRIPT_MIXIN_REMOVE_FIELDS")
                .Replace("{fields}", string.Join("\n", staleFields), StringComparison.Ordinal);
            bool confirmed = await ConfirmationDialog.ShowAsync(
                this,
                LocaleService.Get("SCRIPT_MIXIN_TITLE"),
                message);
            if (!confirmed)
                return;
        }
        List<string> removals = ["scriptPath", .. staleFields];
        if (document.CommitAttributes(
            new Dictionary<string, JsonNode?>(StringComparer.Ordinal),
            removals))
        {
            refreshAttributes();
            refreshPreview();
        }
    }

    private async void onComponentAddRequested(
        object? sender,
        BlueprintComponentFieldsEventArgs args)
    {
        Dictionary<string, BlueprintVariableField> choices = [];
        foreach (BlueprintVariableField field in args.Fields)
            choices[$"{EditorDisplayName.Format(field.Name)} ({field.Type})"] = field;
        if (choices.Count == 0)
            return;
        string? selected = await ItemSelectorDialog.ShowAsync(
            this,
            LocaleService.Get("ADD_COMPONENT"),
            LocaleService.Get("ADD_COMPONENT"),
            choices.Keys);
        if (selected is null
            || !choices.TryGetValue(selected, out BlueprintVariableField? selectedField))
        {
            return;
        }
        if (document.CommitAttribute(
            selectedField.Name,
            materializeComponent(selectedField)))
        {
            refreshAttributes();
        }
    }

    private void onComponentRemoveRequested(
        object? sender,
        BlueprintComponentFieldEventArgs args)
    {
        if (document.RemoveAttribute(args.Field.Name))
            refreshAttributes();
    }

    private static JsonNode materializeComponent(BlueprintVariableField field)
    {
        if (field.Value is not null)
            return field.Value.DeepClone();
        if (field.DefaultValue is not null)
            return field.DefaultValue.DeepClone();
        JsonObject result = [];
        foreach (BlueprintVariableField child in field.Fields)
        {
            if (child.Fields.Count > 0)
                result[child.Name] = materializeComponent(child);
            else
                result[child.Name] = child.Value?.DeepClone()
                    ?? child.DefaultValue?.DeepClone()
                    ?? JsonValue.Create(string.Empty);
        }
        return result;
    }

    private void refreshAll()
    {
        refreshing = true;
        parentField.Text = document.Data["parent"]?.GetValue<string>() ?? string.Empty;
        ResolvedBlueprintClass resolved = classResolver.ResolveBlueprint(
            document.Data,
            document.BlueprintKey);
        resolvedClass = resolved;
        resolvedParent = resolveParentClass();
        revertActions.Clear();
        variableForm.SetFields(fieldBuilder.Build(resolved));
        updateGraphMode();
        refreshing = false;
        refreshPreview();
        refreshGraphList(null, false);
    }

    private void refreshAttributes()
    {
        refreshing = true;
        parentField.Text = document.Data["parent"]?.GetValue<string>() ?? string.Empty;
        ResolvedBlueprintClass resolved = classResolver.ResolveBlueprint(
            document.Data,
            document.BlueprintKey);
        resolvedClass = resolved;
        resolvedParent = resolveParentClass();
        revertActions.Clear();
        variableForm.SetFields(fieldBuilder.Build(resolved));
        updateGraphMode();
        refreshing = false;
    }

    private ResolvedBlueprintClass? resolveParentClass()
    {
        string parent = document.Data["parent"]?.GetValue<string>() ?? string.Empty;
        return parent.Length == 0 ? null : classResolver.Resolve(parent);
    }

    private void refreshPreview()
    {
        ActorVisualDescriptor? descriptor = previewService.tryResolveActorVisual(
            document.Data,
            document.BlueprintKey);
        if (descriptor is not { RequiresPreviewService: true })
        {
            releasePreviewLease();
            replacePreviewFallback(previewService.tryLoadPreview(
                document.Data,
                480,
                document.BlueprintKey));
            return;
        }

        if (previewLease is null)
        {
            previewLease = previewService.ActorPreviews.Acquire(
                descriptor,
                480,
                previewPanel.IsVisible);
            previewLease.FrameChanged += onPreviewFrameChanged;
        }
        else
        {
            previewLease.UpdateDescriptor(descriptor);
        }
        previewLease.IsActive = previewPanel.IsVisible;
        replacePreviewFallback(previewService.tryLoadPreview(
            document.Data,
            480,
            document.BlueprintKey));
    }

    private void replacePreviewFallback(Bitmap? next)
    {
        Bitmap? previous = previewBitmap;
        previewBitmap = next;
        updatePreviewSource();
        previous?.Dispose();
    }

    private void onPreviewFrameChanged(object? sender, EventArgs args)
    {
        if (Dispatcher.UIThread.CheckAccess())
        {
            updatePreviewSource();
            return;
        }
        Dispatcher.UIThread.Post(updatePreviewSource);
    }

    private void updatePreviewSource()
    {
        Bitmap? frame = previewLease?.Frame;
        previewImage.Source = frame ?? previewBitmap;
        previewPlaceholder.IsVisible = previewImage.Source is null;
        previewImage.InvalidateVisual();
    }

    private void releasePreviewLease()
    {
        if (previewLease is null)
            return;
        previewLease.FrameChanged -= onPreviewFrameChanged;
        previewLease.Dispose();
        previewLease = null;
    }

    private bool supportsPreview()
    {
        string parent = document.Data["parent"]?.GetValue<string>() ?? string.Empty;
        return parent.Length != 0 && classResolver.IsDerivedFrom(
            parent,
            "Engine.Actor");
    }

    private void refreshGraphList(string? preferredEvent, bool preferPreview)
    {
        BlueprintEditorTabItem? current = graphList.SelectedItem as BlueprintEditorTabItem;
        string? selectedEvent = preferredEvent ?? current?.EventName;
        bool selectedPreview = preferPreview || current?.IsPreview == true;
        refreshing = true;
        graphList.Items.Clear();
        if (supportsPreview())
            graphList.Items.Add(new BlueprintEditorTabItem(LocaleService.Get("PREVIEW"), null, true));
        foreach (string graphName in getAvailableGraphNames())
        {
            graphList.Items.Add(new BlueprintEditorTabItem(
                EditorDisplayName.Format(graphName),
                graphName,
                false));
        }

        BlueprintEditorTabItem? selection = null;
        if (selectedPreview)
        {
            selection = graphList.Items
                .OfType<BlueprintEditorTabItem>()
                .FirstOrDefault(item => item.IsPreview);
        }
        if (selection is null && selectedEvent is not null)
        {
            selection = graphList.Items
                .OfType<BlueprintEditorTabItem>()
                .FirstOrDefault(item => string.Equals(item.EventName, selectedEvent, StringComparison.Ordinal));
        }
        selection ??= graphList.Items.OfType<BlueprintEditorTabItem>().FirstOrDefault();
        graphList.SelectedItem = selection;
        refreshing = false;
        showSelectedContent();
    }

    private void showSelectedContent()
    {
        previewPanel.IsVisible = false;
        if (previewLease is not null)
            previewLease.IsActive = false;
        foreach (Control graphView in graphViews.Values)
            graphView.IsVisible = false;
        if (graphList.SelectedItem is not BlueprintEditorTabItem selected)
            return;
        if (selected.IsPreview)
        {
            previewPanel.IsVisible = true;
            refreshPreview();
            return;
        }
        string eventName = selected.EventName ?? string.Empty;
        if (graphViews.TryGetValue(eventName, out Control? existing))
        {
            existing.IsVisible = true;
            return;
        }
        JsonObject eventGraph = document.GetEventGraph(eventName);
        BlueprintGraphRequestedEventArgs request = new(document, eventName, eventGraph);
        GraphRequested?.Invoke(this, request);
        Control content = request.Content ?? createGraphControl(eventName, eventGraph);
        graphViews[eventName] = content;
        contentHost.Children.Add(content);
    }

    private BlueprintGraphControl createGraphControl(string eventName, JsonObject eventGraph)
    {
        JsonObject graph = document.Data["graph"] as JsonObject ?? [];
        if (document.Data["graph"] is not JsonObject)
            document.Data["graph"] = graph;
        JsonObject startNodes = graph["startNodes"] as JsonObject ?? [];
        if (graph["startNodes"] is not JsonObject)
            graph["startNodes"] = startNodes;
        IReadOnlyList<BlueprintGraphNodeDefinition> definitions = nodeDefinitionCatalog.GetNodeDefinitions();
        IReadOnlyList<BlueprintGraphEventParameterDefinition> eventParameters =
            nodeDefinitionCatalog.GetEventParameters(eventName);
        BlueprintGraphDocument graphDocument = BlueprintGraphCodec.Load(
            eventName,
            eventGraph,
            startNodes[eventName],
            definitions,
            eventParameters);
        BlueprintGraphControl control = new(
            graphDocument,
            definitions,
            fieldBuilder,
            nodeParameterEditorFactory,
            Path.Combine(gameData.ProjectPath, "Assets"),
            gameData.getCellSize(),
            isGraphReadOnly());
        control.GraphChanged += (_, _) =>
        {
            BlueprintGraphCodec.SaveInto(control.Document, eventGraph, startNodes);
            document.CommitGraph();
        };
        return control;
    }

    private IReadOnlyList<string> getAvailableGraphNames()
    {
        List<string> result = document.GetGraphNames().ToList();
        if (document.Kind != BlueprintEditorDocumentKind.Blueprint)
            return result;
        ResolvedBlueprintClass resolved = classResolver.ResolveBlueprint(
            document.Data,
            document.BlueprintKey);
        if (resolved.RootType is null)
            return result;
        foreach (LuaNodeMemberMetadata member in metadataService.GetNodeMembers(
            resolved.RootType,
            LuaNodeMemberKind.Event))
        {
            if (!result.Contains(member.Name, StringComparer.Ordinal))
                result.Add(member.Name);
        }
        return result;
    }

    private async void onVariableChanged(object? sender, BlueprintVariableValueChangedEventArgs args)
    {
        if (refreshing)
            return;
        if (args.Name == "scriptPath")
        {
            await commitScriptPathAsync(args.Value);
            return;
        }
        if (args.Name == "scriptMixin")
            flushGraphViews();
        if (!document.CommitAttribute(args.Name, args.Value))
            return;
        if (revertActions.TryGetValue(
            args.Name,
            out (Button Button, JsonNode? ParentValue) action))
        {
            action.Button.IsEnabled = !JsonNode.DeepEquals(args.Value, action.ParentValue);
        }
        refreshPreview();
        if (args.RequiresRefresh || args.Name == "scriptMixin")
        {
            clearGraphViews();
            Dispatcher.UIThread.Post(refreshAll);
        }
    }

    private void onGraphSelectionChanged(object? sender, SelectionChangedEventArgs args)
    {
        if (!refreshing)
            showSelectedContent();
    }

    private void onGraphListPointerPressed(object? sender, PointerPressedEventArgs args)
    {
        if (!args.GetCurrentPoint(graphList).Properties.IsRightButtonPressed)
            return;
        BlueprintEditorTabItem? hitItem = null;
        if (args.Source is Visual source)
        {
            Visual? current = source;
            while (current is not null)
            {
                if (current is ListBoxItem listBoxItem
                    && listBoxItem.DataContext is BlueprintEditorTabItem item)
                {
                    hitItem = item;
                    break;
                }
                current = current.GetVisualParent();
            }
        }
        if (hitItem is not null)
            graphList.SelectedItem = hitItem;
        args.Handled = true;
        showGraphContextMenu(hitItem);
    }

    private void showGraphContextMenu(BlueprintEditorTabItem? item)
    {
        ContextMenu menu = new();
        MenuItem newEvent = new()
        {
            Header = LocaleService.Get("NEW_EVENT"),
            IsEnabled = !isGraphReadOnly(),
        };
        newEvent.Click += async (_, _) => await addEventAsync();
        menu.Items.Add(newEvent);
        if (item is { IsPreview: false, EventName: not null })
        {
            MenuItem organize = new()
            {
                Header = LocaleService.Get("ORGANIZE_GRAPH"),
                IsEnabled = !isGraphReadOnly(),
            };
            ToolTip.SetTip(organize, LocaleService.Get("ORGANIZE_GRAPH_TIP"));
            organize.Click += (_, _) => organizeSelectedGraph();
            menu.Items.Add(organize);
            MenuItem rename = new()
            {
                Header = LocaleService.Get("RENAME_EVENT"),
                IsEnabled = !isGraphReadOnly(),
            };
            rename.Click += async (_, _) => await renameSelectedEventAsync();
            menu.Items.Add(rename);
            MenuItem delete = new()
            {
                Header = LocaleService.Get("DELETE_EVENT"),
                IsEnabled = !isGraphReadOnly(),
            };
            delete.Click += async (_, _) => await deleteSelectedEventAsync();
            menu.Items.Add(delete);
        }
        menu.Open(graphList);
    }

    private async Task addEventAsync()
    {
        if (isGraphReadOnly())
            return;
        string? name = await SingleRowDialog.ShowAsync(
            this,
            LocaleService.Get("NEW_EVENT"),
            LocaleService.Get("ENTER_EVENT_NAME"),
            getAvailableGraphNames());
        if (string.IsNullOrWhiteSpace(name))
            return;
        if (!document.AddEvent(name))
        {
            await AlertDialog.ShowAsync(this, LocaleService.Get("ERROR"), LocaleService.Get("INVALID_NAME"));
            return;
        }
        refreshGraphList(name.Trim(), false);
    }

    private async Task renameSelectedEventAsync()
    {
        if (isGraphReadOnly())
            return;
        if (graphList.SelectedItem is not BlueprintEditorTabItem { IsPreview: false, EventName: not null } selected)
            return;
        string? name = await SingleRowDialog.ShowAsync(
            this,
            LocaleService.Get("RENAME_EVENT"),
            LocaleService.Get("ENTER_EVENT_NAME"),
            getAvailableGraphNames().Where(value => !string.Equals(value, selected.EventName, StringComparison.Ordinal)),
            selected.EventName);
        if (string.IsNullOrWhiteSpace(name))
            return;
        flushGraphView(selected.EventName);
        if (!document.RenameEvent(selected.EventName, name))
        {
            await AlertDialog.ShowAsync(this, LocaleService.Get("ERROR"), LocaleService.Get("EVENT_EXISTS"));
            return;
        }
        removeGraphView(selected.EventName);
        refreshGraphList(name.Trim(), false);
    }

    private async Task deleteSelectedEventAsync()
    {
        if (isGraphReadOnly())
            return;
        if (graphList.SelectedItem is not BlueprintEditorTabItem { IsPreview: false, EventName: not null } selected)
            return;
        string message = LocaleService.Get("CONFIRM_DELETE_EVENT")
            .Replace("{name}", selected.EventName, StringComparison.Ordinal);
        bool confirmed = await ConfirmationDialog.ShowAsync(
            this,
            LocaleService.Get("DELETE_EVENT"),
            message);
        if (!confirmed)
            return;
        flushGraphView(selected.EventName);
        if (!document.DeleteEvent(selected.EventName))
            return;
        removeGraphView(selected.EventName);
        refreshGraphList(null, supportsPreview());
    }

    private void organizeSelectedGraph()
    {
        if (isGraphReadOnly())
            return;
        if (graphList.SelectedItem is not BlueprintEditorTabItem { IsPreview: false, EventName: not null } selected)
            return;
        if (!graphViews.TryGetValue(selected.EventName, out Control? content))
        {
            showSelectedContent();
            graphViews.TryGetValue(selected.EventName, out content);
        }
        if (content is BlueprintGraphControl graphControl)
            graphControl.OrganizeLayout();
        else
            GraphOrganizeRequested?.Invoke(this, new BlueprintGraphOrganizeRequestedEventArgs(selected.EventName));
    }

    private void onDataRestored(object? sender, EventArgs args)
    {
        reload(true);
    }

    private void onDataReloaded(object? sender, EventArgs args)
    {
        reload(true);
    }

    private async void onKeyDown(object? sender, KeyEventArgs args)
    {
        if (args.Key == Key.F2 && graphList.IsKeyboardFocusWithin)
        {
            if (isGraphReadOnly())
                return;
            await renameSelectedEventAsync();
            args.Handled = true;
            return;
        }
        if (args.Key == Key.Delete && graphList.IsKeyboardFocusWithin)
        {
            if (isGraphReadOnly())
                return;
            await deleteSelectedEventAsync();
            args.Handled = true;
            return;
        }
        if (!EditorShortcuts.HasPrimaryModifier(args.KeyModifiers))
            return;
        flushGraphViews();
        if (args.Key == Key.S)
            await EditorSaveWorkflow.TrySaveAsync(this, projectSave);
        else if (args.Key == Key.Z)
            EditorFeedback.ShowHistory(toast, "Undo", gameData.Undo());
        else if (args.Key == Key.Y)
            EditorFeedback.ShowHistory(toast, "Redo", gameData.Redo());
        else
            return;
        args.Handled = true;
    }

    private void onClosed(object? sender, EventArgs args)
    {
        gameData.DataRestored -= onDataRestored;
        gameData.DataReloaded -= onDataReloaded;
        clearGraphViews();
        releasePreviewLease();
        previewBitmap?.Dispose();
        previewBitmap = null;
    }

    private void removeGraphView(string eventName)
    {
        if (!graphViews.Remove(eventName, out Control? content))
            return;
        contentHost.Children.Remove(content);
        if (content is IDisposable disposable)
            disposable.Dispose();
    }

    private void clearGraphViews(bool discardPendingChanges = false)
    {
        foreach (Control content in graphViews.Values)
        {
            if (discardPendingChanges && content is BlueprintGraphControl graphControl)
                graphControl.DiscardPendingChanges();
            contentHost.Children.Remove(content);
            if (content is IDisposable disposable)
                disposable.Dispose();
        }
        graphViews.Clear();
    }

    private void flushGraphView(string eventName)
    {
        if (graphViews.TryGetValue(eventName, out Control? content)
            && content is BlueprintGraphControl graphControl)
        {
            graphControl.FlushPendingChanges();
        }
    }

    private void flushGraphViews()
    {
        foreach (Control content in graphViews.Values)
        {
            if (content is BlueprintGraphControl graphControl)
                graphControl.FlushPendingChanges();
        }
    }

    private bool isGraphReadOnly()
    {
        return document.Kind == BlueprintEditorDocumentKind.Blueprint
            && resolvedClass?.ScriptMixin == true;
    }

    private void updateGraphMode()
    {
        bool readOnly = isGraphReadOnly();
        ToolTip.SetTip(
            contentHost,
            readOnly ? LocaleService.Get("SCRIPT_MIXIN_GRAPH_CONFLICT") : null);
        foreach (Control content in graphViews.Values)
        {
            if (content is BlueprintGraphControl graphControl)
                graphControl.SetReadOnly(readOnly);
        }
    }

    private static string getString(JsonNode? value)
    {
        return value is JsonValue scalar && scalar.TryGetValue(out string? result)
            ? result
            : string.Empty;
    }

    private static bool tryGetBoolean(JsonNode? value, out bool result)
    {
        if (value is JsonValue scalar && scalar.TryGetValue(out result))
            return true;
        result = false;
        return false;
    }

    private sealed class BlueprintEditorTabItem
    {
        public BlueprintEditorTabItem(string label, string? eventName, bool isPreview)
        {
            Label = label;
            EventName = eventName;
            IsPreview = isPreview;
        }

        public string Label { get; }
        public string? EventName { get; }
        public bool IsPreview { get; }

        public override string ToString()
        {
            return Label;
        }
    }
}

public sealed class BlueprintGraphRequestedEventArgs(
    BlueprintEditorDocument document,
    string eventName,
    JsonObject eventGraph) : EventArgs
{
    public BlueprintEditorDocument Document { get; } = document;
    public string EventName { get; } = eventName;
    public JsonObject EventGraph { get; } = eventGraph;
    public Control? Content { get; set; }
}

public sealed class BlueprintGraphOrganizeRequestedEventArgs(string eventName) : EventArgs
{
    public string EventName { get; } = eventName;
}
