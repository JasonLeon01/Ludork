using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Media;
using Avalonia.VisualTree;
using Avalonia.Threading;
using Ludork.Controls;
using Ludork.Models;
using Ludork.Services;
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

public sealed class CommonFunctionWindow : Window, IProjectSaveParticipant
{
    private static string? clipboardName;
    private static JsonObject? clipboardData;
    private readonly ProjectDataStore gameData;
    private readonly ProjectSaveService projectSave;
    private readonly EditorDocumentBinding documentBinding;
    private readonly BlueprintVariableFieldBuilder fieldBuilder;
    private readonly BlueprintNodeParameterEditorFactory parameterEditorFactory;
    private BlueprintNodeDefinitionCatalog? nodeDefinitionCatalog;
    private ListBox functionList = null!;
    private Border graphHost = null!;
    private Toast toast = null!;
    private readonly DeferredWindowInitializer initializer;
    private readonly BlueprintNodePickerIndexCache pickerIndexes = new();
    private CommonFunctionEditorDocument? currentDocument;
    private BlueprintGraphControl? graphControl;
    private string? pendingFunctionName;
    private bool refreshing;
    private bool closed;
    private readonly BlueprintGraphLoadSession graphLoading = new();
    private BlueprintGraphControl? loadingControl;
    private CancellationToken loadingToken;
    private readonly Dictionary<Guid, BlueprintGraphControl.ViewState> graphViewStates = [];

    public CommonFunctionWindow(
        ProjectDataStore gameData,
        ProjectSaveService projectSave,
        LuaMetadataService metadataService,
        BlueprintClassResolver classResolver)
    {
        this.gameData = gameData;
        this.projectSave = projectSave;
        fieldBuilder = new BlueprintVariableFieldBuilder(gameData, metadataService);
        parameterEditorFactory = new BlueprintNodeParameterEditorFactory(gameData);
        Title = LocaleService.Get("COMMON_FUNCTIONS");
        Width = 1200;
        Height = 800;
        MinWidth = 1200;
        MinHeight = 800;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        Background = Ludork.Services.EditorTheme.Brush("Background");
        FontFamily = Ludork.Services.EditorTheme.FontFamily;
        EditorWindowIcon.Apply(this);
        EditorLayoutService.AttachWindow(this, nameof(CommonFunctionWindow));

        Content = DeferredWindowInitializer.CreateLoadingContent();

        documentBinding = new EditorDocumentBinding(this, gameData,
            () => currentDocument?.ResourceDocument,
            () => LocaleService.Get("COMMON_FUNCTIONS"));
        documentBinding.HasPendingInputs = () => PendingInputErrors.Count != 0;
        BlueprintInputDraftCloseGuard.Attach(this, () => PendingInputErrors.Count != 0);
        projectSave.RegisterParticipant(this);
        gameData.Documents.ContentChanged += onDocumentsChanged;
        gameData.Documents.DiskReloaded += onDocumentDiskReloaded;
        gameData.DataReloaded += onDataReloaded;
        if (gameData.BlueprintNodeSearch is BlueprintNodeSearchService search)
        {
            search.Invalidated += onNodeSearchInvalidated;
            search.Validating += onNodeSearchValidating;
            search.Changed += onNodeSearchChanged;
        }
        LocaleService.LanguageChanged += onLoadLanguageChanged;
        Closed += onClosed;
        Deactivated += (_, _) => flushGraph();
        AddHandler(KeyDownEvent, onKeyDown, RoutingStrategies.Tunnel);
        HistoryMergeBehavior.AttachBoundary(this, gameData);
        initializer = new DeferredWindowInitializer(this, async cancellationToken =>
        {
            nodeDefinitionCatalog = new BlueprintNodeDefinitionCatalog(
                metadataService,
                classResolver);
            await EditorUiBatch.YieldAsync(cancellationToken);
            Content = createEditorContent();
            await EditorUiBatch.YieldAsync(cancellationToken);
            toast = new Toast(this);
            string? preferredName = pendingFunctionName;
            pendingFunctionName = null;
            refreshList(preferredName);
        });
    }

