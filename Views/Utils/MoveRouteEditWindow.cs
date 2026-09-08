using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Layout;
using Avalonia.Media;
using Ludork.Models;
using Ludork.Services;
using System;
using System.Linq;
using System.Text.Json.Nodes;
using System.Threading.Tasks;

namespace Ludork.Views.Utils;

internal sealed class MoveRouteEditWindow : Window
{
    private readonly GameDataService gameData;
    private readonly ListBox mapList;
    private readonly MoveRouteMapReferenceView mapView;
    private readonly TextBlock routeLabel;

    private MoveRouteEditWindow(
        GameDataService gameData,
        JsonNode? initial)
    {
        this.gameData = gameData;
        Title = LocaleService.Get("MOVE_ROUTE_EDITOR_TITLE");
        Width = 960;
        Height = 615;
        MinWidth = 825;
        MinHeight = 540;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        EditorWindowIcon.Apply(this);

        mapList = new ListBox { MinWidth = 180 };
        mapView = new MoveRouteMapReferenceView(gameData);
        routeLabel = new TextBlock();
        mapList.SelectionChanged += (_, _) => selectMap();
        mapView.RouteChanged += (_, _) => refreshRouteLabel();

        TextBlock hint = new()
        {
            Text = LocaleService.Get("MOVE_ROUTE_EDITOR_HINT"),
            TextWrapping = TextWrapping.Wrap,
        };
        Control mapArea = createMapArea(mapList, mapView);
        Button clearButton = new() { Content = LocaleService.Get("MOVE_ROUTE_CLEAR") };
        Button confirmButton = new() { Content = LocaleService.Get("CONFIRM"), MinWidth = 80 };
        Button cancelButton = new() { Content = LocaleService.Get("CANCEL"), MinWidth = 80 };
        clearButton.Click += (_, _) => mapView.ClearRoute();
        confirmButton.Click += (_, _) => Close(mapView.GetRoute());
        cancelButton.Click += (_, _) => Close(null);

        Grid root = new()
        {
            Margin = new Thickness(12),
            RowDefinitions = new RowDefinitions("Auto,8,*,8,Auto,8,Auto"),
        };
        root.Children.Add(hint);
        Grid.SetRow(mapArea, 2);
        root.Children.Add(mapArea);
        Grid.SetRow(routeLabel, 4);
        root.Children.Add(routeLabel);
        Control actions = createActions(clearButton, confirmButton, cancelButton);
        Grid.SetRow(actions, 6);
        root.Children.Add(actions);
        Content = root;

        KeyDown += onKeyDown;
        Closed += (_, _) => mapView.Dispose();
        loadMaps(string.Empty);
        mapView.SetRoute(initial);
        refreshRouteLabel();
    }

    public static Task<JsonArray?> ShowAsync(
        Window owner,
        GameDataService gameData,
        JsonNode? initial)
    {
        MoveRouteEditWindow window = new(gameData, initial);
        return window.ShowDialog<JsonArray?>(owner);
    }

    private void loadMaps(string preferredKey)
    {
        ListBoxItem? preferred = null;
        foreach (MapCatalogEntry entry in gameData.MapCatalog
                     .Where(item => item.Kind != MapCatalogEntryKind.WorldMap)
                     .OrderBy(item => item.Key, StringComparer.Ordinal))
        {
            string key = entry.Key;
            ListBoxItem item = new()
            {
                Content = new HintedTextPresenter
                {
                    Text = entry.DisplayName,
                },
                Tag = key,
            };
            ToolTip.SetTip(item, key);
            mapList.Items.Add(item);
            if (string.Equals(key, preferredKey, StringComparison.Ordinal))
                preferred = item;
        }
        mapList.SelectedItem = preferred ?? mapList.Items.OfType<ListBoxItem>().FirstOrDefault();
    }

    private void selectMap()
    {
        if (mapList.SelectedItem is not ListBoxItem item || item.Tag is not string key)
        {
            mapView.SetMap(null, null);
            return;
        }
        mapView.SetMap(key, gameData.ReadMapSnapshot(key));
    }

    private void refreshRouteLabel()
    {
        routeLabel.Text = BlueprintNodeParameterValues.FormatRoute(mapView.GetRoute());
    }

    private void onKeyDown(object? sender, KeyEventArgs args)
    {
        if (args.Key != Key.Escape)
            return;
        Close(null);
        args.Handled = true;
    }

    internal static Control createMapArea(ListBox mapList, MapReferenceView mapView)
    {
        ScrollViewer scroll = new()
        {
            Content = mapView,
            HorizontalScrollBarVisibility = Avalonia.Controls.Primitives.ScrollBarVisibility.Auto,
            VerticalScrollBarVisibility = Avalonia.Controls.Primitives.ScrollBarVisibility.Auto,
            HorizontalContentAlignment = HorizontalAlignment.Stretch,
            VerticalContentAlignment = VerticalAlignment.Stretch,
        };
        GridSplitter splitter = new()
        {
            Width = 6,
            ResizeDirection = GridResizeDirection.Columns,
        };
        Grid area = new() { ColumnDefinitions = new ColumnDefinitions("180,6,*") };
        area.Children.Add(mapList);
        Grid.SetColumn(splitter, 1);
        area.Children.Add(splitter);
        Grid.SetColumn(scroll, 2);
        area.Children.Add(scroll);
        return area;
    }

    internal static Control createActions(Button clear, Button confirm, Button cancel)
    {
        Grid actions = new() { ColumnDefinitions = new ColumnDefinitions("Auto,*,Auto,8,Auto") };
        actions.Children.Add(clear);
        Grid.SetColumn(confirm, 2);
        actions.Children.Add(confirm);
        Grid.SetColumn(cancel, 4);
        actions.Children.Add(cancel);
        return actions;
    }
}
