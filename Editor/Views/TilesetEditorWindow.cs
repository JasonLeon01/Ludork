using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Controls.Templates;
using Avalonia.Threading;
using Ludork.Controls;
using Ludork.Services;
using Ludork.ViewModels;
using Ludork.Views.Utils;
using System;
using System.Collections.Generic;

namespace Ludork.Views;

public sealed class TilesetEditorWindow : Window
{
    private readonly GameDataService gameData;
    private readonly ProjectSaveService projectSave;
    private readonly TileSelectViewModel tileSelect;
    private readonly List<TilesetEditorTab> editorTabs = [];
    private readonly HashSet<TabItem> initializedTabs = [];
    private readonly HashSet<TabItem> pendingTabs = [];
    private readonly DeferredWindowInitializer initializer;
    private readonly Toast toast;
    private readonly EditorDocumentBinding documentBinding;
    private TabControl? tabControl;
    private TabItem? tilesetItem;
    private TabItem? autoTileItem;
    private TilesetEditorTab? tilesetTab;
    private TilesetEditorTab? autoTileTab;
    private string? pendingTilesetKey;
    private string? pendingAutoTileKey;
    private bool pendingAutoTilePage;
    private bool closed;

    public TilesetEditorWindow(
        GameDataService gameData,
        ProjectSaveService projectSave,
        TileSelectViewModel tileSelect)
    {
        this.gameData = gameData;
        this.projectSave = projectSave;
        this.tileSelect = tileSelect;
        Title = LocaleService.Get("TILESETS_DATA");
        Width = 560;
        Height = 480;
        MinWidth = 560;
        MinHeight = 480;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        Background = Ludork.Services.EditorTheme.Brush("Surface");
        FontFamily = Ludork.Services.EditorTheme.FontFamily;
        EditorWindowIcon.Apply(this);
        HistoryMergeBehavior.AttachBoundary(this, gameData);

        Content = DeferredWindowInitializer.CreateLoadingContent();
        initializer = new DeferredWindowInitializer(this, async cancellationToken =>
        {
            initializeContent();
            await EditorUiBatch.YieldAsync(cancellationToken);
        });
        toast = new Toast(this);
        documentBinding = new EditorDocumentBinding(this, gameData,
            () => ReferenceEquals(tabControl?.SelectedItem, autoTileItem) ? autoTileTab?.Document : tilesetTab?.Document,
            () => LocaleService.Get("TILESETS_DATA"));
        gameData.Documents.Changed += onDataRestored;
        Closed += (_, _) =>
        {
            closed = true;
            gameData.Documents.Changed -= onDataRestored;
        };
        AddHandler(KeyDownEvent, onKeyDown, RoutingStrategies.Tunnel);
    }

    public void NavigateTo(bool isAutoTile, string? key = null)
    {
        pendingAutoTilePage = isAutoTile;
        if (key is not null)
        {
            if (isAutoTile)
                pendingAutoTileKey = key;
            else
                pendingTilesetKey = key;
        }
        if (initializer.IsInitialized)
            navigateToPendingPage();
    }

    private void initializeContent()
    {
        tabControl = new TabControl
        {
            ItemsPanel = new FuncTemplate<Panel?>(() => new StackPanel { Orientation = Orientation.Horizontal }),
        };
        tilesetItem = new TabItem
        {
            Header = LocaleService.Get("TILESETS_DATA"),
            Content = DeferredWindowInitializer.CreateLoadingContent(),
        };
        autoTileItem = new TabItem
        {
            Header = LocaleService.Get("AUTOTILES_DATA"),
            Content = DeferredWindowInitializer.CreateLoadingContent(),
        };
        tabControl.Items.Add(tilesetItem);
        tabControl.Items.Add(autoTileItem);
        tabControl.SelectionChanged += (_, _) =>
        {
            initializeTab(tabControl.SelectedItem as TabItem);
            documentBinding.Refresh();
        };
        Content = new Border { Padding = new Thickness(5), Child = tabControl };
        navigateToPendingPage();
    }

    private void navigateToPendingPage()
    {
        TabItem? item = pendingAutoTilePage ? autoTileItem : tilesetItem;
        if (tabControl is null || item is null)
            return;
        tabControl.SelectedItem = item;
        initializeTab(item);
        applyPendingSelection(pendingAutoTilePage);
    }

    private void initializeTab(TabItem? item)
    {
        if (item is null || initializedTabs.Contains(item) || !pendingTabs.Add(item))
            return;
        Dispatcher.UIThread.Post(() =>
        {
            pendingTabs.Remove(item);
            if (closed || initializedTabs.Contains(item))
                return;
            bool isAutoTile = ReferenceEquals(item, autoTileItem);
            TilesetEditorTab tab = new(this, gameData, tileSelect, isAutoTile);
            tab.SelectionChanged += (_, _) => documentBinding.Refresh();
            if (isAutoTile)
                autoTileTab = tab;
            else
                tilesetTab = tab;
            editorTabs.Add(tab);
            initializedTabs.Add(item);
            item.Content = tab;
            applyPendingSelection(isAutoTile);
            documentBinding.Refresh();
        }, DispatcherPriority.Background);
    }

    private void applyPendingSelection(bool isAutoTile)
    {
        TilesetEditorTab? tab = isAutoTile ? autoTileTab : tilesetTab;
        string? key = isAutoTile ? pendingAutoTileKey : pendingTilesetKey;
        if (tab is null || key is null)
            return;
        if (isAutoTile)
            pendingAutoTileKey = null;
        else
            pendingTilesetKey = null;
        tab.SelectData(key);
    }

    private void onDataRestored(object? sender, EventArgs args)
    {
        foreach (TilesetEditorTab tab in editorTabs)
            tab.RefreshDocumentList();
    }

    private async void onKeyDown(object? sender, KeyEventArgs args)
    {
        if (!EditorShortcuts.HasPrimaryModifier(args.KeyModifiers))
            return;
        if (args.Key == Key.Z)
            EditorFeedback.ShowHistory(toast, "Undo", documentBinding.Undo());
        else if (args.Key == Key.Y)
            EditorFeedback.ShowHistory(toast, "Redo", documentBinding.Redo());
        else if (args.Key == Key.S)
            await EditorSaveWorkflow.TrySaveAsync(this, projectSave);
        else
            return;
        args.Handled = true;
    }
}