    private Control createEditorContent()
    {
        functionList = new ListBox
        {
            MinWidth = 200,
            MaxWidth = 240,
            Width = 220,
            SelectionMode = SelectionMode.Single,
            HorizontalAlignment = Avalonia.Layout.HorizontalAlignment.Stretch,
            VerticalAlignment = Avalonia.Layout.VerticalAlignment.Stretch,
            ItemTemplate = DocumentStatusPresenter.CreateTemplate(gameData, "CommonFunctions"),
        };
        functionList.SelectionChanged += onSelectionChanged;
        functionList.AddHandler(
            ContextRequestedEvent,
            onListContextRequested,
            RoutingStrategies.Bubble);
        graphHost = new Border
        {
            Background = new SolidColorBrush(Color.Parse("#1c1c1c")),
        };
        Grid root = new()
        {
            ColumnDefinitions = new ColumnDefinitions("Auto,*"),
        };
        root.Children.Add(functionList);
        Grid.SetColumn(graphHost, 1);
        root.Children.Add(graphHost);
        return root;
    }

    public void FlushPendingChanges()
    {
        flushGraph();
    }

    public IReadOnlyList<string> PendingInputErrors => gameData.Documents.All
        .Where(resource => resource.Section == "CommonFunctions" && resource.Exists)
        .SelectMany(resource => (resource.Id == currentDocument?.ResourceDocument?.Id && graphControl is not null
            ? graphControl.GetInputErrors()
            : graphViewStates.TryGetValue(resource.Id, out BlueprintGraphControl.ViewState? state)
                ? BlueprintGraphControl.GetInputErrors("common", state, gameData.Blueprints.CommonFunctionsData[resource.Key].Graph.Events.GetValueOrDefault("common")?.GetNodeJson() ?? [])
                : []).Select(error => $"CommonFunctions/{resource.Key} / {error}"))
        .ToArray();

    public IReadOnlyList<string> PendingInputPaths => gameData.Documents.All
        .Where(resource => resource.Section == "CommonFunctions" && resource.Exists
            && (resource.Id == currentDocument?.ResourceDocument?.Id && graphControl is not null
                ? graphControl.GetInputErrors().Count != 0
                : graphViewStates.TryGetValue(resource.Id, out BlueprintGraphControl.ViewState? state)
                    && BlueprintGraphControl.GetInputErrors("common", state, gameData.Blueprints.CommonFunctionsData[resource.Key].Graph.Events.GetValueOrDefault("common")?.GetNodeJson() ?? []).Any()))
        .Select(resource => resource.Path).ToArray();

    private void onInputDraftChanged(object? sender, EventArgs args)
    {
        documentBinding.Refresh();
        projectSave.NotifyPendingInputsChanged();
    }

    public void SelectFunction(string? name)
    {
        if (name is null)
            return;
        if (!initializer.IsInitialized)
        {
            pendingFunctionName = name;
            return;
        }
        refreshList(name);
    }

    private void onSelectionChanged(object? sender, SelectionChangedEventArgs args)
    {
        if (!refreshing)
            showSelectedFunction();
    }

