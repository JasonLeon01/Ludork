using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Layout;
using Ludork.Controls;
using Ludork.Models;
using Ludork.Services;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;
using System.Threading.Tasks;

namespace Ludork.Views.Utils;

internal sealed record MapTargetPickerResult(string RuntimePath, JsonArray? Position);

internal sealed class MapTargetPickerWindow : Window
{
    private readonly GameDataService gameData;
    private readonly bool pickPosition;
    private readonly TreeView mapTree;
    private readonly TransferPositionMapReferenceView mapView;
    private readonly WorldMapCanvas worldView;
    private readonly ScrollViewer worldScroll;
    private readonly TextBlock positionLabel;
    private readonly Button confirmButton;
    private readonly JsonNode? initialPosition;
    private MapTargetItem? selectedTarget;

    private MapTargetPickerWindow(
        GameDataService gameData,
        string preferredRuntimePath,
        JsonNode? position,
        bool pickPosition)
    {
        this.gameData = gameData;
        this.pickPosition = pickPosition;
        initialPosition = position?.DeepClone();
        Title = pickPosition
            ? LocaleService.Get("TRANSFER_POS_EDITOR_TITLE")
            : LocaleService.Get("SELECT_FILE");
        Width = 960;
        Height = 615;
        MinWidth = 825;
        MinHeight = 540;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        EditorWindowIcon.Apply(this);

        mapTree = new TreeView { MinWidth = 210 };
        mapTree.SelectionChanged += (_, _) => selectTarget();
        mapView = new TransferPositionMapReferenceView(gameData);
        mapView.PositionChanged += (_, _) => refreshPositionLabel();
        worldView = new WorldMapCanvas
        {
            PlacementEditingEnabled = false,
        };
        worldView.Configure(gameData);
        worldView.WorldCellSelected += (_, _) => refreshPositionLabel();
        positionLabel = new TextBlock { IsVisible = pickPosition };

        ScrollViewer mapScroll = new()
        {
            HorizontalScrollBarVisibility = Avalonia.Controls.Primitives.ScrollBarVisibility.Auto,
            VerticalScrollBarVisibility = Avalonia.Controls.Primitives.ScrollBarVisibility.Auto,
            Content = mapView,
        };
        worldScroll = new ScrollViewer
        {
            HorizontalScrollBarVisibility = Avalonia.Controls.Primitives.ScrollBarVisibility.Auto,
            VerticalScrollBarVisibility = Avalonia.Controls.Primitives.ScrollBarVisibility.Auto,
            HorizontalContentAlignment = HorizontalAlignment.Stretch,
            VerticalContentAlignment = VerticalAlignment.Stretch,
            Content = worldView,
            IsVisible = false,
        };
        Grid previewHost = new();
        previewHost.Children.Add(mapScroll);
        previewHost.Children.Add(worldScroll);
        Grid mapArea = new()
        {
            ColumnDefinitions = new ColumnDefinitions("210,8,*"),
        };
        mapArea.Children.Add(mapTree);
        Grid.SetColumn(previewHost, 2);
        mapArea.Children.Add(previewHost);

        TextBlock hint = new()
        {
            Text = pickPosition
                ? LocaleService.Get("TRANSFER_POS_EDITOR_HINT")
                : LocaleService.Get("SELECT_FILE"),
            TextWrapping = Avalonia.Media.TextWrapping.Wrap,
        };
        Button clearButton = new()
        {
            Content = LocaleService.Get("TRANSFER_POS_CLEAR"),
            IsVisible = pickPosition,
        };
        confirmButton = new()
        {
            Content = LocaleService.Get("CONFIRM"),
            MinWidth = 80,
            IsEnabled = false,
        };
        Button cancelButton = new()
        {
            Content = LocaleService.Get("CANCEL"),
            MinWidth = 80,
        };
        clearButton.Click += (_, _) => clearPosition();
        confirmButton.Click += (_, _) => confirm();
        cancelButton.Click += (_, _) => Close(null);

        Grid root = new()
        {
            Margin = new Thickness(12),
            RowDefinitions = new RowDefinitions("Auto,8,*,8,Auto,8,Auto"),
        };
        root.Children.Add(hint);
        Grid.SetRow(mapArea, 2);
        root.Children.Add(mapArea);
        Grid.SetRow(positionLabel, 4);
        root.Children.Add(positionLabel);
        Control actions = MoveRouteEditWindow.createActions(clearButton, confirmButton, cancelButton);
        Grid.SetRow(actions, 6);
        root.Children.Add(actions);
        Content = root;

        KeyDown += onKeyDown;
        Closed += (_, _) =>
        {
            mapView.Dispose();
            worldView.Dispose();
        };
        loadTargets(normalizeRuntimePath(preferredRuntimePath));
        refreshPositionLabel();
    }

