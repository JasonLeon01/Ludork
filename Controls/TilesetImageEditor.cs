using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using Ludork.Services;
using System;
using System.Text.Json.Nodes;

namespace Ludork.Controls;

public enum TilesetEditMode
{
    Passable,
    Material,
    Dir4,
}

public sealed class TilesetImageEditor : Control, IDisposable
{
    private readonly int cellSize;
    private Bitmap? image;
    private JsonObject? data;
    private bool isAutoTile;
    private bool batchPainting;
    private bool batchChanged;
    private bool batchSnapshotRecorded;
    private int batchColumns;
    private int batchRows;
    private int batchCount;
    private TilesetEditMode batchMode;
    private (int X, int Y) batchLastCell;
    private JsonNode? batchSourceValue;

    public TilesetImageEditor(int cellSize)
    {
        this.cellSize = Math.Max(1, cellSize);
        Focusable = true;
    }

    public TilesetEditMode Mode { get; set; }
    public Action? BeforeDataChanged { get; set; }
    public Action? DataChanged { get; set; }
    public Action<JsonObject, Action<JsonObject>>? MaterialEditRequested { get; set; }

    public void setData(
        JsonObject? nextData,
        string projectPath,
        string? assetPath,
        bool nextIsAutoTile)
    {
        data = nextData;
        isAutoTile = nextIsAutoTile;
        image?.Dispose();
        image = null;
        if (GameAssetPath.TryResolveExistingFile(
                projectPath,
                assetPath,
                out string filePath))
        {
            image = new Bitmap(filePath);
        }
        if (image is null)
        {
            Width = 0;
            Height = 0;
        }
        else
        {
            Width = image.PixelSize.Width;
            Height = image.PixelSize.Height;
        }
        InvalidateMeasure();
        InvalidateVisual();
    }

    public override void Render(DrawingContext context)
    {
        context.FillRectangle(new SolidColorBrush(Color.FromRgb(30, 30, 30)), Bounds);
        if (image is null)
            return;
        Rect imageBounds = new(0, 0, image.PixelSize.Width, image.PixelSize.Height);
        context.DrawImage(image, imageBounds);
        int columns = image.PixelSize.Width / cellSize;
        int rows = image.PixelSize.Height / cellSize;
        Pen gridPen = new(new SolidColorBrush(Color.FromArgb(100, 0, 0, 0)));
        for (int x = 0; x <= columns; x++)
            context.DrawLine(gridPen, new Point(x * cellSize, 0), new Point(x * cellSize, rows * cellSize));
        for (int y = 0; y <= rows; y++)
            context.DrawLine(gridPen, new Point(0, y * cellSize), new Point(columns * cellSize, y * cellSize));
        if (data is null)
            return;
        if (isAutoTile)
            drawAutoTileOverlay(context, new Rect(0, 0, Math.Min(3, columns) * cellSize, Math.Min(4, rows) * cellSize));
        else
            drawTilesetOverlays(context, columns, rows);
    }

    protected override void OnPointerPressed(PointerPressedEventArgs args)
    {
        base.OnPointerPressed(args);
        if (image is null || data is null || !args.GetCurrentPoint(this).Properties.IsLeftButtonPressed)
            return;
        Point position = args.GetPosition(this);
        int columns = image.PixelSize.Width / cellSize;
        int rows = image.PixelSize.Height / cellSize;
        int x = (int)(position.X / cellSize);
        int y = (int)(position.Y / cellSize);
        if (x < 0 || y < 0 || x >= columns || y >= rows)
            return;
        Focus();
        if (isAutoTile)
            editAutoTile();
        else if (args.KeyModifiers.HasFlag(KeyModifiers.Shift))
        {
            beginBatchPaint(x, y, columns, rows);
            args.Pointer.Capture(this);
        }
        else
            editTileset(y * columns + x, position, x, y, columns * rows);
        args.Handled = true;
    }

    protected override void OnPointerMoved(PointerEventArgs args)
    {
        base.OnPointerMoved(args);
        if (!batchPainting
            || image is null
            || data is null
            || !args.GetCurrentPoint(this).Properties.IsLeftButtonPressed
            || !tryGetCell(args.GetPosition(this), batchColumns, batchRows, out int x, out int y))
        {
            return;
        }
        if (batchLastCell == (x, y))
            return;
        paintBatchLine(batchLastCell, (x, y));
        batchLastCell = (x, y);
        args.Handled = true;
    }

    protected override void OnPointerReleased(PointerReleasedEventArgs args)
    {
        base.OnPointerReleased(args);
        if (!batchPainting)
            return;
        if (tryGetCell(
                args.GetPosition(this),
                batchColumns,
                batchRows,
                out int x,
                out int y)
            && batchLastCell != (x, y))
        {
            paintBatchLine(batchLastCell, (x, y));
            batchLastCell = (x, y);
        }
        completeBatchPaint();
        args.Pointer.Capture(null);
        args.Handled = true;
    }