    private async void showSelectedFunction()
    {
        if (closed)
            return;
        disposeGraph(false);
        if (functionList.SelectedItem is not string name)
            return;
        CommonFunctionEditorDocument? selectedDocument = CommonFunctionEditorDocument.Create(gameData, name);
        currentDocument = selectedDocument;
        documentBinding.Refresh();
        if (selectedDocument is null)
            return;
        selectedDocument.ExternalChanged += onDataRestored;
        CancellationToken cancellationToken = graphLoading.Begin();
        loadingToken = cancellationToken;
        graphHost.Child = DeferredWindowInitializer.CreateLoadingContent();
        EditorDocument? resource = selectedDocument.ResourceDocument;
        long revision = resource?.Revision ?? 0;
        long? metadataRevision = null;
        string language = LocaleService.CurrentLanguage;
        bool isCurrent() => !closed && graphLoading.IsCurrent(cancellationToken)
            && ReferenceEquals(selectedDocument, currentDocument)
            && (resource?.Revision ?? 0) == revision
            && (metadataRevision is null || gameData.Metadata.CacheRevision == metadataRevision)
            && LocaleService.CurrentLanguage == language
            && functionList.SelectedItem is string selected && selected == name;
        try
        {
            BlueprintNodeDefinitionSet definitionSet;
            using (IDisposable? read = gameData.BlueprintNodeSearch?.IsCurrent == true
                ? gameData.Metadata.BeginPreparedRead() : null)
                definitionSet = nodeDefinitionCatalog!.GetNodeDefinitionSet();
            metadataRevision = gameData.Metadata.CacheRevision;
            BlueprintGraphDocument graphDocument = await graphLoading.LoadAsync("common",
                selectedDocument.GetEventGraph(), selectedDocument.GetStartNodes()["common"], definitionSet, [], false,
                () => gameData.BlueprintNodeSearch?.IsCurrent == true
                    ? gameData.Metadata.BeginPreparedRead() : gameData.Metadata.BeginRead(),
                () => isCurrent() && gameData.Metadata.CacheRevision == metadataRevision, cancellationToken);
            cancellationToken.ThrowIfCancellationRequested();
            BlueprintGraphControl control = new(gameData, graphDocument, definitionSet.Definitions,
                fieldBuilder, parameterEditorFactory, projectSave.GameVariables,
                Path.Combine(gameData.ProjectPath, "Assets"), gameData.Configs.getCellSize(), false, pickerIndexes);
            loadingControl = control;
            if (resource is not null && graphViewStates.TryGetValue(resource.Id, out BlueprintGraphControl.ViewState? state))
                control.RestoreViewState(state);
            graphHost.Child = control;
            await control.InitializeVisualsAsync(cancellationToken);
            if (!isCurrent())
                throw new OperationCanceledException(cancellationToken);
            control.GraphChanged += onGraphChanged;
            control.InputDraftChanged += onInputDraftChanged;
            graphControl = control;
            loadingControl = null;
            loadingToken = default;
            graphLoading.Complete(cancellationToken);
            onInputDraftChanged(this, EventArgs.Empty);
        }
        catch (OperationCanceledException)
        {
            if (loadingToken != cancellationToken)
                return;
            cancelGraphLoad();
            if (!closed && ReferenceEquals(selectedDocument, currentDocument)
                && gameData.BlueprintNodeSearch?.IsCurrent != false)
                Dispatcher.UIThread.Post(showSelectedFunction, DispatcherPriority.Background);
        }
        catch (Exception error)
        {
            if (!graphLoading.IsCurrent(cancellationToken))
                return;
            showGraphLoadError(error.Message);
        }
    }

    private void showGraphLoadError(string error)
    {
        cancelGraphLoad();
        Button retry = new() { Content = LocaleService.Get("RETRY") };
        retry.Click += (_, _) => showSelectedFunction();
        graphHost.Child = new StackPanel
        {
            Margin = new Thickness(16),
            Spacing = 8,
            Children =
            {
                new TextBlock { Text = error, TextWrapping = TextWrapping.Wrap },
                retry,
            },
        };
    }

    private void cancelGraphLoad()
    {
        graphLoading.Cancel();
        loadingToken = default;
        BlueprintGraphControl? pending = loadingControl;
        loadingControl = null;
        if (pending is not null)
        {
            pending.DiscardPendingChanges();
            pending.Dispose();
        }
        if (graphControl is null && graphHost is not null)
            graphHost.Child = null;
    }

    private void onGraphChanged(object? sender, EventArgs args)
    {
        if (graphControl is null || currentDocument is null)
            return;
        currentDocument.CommitGraph(BlueprintGraphCodec.Save(graphControl.Document));
        onInputDraftChanged(sender, args);
    }