    public static Task<MapTargetPickerResult?> ShowPositionAsync(
        Window owner,
        GameDataService gameData,
        string preferredRuntimePath,
        JsonNode? position)
    {
        MapTargetPickerWindow window = new(
            gameData,
            preferredRuntimePath,
            position,
            true);
        return window.ShowDialog<MapTargetPickerResult?>(owner);
    }

    public static async Task<string?> ShowPathAsync(
        Window owner,
        GameDataService gameData,
        string preferredRuntimePath)
    {
        MapTargetPickerWindow window = new(
            gameData,
            preferredRuntimePath,
            null,
            false);
        MapTargetPickerResult? result = await window.ShowDialog<MapTargetPickerResult?>(owner);
        return result?.RuntimePath;
    }

    private void loadTargets(string preferredRuntimePath)
    {
        TreeViewItem? preferred = null;
        IReadOnlyList<MapCatalogEntry> catalog = gameData.MapCatalog;
        foreach (MapCatalogEntry entry in catalog
                     .Where(item => item.Kind == MapCatalogEntryKind.StandaloneMap)
                     .OrderBy(item => item.Key, StringComparer.Ordinal))
        {
            TreeViewItem item = createTreeItem(createMapTarget(entry));
            mapTree.Items.Add(item);
            if (string.Equals(
                    normalizeRuntimePath(((MapTargetItem)item.Tag!).RuntimePath),
                    preferredRuntimePath,
                    StringComparison.Ordinal))
            {
                preferred = item;
            }
        }

        foreach (MapCatalogEntry world in catalog
                     .Where(item => item.Kind == MapCatalogEntryKind.WorldMap)
                     .OrderBy(item => item.Key, StringComparer.Ordinal))
        {
            MapTargetItem worldTarget = new(
                world.Key,
                world.DisplayName,
                MapCatalogEntryKind.WorldMap,
                gameData.GetWorldManifestRuntimePath(world.Key),
                world.Key,
                true);
            TreeViewItem worldItem = createTreeItem(worldTarget);
            HashSet<string> placedChildren = getPlacedChildren(world.Key);
            foreach (MapCatalogEntry child in catalog
                         .Where(item => item.Kind == MapCatalogEntryKind.WorldChildMap
                             && string.Equals(item.WorldKey, world.Key, StringComparison.Ordinal))
                         .OrderBy(item => item.Key, StringComparer.Ordinal))
            {
                bool placed = placedChildren.Contains(child.Key);
                MapTargetItem childTarget = new(
                    child.Key,
                    child.DisplayName,
                    MapCatalogEntryKind.WorldChildMap,
                    gameData.GetMapRuntimePath(child.Key),
                    world.Key,
                    placed);
                TreeViewItem childItem = createTreeItem(childTarget);
                childItem.IsEnabled = placed;
                worldItem.Items.Add(childItem);
                if (placed
                    && string.Equals(
                        normalizeRuntimePath(childTarget.RuntimePath),
                        preferredRuntimePath,
                        StringComparison.Ordinal))
                {
                    preferred = childItem;
                    worldItem.IsExpanded = true;
                }
            }
            mapTree.Items.Add(worldItem);
            if (string.Equals(
                    normalizeRuntimePath(worldTarget.RuntimePath),
                    preferredRuntimePath,
                    StringComparison.Ordinal))
            {
                preferred = worldItem;
            }
        }

        TreeViewItem? first = mapTree.Items.OfType<TreeViewItem>().FirstOrDefault();
        TreeViewItem? selection = preferred ?? first;
        if (selection is not null)
        {
            selection.IsSelected = true;
            mapTree.SelectedItem = selection;
        }
    }