    protected override void OnPointerCaptureLost(PointerCaptureLostEventArgs args)
    {
        base.OnPointerCaptureLost(args);
        completeBatchPaint();
    }

    public void Dispose()
    {
        image?.Dispose();
        image = null;
    }

    private void drawTilesetOverlays(DrawingContext context, int columns, int rows)
    {
        for (int index = 0; index < columns * rows; index++)
        {
            Rect cell = new(index % columns * cellSize, index / columns * cellSize, cellSize, cellSize);
            switch (Mode)
            {
                case TilesetEditMode.Passable:
                    drawPassable(context, cell, getBool(getArray("passable"), index, false));
                    break;
                case TilesetEditMode.Material:
                    if (!isDefaultMaterial(getObject(getArray("materials"), index)))
                        drawMaterial(context, cell);
                    break;
                case TilesetEditMode.Dir4:
                    drawDir4(context, cell, getDir4(index));
                    break;
            }
        }
    }

    private void drawAutoTileOverlay(DrawingContext context, Rect frame)
    {
        if (frame.Width <= 0 || frame.Height <= 0 || data is null)
            return;
        if (Mode == TilesetEditMode.Passable)
            drawAutoTileMarker(context, frame, data["passable"]?.GetValue<bool?>() ?? true ? "O" : "X");
        else if (Mode == TilesetEditMode.Material && !isDefaultMaterial(data["material"] as JsonObject))
            drawAutoTileMarker(context, frame, "M");
    }

    private static void drawAutoTileMarker(DrawingContext context, Rect frame, string value)
    {
        FormattedText marker = new(
            value,
            System.Globalization.CultureInfo.InvariantCulture,
            FlowDirection.LeftToRight,
            Typeface.Default,
            24,
            Brushes.White);
        context.DrawText(
            marker,
            new Point(frame.Center.X - marker.Width / 2, frame.Center.Y - marker.Height / 2));
    }

    private static void drawPassable(DrawingContext context, Rect cell, bool passable)
    {
        Rect marker = cell.Deflate(Math.Min(10, Math.Min(cell.Width, cell.Height) / 3));
        Pen pen = new(new SolidColorBrush(Color.FromArgb(220, 255, 255, 255)), 2);
        if (passable)
            context.DrawEllipse(null, pen, marker.Center, marker.Width / 2, marker.Height / 2);
        else
        {
            context.DrawLine(pen, marker.TopLeft, marker.BottomRight);
            context.DrawLine(pen, marker.TopRight, marker.BottomLeft);
        }
    }

    private static void drawMaterial(DrawingContext context, Rect cell)
    {
        context.DrawRectangle(null, new Pen(new SolidColorBrush(Color.FromArgb(220, 100, 255, 100)), 2), cell.Deflate(4));
        context.DrawText(
            new FormattedText("M", System.Globalization.CultureInfo.InvariantCulture, FlowDirection.LeftToRight, Typeface.Default, Math.Max(12, cell.Height / 2), Brushes.White),
            new Point(cell.Center.X - 5, cell.Center.Y - cell.Height / 4)
        );
    }

    private static void drawDir4(DrawingContext context, Rect cell, bool[] values)
    {
        double length = Math.Max(7, cell.Width / 3);
        Point center = cell.Center;
        drawArrow(context, center, new Vector(0, length), values[0]);
        drawArrow(context, center, new Vector(-length, 0), values[1]);
        drawArrow(context, center, new Vector(length, 0), values[2]);
        drawArrow(context, center, new Vector(0, -length), values[3]);
    }

    private static void drawArrow(DrawingContext context, Point center, Vector vector, bool enabled)
    {
        IBrush brush = new SolidColorBrush(enabled ? Color.FromArgb(210, 100, 255, 100) : Color.FromArgb(210, 255, 100, 100));
        Pen pen = new(brush, 2);
        Point tip = center + vector;
        context.DrawLine(pen, center, tip);
        Vector side = new Vector(-vector.Y, vector.X).Normalize() * 4;
        context.DrawLine(pen, tip, tip - vector.Normalize() * 5 + side);
        context.DrawLine(pen, tip, tip - vector.Normalize() * 5 - side);
    }

