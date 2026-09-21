using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using Ludork.Services;
using System;
using System.Collections.Generic;
using System.Text.Json.Nodes;
using System.Threading;
using System.Threading.Tasks;

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
    private readonly ProjectDataStore gameData;
    private EditorThumbnailLease? imageLease;
    private CancellationTokenSource? imageRequest;
    private string? imageAssetPath;
    private Rect viewport;
    private Bitmap? image;
    private JsonObject? data;
    private bool isAutoTile;
    private bool batchPainting;
    private int batchColumns;
    private int batchRows;
    private int batchCount;
    private TilesetEditMode batchMode;
    private (int X, int Y) batchLastCell;
    private JsonNode? batchSourceValue;

    public TilesetImageEditor(ProjectDataStore gameData, int cellSize)
    {
        this.gameData = gameData;
        this.cellSize = Math.Max(1, cellSize);
        EffectiveViewportChanged += (_, args) =>
        {
            viewport = args.EffectiveViewport;
            InvalidateVisual();
        };
        Focusable = true;
    }

    public TilesetEditMode Mode { get; set; }
    public event EventHandler? ImageChanged;
    public bool HasImage => image is not null;
    public Func<string, JsonNode, IReadOnlyList<int>, int, bool>? EditRequested { get; set; }
    public Func<int, int, int, bool, bool>? DirectionEditRequested { get; set; }
    public Func<int, int, JsonObject, JsonObject, bool>? MaterialCommitRequested { get; set; }
    public Action? GestureStarted { get; set; }
    public Action? GestureCompleted { get; set; }
    public Action<JsonObject, Action<JsonObject>>? MaterialEditRequested { get; set; }

    public void setData(
        JsonObject? nextData,
        string projectPath,
        string? assetPath,
        bool nextIsAutoTile)
    {
        data = nextData;
        isAutoTile = nextIsAutoTile;
        InvalidateVisual();
        if (imageRequest is not null && imageAssetPath == assetPath)
            return;
        releaseImage();
        imageAssetPath = assetPath;
        imageRequest = new CancellationTokenSource();
        loadImage(projectPath, assetPath, imageRequest.Token);
    }

    public void ReloadImage()
    {
        string? path = imageAssetPath;
        releaseImage();
        setData(data, gameData.ProjectPath, path, isAutoTile);
    }

    private async void loadImage(string projectPath, string? assetPath, CancellationToken cancellationToken)
    {
        try
        {
            string? path = await Task.Run(() =>
                GameAssetPath.TryResolveExistingFile(projectPath, assetPath, out string resolved) ? resolved : null,
                cancellationToken);
            EditorThumbnailLease? lease = path is null ? null
                : await gameData.Thumbnails.AcquireAsync(path, 0, cancellationToken);
            if (cancellationToken.IsCancellationRequested)
            {
                lease?.Dispose();
                return;
            }
            imageLease = lease;
            image = lease?.Bitmap;
            Width = image?.PixelSize.Width ?? 0;
            Height = image?.PixelSize.Height ?? 0;
            InvalidateMeasure();
            InvalidateVisual();
            ImageChanged?.Invoke(this, EventArgs.Empty);
        }
        catch (OperationCanceledException) when (cancellationToken.IsCancellationRequested)
        {
        }
    }

    private void releaseImage()
    {
        imageRequest?.Cancel();
        imageRequest?.Dispose();
        imageRequest = null;
        image = null;
        imageLease?.Dispose();
        imageLease = null;
        Width = 0;
        Height = 0;
    }

    public override void Render(DrawingContext context)
    {
        context.FillRectangle(new SolidColorBrush(Color.FromRgb(30, 30, 30)), Bounds);
        if (image is null)
            return;
        Rect imageBounds = new(0, 0, image.PixelSize.Width, image.PixelSize.Height);
        Rect visible = imageBounds.Intersect(viewport);
        if (visible.Width <= 0 || visible.Height <= 0)
            return;
        using IDisposable clip = context.PushClip(visible);
        context.DrawImage(image, imageBounds);
        int columns = image.PixelSize.Width / cellSize;
        int rows = image.PixelSize.Height / cellSize;
        Pen gridPen = new(new SolidColorBrush(Color.FromArgb(100, 0, 0, 0)));
        int firstColumn = Math.Max(0, (int)Math.Floor(visible.Left / cellSize));
        int lastColumn = Math.Min(columns, (int)Math.Ceiling(visible.Right / cellSize));
        int firstRow = Math.Max(0, (int)Math.Floor(visible.Top / cellSize));
        int lastRow = Math.Min(rows, (int)Math.Ceiling(visible.Bottom / cellSize));
        for (int x = firstColumn; x <= lastColumn; x++)
            context.DrawLine(gridPen, new Point(x * cellSize, 0), new Point(x * cellSize, rows * cellSize));
        for (int y = firstRow; y <= lastRow; y++)
            context.DrawLine(gridPen, new Point(0, y * cellSize), new Point(columns * cellSize, y * cellSize));
        if (data is null)
            return;
        if (isAutoTile)
            drawAutoTileOverlay(context, new Rect(0, 0, Math.Min(3, columns) * cellSize, Math.Min(4, rows) * cellSize));
        else
            drawTilesetOverlays(context, columns, firstColumn, lastColumn, firstRow, lastRow);
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
        completeBatchPaint();
        releaseImage();
    }

    private void drawTilesetOverlays(DrawingContext context, int columns, int firstColumn, int lastColumn, int firstRow, int lastRow)
    {
        for (int y = firstRow; y < lastRow; y++)
        for (int x = firstColumn; x < lastColumn; x++)
        {
            int index = y * columns + x;
            Rect cell = new(x * cellSize, y * cellSize, cellSize, cellSize);
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
                submitEdit("passable", JsonValue.Create(!getBool(getArray("passable"), index, false))!, [index], count);
                break;
            case TilesetEditMode.Material:
                requestMaterialEdit(data, "materials", index, count);
                return;
            case TilesetEditMode.Dir4:
                JsonArray value = (JsonArray)TilesetMetadata.Read(data, "dir4", index, false);
                int localX = (int)position.X - x * cellSize;
                int localY = (int)position.Y - y * cellSize;
                int edge = getDirIndex(localX, localY);
                value[edge] = !(value[edge]?.GetValue<bool?>() ?? true);
                if (DirectionEditRequested?.Invoke(index, count, edge, value[edge]!.GetValue<bool>()) == true)
                {
                    TilesetMetadata.Apply(data, "dir4", value, [index], count, false);
                    InvalidateVisual();
                }
                break;
        }
    }

    private void editAutoTile()
    {
        if (data is null)
            return;
        if (Mode == TilesetEditMode.Passable)
        {
            submitEdit("passable", JsonValue.Create(!(data["passable"]?.GetValue<bool?>() ?? true))!, [], 0);
            return;
        }
        requestMaterialEdit(data, "material", 0, 0);
    }

    private void beginBatchPaint(int x, int y, int columns, int rows)
    {
        if (data is null)
            return;
        batchPainting = true;
        GestureStarted?.Invoke();
        batchColumns = columns;
        batchRows = rows;
        batchCount = columns * rows;
        batchMode = Mode;
        batchLastCell = (x, y);
        batchSourceValue = TilesetMetadata.Read(data, getProperty(batchMode), y * columns + x, false);
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
        List<int> indices = [];
        while (true)
        {
            indices.Add(y * batchColumns + x);
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
        if (data is null || batchSourceValue is null)
            return;
        string property = getProperty(batchMode);
        indices.RemoveAll(index => JsonNode.DeepEquals(TilesetMetadata.Read(data, property, index, false), batchSourceValue));
        if (indices.Count != 0)
            submitEdit(property, batchSourceValue, indices, batchCount);
    }

    private bool submitEdit(string property, JsonNode value, IReadOnlyList<int> indices, int count)
    {
        JsonObject? target = data;
        if (target is null || EditRequested?.Invoke(property, value, indices, count) != true)
            return false;
        TilesetMetadata.Apply(target, property, value, indices, count, isAutoTile);
        InvalidateVisual();
        return true;
    }

    private void completeBatchPaint()
    {
        if (!batchPainting)
            return;
        batchPainting = false;
        batchSourceValue = null;
        GestureCompleted?.Invoke();
        InvalidateVisual();
    }

    private void requestMaterialEdit(JsonObject target, string property, int index, int count)
    {
        JsonObject material = (JsonObject)TilesetMetadata.Read(target, property, index, isAutoTile);
        MaterialEditRequested?.Invoke(material, edited =>
        {
            if (!ReferenceEquals(data, target) || MaterialCommitRequested?.Invoke(index, count, material, edited) != true)
                return;
            JsonObject current = (JsonObject)TilesetMetadata.Read(target, property, index, isAutoTile);
            JsonObject merged = TilesetMetadata.MergeMaterial(current, material, edited);
            TilesetMetadata.Apply(target, property, merged, isAutoTile ? [] : [index], count, isAutoTile);
            InvalidateVisual();
        });
    }

    private static string getProperty(TilesetEditMode mode) => mode switch
    {
        TilesetEditMode.Passable => "passable",
        TilesetEditMode.Material => "materials",
        TilesetEditMode.Dir4 => "dir4",
        _ => throw new InvalidOperationException(),
    };

    private bool tryGetCell(Point position, int columns, int rows, out int x, out int y)
    {
        x = (int)(position.X / cellSize);
        y = (int)(position.Y / cellSize);
        return x >= 0 && y >= 0 && x < columns && y < rows;
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