    private TreeViewItem createTreeItem(MapTargetItem target)
    {
        string text = target.Selectable
            ? target.DisplayName
            : target.DisplayName + " (" + LocaleService.Get("WORLD_NOT_PLACED") + ")";
        TreeViewItem item = new()
        {
            Header = new HintedTextPresenter { Text = text },
            Tag = target,
        };
        ToolTip.SetTip(item, target.RuntimePath);
        return item;
    }

    private MapTargetItem createMapTarget(MapCatalogEntry entry)
    {
        return new MapTargetItem(
            entry.Key,
            entry.DisplayName,
            entry.Kind,
            gameData.GetMapRuntimePath(entry.Key),
            entry.WorldKey,
            true);
    }

    private HashSet<string> getPlacedChildren(string worldKey)
    {
        HashSet<string> result = new(StringComparer.Ordinal);
        if (gameData.getWorldMap(worldKey)?["placements"] is not JsonArray placements)
            return result;
        foreach (JsonNode? node in placements)
        {
            if (node is not JsonObject placement
                || placement["map"] is not JsonValue value
                || !value.TryGetValue(out string? fileName)
                || string.IsNullOrWhiteSpace(fileName))
            {
                continue;
            }
            string childName = System.IO.Path.GetFileNameWithoutExtension(fileName);
            result.Add(worldKey + "/" + childName);
        }
        return result;
    }

    private void selectTarget()
    {
        MapTargetItem? previousTarget = selectedTarget;
        JsonArray? previousPosition = previousTarget is null
            ? BlueprintNodeParameterValues.NormalizePosition(initialPosition)
            : getPosition();
        selectedTarget = mapTree.SelectedItem is TreeViewItem item
            && item.Tag is MapTargetItem target
            && target.Selectable
                ? target
                : null;
        JsonArray? position = translatePosition(previousTarget, selectedTarget, previousPosition);
        confirmButton.IsEnabled = selectedTarget is not null;
        if (selectedTarget is null)
        {
            mapView.SetMap(null, null);
            worldView.SetWorld(null, null, []);
            refreshPositionLabel();
            return;
        }
        if (selectedTarget.Kind == MapCatalogEntryKind.WorldMap)
        {
            IReadOnlyList<WorldMapChildSource> children = gameData.MapCatalog
                .Where(entry => entry.Kind == MapCatalogEntryKind.WorldChildMap
                    && string.Equals(entry.WorldKey, selectedTarget.WorldKey, StringComparison.Ordinal))
                .OrderBy(entry => entry.Key, StringComparer.Ordinal)
                .Select(entry => new WorldMapChildSource(
                    entry.Key,
                    entry.DisplayName,
                    entry.Width,
                    entry.Height,
                    () => gameData.getMap(entry.Key),
                    entry.LayerOrder,
                    () => gameData.LoadedMapData.ContainsKey(entry.Key),
                    () => gameData.ReadWorldChildMapSnapshotAsync(entry.Key),
                    snapshot => gameData.InstallWorldChildMapSnapshot(entry.Key, snapshot)))
                .ToArray();
            worldView.SetWorld(
                selectedTarget.WorldKey,
                gameData.getWorldMap(selectedTarget.WorldKey ?? string.Empty),
                children);
            worldView.SetSelectedWorldCell(position);
            worldScroll.IsVisible = true;
            mapView.IsVisible = false;
        }
        else
        {
            mapView.SetMap(selectedTarget.Key, gameData.getMap(selectedTarget.Key));
            mapView.SetPosition(position);
            mapView.IsVisible = true;
            worldScroll.IsVisible = false;
        }
        refreshPositionLabel();
    }

