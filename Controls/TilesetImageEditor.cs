using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using System;
using System.IO;
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

    public TilesetImageEditor(int cellSize)
    {
        this.cellSize = Math.Max(1, cellSize);
        Focusable = true;
    }

    public TilesetEditMode Mode { get; set; }
    public Action? BeforeDataChanged { get; set; }
    public Action? DataChanged { get; set; }
    public Action<JsonObject>? MaterialEditRequested { get; set; }

    public void setData(JsonObject? nextData, string? imagePath, bool nextIsAutoTile)
    {
        data = nextData;
        isAutoTile = nextIsAutoTile;
        image?.Dispose();
        image = null;
        if (!string.IsNullOrWhiteSpace(imagePath) && File.Exists(imagePath))
            image = new Bitmap(imagePath);
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
        else
            editTileset(y * columns + x, position, x, y, columns * rows);
        args.Handled = true;
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
            drawPassable(context, frame, data["passable"]?.GetValue<bool?>() ?? true);
        else if (Mode == TilesetEditMode.Material && !isDefaultMaterial(data["material"] as JsonObject))
            drawMaterial(context, frame);
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
        BeforeDataChanged?.Invoke();
        switch (Mode)
        {
            case TilesetEditMode.Passable:
                JsonArray passable = ensureArray("passable", count, false);
                passable[index] = !(passable[index]?.GetValue<bool?>() ?? false);
                break;
            case TilesetEditMode.Material:
                JsonArray materials = ensureArray("materials", count, createDefaultMaterial);
                MaterialEditRequested?.Invoke(getObject(materials, index) ?? createAndAssign(materials, index));
                return;
            case TilesetEditMode.Dir4:
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
        if (data["material"] is null)
            BeforeDataChanged?.Invoke();
        JsonObject material = data["material"] as JsonObject ?? createDefaultMaterial();
        data["material"] = material;
        MaterialEditRequested?.Invoke(material);
    }

    private JsonArray ensureArray(string name, int count, Func<JsonNode?> createValue)
    {
        JsonArray value = data?[name] as JsonArray ?? new JsonArray();
        while (value.Count < count)
            value.Add(createValue());
        while (value.Count > count)
            value.RemoveAt(value.Count - 1);
        if (data is not null)
            data[name] = value;
        return value;
    }

    private JsonArray ensureArray(string name, int count, bool defaultValue)
    {
        return ensureArray(name, count, () => defaultValue);
    }

    private JsonArray ensureArray(string name, int count, Func<JsonArray> createValue)
    {
        return ensureArray(name, count, () => createValue());
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
