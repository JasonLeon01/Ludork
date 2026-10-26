using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Data;
using Avalonia.Interactivity;
using Avalonia.Layout;
using Avalonia.VisualTree;
using Ludork.Controls;
using Ludork.Services;
using Ludork.ViewModels;
using System;
using System.Collections.Generic;
using System.Collections.Specialized;
using System.ComponentModel;
using System.Linq;
using System.Threading.Tasks;

namespace Ludork.Views;

public partial class MainWindow
{
    private readonly Dictionary<FileExplorerTabViewModel, TabItem> explorerTabs = [];
    private FileExplorerWorkspaceViewModel? explorerWorkspace;
    private NativeMenuItem? newExplorerMenu;
    private Button? newExplorerButton;
    private bool updatingExplorerTabs;
    private bool preservingBottomSelection;
    private bool explorerPanelsEnabled = true;

    private void initializeFileExplorerMenu()
    {
        NativeMenu? menu = NativeMenu.GetMenu(this);
        if (menu is null)
            return;
        newExplorerMenu = new NativeMenuItem { Header = LocaleService.Get("FILE_EXPLORER_NEW_TAB"), IsEnabled = false };
        newExplorerMenu.Click += onNewFileExplorerTab;
        NativeMenu view = new();
        view.Items.Add(newExplorerMenu);
        menu.Items.Insert(1, new NativeMenuItem { Header = LocaleService.Get("VIEW_MENU"), Menu = view });
    }

    private void onBottomTabsTemplateApplied(object? sender, TemplateAppliedEventArgs args)
    {
        if (newExplorerButton is not null)
            newExplorerButton.Click -= onNewFileExplorerTab;
        newExplorerButton = args.NameScope.Find<Button>("NewExplorerButton");
        if (newExplorerButton is not null)
        {
            newExplorerButton.Click += onNewFileExplorerTab;
            string label = LocaleService.Get("FILE_EXPLORER_NEW_TAB");
            ToolTip.SetTip(newExplorerButton, label);
            Avalonia.Automation.AutomationProperties.SetName(newExplorerButton, label);
        }
        updateFileExplorerActions();
    }

    private void attachFileExplorers(FileExplorerWorkspaceViewModel? workspace)
    {
        updatingExplorerTabs = true;
        if (explorerWorkspace is not null)
        {
            explorerWorkspace.Tabs.CollectionChanged -= onExplorerTabsChanged;
            explorerWorkspace.PropertyChanged -= onExplorerWorkspaceChanged;
            explorerWorkspace.SetVisible(false);
        }
        foreach (TabItem item in explorerTabs.Values)
        {
            BottomTabs.Items.Remove(item);
            item.Content = null;
        }
        explorerTabs.Clear();
        explorerWorkspace = workspace;
        if (workspace is not null)
        {
            workspace.Tabs.CollectionChanged += onExplorerTabsChanged;
            workspace.PropertyChanged += onExplorerWorkspaceChanged;
        }
        updatingExplorerTabs = false;
        synchronizeExplorerTabs();
        updateFileExplorerActions();
    }

    private void onExplorerTabsChanged(object? sender, NotifyCollectionChangedEventArgs args)
        => synchronizeExplorerTabs();

    private void synchronizeExplorerTabs()
    {
        if (explorerWorkspace is null)
            return;
        bool hadTabs = explorerTabs.Count != 0;
        bool wasExplorer = BottomTabs.SelectedItem is TabItem { DataContext: FileExplorerTabViewModel };
        updatingExplorerTabs = true;
        foreach (FileExplorerTabViewModel tab in explorerTabs.Keys.Where(tab => !explorerWorkspace.Tabs.Contains(tab)).ToArray())
        {
            TabItem item = explorerTabs[tab];
            BottomTabs.Items.Remove(item);
            item.Content = null;
            explorerTabs.Remove(tab);
        }
        for (int index = 0; index < explorerWorkspace.Tabs.Count; index++)
        {
            FileExplorerTabViewModel tab = explorerWorkspace.Tabs[index];
            if (!explorerTabs.TryGetValue(tab, out TabItem? item))
            {
                item = createExplorerTab(tab);
                explorerTabs.Add(tab, item);
                BottomTabs.Items.Insert(index, item);
            }
        }
        updatingExplorerTabs = false;
        if (!hadTabs || wasExplorer)
            selectActiveExplorerTab();
    }

