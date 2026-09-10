using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Controls.Templates;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Threading;
using Ludork.Models;
using Ludork.Plugin.Avalonia;
using Ludork.Services;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;
using System.Threading.Tasks;

namespace Ludork.Controls;

public sealed class WorldMapPanel : Grid, IDisposable
{
    private readonly ObservableCollection<WorldMapChildListItem> childItems = [];
    private readonly ListBox childList;
    private readonly TextBlock emptyText;
    private readonly TextBlock statusText;
    private readonly WorldMapCanvas canvas;
    private WorldMapPreviewRenderer? renderer;
    private bool disposed;

    public WorldMapPanel()
    {
        ColumnDefinitions = new ColumnDefinitions("220,4,*");
        Background = new SolidColorBrush(Color.Parse("#121212"));

        TextBlock childTitle = new()
        {
            Height = 32,
            FontSize = 16,
            FontWeight = FontWeight.Bold,
            HorizontalAlignment = HorizontalAlignment.Center,
            VerticalAlignment = VerticalAlignment.Center,
            Text = LocaleService.Get("WORLD_CHILD_MAPS"),
        };
        childList = new ListBox
        {
            ItemsSource = childItems,
            SelectionMode = SelectionMode.Single,
            Background = new SolidColorBrush(Color.Parse("#1f1f1f")),
            ItemTemplate = new FuncDataTemplate<WorldMapChildListItem>(createChildItem),
        };
        emptyText = new TextBlock
        {
            Margin = new Thickness(12),
            Text = LocaleService.Get("WORLD_NO_CHILD_MAPS"),
            TextWrapping = TextWrapping.Wrap,
            HorizontalAlignment = HorizontalAlignment.Center,
            VerticalAlignment = VerticalAlignment.Center,
        };
        Grid listArea = new() { RowDefinitions = new RowDefinitions("32,*") };
        listArea.Children.Add(childTitle);
        Grid listBody = new();
        listBody.Children.Add(childList);
        listBody.Children.Add(emptyText);
        Grid.SetRow(listBody, 1);
        listArea.Children.Add(listBody);
        Children.Add(listArea);

        GridSplitter splitter = new() { ResizeDirection = GridResizeDirection.Columns };
        Grid.SetColumn(splitter, 1);
        Children.Add(splitter);

        canvas = new WorldMapCanvas();
        statusText = new TextBlock
        {
            Margin = new Thickness(8, 4),
            Padding = new Thickness(8, 4),
            Background = new SolidColorBrush(Color.FromArgb(180, 25, 25, 25)),
            HorizontalAlignment = HorizontalAlignment.Left,
            VerticalAlignment = VerticalAlignment.Bottom,
        };
        Grid previewArea = new()
        {
            RowDefinitions = new RowDefinitions("*,Auto"),
        };
        ScrollViewer canvasScroll = new()
        {
            HorizontalScrollBarVisibility = ScrollBarVisibility.Auto,
            VerticalScrollBarVisibility = ScrollBarVisibility.Auto,
            HorizontalContentAlignment = HorizontalAlignment.Stretch,
            VerticalContentAlignment = VerticalAlignment.Stretch,
            Content = canvas,
        };
        previewArea.Children.Add(canvasScroll);
        Grid.SetRow(statusText, 1);
        previewArea.Children.Add(statusText);
        Grid.SetColumn(previewArea, 2);
        Children.Add(previewArea);

        canvas.PlacementChanged += (_, args) => PlacementChanged?.Invoke(this, args);
        canvas.PlacementRemoved += (_, args) => PlacementRemoved?.Invoke(this, args);
        canvas.PointerMoved += (_, args) => updateStatus(args.GetPosition(canvas));
        Unloaded += (_, _) => Dispose();
    }

    public event EventHandler<WorldMapPlacementChangedEventArgs>? PlacementChanged;
    public event EventHandler<WorldMapPlacementRemovedEventArgs>? PlacementRemoved;
    public event EventHandler<string>? ChildMapOpenRequested;

    public void Configure(
        GameDataService gameData,
        BlueprintPreviewService previewService)
    {
        renderer?.Dispose();
        renderer = new WorldMapPreviewRenderer(gameData, previewService);
        canvas.Configure(renderer);
    }

    public void SetWorld(
        string? worldKey,
        JsonObject? manifest,
        IReadOnlyList<WorldMapChildSource> children)
    {
        IReadOnlyDictionary<string, (int X, int Y)> positions = getPlacementPositions(worldKey, manifest);
        List<WorldMapChildListItem> nextItems = [];
        foreach (WorldMapChildSource child in children.OrderBy(item => item.Key, StringComparer.Ordinal))
        {
            positions.TryGetValue(child.Key, out (int X, int Y) position);
            bool placed = positions.ContainsKey(child.Key);
            nextItems.Add(new WorldMapChildListItem(
                child,
                placed,
                placed
                    ? string.Format(CultureInfo.CurrentCulture, LocaleService.Get("WORLD_PLACED_AT"), position.X, position.Y)
                    : LocaleService.Get("WORLD_NOT_PLACED")));
        }
        if (childItems.Count == nextItems.Count
            && childItems.Select(item => item.Source.Key)
                .SequenceEqual(nextItems.Select(item => item.Source.Key), StringComparer.Ordinal))
        {
            for (int index = 0; index < nextItems.Count; index += 1)
            {
                WorldMapChildListItem current = childItems[index];
                WorldMapChildListItem next = nextItems[index];
                if (current.IsPlaced != next.IsPlaced
                    || !string.Equals(current.PlacementText, next.PlacementText, StringComparison.Ordinal)
                    || current.Source.Width != next.Source.Width
                    || current.Source.Height != next.Source.Height
                    || !string.Equals(current.Source.DisplayName, next.Source.DisplayName, StringComparison.Ordinal)
                    || !current.Source.LayerOrder.SequenceEqual(next.Source.LayerOrder, StringComparer.Ordinal))
                {
                    childItems[index] = next;
                }
            }
        }
        else
        {
            childItems.Clear();
            foreach (WorldMapChildListItem item in nextItems)
                childItems.Add(item);
        }
        emptyText.IsVisible = childItems.Count == 0;
        childList.IsVisible = childItems.Count != 0;
        canvas.SetWorld(worldKey, manifest, children);
        statusText.Text = getWorldSizeText(manifest);
    }

