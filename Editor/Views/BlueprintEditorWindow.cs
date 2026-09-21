using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Controls.Templates;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Threading;
using Avalonia.VisualTree;
using Ludork.Controls;
using Ludork.Models;
using Ludork.Services;
using Ludork.ViewModels;
using Ludork.Views.Utils;
using Ludork.Views.Utils.BlueprintGraph;
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
    private readonly BlueprintEditorViewModel viewModel;
    private BlueprintEditorDocument document => viewModel.Document;
    private readonly EditorDocumentBinding documentBinding;
    private readonly ProjectDataStore gameData;
    private readonly ProjectSaveService projectSave;
    private readonly LuaMetadataService metadataService;
    private readonly BlueprintClassResolver classResolver;
    private readonly BlueprintEditorPreviewSession previewSession;
    private readonly BlueprintNodeParameterEditorFactory nodeParameterEditorFactory;
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
    private bool closed;
    private ResolvedBlueprintClass? resolvedParent => viewModel.ResolvedParent;
    private ResolvedBlueprintClass? resolvedClass => viewModel.ResolvedClass;
    private bool suppressLiveVisualInvalidation;
    private bool refreshing;
    private bool inheritanceRefreshPending;

    public BlueprintEditorWindow(
        BlueprintEditorDocument document,
        ProjectDataStore gameData,
        ProjectSaveService projectSave,
        LuaMetadataService metadataService,
        BlueprintClassResolver classResolver,
        BlueprintPreviewService previewService)
    {
        viewModel = new BlueprintEditorViewModel(document, gameData, metadataService, classResolver);
        previewSession = new BlueprintEditorPreviewSession(previewService);
        previewSession.FrameChanged += onPreviewFrameChanged;
        this.gameData = gameData;
        this.projectSave = projectSave;
        this.metadataService = metadataService;
        this.classResolver = classResolver;
        nodeParameterEditorFactory = new BlueprintNodeParameterEditorFactory(
            gameData,
            metadataService,
            classResolver);
        DataContext = viewModel;
        Title = document.Title;
        Width = 1200;
        Height = 600;
        MaxHeight = 600;
        MinWidth = 700;
        MinHeight = 420;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        Background = EditorTheme.Brush("Background");
        FontFamily = EditorTheme.FontFamily;
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
        BlueprintEditorContent content = new() { DataContext = viewModel };
        Grid editorLayout = content.FindControl<Grid>("EditorLayout")!;
        parentField = content.FindControl<TextBox>("ParentField")!;
        variableForm = content.FindControl<BlueprintVariableForm>("VariableForm")!;
        graphList = content.FindControl<ListBox>("GraphList")!;
        rightPanel = content.FindControl<Grid>("RightPanel")!;
        contentHost = content.FindControl<Grid>("ContentHost")!;
        previewPanel = content.FindControl<Grid>("PreviewPanel")!;
        previewImage = content.FindControl<Image>("PreviewImage")!;
        previewPlaceholder = content.FindControl<TextBlock>("PreviewPlaceholder")!;
        EditorInputs.ApplyReadOnly(parentField);
        content.FindControl<TextBlock>("ParentLabel")!.Text = LocaleService.Get("PARENT");
        Button parentPicker = content.FindControl<Button>("ParentPicker")!;
        parentPicker.Height = EditorInputs.FieldMinHeight;
        parentPicker.Click += async (_, _) => await selectParentAsync();
        Button addAttribute = content.FindControl<Button>("AddAttributeButton")!;
        addAttribute.Height = EditorInputs.FieldMinHeight;
        addAttribute.Click += async (_, _) => await addAttributeAsync();
        Button validateButton = content.FindControl<Button>("ValidateButton")!;
        validateButton.Content = LocaleService.Get("VALIDATE_BLUEPRINT");
        validateButton.Click += onValidateBlueprint;
        previewPlaceholder.Text = LocaleService.Get("PREVIEW");
        variableForm.AssetsDirectory = Path.Combine(gameData.ProjectPath, "Assets");
        variableForm.ProjectDirectory = gameData.ProjectPath;
        variableForm.CellSize = gameData.Configs.getCellSize();
        variableForm.GameVariables = projectSave.GameVariables;
        variableForm.IsReadOnly = !document.CanEditAttributes;
        variableForm.ShowSourceGroups = document.Kind == BlueprintEditorDocumentKind.Blueprint;
        variableForm.HistoryGameData = gameData;
        variableForm.FieldActionFactory = createAttributeAction;
        variableForm.CanRemoveComponent = field => viewModel.HasLocalAttribute(field.Name);
        variableForm.ValueChanged += onVariableChanged;
        variableForm.ComponentAddRequested += onComponentAddRequested;
        variableForm.ComponentRemoveRequested += onComponentRemoveRequested;
        graphList.SelectionChanged += onGraphSelectionChanged;
        graphList.AddHandler(ContextRequestedEvent, onGraphListContextRequested, RoutingStrategies.Bubble);
        ContentWidthScrollViewer leftScroll = content.FindControl<ContentWidthScrollViewer>("AttributeScroll")!;
        GridSplitter splitter = content.FindControl<GridSplitter>("ContentSplitter")!;
        if (document.IsGraphOnly)
        {
            leftScroll.IsVisible = false;
            splitter.IsVisible = false;
            editorLayout.ColumnDefinitions = new ColumnDefinitions("0,0,*");
            return content;
        }
        splitLayout = editorLayout;
        contentSplitter = splitter;
        void updateVariableColumnWidth()
        {
            double availableWidth = editorLayout.Bounds.Width;
            double splitterWidth = splitter.IsVisible ? splitter.Bounds.Width : 0;
            double maximum = availableWidth > 0 ? Math.Max(0, availableWidth - splitterWidth) : double.PositiveInfinity;
            ColumnDefinition column = editorLayout.ColumnDefinitions[0];
            column.MaxWidth = maximum;
            column.MinWidth = Math.Min(leftScroll.RequiredWidth, maximum);
            leftScroll.MaxWidth = maximum;
        }
        leftScroll.RequiredWidthChanged += (_, _) => updateVariableColumnWidth();
        editorLayout.LayoutUpdated += (_, _) => updateVariableColumnWidth();
        updateVariableColumnWidth();
        return content;
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
        BlueprintValidationResult result = viewModel.Validate(key);
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
        if (viewModel.SelectedTab is BlueprintEditorTabItem selected
            && !selected.IsPreview
            && string.Equals(selected.EventName, eventName, StringComparison.Ordinal))
        {
            showSelectedContent();
        }
    }

    private async Task selectParentAsync()
    {
        string current = viewModel.ParentReference;
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
        if (!viewModel.CanChangeParent(selected))
        {
            await AlertDialog.ShowAsync(
                this,
                LocaleService.Get("ERROR"),
                LocaleService.Get("SCRIPT_MIXIN_INHERITANCE_CONFLICT"));
            return;
        }
        flushGraphViews();
        clearGraphViews();
        if (viewModel.CommitParent(selected))
            refreshAll();
    }

    private async Task commitScriptPathAsync(JsonNode? value)
    {
        BlueprintAttributeChange change = viewModel.PrepareScriptPath(value);
        if (change.Error is string error)
        {
            await showScriptMixinErrorAsync(error);
            refreshAttributes();
            return;
        }
        IReadOnlyList<string> staleFields = change.StaleFields;
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
        if (viewModel.CommitAttributeChange(change))
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
        ResolvedBlueprintClass resolved = viewModel.ResolveClass();
        string? name = await SingleRowDialog.ShowAsync(
            this,
            LocaleService.Get("ADD_ATTR"),
            LocaleService.Get("ATTR_NAME"),
            resolved.Fields.Select(field => field.Name));
        if (string.IsNullOrWhiteSpace(name))
            return;
        string attributeName = name.Trim();
        if (viewModel.ValidateAttributeName(attributeName) is string error)
        {
            await AlertDialog.ShowAsync(this, LocaleService.Get("ERROR"), error);
            return;
        }
        if (viewModel.CommitAttribute(attributeName, JsonValue.Create(string.Empty)))
            refreshAttributes();
    }

    private Control? createAttributeAction(BlueprintVariableField field)
    {
        if (!document.CanEditAttributes || field.IsComponent || field.IsReadOnly)
            return null;
        if (field.Name == "scriptPath" && resolvedClass?.HasBlueprintParent != true)
            return null;
        bool hasLocalValue = viewModel.HasLocalAttribute(field.Name);
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
                if (!viewModel.CommitAttribute(field.Name, parentValue))
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
            if (!viewModel.RemoveAttribute(field.Name))
                return;
            refreshAttributes();
            refreshPreview(resolvedClass);
        };
        return remove;
    }

    private async Task removeLocalScriptPathAsync()
    {
        BlueprintAttributeChange? change = viewModel.PrepareRemoveScriptPath();
        if (change is null)
            return;
        IReadOnlyList<string> staleFields = change.StaleFields;
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
        if (viewModel.CommitAttributeChange(change))
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
        if (viewModel.AddComponent(selectedField))
        {
            refreshAttributes();
        }
    }

    private void onComponentRemoveRequested(
        object? sender,
        BlueprintComponentFieldEventArgs args)
    {
        if (viewModel.RemoveAttribute(args.Field.Name))
            refreshAttributes();
    }

    private async Task initializeFieldsAsync(CancellationToken cancellationToken)
    {
        refreshing = true;
        parentField.Text = viewModel.ParentReference;
        IReadOnlyList<BlueprintVariableField> fields = viewModel.RefreshFields();
        revertActions.Clear();
        await variableForm.SetFieldsAsync(fields, cancellationToken);
        updateGraphMode();
        refreshing = false;
        await EditorUiBatch.YieldAsync(cancellationToken);
        refreshGraphList(null, false);
    }

    private void refreshAll()
    {
        refreshing = true;
        parentField.Text = viewModel.ParentReference;
        IReadOnlyList<BlueprintVariableField> fields = viewModel.RefreshFields();
        revertActions.Clear();
        variableForm.SetFields(fields);
        updateGraphMode();
        refreshing = false;
        refreshGraphList(null, false);
    }

    private void refreshAttributes()
    {
        refreshing = true;
        parentField.Text = viewModel.ParentReference;
        IReadOnlyList<BlueprintVariableField> fields = viewModel.RefreshFields();
        revertActions.Clear();
        variableForm.SetFields(fields);
        bool graphModeChanged = updateGraphMode();
        refreshing = false;
        if (graphModeChanged)
            refreshGraphList(null, false);
    }

    private async void refreshPreview(ResolvedBlueprintClass? resolved = null)
    {
        if (closed)
            return;
        resolved ??= viewModel.ResolveClass();
        previewSession.IsReadOnly = viewModel.IsGraphReadOnly;
        previewSession.IsVisible = previewPanel.IsVisible;
        await previewSession.RefreshAsync(resolved, suppressLiveVisualInvalidation);
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
        previewImage.Source = previewSession.Frame;
        previewPlaceholder.IsVisible = previewImage.Source is null;
        previewImage.InvalidateVisual();
    }

    private void refreshGraphList(string? preferredEvent, bool preferPreview)
    {
        refreshing = true;
        viewModel.RefreshTabs(preferredEvent, preferPreview);
        graphList.ItemsSource = viewModel.Tabs;
        graphList.SelectedItem = viewModel.SelectedTab;
        refreshing = false;
        showSelectedContent();
    }

    private void showSelectedContent()
    {
        previewPanel.IsVisible = false;
        previewSession.IsVisible = false;
        foreach (Control graphView in graphViews.Values)
            graphView.IsVisible = false;
        if (viewModel.IsGraphReadOnly)
        {
            refreshPreview(resolvedClass);
            return;
        }
        if (viewModel.SelectedTab is not BlueprintEditorTabItem selected)
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
        BlueprintGraphEditorData graphData = viewModel.LoadGraph(eventName, eventGraph);
        BlueprintGraphControl control = new(
            gameData,
            graphData.Document,
            graphData.Definitions,
            viewModel.FieldBuilder,
            nodeParameterEditorFactory,
            projectSave.GameVariables,
            Path.Combine(gameData.ProjectPath, "Assets"),
            gameData.Configs.getCellSize(),
            viewModel.IsGraphReadOnly);
        if (graphViewStates.TryGetValue(eventName, out BlueprintGraphControl.ViewState? state))
            control.RestoreViewState(state);
        control.GraphChanged += (_, _) =>
        {
            viewModel.CommitGraph(eventName, control.Document);
            onInputDraftChanged(control, EventArgs.Empty);
        };
        control.InputDraftChanged += onInputDraftChanged;
        return control;
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
        bool generalDataSelectorChanged = viewModel.IsGeneralDataSelector(args.Name);
        if (!viewModel.CommitAttribute(args.Name, args.Value))
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
        {
            viewModel.SelectedTab = graphList.SelectedItem as BlueprintEditorTabItem;
            showSelectedContent();
        }
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
            hitItem ??= viewModel.SelectedTab;
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
                IsEnabled = !viewModel.IsGraphReadOnly,
            };
            newEvent.Click += async (_, _) => await addEventAsync();
            menu.Items.Add(newEvent);
        }
        if (item is { IsPreview: false, EventName: not null })
        {
            MenuItem organize = new()
            {
                Header = LocaleService.Get("ORGANIZE_GRAPH"),
                IsEnabled = !viewModel.IsGraphReadOnly,
            };
            ToolTip.SetTip(organize, LocaleService.Get("ORGANIZE_GRAPH_TIP"));
            organize.Click += (_, _) => organizeSelectedGraph();
            menu.Items.Add(organize);
            if (document.CanEditGraphEvents)
            {
                MenuItem rename = new()
                {
                    Header = LocaleService.Get("RENAME_EVENT"),
                    IsEnabled = !viewModel.IsGraphReadOnly,
                };
                rename.Click += async (_, _) => await renameSelectedEventAsync();
                menu.Items.Add(rename);
                MenuItem delete = new()
                {
                    Header = LocaleService.Get("DELETE_EVENT"),
                    IsEnabled = !viewModel.IsGraphReadOnly,
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
        if (viewModel.IsGraphReadOnly || !document.CanEditGraphEvents)
            return;
        string? name = await SingleRowDialog.ShowAsync(
            this,
            LocaleService.Get("NEW_EVENT"),
            LocaleService.Get("ENTER_EVENT_NAME"),
            viewModel.GetAvailableGraphNames());
        if (string.IsNullOrWhiteSpace(name))
            return;
        if (!viewModel.AddEvent(name))
        {
            await AlertDialog.ShowAsync(this, LocaleService.Get("ERROR"), LocaleService.Get("INVALID_NAME"));
            return;
        }
        refreshGraphList(name.Trim(), false);
    }

    private async Task renameSelectedEventAsync()
    {
        if (viewModel.IsGraphReadOnly || !document.CanEditGraphEvents)
            return;
        if (viewModel.SelectedTab is not BlueprintEditorTabItem { IsPreview: false, EventName: not null } selected)
            return;
        string? name = await SingleRowDialog.ShowAsync(
            this,
            LocaleService.Get("RENAME_EVENT"),
            LocaleService.Get("ENTER_EVENT_NAME"),
            viewModel.GetAvailableGraphNames().Where(value => !string.Equals(value, selected.EventName, StringComparison.Ordinal)),
            selected.EventName);
        if (string.IsNullOrWhiteSpace(name))
            return;
        flushGraphView(selected.EventName);
        BlueprintGraphControl.ViewState? inputState = graphViews.TryGetValue(selected.EventName, out Control? content)
            && content is BlueprintGraphControl graphControl ? graphControl.CaptureViewState()
            : graphViewStates.GetValueOrDefault(selected.EventName);
        if (!viewModel.RenameEvent(selected.EventName, name))
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
        if (viewModel.IsGraphReadOnly || !document.CanEditGraphEvents)
            return;
        if (viewModel.SelectedTab is not BlueprintEditorTabItem { IsPreview: false, EventName: not null } selected)
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
        if (!viewModel.DeleteEvent(selected.EventName))
            return;
        removeGraphView(selected.EventName);
        refreshGraphList(null, viewModel.SupportsPreview());
    }

    private void organizeSelectedGraph()
    {
        if (viewModel.IsGraphReadOnly)
            return;
        if (viewModel.SelectedTab is not BlueprintEditorTabItem { IsPreview: false, EventName: not null } selected)
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
        if (!viewModel.DependsOnChangedAncestor(args))
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
        viewModel.InvalidateNodeDefinitions();
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
            if (viewModel.IsGraphReadOnly || !document.CanEditGraphEvents)
                return;
            await renameSelectedEventAsync();
            args.Handled = true;
            return;
        }
        if (args.Key == Key.Delete && graphList.IsKeyboardFocusWithin)
        {
            if (viewModel.IsGraphReadOnly || !document.CanEditGraphEvents)
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
        previewSession.FrameChanged -= onPreviewFrameChanged;
        previewSession.Dispose();
        variableForm?.Dispose();
        document.ExternalChanged -= onDataRestored;
        viewModel.Dispose();
        projectSave.UnregisterParticipant(this);
        gameData.DataReloaded -= onDataReloaded;
        clearGraphViews();
        previewImage?.SetValue(Image.SourceProperty, null);
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

    private bool updateGraphMode()
    {
        bool readOnly = viewModel.IsGraphReadOnly;
        previewSession.IsReadOnly = readOnly;
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

}