    private void onListContextRequested(object? sender, ContextRequestedEventArgs args)
    {
        bool requestedByPointer = args.TryGetPosition(functionList, out _);
        string? hitName = null;
        if (args.Source is Visual source)
        {
            Visual? current = source;
            while (current is not null)
            {
                if (current is ListBoxItem item && item.Content is string name)
                {
                    hitName = name;
                    break;
                }
                current = current.GetVisualParent();
            }
        }
        if (!requestedByPointer)
            hitName ??= functionList.SelectedItem as string;
        if (hitName is not null)
            functionList.SelectedItem = hitName;
        args.Handled = true;
        showContextMenu(hitName, requestedByPointer);
    }

    private void showContextMenu(string? name, bool requestedByPointer)
    {
        ContextMenu menu = new() { Placement = requestedByPointer ? PlacementMode.Pointer : PlacementMode.Bottom };
        if (name is not null)
        {
            MenuItem organize = new() { Header = LocaleService.Get("ORGANIZE_GRAPH") };
            ToolTip.SetTip(organize, LocaleService.Get("ORGANIZE_GRAPH_TIP"));
            organize.Click += (_, _) => organizeGraph();
            menu.Items.Add(organize);
            MenuItem copy = new() { Header = LocaleService.Get("COPY") };
            copy.Click += (_, _) => copySelected();
            menu.Items.Add(copy);
            MenuItem rename = new() { Header = LocaleService.Get("RENAME_FUNC") };
            rename.Click += async (_, _) => await renameSelectedAsync();
            menu.Items.Add(rename);
            MenuItem delete = new() { Header = LocaleService.Get("DELETE") };
            delete.Click += async (_, _) => await deleteSelectedAsync();
            menu.Items.Add(delete);
        }
        else
        {
            MenuItem create = new() { Header = LocaleService.Get("NEW_COMMON_FUNC") };
            create.Click += async (_, _) => await createFunctionAsync();
            menu.Items.Add(create);
            if (clipboardData is not null)
            {
                MenuItem paste = new() { Header = LocaleService.Get("PASTE") };
                paste.Click += (_, _) => pasteFunction();
                menu.Items.Add(paste);
            }
        }
        if (menu.ItemCount > 0)
            menu.Open(functionList.ContainerFromIndex(functionList.SelectedIndex) ?? (Control)functionList);
    }

    private async Task createFunctionAsync()
    {
        string? name = await SingleRowDialog.ShowAsync(
            this,
            LocaleService.Get("NEW_COMMON_FUNC"),
            LocaleService.Get("ENTER_FUNC_NAME"),
            gameData.Blueprints.CommonFunctionsData.Keys);
        if (string.IsNullOrWhiteSpace(name))
            return;
        flushGraph();
        if (!gameData.Blueprints.CreateCommonFunction(name))
        {
            toast.ShowMessage(LocaleService.Get("FUNC_NAME_EXISTS"));
            return;
        }
        refreshList(name.Trim());
    }

    private void copySelected()
    {
        flushGraph();
        if (functionList.SelectedItem is not string name
            || !gameData.Blueprints.CommonFunctionsData.TryGetValue(name, out CommonFunctionSnapshot? data))
        {
            return;
        }
        clipboardName = name;
        clipboardData = data.ToJson();
    }

    private void pasteFunction()
    {
        if (clipboardData is null || clipboardName is null)
            return;
        flushGraph();
        string name = clipboardName + " (copy)";
        if (gameData.Blueprints.CommonFunctionsData.ContainsKey(name))
        {
            int index = 1;
            while (gameData.Blueprints.CommonFunctionsData.ContainsKey($"{name}_{index}"))
                index += 1;
            name = $"{name}_{index}";
        }
        if (gameData.Blueprints.CreateCommonFunction(name, clipboardData))
            refreshList(name);
    }

