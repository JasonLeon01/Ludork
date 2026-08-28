using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Layout;
using Avalonia.Media;
using Ludork.Controls;
using Ludork.Models;
using Ludork.Services;
using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Text.Json.Nodes;
using System.Threading.Tasks;

namespace Ludork.Views.Utils;

public sealed class BlueprintNodeParameterEditorFactory
{
    private readonly GameDataService gameData;
    private readonly LuaMetadataService metadataService;
    private readonly BlueprintClassResolver classResolver;

    public BlueprintNodeParameterEditorFactory(
        GameDataService gameData,
        LuaMetadataService metadataService,
        BlueprintClassResolver classResolver)
    {
        this.gameData = gameData;
        this.metadataService = metadataService;
        this.classResolver = classResolver;
    }

    public Control? Create(
        BlueprintVariableEditorRequest request,
        Func<string, JsonNode?> getRawSiblingValue,
        Action<string, JsonNode?> setRawSiblingValue,
        Func<bool> isAlive)
    {
        return request.Field.EditorKind switch
        {
            BlueprintVariableEditorKind.MoveRoute => createMoveRouteEditor(request, isAlive),
            BlueprintVariableEditorKind.TransferPosition => createTransferPositionEditor(
                request,
                getRawSiblingValue,
                setRawSiblingValue,
                isAlive),
            BlueprintVariableEditorKind.BlueprintClass => createBlueprintClassEditor(request, isAlive),
            BlueprintVariableEditorKind.CommonFunction => createCommonFunctionEditor(request, isAlive),
            _ => null,
        };
    }

    private Control createMoveRouteEditor(BlueprintVariableEditorRequest request, Func<bool> isAlive)
    {
        JsonArray current = BlueprintNodeParameterValues.NormalizeRoute(request.Value);
        TextBox summary = EditorInputs.CreateReadOnlyTextBox(
            BlueprintNodeParameterValues.FormatRoute(current));
        Button editButton = new()
        {
            Content = LocaleService.Get("MOVE_ROUTE_EDIT"),
            MinHeight = EditorInputs.FieldMinHeight,
        };
        Grid editor = createSummaryEditor(summary, editButton);
        editButton.Click += async (_, _) =>
        {
            if (TopLevel.GetTopLevel(editor) is not Window owner)
                return;
            JsonArray? selected = await MoveRouteEditWindow.ShowAsync(
                owner,
                gameData,
                current);
            if (selected is null || !isControlAlive(editor, isAlive))
                return;
            if (!JsonNode.DeepEquals(current, selected))
            {
                current = (JsonArray)selected.DeepClone();
                request.Commit(current, false);
            }
            summary.Text = BlueprintNodeParameterValues.FormatRoute(current);
        };
        return editor;
    }

    private Control createTransferPositionEditor(
        BlueprintVariableEditorRequest request,
        Func<string, JsonNode?> getRawSiblingValue,
        Action<string, JsonNode?> setRawSiblingValue,
        Func<bool> isAlive)
    {
        JsonArray? current = BlueprintNodeParameterValues.NormalizePosition(request.Value);
        TextBox summary = EditorInputs.CreateReadOnlyTextBox(
            BlueprintNodeParameterValues.FormatPosition(current));
        Button editButton = new()
        {
            Content = LocaleService.Get("TRANSFER_POS_EDIT"),
            MinHeight = EditorInputs.FieldMinHeight,
        };
        Grid editor = createSummaryEditor(summary, editButton);
        editButton.Click += async (_, _) =>
        {
            if (TopLevel.GetTopLevel(editor) is not Window owner)
                return;
            string relatedFieldName = request.Field.RelatedFieldName ?? string.Empty;
            JsonNode? rawMapValue = relatedFieldName.Length == 0
                ? null
                : getRawSiblingValue(relatedFieldName);
            string currentMapReference = BlueprintNodeParameterValues.GetString(rawMapValue);
            TransferPositionSelection? selected = await TransferPositionPickWindow.ShowAsync(
                owner,
                gameData,
                current,
                currentMapReference);
            if (selected is null || !isControlAlive(editor, isAlive))
                return;
            if (!JsonNode.DeepEquals(current, selected.Position))
            {
                current = selected.Position?.DeepClone() as JsonArray;
                request.Commit(current, false);
            }
            summary.Text = BlueprintNodeParameterValues.FormatPosition(current);
            if (relatedFieldName.Length == 0 || selected.MapKey.Length == 0)
                return;
            string resolvedPath = resolveMapPath(selected.MapKey);
            string currentPath = resolveMapPath(
                BlueprintNodeParameterValues.GetString(getRawSiblingValue(relatedFieldName)));
            if (resolvedPath.Length != 0
                && !string.Equals(resolvedPath, currentPath, StringComparison.Ordinal)
                && isControlAlive(editor, isAlive))
            {
                setRawSiblingValue(relatedFieldName, JsonValue.Create(resolvedPath));
            }
        };
        return editor;
    }