    public void Dispose()
    {
        if (disposed)
            return;
        disposed = true;
        renderer?.Dispose();
        renderer = null;
        canvas.Dispose();
    }

    private Control? createChildItem(WorldMapChildListItem? item, INameScope? scope)
    {
        if (item is null)
            return null;
        WorldMapThumbnail thumbnail = new(renderer, item.Source)
        {
            Width = 176,
            Height = 108,
            HorizontalAlignment = HorizontalAlignment.Stretch,
        };
        TextBlock name = new()
        {
            Text = item.Source.DisplayName,
            FontWeight = FontWeight.Bold,
            TextTrimming = TextTrimming.CharacterEllipsis,
        };
        ToolTip.SetTip(name, item.Source.Key);
        TextBlock placement = new()
        {
            Text = item.PlacementText,
            FontSize = 11,
            Foreground = new SolidColorBrush(item.IsPlaced ? Color.Parse("#8fd694") : Color.Parse("#b0b0b0")),
        };
        StackPanel content = new()
        {
            Spacing = 4,
            Margin = new Thickness(4),
            Children = { thumbnail, name, placement },
        };
        Border border = new()
        {
            Padding = new Thickness(4),
            Margin = new Thickness(2),
            BorderBrush = new SolidColorBrush(item.IsPlaced ? Color.Parse("#547f58") : Color.Parse("#3a3a3a")),
            BorderThickness = new Thickness(1),
            Background = new SolidColorBrush(Color.Parse("#292929")),
            Child = content,
        };
        PointerPressedEventArgs? dragPress = null;
        Point dragStart = default;
        border.PointerPressed += (_, args) =>
        {
            if (!args.GetCurrentPoint(border).Properties.IsLeftButtonPressed)
                return;
            dragPress = args;
            dragStart = args.GetPosition(border);
        };
        border.PointerMoved += async (_, args) =>
        {
            if (dragPress is null || !args.GetCurrentPoint(border).Properties.IsLeftButtonPressed)
                return;
            Point current = args.GetPosition(border);
            if (Math.Abs(current.X - dragStart.X) + Math.Abs(current.Y - dragStart.Y) < 8)
                return;
            PointerPressedEventArgs press = dragPress;
            dragPress = null;
            DataTransfer data = new();
            data.Add(DataTransferItem.CreateText(WorldMapCanvas.ChildDragPrefix + item.Source.Key));
            await DragDrop.DoDragDropAsync(press, data, DragDropEffects.Copy);
        };
        border.PointerReleased += (_, _) => dragPress = null;
        border.DoubleTapped += (_, args) =>
        {
            ChildMapOpenRequested?.Invoke(this, item.Source.Key);
            args.Handled = true;
        };
        return border;
    }

    private void updateStatus(Point point)
    {
        if (canvas.WorldCellAt(point) is not { } cell)
        {
            statusText.Text = getWorldSizeText(canvas.Manifest);
            return;
        }
        statusText.Text = string.Format(
            CultureInfo.CurrentCulture,
            LocaleService.Get("WORLD_CURSOR_POSITION"),
            cell.X,
            cell.Y,
            canvas.WorldWidth,
            canvas.WorldHeight);
    }

    private static string getWorldSizeText(JsonObject? manifest)
    {
        int width = getInt(manifest?["width"]);
        int height = getInt(manifest?["height"]);
        string localeKey = EditorZoomInput.IsMacOS
            ? "WORLD_SIZE_STATUS_MAC"
            : "WORLD_SIZE_STATUS";
        return string.Format(CultureInfo.CurrentCulture, LocaleService.Get(localeKey), width, height);
    }

    private static IReadOnlyDictionary<string, (int X, int Y)> getPlacementPositions(
        string? worldKey,
        JsonObject? manifest)
    {
        Dictionary<string, (int X, int Y)> result = new(StringComparer.Ordinal);
        if (string.IsNullOrWhiteSpace(worldKey) || manifest?["placements"] is not JsonArray placements)
            return result;
        foreach (JsonNode? node in placements)
        {
            if (node is not JsonObject placement
                || getString(placement["map"]) is not string childPath
                || placement["rect"] is not JsonArray { Count: >= 2 } rect)
            {
                continue;
            }
            string childKey = combineChildKey(worldKey, childPath);
            result[childKey] = (getInt(rect[0]), getInt(rect[1]));
        }
        return result;
    }

    internal static string combineChildKey(string worldKey, string childPath)
    {
        string childName = childPath.Replace('\\', '/').Trim('/');
        if (childName.EndsWith(".json", StringComparison.OrdinalIgnoreCase))
            childName = childName[..^5];
        return worldKey.Trim('/') + "/" + childName;
    }

    private static int getInt(JsonNode? value)
    {
        return value?.GetValue<int?>() ?? 0;
    }

    private static string? getString(JsonNode? value)
    {
        return value is JsonValue scalar && scalar.TryGetValue(out string? text) ? text : null;
    }

    private sealed record WorldMapChildListItem(
        WorldMapChildSource Source,
        bool IsPlaced,
        string PlacementText);
}