    private JsonArray? translatePosition(
        MapTargetItem? source,
        MapTargetItem? destination,
        JsonArray? position)
    {
        if (destination is null || position is not { Count: >= 2 })
            return null;
        if (source is null || string.Equals(source.Key, destination.Key, StringComparison.Ordinal))
            return (JsonArray)position.DeepClone();
        if (source.WorldKey is null
            || destination.WorldKey is null
            || !string.Equals(source.WorldKey, destination.WorldKey, StringComparison.Ordinal)
            || !tryGetPosition(position, out int x, out int y))
        {
            return null;
        }
        int worldX = x;
        int worldY = y;
        if (source.Kind == MapCatalogEntryKind.WorldChildMap)
        {
            if (!tryGetPlacement(source, out WorldMapRect sourceRect))
                return null;
            worldX += sourceRect.X;
            worldY += sourceRect.Y;
        }
        else if (source.Kind != MapCatalogEntryKind.WorldMap)
        {
            return null;
        }
        if (destination.Kind == MapCatalogEntryKind.WorldMap)
            return new JsonArray(worldX, worldY);
        if (destination.Kind != MapCatalogEntryKind.WorldChildMap
            || !tryGetPlacement(destination, out WorldMapRect destinationRect)
            || worldX < destinationRect.X
            || worldY < destinationRect.Y
            || worldX >= destinationRect.X + destinationRect.Width
            || worldY >= destinationRect.Y + destinationRect.Height)
        {
            return null;
        }
        return new JsonArray(worldX - destinationRect.X, worldY - destinationRect.Y);
    }

    private bool tryGetPlacement(MapTargetItem target, out WorldMapRect rect)
    {
        rect = default;
        if (target.WorldKey is null
            || gameData.getWorldMap(target.WorldKey)?["placements"] is not JsonArray placements)
        {
            return false;
        }
        string fileName = System.IO.Path.GetFileName(target.Key) + ".json";
        foreach (JsonObject placement in placements.OfType<JsonObject>())
        {
            if (placement["map"] is not JsonValue mapValue
                || !mapValue.TryGetValue(out string? placedFile)
                || !string.Equals(placedFile, fileName, StringComparison.Ordinal)
                || placement["rect"] is not JsonArray { Count: >= 4 } values
                || !tryGetPosition(values, out int x, out int y)
                || !tryGetInt(values[2], out int width)
                || !tryGetInt(values[3], out int height))
            {
                continue;
            }
            rect = new WorldMapRect(x, y, width, height);
            return true;
        }
        return false;
    }

    private static bool tryGetPosition(JsonArray value, out int x, out int y)
    {
        bool hasX = tryGetInt(value[0], out x);
        bool hasY = tryGetInt(value[1], out y);
        return hasX && hasY;
    }

    private static bool tryGetInt(JsonNode? value, out int result)
    {
        result = 0;
        return value is JsonValue scalar && scalar.TryGetValue(out result);
    }

    private JsonArray? getPosition()
    {
        if (!pickPosition || selectedTarget is null)
            return null;
        if (selectedTarget.Kind == MapCatalogEntryKind.WorldMap)
        {
            return worldView.SelectedWorldCell is { } cell
                ? new JsonArray(cell.X, cell.Y)
                : null;
        }
        return mapView.GetPosition();
    }

    private void clearPosition()
    {
        if (selectedTarget?.Kind == MapCatalogEntryKind.WorldMap)
            worldView.SelectWorldCell(null, null);
        else
            mapView.ClearPosition();
        refreshPositionLabel();
    }

    private void refreshPositionLabel()
    {
        if (pickPosition)
            positionLabel.Text = BlueprintNodeParameterValues.FormatPosition(getPosition());
    }

    private void confirm()
    {
        if (selectedTarget is null)
            return;
        Close(new MapTargetPickerResult(selectedTarget.RuntimePath, getPosition()));
    }

    private void onKeyDown(object? sender, KeyEventArgs args)
    {
        if (args.Key != Key.Escape)
            return;
        Close(null);
        args.Handled = true;
    }

    private static string normalizeRuntimePath(string value)
    {
        string path = (value ?? string.Empty).Replace('\\', '/').Trim('/');
        while (path.StartsWith("./", StringComparison.Ordinal))
            path = path[2..];
        const string marker = "Data/Maps/";
        int markerIndex = path.IndexOf(marker, StringComparison.Ordinal);
        return markerIndex < 0 ? path : path[(markerIndex + marker.Length)..];
    }

    private sealed record MapTargetItem(
        string Key,
        string DisplayName,
        MapCatalogEntryKind Kind,
        string RuntimePath,
        string? WorldKey,
        bool Selectable);
}