    private Control createBlueprintClassEditor(
        BlueprintVariableEditorRequest request,
        Func<bool> isAlive)
    {
        string current = BlueprintNodeParameterValues.GetString(request.Value);
        TextBox summary = EditorInputs.CreateReadOnlyTextBox(current);
        Button browseButton = createBrowseButton();
        Grid editor = createSummaryEditor(summary, browseButton);
        browseButton.Click += async (_, _) =>
        {
            if (TopLevel.GetTopLevel(editor) is not Window owner)
                return;
            string? selected = await BlueprintClassSelector.ShowAsync(
                owner,
                gameData,
                metadataService,
                classResolver,
                current,
                null,
                BlueprintClassSelectorMode.NodeParameter);
            if (string.IsNullOrWhiteSpace(selected)
                || !isControlAlive(editor, isAlive)
                || string.Equals(current, selected, StringComparison.Ordinal))
            {
                return;
            }
            current = selected;
            summary.Text = current;
            request.Commit(JsonValue.Create(current), false);
        };
        return editor;
    }

    private Control createCommonFunctionEditor(
        BlueprintVariableEditorRequest request,
        Func<bool> isAlive)
    {
        string current = BlueprintNodeParameterValues.GetString(request.Value);
        TextBox summary = EditorInputs.CreateReadOnlyTextBox(current);
        Button browseButton = createBrowseButton();
        Grid editor = createSummaryEditor(summary, browseButton);
        browseButton.Click += async (_, _) =>
        {
            if (TopLevel.GetTopLevel(editor) is not Window owner)
                return;
            string? selected = await SearchSelectorDialog.ShowAsync(
                owner,
                LocaleService.Get("COMMON_FUNCTIONS"),
                gameData.CommonFunctionsData.Keys.OrderBy(value => value, StringComparer.Ordinal),
                current);
            if (string.IsNullOrWhiteSpace(selected)
                || !isControlAlive(editor, isAlive)
                || string.Equals(current, selected, StringComparison.Ordinal))
            {
                return;
            }
            current = selected;
            summary.Text = current;
            request.Commit(JsonValue.Create(current), false);
        };
        return editor;
    }

    private static Grid createSummaryEditor(TextBox summary, Button button)
    {
        Grid editor = new()
        {
            ColumnDefinitions = new ColumnDefinitions("*,4,Auto"),
        };
        editor.Children.Add(summary);
        Grid.SetColumn(button, 2);
        editor.Children.Add(button);
        return editor;
    }

    private static Button createBrowseButton()
    {
        return new Button
        {
            Content = "...",
            Width = 24,
            MinHeight = EditorInputs.FieldMinHeight,
            Padding = new Thickness(0),
        };
    }

    private static bool isControlAlive(Control control, Func<bool> isAlive)
    {
        return isAlive() && TopLevel.GetTopLevel(control) is not null;
    }

    internal static string normalizeMapKey(string value)
    {
        string path = value.Replace('\\', '/');
        while (path.StartsWith("./", StringComparison.Ordinal))
            path = path[2..];
        const string marker = "Data/Maps/";
        int markerIndex = path.IndexOf(marker, StringComparison.Ordinal);
        if (markerIndex >= 0)
            path = path[(markerIndex + marker.Length)..];
        int slashIndex = path.LastIndexOf('/');
        int extensionIndex = path.LastIndexOf('.');
        if (extensionIndex > slashIndex)
            path = path[..extensionIndex];
        return path;
    }

