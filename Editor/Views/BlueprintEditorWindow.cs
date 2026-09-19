using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
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
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Views;

public sealed class BlueprintEditorWindow : Window, IProjectSaveParticipant
{
    private readonly BlueprintEditorDocument document;
    private readonly EditorDocumentBinding documentBinding;
    private readonly GameDataService gameData;
    private readonly ProjectSaveService projectSave;
    private readonly LuaMetadataService metadataService;
    private readonly BlueprintClassResolver classResolver;
    private readonly BlueprintPreviewService previewService;
    private readonly BlueprintValidationService validationService;
    private readonly BlueprintVariableFieldBuilder fieldBuilder;
    private readonly BlueprintNodeParameterEditorFactory nodeParameterEditorFactory;
    private BlueprintNodeDefinitionCatalog? nodeDefinitionCatalog;
    private BlueprintVariableForm variableForm = null!;
    private TextBox parentField = null!;
    private ListBox graphList = null!;
    private Grid rightPanel = null!;
    private Grid? splitLayout;
    private GridSplitter? contentSplitter;
    private GridLength leftColumnWidth;
    private GridLength rightColumnWidth;
    private Grid contentHost = null!;
    private Grid previewPanel = null!;
    private Image previewImage = null!;
    private TextBlock previewPlaceholder = null!;
    private readonly Dictionary<string, Control> graphViews = new(StringComparer.Ordinal);
    private readonly Dictionary<string, BlueprintGraphControl.ViewState> graphViewStates = new(StringComparer.Ordinal);
    private readonly Dictionary<string, (Button Button, JsonNode? ParentValue)> revertActions = new(StringComparer.Ordinal);
    private Toast toast = null!;
    private readonly DeferredWindowInitializer initializer;
    private ActorPreviewLease? previewLease;
    private EditorThumbnailLease? previewThumbnail;
    private CancellationTokenSource? previewRequest;
    private bool closed;
    private ActorVisualDescriptor? publishedVisualDescriptor;
    private ResolvedBlueprintClass? resolvedParent;
    private ResolvedBlueprintClass? resolvedClass;
    private bool suppressLiveVisualInvalidation;
    private bool visualDescriptorPublished;
    private bool refreshing;
    private bool inheritanceRefreshPending;

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
        Title = document.Title;
        Width = 1200;
        Height = 600;
        MaxHeight = 600;
        MinWidth = 700;
        MinHeight = 420;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        Background = Ludork.Services.EditorTheme.Brush("Background");
        FontFamily = Ludork.Services.EditorTheme.FontFamily;
        EditorWindowIcon.Apply(this);
        HistoryMergeBehavior.AttachBoundary(this, gameData);

        Content = DeferredWindowInitializer.CreateLoadingContent();

