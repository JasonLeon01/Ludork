using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Media;
using Avalonia.VisualTree;
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
using System.Threading.Tasks;

namespace Ludork.Views;

public sealed class CommonFunctionWindow : Window
{
    private static string? clipboardName;
    private static JsonObject? clipboardData;
    private readonly GameDataService gameData;
    private readonly ProjectSaveService projectSave;
    private readonly BlueprintVariableFieldBuilder fieldBuilder;
    private readonly BlueprintNodeParameterEditorFactory parameterEditorFactory;
    private readonly BlueprintNodeDefinitionCatalog nodeDefinitionCatalog;
    private readonly ListBox functionList;
    private readonly Border graphHost;
    private readonly Toast toast;
    private CommonFunctionEditorDocument? currentDocument;
    private BlueprintGraphControl? graphControl;
    private bool refreshing;

    public CommonFunctionWindow(
        GameDataService gameData,
        ProjectSaveService projectSave,
        LuaMetadataService metadataService,
        BlueprintClassResolver classResolver)
    {
        this.gameData = gameData;
        this.projectSave = projectSave;
        fieldBuilder = new BlueprintVariableFieldBuilder(gameData, metadataService);
        parameterEditorFactory = new BlueprintNodeParameterEditorFactory(
            gameData,
            metadataService,
            classResolver);
        nodeDefinitionCatalog = BlueprintNodeDefinitionCatalog.CreateGlobal(
            metadataService,
            classResolver);

        Title = LocaleService.Get("COMMON_FUNCTIONS");
        Width = 1200;
        Height = 800;
        MinWidth = 1200;
        MinHeight = 800;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        Background = new SolidColorBrush(Color.Parse("#1e1e1e"));
        FontFamily = FontFamily.Parse("avares://Ludork/Assets/HarmonyOS_Sans_SC_Regular.ttf#HarmonyOS Sans SC");
        EditorWindowIcon.Apply(this);

        functionList = new ListBox
        {
            MinWidth = 200,
            MaxWidth = 240,
            Width = 220,
            SelectionMode = SelectionMode.Single,
            HorizontalAlignment = Avalonia.Layout.HorizontalAlignment.Stretch,
            VerticalAlignment = Avalonia.Layout.VerticalAlignment.Stretch,
            ItemTemplate = HintedTextPresenter.StringItemTemplate,
        };
        functionList.SelectionChanged += onSelectionChanged;
        functionList.AddHandler(
            PointerPressedEvent,
            onListPointerPressed,
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
        Content = root;

        toast = new Toast(this);
        gameData.DataRestored += onDataRestored;
        gameData.DataReloaded += onDataReloaded;
        Closed += onClosed;
        Deactivated += (_, _) => flushGraph();
        AddHandler(KeyDownEvent, onKeyDown, RoutingStrategies.Tunnel);
        refreshList(null);
    }

    public void FlushPendingChanges()
    {
        flushGraph();
    }

    private void onSelectionChanged(object? sender, SelectionChangedEventArgs args)
    {
        if (!refreshing)
            showSelectedFunction();
    }

    private void showSelectedFunction()
    {
        disposeGraph(false);
        if (functionList.SelectedItem is not string name)
            return;
        currentDocument = CommonFunctionEditorDocument.Create(gameData, name);
        if (currentDocument is null)
            return;
        JsonObject eventGraph = currentDocument.GetEventGraph();
        JsonObject startNodes = currentDocument.GetStartNodes();
        IReadOnlyList<BlueprintGraphNodeDefinition> definitions =
            nodeDefinitionCatalog.GetNodeDefinitions();
        BlueprintGraphDocument graphDocument = BlueprintGraphCodec.Load(
            "common",
            eventGraph,
            startNodes["common"],
            definitions,
            []);
        graphControl = new BlueprintGraphControl(
            graphDocument,
            definitions,
            fieldBuilder,
            parameterEditorFactory,
            Path.Combine(gameData.ProjectPath, "Assets"),
            gameData.getCellSize());
        graphControl.GraphChanged += onGraphChanged;
        graphHost.Child = graphControl;
    }

    private void onGraphChanged(object? sender, EventArgs args)
    {
        if (graphControl is null || currentDocument is null)
            return;
        BlueprintGraphCodec.SaveInto(
            graphControl.Document,
            currentDocument.GetEventGraph(),
            currentDocument.GetStartNodes());
        currentDocument.CommitGraph();
    }

    private void onListPointerPressed(object? sender, PointerPressedEventArgs args)
    {
        if (!args.GetCurrentPoint(functionList).Properties.IsRightButtonPressed)
            return;
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
        if (hitName is not null)
            functionList.SelectedItem = hitName;
        args.Handled = true;
        showContextMenu(hitName);
    }

    private void showContextMenu(string? name)
    {
        ContextMenu menu = new();
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
        menu.Open(functionList);
    }

    private async Task createFunctionAsync()
    {
        string? name = await SingleRowDialog.ShowAsync(
            this,
            LocaleService.Get("NEW_COMMON_FUNC"),
            LocaleService.Get("ENTER_FUNC_NAME"),
            gameData.CommonFunctionsData.Keys);
        if (string.IsNullOrWhiteSpace(name))
            return;
        flushGraph();
        if (!gameData.CreateCommonFunction(name))
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
            || !gameData.CommonFunctionsData.TryGetValue(name, out JsonObject? data))
        {
            return;
        }
        clipboardName = name;
        clipboardData = (JsonObject)data.DeepClone();
    }