    private static string resolveMapPath(string mapKey)
    {
        string normalized = normalizeMapKey(mapKey);
        return normalized.Length == 0 ? string.Empty : normalized + DataConfig.DataFileExtension;
    }
}

internal readonly record struct RouteStep(int X, int Y);

internal sealed record TransferPositionSelection(JsonArray? Position, string MapKey);

internal static class BlueprintNodeParameterValues
{
    public static JsonArray NormalizeRoute(JsonNode? value)
    {
        JsonArray result = [];
        if (value is not JsonArray route)
            return result;
        foreach (JsonNode? item in route)
        {
            if (item is JsonArray step
                && step.Count >= 2
                && tryGetInt(step[0], out int x)
                && tryGetInt(step[1], out int y))
            {
                result.Add(new JsonArray(x, y));
            }
        }
        return result;
    }

    public static JsonArray? NormalizePosition(JsonNode? value)
    {
        if (value is not JsonArray position
            || position.Count < 2
            || !tryGetInt(position[0], out int x)
            || !tryGetInt(position[1], out int y))
        {
            return null;
        }
        return new JsonArray(x, y);
    }

    public static string FormatRoute(JsonNode? value)
    {
        JsonArray route = NormalizeRoute(value);
        List<string> steps = [];
        foreach (JsonNode? item in route)
        {
            JsonArray step = (JsonArray)item!;
            steps.Add($"({getInt(step[0])}, {getInt(step[1])})");
        }
        return "[" + string.Join(", ", steps) + "]";
    }

    public static string FormatPosition(JsonNode? value)
    {
        JsonArray? position = NormalizePosition(value);
        if (position is null)
            return LocaleService.Get("TRANSFER_POS_NONE");
        return LocaleService.Get("TRANSFER_POS_LABEL")
            .Replace("{x}", getInt(position[0]).ToString(CultureInfo.InvariantCulture), StringComparison.Ordinal)
            .Replace("{y}", getInt(position[1]).ToString(CultureInfo.InvariantCulture), StringComparison.Ordinal);
    }

    public static string GetString(JsonNode? value)
    {
        return value is JsonValue scalar && scalar.TryGetValue(out string? text)
            ? text ?? string.Empty
            : string.Empty;
    }

    public static IReadOnlyList<RouteStep> GetRouteSteps(JsonNode? value)
    {
        JsonArray route = NormalizeRoute(value);
        List<RouteStep> result = new(route.Count);
        foreach (JsonNode? item in route)
        {
            JsonArray step = (JsonArray)item!;
            result.Add(new RouteStep(getInt(step[0]), getInt(step[1])));
        }
        return result;
    }

    public static JsonArray RouteToJson(IEnumerable<RouteStep> route)
    {
        JsonArray result = [];
        foreach (RouteStep step in route)
            result.Add(new JsonArray(step.X, step.Y));
        return result;
    }

    private static int getInt(JsonNode? value)
    {
        return tryGetInt(value, out int result) ? result : 0;
    }

    private static bool tryGetInt(JsonNode? value, out int result)
    {
        if (value is JsonValue scalar)
        {
            if (scalar.TryGetValue(out int integer))
            {
                result = integer;
                return true;
            }
            if (scalar.TryGetValue(out long longValue))
            {
                result = (int)Math.Clamp(longValue, int.MinValue, int.MaxValue);
                return true;
            }
            if (scalar.TryGetValue(out double number) && double.IsFinite(number))
            {
                result = (int)Math.Clamp(number, int.MinValue, int.MaxValue);
                return true;
            }
            if (scalar.TryGetValue(out string? text)
                && int.TryParse(text, NumberStyles.Integer, CultureInfo.InvariantCulture, out integer))
            {
                result = integer;
                return true;
            }
        }
        result = 0;
        return false;
    }
}