    private void editTileset(int index, Point position, int x, int y, int count)
    {
        if (data is null)
            return;
        switch (Mode)
        {
            case TilesetEditMode.Passable:
                BeforeDataChanged?.Invoke();
                JsonArray passable = ensureArray("passable", count, false);
                passable[index] = !(passable[index]?.GetValue<bool?>() ?? false);
                break;
            case TilesetEditMode.Material:
                requestTilesetMaterialEdit(data, index, count);
                return;
            case TilesetEditMode.Dir4:
                BeforeDataChanged?.Invoke();
                JsonArray dir4 = ensureArray("dir4", count, createDefaultDir4);
                JsonArray value = getArray(dir4[index]) ?? createAndAssignDir4(dir4, index);
                int localX = (int)position.X - x * cellSize;
                int localY = (int)position.Y - y * cellSize;
                int edge = getDirIndex(localX, localY);
                value[edge] = !(value[edge]?.GetValue<bool?>() ?? true);
                break;
        }
        DataChanged?.Invoke();
        InvalidateVisual();
    }

    private void editAutoTile()
    {
        if (data is null)
            return;
        if (Mode == TilesetEditMode.Passable)
        {
            BeforeDataChanged?.Invoke();
            data["passable"] = !(data["passable"]?.GetValue<bool?>() ?? true);
            DataChanged?.Invoke();
            InvalidateVisual();
            return;
        }
        JsonObject target = data;
        JsonObject material = (JsonObject)(target["material"] as JsonObject ?? createDefaultMaterial()).DeepClone();
        MaterialEditRequested?.Invoke(material, edited => applyAutoTileMaterial(target, edited));
    }

    private void beginBatchPaint(int x, int y, int columns, int rows)
    {
        if (data is null)
            return;
        batchPainting = true;
        batchChanged = false;
        batchSnapshotRecorded = false;
        batchColumns = columns;
        batchRows = rows;
        batchCount = columns * rows;
        batchMode = Mode;
        batchLastCell = (x, y);
        batchSourceValue = getModeValue(data, y * columns + x, batchMode);
    }

    private void paintBatchLine((int X, int Y) start, (int X, int Y) end)
    {
        int x = start.X;
        int y = start.Y;
        int dx = Math.Abs(end.X - start.X);
        int sx = start.X < end.X ? 1 : -1;
        int dy = -Math.Abs(end.Y - start.Y);
        int sy = start.Y < end.Y ? 1 : -1;
        int error = dx + dy;
        bool changed = false;
        while (true)
        {
            changed |= paintBatchCell(y * batchColumns + x);
            if (x == end.X && y == end.Y)
                break;
            int doubled = error * 2;
            if (doubled >= dy)
            {
                error += dy;
                x += sx;
            }
            if (doubled <= dx)
            {
                error += dx;
                y += sy;
            }
        }
        if (changed)
            InvalidateVisual();
    }

    private bool paintBatchCell(int index)
    {
        if (data is null || batchSourceValue is null || index < 0 || index >= batchCount)
            return false;
        JsonNode current = getModeValue(data, index, batchMode);
        if (JsonNode.DeepEquals(current, batchSourceValue))
            return false;
        if (!batchSnapshotRecorded)
        {
            BeforeDataChanged?.Invoke();
            batchSnapshotRecorded = true;
        }
        switch (batchMode)
        {
            case TilesetEditMode.Passable:
                ensureArray("passable", batchCount, false)[index] = batchSourceValue.GetValue<bool>();
                break;
            case TilesetEditMode.Material:
                ensureArray("materials", batchCount, createDefaultMaterial)[index] = batchSourceValue.DeepClone();
                break;
            case TilesetEditMode.Dir4:
                ensureArray("dir4", batchCount, createDefaultDir4)[index] = batchSourceValue.DeepClone();
                break;
        }
        batchChanged = true;
        return true;
    }

    private void completeBatchPaint()
    {
        if (!batchPainting)
            return;
        batchPainting = false;
        batchSourceValue = null;
        if (!batchChanged)
            return;
        batchChanged = false;
        DataChanged?.Invoke();
        InvalidateVisual();
    }

    private void requestTilesetMaterialEdit(JsonObject target, int index, int count)
    {
        JsonObject material = (JsonObject)(getObject(target["materials"] as JsonArray, index) ?? createDefaultMaterial()).DeepClone();
        MaterialEditRequested?.Invoke(material, edited => applyTilesetMaterial(target, index, count, edited));
    }

    private void applyTilesetMaterial(JsonObject target, int index, int count, JsonObject edited)
    {
        JsonObject current = getObject(target["materials"] as JsonArray, index) ?? createDefaultMaterial();
        if (JsonNode.DeepEquals(current, edited))
            return;
        BeforeDataChanged?.Invoke();
        ensureArray(target, "materials", count, createDefaultMaterial)[index] = edited.DeepClone();
        DataChanged?.Invoke();
        InvalidateVisual();
    }