    private void pasteFunction()
    {
        if (clipboardData is null || clipboardName is null)
            return;
        flushGraph();
        string name = clipboardName + " (copy)";
        if (gameData.CommonFunctionsData.ContainsKey(name))
        {
            int index = 1;
            while (gameData.CommonFunctionsData.ContainsKey($"{name}_{index}"))
                index += 1;
            name = $"{name}_{index}";
        }
        if (gameData.CreateCommonFunction(name, clipboardData))
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
            gameData.CommonFunctionsData.Keys.Where(name =>
                !string.Equals(name, oldName, StringComparison.Ordinal)),
            oldName);
        if (string.IsNullOrWhiteSpace(newName)
            || string.Equals(oldName, newName.Trim(), StringComparison.Ordinal))
        {
            return;
        }
        flushGraph();
        if (!gameData.RenameCommonFunction(oldName, newName))
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
            .Replace("{name}", name, StringComparison.Ordinal);
        bool confirmed = await ConfirmationDialog.ShowAsync(
            this,
            LocaleService.Get("CONFIRM_DELETE"),
            message);
        if (!confirmed)
            return;
        flushGraph();
        if (gameData.DeleteCommonFunction(name))
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

    private void refreshList(string? preferredName)
    {
        string? selectedName = preferredName ?? functionList.SelectedItem as string;
        string[] names = gameData.CommonFunctionsData.Keys
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

    private void onDataRestored(object? sender, EventArgs args)
    {
        disposeGraph(true);
        refreshList(functionList.SelectedItem as string);
    }

    private void onDataReloaded(object? sender, EventArgs args)
    {
        disposeGraph(true);
        refreshList(functionList.SelectedItem as string);
    }

    private void flushGraph()
    {
        graphControl?.FlushPendingChanges();
    }

    private void disposeGraph(bool discardPendingChanges)
    {
        if (graphControl is null)
        {
            currentDocument = null;
            graphHost.Child = null;
            return;
        }
        if (discardPendingChanges)
            graphControl.DiscardPendingChanges();
        else
            graphControl.FlushPendingChanges();
        graphControl.GraphChanged -= onGraphChanged;
        graphHost.Child = null;
        graphControl.Dispose();
        graphControl = null;
        currentDocument = null;
    }

    private void onClosed(object? sender, EventArgs args)
    {
        gameData.DataRestored -= onDataRestored;
        gameData.DataReloaded -= onDataReloaded;
        disposeGraph(false);
    }
}