internal sealed class MoveRouteMapReferenceView : MapReferenceView
{
    private readonly List<RouteStep> routeSteps = [];
    private readonly List<(int X, int Y)> routeCells = [];
    private (int X, int Y)? currentCell;
    private bool dragging;

    public MoveRouteMapReferenceView(GameDataService gameData) : base(gameData)
    {
    }

    public event EventHandler? RouteChanged;

    public override void SetMap(string? mapKey, JsonObject? mapData)
    {
        base.SetMap(mapKey, mapData);
        ClearRoute();
    }

    public void SetRoute(JsonNode? value)
    {
        dragging = false;
        currentCell = null;
        routeCells.Clear();
        routeSteps.Clear();
        routeSteps.AddRange(BlueprintNodeParameterValues.GetRouteSteps(value));
        InvalidateVisual();
    }

    public JsonArray GetRoute()
    {
        return BlueprintNodeParameterValues.RouteToJson(routeSteps);
    }

    public void ClearRoute()
    {
        dragging = false;
        currentCell = null;
        routeCells.Clear();
        routeSteps.Clear();
        RouteChanged?.Invoke(this, EventArgs.Empty);
        InvalidateVisual();
    }

    protected override void OnPointerPressed(PointerPressedEventArgs args)
    {
        base.OnPointerPressed(args);
        PointerPoint point = args.GetCurrentPoint(this);
        if (!point.Properties.IsLeftButtonPressed || GetCell(point.Position) is not { } cell)
            return;
        Focus();
        dragging = true;
        currentCell = cell;
        routeCells.Clear();
        routeCells.Add(cell);
        routeSteps.Clear();
        RouteChanged?.Invoke(this, EventArgs.Empty);
        args.Pointer.Capture(this);
        args.Handled = true;
        InvalidateVisual();
    }

    protected override void OnPointerMoved(PointerEventArgs args)
    {
        base.OnPointerMoved(args);
        if (!dragging || !args.GetCurrentPoint(this).Properties.IsLeftButtonPressed)
            return;
        if (GetCell(args.GetPosition(this)) is { } cell)
            appendPathTo(cell);
    }

    protected override void OnPointerReleased(PointerReleasedEventArgs args)
    {
        base.OnPointerReleased(args);
        if (!dragging)
            return;
        if (GetCell(args.GetPosition(this)) is { } cell)
            appendPathTo(cell);
        dragging = false;
        args.Pointer.Capture(null);
        args.Handled = true;
        InvalidateVisual();
    }

    protected override void DrawOverlay(DrawingContext context)
    {
        if (routeCells.Count == 0)
            return;
        Pen routePen = new(
            new SolidColorBrush(Color.FromRgb(32, 180, 255)),
            Math.Max(2, TileSize / 8.0));
        for (int index = 1; index < routeCells.Count; index++)
        {
            (int X, int Y) previous = routeCells[index - 1];
            (int X, int Y) current = routeCells[index];
            context.DrawLine(
                routePen,
                GetCellCenter(previous.X, previous.Y),
                GetCellCenter(current.X, current.Y));
        }
        Point start = GetCellCenter(routeCells[0].X, routeCells[0].Y);
        double radius = Math.Max(4, TileSize / 5.0);
        context.DrawEllipse(new SolidColorBrush(Color.FromArgb(220, 255, 215, 0)), null, start, radius, radius);
        if (routeCells.Count > 1)
        {
            (int X, int Y) lastCell = routeCells[^1];
            Point end = GetCellCenter(lastCell.X, lastCell.Y);
            context.DrawEllipse(new SolidColorBrush(Color.FromArgb(230, 32, 180, 255)), null, end, radius, radius);
        }
    }

    private void appendPathTo((int X, int Y) target)
    {
        if (currentCell is not { } current || current == target)
            return;
        int x = current.X;
        int y = current.Y;
        bool changed = false;
        while (x != target.X)
        {
            int step = target.X > x ? 1 : -1;
            x += step;
            if (!IsInMap(x, y))
                break;
            routeSteps.Add(new RouteStep(step, 0));
            routeCells.Add((x, y));
            changed = true;
        }
        while (y != target.Y)
        {
            int step = target.Y > y ? 1 : -1;
            y += step;
            if (!IsInMap(x, y))
                break;
            routeSteps.Add(new RouteStep(0, step));
            routeCells.Add((x, y));
            changed = true;
        }
        if (!changed)
            return;
        currentCell = (x, y);
        RouteChanged?.Invoke(this, EventArgs.Empty);
        InvalidateVisual();
    }
}