    private TabItem createExplorerTab(FileExplorerTabViewModel tab)
    {
        FileExplorerPanel panel = new() { DataContext = tab.Explorer, IsEnabled = explorerPanelsEnabled };
        panel.Classes.Add("surface");
        Grid header = new() { ColumnDefinitions = new ColumnDefinitions("Auto,Auto"), ColumnSpacing = 6 };
        TextBlock title = new() { VerticalAlignment = VerticalAlignment.Center };
        title.Bind(TextBlock.TextProperty, new Binding(nameof(FileExplorerTabViewModel.Title)) { Source = tab });
        header.Children.Add(title);
        if (tab.CanClose)
        {
            Button close = new()
            {
                Content = "×",
                Padding = new Thickness(3, 0),
                MinWidth = 18,
                MinHeight = 18,
                Height = 20,
                FontSize = 16,
                Focusable = false,
                VerticalAlignment = VerticalAlignment.Center,
            };
            close.Classes.Add("toolbar");
            ToolTip.SetTip(close, LocaleService.Get("CLOSE"));
            Avalonia.Automation.AutomationProperties.SetName(close, LocaleService.Get("CLOSE"));
            close.Click += (_, args) =>
            {
                args.Handled = true;
                preservingBottomSelection = BottomTabs.SelectedItem is not TabItem selected
                    || !ReferenceEquals(selected.DataContext, tab);
                try
                {
                    explorerWorkspace?.CloseTab(tab);
                }
                finally
                {
                    preservingBottomSelection = false;
                }
            };
            Grid.SetColumn(close, 1);
            header.Children.Add(close);
        }
        TabItem item = new() { Header = header, Content = panel, DataContext = tab };
        item.Bind(ToolTip.TipProperty, new Binding(nameof(FileExplorerTabViewModel.RelativePath)) { Source = tab });
        return item;
    }

    private void onNewFileExplorerTab(object? sender, EventArgs args)
    {
        if (explorerPanelsEnabled)
            explorerWorkspace?.AddTab();
    }

    private void onExplorerWorkspaceChanged(object? sender, PropertyChangedEventArgs args)
    {
        if (!preservingBottomSelection && args.PropertyName == nameof(FileExplorerWorkspaceViewModel.ActiveTab))
            selectActiveExplorerTab();
    }

    private void selectActiveExplorerTab()
    {
        if (explorerWorkspace is null || !explorerTabs.TryGetValue(explorerWorkspace.ActiveTab, out TabItem? item))
            return;
        BottomTabs.SelectedItem = item;
        explorerWorkspace.SetVisible(true);
        item.BringIntoView();
    }

    private void onBottomTabSelectionChanged(object? sender, SelectionChangedEventArgs args)
    {
        if (updatingExplorerTabs || explorerWorkspace is null || !ReferenceEquals(args.Source, BottomTabs))
            return;
        if (BottomTabs.SelectedItem is TabItem { DataContext: FileExplorerTabViewModel tab })
        {
            explorerWorkspace.ActivateTab(tab);
            explorerWorkspace.SetVisible(true);
        }
        else
            explorerWorkspace.SetVisible(false);
    }

    private void setFileExplorersEnabled(bool enabled)
    {
        explorerPanelsEnabled = enabled;
        foreach (TabItem item in explorerTabs.Values)
        {
            if (item.Content is FileExplorerPanel panel)
                panel.IsEnabled = enabled;
        }
        updateFileExplorerActions();
    }

    private void updateFileExplorerActions()
    {
        bool enabled = explorerWorkspace is not null && explorerPanelsEnabled;
        if (newExplorerMenu is not null)
            newExplorerMenu.IsEnabled = enabled;
        if (newExplorerButton is not null)
            newExplorerButton.IsEnabled = enabled;
    }

    private async Task locateExplorerPathAsync(string path)
    {
        if (explorerWorkspace is null)
            return;
        selectActiveExplorerTab();
        if (explorerTabs[explorerWorkspace.ActiveTab].Content is FileExplorerPanel panel)
            await panel.LocatePathAsync(path);
    }
}