    private async Task renameSelectedAsync()
    {
        if (functionList.SelectedItem is not string oldName)
            return;
        string? newName = await SingleRowDialog.ShowAsync(
            this,
            LocaleService.Get("RENAME_FUNC"),
            LocaleService.Get("ENTER_FUNC_NAME"),
            gameData.Blueprints.CommonFunctionsData.Keys.Where(name =>
                !string.Equals(name, oldName, StringComparison.Ordinal)),
            oldName);
        if (string.IsNullOrWhiteSpace(newName)
            || string.Equals(oldName, newName.Trim(), StringComparison.Ordinal))
        {
            return;
        }
        flushGraph();
        bool? renamed = await EditorEditWorkflow.RunAsync<bool?>(this, gameData, LocaleService.Get("EDIT_OPERATION_RENAMING"),
            async progress => await gameData.RenameDocumentResourceAsync("CommonFunctions", oldName, newName.Trim(), progress), null);
        if (renamed is null)
            return;
        if (!renamed.Value)
        {
            toast.ShowMessage(LocaleService.Get("FUNC_NAME_EXISTS"));
            return;
        }
        refreshList(newName.Trim());
    }

    private async Task deleteSelectedAsync()
    {
        if (functionList.SelectedItem is not string name)
            return;
        string message = LocaleService.Get("CONFIRM_DELETE_FUNC")
            .Replace("{name}", name, StringComparison.Ordinal) + Environment.NewLine + LocaleService.Get("DELETE_DOCUMENT_CONFIRMATION");
        bool confirmed = await ConfirmationDialog.ShowAsync(
            this,
            LocaleService.Get("CONFIRM_DELETE"),
            message, details: LocaleService.Get("DELETE_DOCUMENT_DETAILS"));
        if (!confirmed)
            return;
        flushGraph();
        if (await EditorResourceOperations.DeleteAsync(this, gameData,
            progress => gameData.DeleteDocumentResourceAsync("CommonFunctions", name, progress)))
            refreshList(null);
    }

    private void organizeGraph()
    {
        if (graphControl is null)
            showSelectedFunction();
        graphControl?.OrganizeLayout();
    }

    private async void onKeyDown(object? sender, KeyEventArgs args)
    {
        if (projectSave.GameData.EditOperations.IsBusy)
        {
            args.Handled = true;
            return;
        }
        if (!initializer.IsInitialized)
            return;
        if (functionList.IsKeyboardFocusWithin)
        {
            if (args.Key == Key.Delete)
            {
                await deleteSelectedAsync();
                args.Handled = true;
                return;
            }
            if (args.Key == Key.F2)
            {
                await renameSelectedAsync();
                args.Handled = true;
                return;
            }
            if (EditorShortcuts.HasPrimaryModifier(args.KeyModifiers))
            {
                if (args.Key == Key.C)
                {
                    copySelected();
                    args.Handled = true;
                    return;
                }
                else if (args.Key == Key.V)
                {
                    pasteFunction();
                    args.Handled = true;
                    return;
                }
            }
        }

        if (!EditorShortcuts.HasPrimaryModifier(args.KeyModifiers))
            return;
        flushGraph();
        await documentBinding.HandleShortcutAsync(args, projectSave, toast);
    }

    private void refreshList(string? preferredName)
    {
        string? selectedName = preferredName ?? functionList.SelectedItem as string;
        string[] names = gameData.Blueprints.CommonFunctionsData.Keys
            .OrderBy(name => name, StringComparer.Ordinal)
            .ToArray();
        refreshing = true;
        functionList.ItemsSource = names;
        functionList.SelectedItem = selectedName is not null
            && names.Contains(selectedName, StringComparer.Ordinal)
                ? selectedName
                : names.FirstOrDefault();
        refreshing = false;
        showSelectedFunction();
    }

    private void onDocumentsChanged(object? sender, EditorDocumentsChangedEventArgs args)
    {
        if (!initializer.IsInitialized || !args.Reset
            && !args.Changes.Any(change => change.Section == "CommonFunctions" && change.IdentityChanged))
            return;
        string[] keys = gameData.Blueprints.CommonFunctionsData.Keys.OrderBy(key => key, StringComparer.Ordinal).ToArray();
        if (!keys.SequenceEqual(functionList.ItemsSource?.Cast<string>() ?? [], StringComparer.Ordinal))
            refreshList(currentDocument?.Name);
    }