        document.ExternalChanged += onDataRestored;
        documentBinding = new EditorDocumentBinding(this, gameData, () => document.ResourceDocument, () => document.Title, closeWhenDeleted: true);
        documentBinding.HasPendingInputs = () => PendingInputErrors.Count != 0;
        BlueprintInputDraftCloseGuard.Attach(this, () => PendingInputErrors.Count != 0);
        projectSave.RegisterParticipant(this);
        gameData.DataReloaded += onDataReloaded;
        gameData.Documents.ContentChanged += onProjectContentChanged;
        Closed += onClosed;
        Deactivated += (_, _) => flushGraphViews();
        AddHandler(KeyDownEvent, onKeyDown, RoutingStrategies.Tunnel);
        initializer = new DeferredWindowInitializer(this, async cancellationToken =>
        {
            nodeDefinitionCatalog = new BlueprintNodeDefinitionCatalog(
                metadataService,
                classResolver);
            await EditorUiBatch.YieldAsync(cancellationToken);
            Content = createEditorContent();
            await EditorUiBatch.YieldAsync(cancellationToken);
            toast = new Toast(this);
            await initializeFieldsAsync(cancellationToken);
            if (inheritanceRefreshPending)
                Dispatcher.UIThread.Post(refreshInheritance, DispatcherPriority.Background);
        });
    }

    private Control createEditorContent()
    {
        parentField = EditorInputs.CreateReadOnlyTextBox();
        variableForm = new BlueprintVariableForm
        {
            AssetsDirectory = Path.Combine(gameData.ProjectPath, "Assets"),
            ProjectDirectory = gameData.ProjectPath,
            CellSize = gameData.getCellSize(),
            GameVariables = projectSave.GameVariables,
            IsReadOnly = !document.CanEditAttributes,
            ShowSourceGroups = document.Kind == BlueprintEditorDocumentKind.Blueprint,
            HistoryGameData = gameData,
            FieldActionFactory = createAttributeAction,
            CanRemoveComponent = field => document.Data["attrs"] is JsonObject attrs
                && attrs.ContainsKey(field.Name),
        };
        variableForm.ValueChanged += onVariableChanged;
        variableForm.ComponentAddRequested += onComponentAddRequested;
        variableForm.ComponentRemoveRequested += onComponentRemoveRequested;

        ContentWidthScrollViewer leftScroll = new()
        {
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
            Background = Ludork.Services.EditorTheme.Brush("Surface"),
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
        graphList.AddHandler(ContextRequestedEvent, onGraphListContextRequested, RoutingStrategies.Bubble);

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
            Foreground = EditorTheme.Brush("TextMuted"),
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

        rightPanel = new Grid
        {
            RowDefinitions = new RowDefinitions("50,*"),
        };
        rightPanel.Children.Add(tabBar);
        Grid.SetRow(contentHost, 1);
        rightPanel.Children.Add(contentHost);
        if (document.IsGraphOnly)
            return rightPanel;

        contentSplitter = new GridSplitter
        {
            Width = 4,
            Background = new SolidColorBrush(Color.Parse("#323232")),
            VerticalAlignment = VerticalAlignment.Stretch,
            ResizeDirection = GridResizeDirection.Columns,
        };
        splitLayout = new Grid
        {
            ColumnDefinitions = new ColumnDefinitions("Auto,4,*"),
        };
        void updateVariableColumnWidth()
        {
            double availableWidth = splitLayout.Bounds.Width;
            double splitterWidth = contentSplitter.IsVisible ? contentSplitter.Bounds.Width : 0;
            double maximum = availableWidth > 0
                ? Math.Max(0, availableWidth - splitterWidth)
                : double.PositiveInfinity;
            ColumnDefinition column = splitLayout.ColumnDefinitions[0];
            column.MaxWidth = maximum;
            column.MinWidth = Math.Min(leftScroll.RequiredWidth, maximum);
            leftScroll.MaxWidth = maximum;
        }
        leftScroll.RequiredWidthChanged += (_, _) => updateVariableColumnWidth();
        splitLayout.LayoutUpdated += (_, _) => updateVariableColumnWidth();
        updateVariableColumnWidth();
        splitLayout.Children.Add(leftScroll);
        Grid.SetColumn(contentSplitter, 1);
        splitLayout.Children.Add(contentSplitter);
        Grid.SetColumn(rightPanel, 2);
        splitLayout.Children.Add(rightPanel);
        return splitLayout;
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

    public IReadOnlyList<string> PendingInputErrors => document.ResourceDocument?.Exists == false ? [] : getInputErrors().ToArray();
    public IReadOnlyList<string> PendingInputPaths => PendingInputErrors.Count != 0 && document.ResourceDocument is EditorDocument resource ? [resource.Path] : [];

    private IEnumerable<string> getInputErrors()
    {
        foreach (string name in document.GetGraphNames())
        {
            IEnumerable<string> errors = graphViews.TryGetValue(name, out Control? content) && content is BlueprintGraphControl graph
                ? graph.GetInputErrors()
                : graphViewStates.TryGetValue(name, out BlueprintGraphControl.ViewState? state)
                    ? BlueprintGraphControl.GetInputErrors(name, state, document.GetEventGraph(name)["nodes"] as JsonArray ?? [])
                    : [];
            foreach (string error in errors)
                yield return document.Title + " / " + error;
        }
    }

    private void onInputDraftChanged(object? sender, EventArgs args)
    {
        documentBinding.Refresh();
        projectSave.NotifyPendingInputsChanged();
    }

    public bool RekeyBlueprint(string key)
    {
        if (!document.RekeyBlueprint(key))
            return false;
        FlushPendingChanges();
        documentBinding.Refresh();
        if (initializer.IsInitialized)
            refreshAll();
        onInputDraftChanged(this, EventArgs.Empty);
        return true;
    }

    private async void onValidateBlueprint(object? sender, RoutedEventArgs args)
    {
        FlushPendingChanges();
        if (document.BlueprintKey is not string key)
            return;
        if (PendingInputErrors.Count != 0)
        {
            await BlueprintValidationDialog.ShowResultsAsync(this,
                [new BlueprintValidationResult(key, false, PendingInputErrors)]);
            return;
        }
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
        if (initializer.IsInitialized)
            refreshAll();
        onInputDraftChanged(this, EventArgs.Empty);
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
            Height = EditorInputs.FieldMinHeight,
            Padding = new Thickness(0),
            IsVisible = document.CanEditAttributes,
            IsEnabled = document.CanEditAttributes,
        };
        parentPicker.Click += async (_, _) => await selectParentAsync();
        Grid.SetColumn(parentPicker, 4);
        parentRow.Children.Add(parentPicker);

        StackPanel panel = new()
        {
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
                Height = EditorInputs.FieldMinHeight,
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
            refreshPreview(resolvedClass);
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
                refreshPreview(resolvedClass);
            };
            return revert;
        }
        Button remove = new()
        {
            Content = "-",
            Width = 24,
            Height = EditorInputs.FieldMinHeight,
            Padding = new Thickness(0),
            IsEnabled = hasLocalValue,
        };
        remove.Click += (_, _) =>
        {
            if (!document.RemoveAttribute(field.Name))
                return;
            refreshAttributes();
            refreshPreview(resolvedClass);
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
            refreshPreview(resolvedClass);
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

    private async Task initializeFieldsAsync(CancellationToken cancellationToken)
    {
        refreshing = true;
        parentField.Text = document.Data["parent"]?.GetValue<string>() ?? string.Empty;
        ResolvedBlueprintClass resolved = classResolver.ResolveBlueprint(document.Data, document.BlueprintKey);
        resolvedClass = resolved;
        resolvedParent = resolveParentClass();
        revertActions.Clear();
        await variableForm.SetFieldsAsync(fieldBuilder.Build(resolved,
            document.Kind == BlueprintEditorDocumentKind.Blueprint), cancellationToken);
        updateGraphMode();
        refreshing = false;
        await EditorUiBatch.YieldAsync(cancellationToken);
        refreshGraphList(null, false);
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
        variableForm.SetFields(fieldBuilder.Build(
            resolved,
            document.Kind == BlueprintEditorDocumentKind.Blueprint));
        updateGraphMode();
        refreshing = false;
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
        variableForm.SetFields(fieldBuilder.Build(
            resolved,
            document.Kind == BlueprintEditorDocumentKind.Blueprint));
        bool graphModeChanged = updateGraphMode();
        refreshing = false;
        if (graphModeChanged)
            refreshGraphList(null, false);
    }

    private ResolvedBlueprintClass? resolveParentClass()
    {
        string parent = document.Data["parent"]?.GetValue<string>() ?? string.Empty;
        return parent.Length == 0 ? null : classResolver.Resolve(parent);
    }

    private async void refreshPreview(ResolvedBlueprintClass? resolved = null)
    {
        if (closed)
            return;
        resolved ??= classResolver.ResolveBlueprint(document.Data, document.BlueprintKey);
        resolvedClass = resolved;
        previewRequest?.Cancel();
        previewRequest?.Dispose();
        previewRequest = new CancellationTokenSource();
        CancellationToken cancellationToken = previewRequest.Token;
        bool suppressInvalidation = suppressLiveVisualInvalidation;
        try
        {
            (EditorThumbnailLease? thumbnail, ActorVisualDescriptor? descriptor) =
                await previewService.LoadPreviewAsync(resolved, resolved.ClassReference, 480, cancellationToken);
            if (cancellationToken.IsCancellationRequested || closed)
            {
                thumbnail?.Dispose();
                return;
            }
            publishVisualDescriptor(descriptor, suppressInvalidation);
            if (isGraphReadOnly())
            {
                thumbnail?.Dispose();
                if (previewLease is not null)
                    previewLease.IsActive = false;
                return;
            }
            if (descriptor is not { RequiresPreviewService: true })
                releasePreviewLease();
            else if (previewLease is null)
            {
                previewLease = previewService.ActorPreviews.Acquire(descriptor, 480, previewPanel.IsVisible);
                previewLease.FrameChanged += onPreviewFrameChanged;
            }
            else
                previewLease.UpdateDescriptor(descriptor);
            if (previewLease is not null)
                previewLease.IsActive = previewPanel.IsVisible;
            replacePreviewFallback(thumbnail);
        }
        catch (OperationCanceledException) when (cancellationToken.IsCancellationRequested)
        {
        }
    }

    private void publishVisualDescriptor(ActorVisualDescriptor? descriptor, bool suppressInvalidation)
    {
        if (!suppressInvalidation
            && visualDescriptorPublished
            && !Equals(publishedVisualDescriptor, descriptor))
        {
            previewService.InvalidateVisuals();
        }
        publishedVisualDescriptor = descriptor;
        visualDescriptorPublished = true;
    }

    private void replacePreviewFallback(EditorThumbnailLease? next)
    {
        EditorThumbnailLease? previous = previewThumbnail;
        previewThumbnail = next;
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
        if (closed)
            return;
        Bitmap? frame = previewLease?.Frame;
        previewImage.Source = frame ?? previewThumbnail?.Bitmap;
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
        ResolvedBlueprintClass resolved = resolvedClass
            ?? classResolver.ResolveBlueprint(document.Data, document.BlueprintKey);
        return classResolver.IsDerivedFrom(resolved, "Engine.Actor");
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
        if (isGraphReadOnly())
        {
            refreshPreview(resolvedClass);
            return;
        }
        if (graphList.SelectedItem is not BlueprintEditorTabItem selected)
            return;
        if (selected.IsPreview)
        {
            previewPanel.IsVisible = true;
            refreshPreview(resolvedClass);
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
        JsonObject startNodes = graph["startNodes"] as JsonObject ?? [];
        BlueprintNodeDefinitionSet definitionSet;
        IReadOnlyList<BlueprintGraphEventParameterDefinition> eventParameters;
        using (IDisposable metadataBatch = classResolver.BeginBatch())
        {
            definitionSet = nodeDefinitionCatalog!.GetNodeDefinitionSet(
                new BlueprintGraphContext(document.Data, document.BlueprintKey),
                resolvedClass);
            eventParameters = definitionSet.EventParameters.TryGetValue(
                eventName,
                out IReadOnlyList<BlueprintGraphEventParameterDefinition>? parameters)
                ? parameters
                : [];
        }
        BlueprintGraphDocument graphDocument = BlueprintGraphCodec.Load(
            eventName,
            eventGraph,
            startNodes[eventName],
            definitionSet,
            eventParameters);
        IReadOnlyList<BlueprintGraphNodeDefinition> definitions = document.IsGraphOnly
            ? definitionSet.Definitions.Where(definition => !definition.IsLatent).ToArray()
            : definitionSet.Definitions;
        BlueprintGraphControl control = new(
            gameData,
            graphDocument,
            definitions,
            fieldBuilder,
            nodeParameterEditorFactory,
            projectSave.GameVariables,
            Path.Combine(gameData.ProjectPath, "Assets"),
            gameData.getCellSize(),
            isGraphReadOnly());
        if (graphViewStates.TryGetValue(eventName, out BlueprintGraphControl.ViewState? state))
            control.RestoreViewState(state);
        control.GraphChanged += (_, _) =>
        {
            document.CommitEventGraph(eventName, BlueprintGraphCodec.Save(control.Document));
            onInputDraftChanged(control, EventArgs.Empty);
        };
        control.InputDraftChanged += onInputDraftChanged;
        return control;
    }

    private IReadOnlyList<string> getAvailableGraphNames()
    {
        List<string> result = document.GetGraphNames().ToList();
        if (document.Kind != BlueprintEditorDocumentKind.Blueprint)
            return result;
        ResolvedBlueprintClass resolved = resolvedClass
            ?? classResolver.ResolveBlueprint(document.Data, document.BlueprintKey);
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
        bool generalDataSelectorChanged = resolvedClass is not null
            && fieldBuilder.IsGeneralDataSelector(resolvedClass, args.Name);
        if (!document.CommitAttribute(args.Name, args.Value))
            return;
        if (revertActions.TryGetValue(
            args.Name,
            out (Button Button, JsonNode? ParentValue) action))
        {
            action.Button.IsEnabled = !JsonNode.DeepEquals(args.Value, action.ParentValue);
        }
        if (generalDataSelectorChanged)
        {
            Dispatcher.UIThread.Post(() =>
            {
                refreshAttributes();
                refreshPreview(resolvedClass);
            });
            return;
        }
        if (args.RequiresRefresh || args.Name == "scriptMixin")
        {
            clearGraphViews();
            Dispatcher.UIThread.Post(refreshAll);
            return;
        }
        refreshPreview();
    }

    private void onGraphSelectionChanged(object? sender, SelectionChangedEventArgs args)
    {
        if (!refreshing)
            showSelectedContent();
    }

    private void onGraphListContextRequested(object? sender, ContextRequestedEventArgs args)
    {
        bool requestedByPointer = args.TryGetPosition(graphList, out _);
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
        if (!requestedByPointer)
            hitItem ??= graphList.SelectedItem as BlueprintEditorTabItem;
        if (hitItem is not null)
            graphList.SelectedItem = hitItem;
        args.Handled = true;
        showGraphContextMenu(hitItem, requestedByPointer);
    }

    private void showGraphContextMenu(BlueprintEditorTabItem? item, bool requestedByPointer)
    {
        ContextMenu menu = new() { Placement = requestedByPointer ? PlacementMode.Pointer : PlacementMode.Bottom };
        if (document.CanEditGraphEvents)
        {
            MenuItem newEvent = new()
            {
                Header = LocaleService.Get("NEW_EVENT"),
                IsEnabled = !isGraphReadOnly(),
            };
            newEvent.Click += async (_, _) => await addEventAsync();
            menu.Items.Add(newEvent);
        }
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
            if (document.CanEditGraphEvents)
            {
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
        }
        if (menu.ItemCount > 0)
            menu.Open(graphList.ContainerFromIndex(graphList.SelectedIndex) ?? (Control)graphList);
    }

    private async Task addEventAsync()
    {
        if (isGraphReadOnly() || !document.CanEditGraphEvents)
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
        if (isGraphReadOnly() || !document.CanEditGraphEvents)
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
        BlueprintGraphControl.ViewState? inputState = graphViews.TryGetValue(selected.EventName, out Control? content)
            && content is BlueprintGraphControl graphControl ? graphControl.CaptureViewState()
            : graphViewStates.GetValueOrDefault(selected.EventName);
        if (!document.RenameEvent(selected.EventName, name))
        {
            await AlertDialog.ShowAsync(this, LocaleService.Get("ERROR"), LocaleService.Get("EVENT_EXISTS"));
            return;
        }
        removeGraphView(selected.EventName);
        if (inputState is not null)
            graphViewStates[name.Trim()] = inputState;
        refreshGraphList(name.Trim(), false);
        onInputDraftChanged(this, EventArgs.Empty);
    }

    private async Task deleteSelectedEventAsync()
    {
        if (isGraphReadOnly() || !document.CanEditGraphEvents)
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
        reloadFromDataChange();
    }

    private void onProjectContentChanged(object? sender, EditorDocumentsChangedEventArgs args)
    {
        if (closed || args.Reset || inheritanceRefreshPending
            || document.Kind != BlueprintEditorDocumentKind.Blueprint || resolvedClass is null)
        {
            return;
        }
        bool ancestorChanged = args.Changes.Any(change => change.Section == "Blueprints"
            && change.DocumentId != document.ResourceDocument?.Id
            && (change.Key is string key && resolvedClass.DependsOnBlueprint(key)
                || change.PreviousKey is string previousKey && resolvedClass.DependsOnBlueprint(previousKey)));
        if (!ancestorChanged)
            return;
        inheritanceRefreshPending = true;
        if (initializer.IsInitialized)
            Dispatcher.UIThread.Post(refreshInheritance, DispatcherPriority.Background);
    }

    private void refreshInheritance()
    {
        if (closed || !inheritanceRefreshPending || !initializer.IsInitialized)
            return;
        inheritanceRefreshPending = false;
        flushGraphViews();
        nodeDefinitionCatalog?.Invalidate();
        reload(false);
    }

    private void onDataReloaded(object? sender, EventArgs args)
    {
        reloadFromDataChange();
    }

    private void reloadFromDataChange()
    {
        inheritanceRefreshPending = false;
        suppressLiveVisualInvalidation = true;
        try
        {
            reload(true);
        }
        finally
        {
            suppressLiveVisualInvalidation = false;
        }
    }

    private async void onKeyDown(object? sender, KeyEventArgs args)
    {
        if (!initializer.IsInitialized)
            return;
        if (args.Key == Key.F2 && graphList.IsKeyboardFocusWithin)
        {
            if (isGraphReadOnly() || !document.CanEditGraphEvents)
                return;
            await renameSelectedEventAsync();
            args.Handled = true;
            return;
        }
        if (args.Key == Key.Delete && graphList.IsKeyboardFocusWithin)
        {
            if (isGraphReadOnly() || !document.CanEditGraphEvents)
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
            EditorFeedback.ShowHistory(toast, "Undo", documentBinding.Undo());
        else if (args.Key == Key.Y)
            EditorFeedback.ShowHistory(toast, "Redo", documentBinding.Redo());
        else
            return;
        args.Handled = true;
    }

    private void onClosed(object? sender, EventArgs args)
    {
        closed = true;
        inheritanceRefreshPending = false;
        gameData.Documents.ContentChanged -= onProjectContentChanged;
        previewRequest?.Cancel();
        previewRequest?.Dispose();
        previewRequest = null;
        variableForm?.Dispose();
        document.ExternalChanged -= onDataRestored;
        document.Dispose();
        projectSave.UnregisterParticipant(this);
        gameData.DataReloaded -= onDataReloaded;
        clearGraphViews();
        releasePreviewLease();
        previewImage?.SetValue(Image.SourceProperty, null);
        previewThumbnail?.Dispose();
        previewThumbnail = null;
    }

    private void removeGraphView(string eventName)
    {
        graphViewStates.Remove(eventName);
        if (!graphViews.Remove(eventName, out Control? content))
            return;
        contentHost.Children.Remove(content);
        if (content is IDisposable disposable)
            disposable.Dispose();
        onInputDraftChanged(this, EventArgs.Empty);
    }

    private void clearGraphViews(bool discardPendingChanges = false)
    {
        foreach (KeyValuePair<string, Control> entry in graphViews)
        {
            Control content = entry.Value;
            if (content is BlueprintGraphControl graph)
                graphViewStates[entry.Key] = graph.CaptureViewState();
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

    private bool updateGraphMode()
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
        if (splitLayout is null || contentSplitter is null || rightPanel.IsVisible == !readOnly)
            return false;
        if (readOnly)
        {
            leftColumnWidth = splitLayout.ColumnDefinitions[0].Width;
            rightColumnWidth = splitLayout.ColumnDefinitions[2].Width;
            splitLayout.ColumnDefinitions[0].Width = new GridLength(1, GridUnitType.Star);
            splitLayout.ColumnDefinitions[1].Width = new GridLength(0);
            splitLayout.ColumnDefinitions[2].Width = new GridLength(0);
            showSelectedContent();
        }
        else
        {
            splitLayout.ColumnDefinitions[0].Width = leftColumnWidth;
            splitLayout.ColumnDefinitions[1].Width = new GridLength(4);
            splitLayout.ColumnDefinitions[2].Width = rightColumnWidth;
        }
        rightPanel.IsVisible = !readOnly;
        contentSplitter.IsVisible = !readOnly;
        return true;
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