internal sealed class TransferPositionMapReferenceView : MapReferenceView
{
    private (int X, int Y)? selectedCell;
    private (int X, int Y)? hoverCell;

    public TransferPositionMapReferenceView(GameDataService gameData) : base(gameData)
    {
    }

    public event EventHandler? PositionChanged;

    public JsonArray? GetPosition()
    {
        return selectedCell is { } position ? new JsonArray(position.X, position.Y) : null;
    }

    public void SetPosition(JsonNode? value)
    {
        JsonArray? position = BlueprintNodeParameterValues.NormalizePosition(value);
        selectedCell = position is null
            ? null
            : (position[0]!.GetValue<int>(), position[1]!.GetValue<int>());
        InvalidateVisual();
    }

    public void ClearPosition()
    {
        selectedCell = null;
        PositionChanged?.Invoke(this, EventArgs.Empty);
        InvalidateVisual();
    }

    protected override void OnPointerPressed(PointerPressedEventArgs args)
    {
        base.OnPointerPressed(args);
        PointerPoint point = args.GetCurrentPoint(this);
        if (!point.Properties.IsLeftButtonPressed || GetCell(point.Position) is not { } cell)
            return;
        Focus();
        selectedCell = cell;
        PositionChanged?.Invoke(this, EventArgs.Empty);
        args.Handled = true;
        InvalidateVisual();
    }

    protected override void OnPointerMoved(PointerEventArgs args)
    {
        base.OnPointerMoved(args);
        (int X, int Y)? cell = GetCell(args.GetPosition(this));
        if (hoverCell == cell)
            return;
        hoverCell = cell;
        InvalidateVisual();
    }

    protected override void OnPointerExited(PointerEventArgs args)
    {
        base.OnPointerExited(args);
        hoverCell = null;
        InvalidateVisual();
    }

    protected override void DrawOverlay(DrawingContext context)
    {
        if (hoverCell is { } hover && hover != selectedCell)
            context.FillRectangle(new SolidColorBrush(Color.FromArgb(35, 255, 255, 255)), GetCellRect(hover.X, hover.Y));
        if (selectedCell is not { } selected || !IsInMap(selected.X, selected.Y))
            return;
        Rect cell = GetCellRect(selected.X, selected.Y);
        context.FillRectangle(new SolidColorBrush(Color.FromArgb(90, 32, 180, 255)), cell);
        Rect outline = new(cell.X + 1, cell.Y + 1, Math.Max(0, cell.Width - 2), Math.Max(0, cell.Height - 2));
        context.DrawRectangle(null, new Pen(new SolidColorBrush(Color.FromRgb(32, 180, 255)), 2), outline);
        Point center = GetCellCenter(selected.X, selected.Y);
        double half = Math.Max(3, TileSize / 4.0);
        Pen crossPen = new(new SolidColorBrush(Color.FromRgb(255, 215, 0)), 2);
        context.DrawLine(crossPen, new Point(center.X - half, center.Y), new Point(center.X + half, center.Y));
        context.DrawLine(crossPen, new Point(center.X, center.Y - half), new Point(center.X, center.Y + half));
    }
}

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
        mapView.SetMap(key, gameData.getMap(key));
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

internal static class TransferPositionPickWindow
{
    public static async Task<TransferPositionSelection?> ShowAsync(
        Window owner,
        GameDataService gameData,
        JsonNode? initial,
        string mapReference)
    {
        MapTargetPickerResult? result = await MapTargetPickerWindow.ShowPositionAsync(
            owner,
            gameData,
            mapReference,
            initial);
        return result is null
            ? null
            : new TransferPositionSelection(result.Position, result.RuntimePath);
    }
}