    private void applyAutoTileMaterial(JsonObject target, JsonObject edited)
    {
        JsonObject current = target["material"] as JsonObject ?? createDefaultMaterial();
        if (JsonNode.DeepEquals(current, edited))
            return;
        BeforeDataChanged?.Invoke();
        target["material"] = edited.DeepClone();
        DataChanged?.Invoke();
        InvalidateVisual();
    }

    private static JsonNode getModeValue(JsonObject source, int index, TilesetEditMode mode)
    {
        return mode switch
        {
            TilesetEditMode.Passable => JsonValue.Create(getBool(source["passable"] as JsonArray, index, false))!,
            TilesetEditMode.Material => (getObject(source["materials"] as JsonArray, index) ?? createDefaultMaterial()).DeepClone(),
            TilesetEditMode.Dir4 => createDir4Value(source, index),
            _ => throw new InvalidOperationException(),
        };
    }

    private static JsonArray createDir4Value(JsonObject source, int index)
    {
        JsonArray? value = getArray(source["dir4"] is JsonArray values && index < values.Count ? values[index] : null);
        return new JsonArray(
            value?[0]?.GetValue<bool?>() ?? true,
            value?[1]?.GetValue<bool?>() ?? true,
            value?[2]?.GetValue<bool?>() ?? true,
            value?[3]?.GetValue<bool?>() ?? true);
    }

    private bool tryGetCell(Point position, int columns, int rows, out int x, out int y)
    {
        x = (int)(position.X / cellSize);
        y = (int)(position.Y / cellSize);
        return x >= 0 && y >= 0 && x < columns && y < rows;
    }

    private JsonArray ensureArray(string name, int count, Func<JsonNode?> createValue)
    {
        return ensureArray(data!, name, count, createValue);
    }

    private static JsonArray ensureArray(JsonObject target, string name, int count, Func<JsonNode?> createValue)
    {
        JsonArray value = target[name] as JsonArray ?? new JsonArray();
        while (value.Count < count)
            value.Add(createValue());
        while (value.Count > count)
            value.RemoveAt(value.Count - 1);
        target[name] = value;
        return value;
    }

    private JsonArray ensureArray(string name, int count, bool defaultValue)
    {
        return ensureArray(name, count, () => defaultValue);
    }

    private JsonArray? getArray(string name) => data?[name] as JsonArray;
    private static JsonArray? getArray(JsonNode? node) => node as JsonArray;
    private static JsonObject? getObject(JsonArray? value, int index) => value is not null && index < value.Count ? value[index] as JsonObject : null;
    private static bool getBool(JsonArray? value, int index, bool fallback) => value is not null && index < value.Count ? value[index]?.GetValue<bool?>() ?? fallback : fallback;

    private bool[] getDir4(int index)
    {
        JsonArray? value = getArray(getArray("dir4") is { } values && index < values.Count ? values[index] : null);
        return
        [
            value?[0]?.GetValue<bool?>() ?? true,
            value?[1]?.GetValue<bool?>() ?? true,
            value?[2]?.GetValue<bool?>() ?? true,
            value?[3]?.GetValue<bool?>() ?? true,
        ];
    }

    private int getDirIndex(int localX, int localY)
    {
        int[] distances = [localY, cellSize - 1 - localX, cellSize - 1 - localY, localX];
        int edge = 0;
        for (int index = 1; index < distances.Length; index++)
            if (distances[index] < distances[edge])
                edge = index;
        return edge switch { 0 => 3, 1 => 2, 2 => 0, _ => 1 };
    }

    private static JsonObject createDefaultMaterial() => new()
    {
        ["lightBlock"] = 0.0,
        ["mirror"] = false,
        ["reflectionStrength"] = 0.5,
        ["opacity"] = 1.0,
        ["speedRate"] = 1.0,
    };

    private static JsonArray createDefaultDir4() => new(true, true, true, true);
    private static JsonObject createAndAssign(JsonArray array, int index)
    {
        JsonObject value = createDefaultMaterial();
        array[index] = value;
        return value;
    }

    private static JsonArray createAndAssignDir4(JsonArray array, int index)
    {
        JsonArray value = createDefaultDir4();
        array[index] = value;
        return value;
    }

    private static bool isDefaultMaterial(JsonObject? value)
    {
        return value is null
            || (value["lightBlock"]?.GetValue<double?>() ?? 0.0) == 0.0
            && !(value["mirror"]?.GetValue<bool?>() ?? false)
            && (value["reflectionStrength"]?.GetValue<double?>() ?? 0.5) == 0.5
            && (value["opacity"]?.GetValue<double?>() ?? 1.0) == 1.0
            && (value["speedRate"]?.GetValue<double?>() ?? 1.0) == 1.0;
    }
}