    private void onDataRestored(object? sender, EventArgs args)
    {
        if (!initializer.IsInitialized)
            return;
        string? name = currentDocument?.Name ?? functionList.SelectedItem as string;
        disposeGraph(true);
        refreshList(name);
    }

    private void onDataReloaded(object? sender, EventArgs args)
    {
        if (!initializer.IsInitialized)
            return;
        string? name = currentDocument?.Name ?? functionList.SelectedItem as string;
        disposeGraph(true);
        refreshList(name);
    }

    private void onDocumentDiskReloaded(EditorDocument resource)
    {
        if (resource.Section != "CommonFunctions")
            return;
        if (currentDocument?.ResourceDocument?.Id == resource.Id && initializer.IsInitialized)
        {
            string name = resource.Key;
            disposeGraph(true);
            graphViewStates.Remove(resource.Id);
            refreshList(name);
        }
        else
            graphViewStates.Remove(resource.Id);
        projectSave.NotifyPendingInputsChanged();
    }

    private void flushGraph()
    {
        graphControl?.FlushPendingChanges();
    }

    private void onNodeSearchValidating(object? sender, BlueprintNodeSearchValidationEventArgs args)
    {
        if (!initializer.IsInitialized || graphControl is null)
            return;
        BlueprintNodeDefinitionCatalog catalog = new(args.Metadata, args.Classes);
        catalog.GetNodeDefinitionSet();
    }

    private void onLoadLanguageChanged(object? sender, EventArgs args)
    {
        if (loadingToken == default)
            return;
        cancelGraphLoad();
        Dispatcher.UIThread.Post(showSelectedFunction, DispatcherPriority.Background);
    }

    private void onNodeSearchInvalidated(object? sender, EventArgs args) => cancelGraphLoad();

    private void onNodeSearchChanged(object? sender, EventArgs args)
    {
        if (closed || !initializer.IsInitialized)
            return;
        if (gameData.BlueprintNodeSearch?.Error is string error)
        {
            if (graphControl is null && currentDocument is not null)
                showGraphLoadError(error);
            return;
        }
        flushGraph();
        nodeDefinitionCatalog?.Invalidate();
        pickerIndexes.Clear();
        showSelectedFunction();
    }

    private void disposeGraph(bool discardPendingChanges)
    {
        cancelGraphLoad();
        if (graphControl is not null)
        {
            if (currentDocument?.ResourceDocument is EditorDocument resource)
                graphViewStates[resource.Id] = graphControl.CaptureViewState();
            if (discardPendingChanges)
                graphControl.DiscardPendingChanges();
            else
                graphControl.FlushPendingChanges();
            graphControl.GraphChanged -= onGraphChanged;
            graphHost.Child = null;
            graphControl.Dispose();
            graphControl = null;
        }
        if (currentDocument is not null)
        {
            currentDocument.ExternalChanged -= onDataRestored;
            currentDocument.Dispose();
        }
        currentDocument = null;
        if (initializer.IsInitialized)
            graphHost.Child = null;
    }

    private void onClosed(object? sender, EventArgs args)
    {
        closed = true;
        LocaleService.LanguageChanged -= onLoadLanguageChanged;
        projectSave.UnregisterParticipant(this);
        gameData.Documents.ContentChanged -= onDocumentsChanged;
        gameData.Documents.DiskReloaded -= onDocumentDiskReloaded;
        gameData.DataReloaded -= onDataReloaded;
        if (gameData.BlueprintNodeSearch is BlueprintNodeSearchService search)
        {
            search.Invalidated -= onNodeSearchInvalidated;
            search.Validating -= onNodeSearchValidating;
            search.Changed -= onNodeSearchChanged;
        }
        disposeGraph(false);
    }
}
