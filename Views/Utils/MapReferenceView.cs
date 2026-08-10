using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using Avalonia.VisualTree;
using Ludork.Plugin.Avalonia;
using Ludork.Services;
using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Text.Json.Nodes;

namespace Ludork.Views.Utils;

public class MapReferenceView : Control, IDisposable
{
    private const int MinimumTileSize = 12;
    private const int MaximumTileSize = 96;
    private const int TileSizeStep = 4;
    private static readonly IBrush BackgroundBrush = new SolidColorBrush(Color.Parse("#262626"));
    private static readonly Pen GridPen = new(new SolidColorBrush(Color.FromArgb(45, 255, 255, 255)), 1);

    private readonly GameDataService gameData;
    private readonly AutoTileRenderer autoTileRenderer;
    private readonly Dictionary<string, Bitmap> tilesetCache = new(StringComparer.OrdinalIgnoreCase);
    private readonly EditorZoomInput zoomInput = new();
    private ScrollViewer? hostScrollViewer;
    private int tileSize;
    private double continuousTileSize;
    private bool disposed;
    private MapZoomAnchor? pendingMapZoomAnchor;

    public MapReferenceView(GameDataService gameData)
    {
        this.gameData = gameData;
        autoTileRenderer = new AutoTileRenderer(gameData);
        tileSize = Math.Clamp(
            Math.Max(16, gameData.getCellSize()),
            MinimumTileSize,
            MaximumTileSize);
        continuousTileSize = tileSize;
        MinWidth = 540;
        MinHeight = 360;
        Focusable = true;
        PointerTouchPadGestureMagnify += onPointerTouchPadGestureMagnify;
    }

    public string? MapKey { get; private set; }
    public JsonObject? MapData { get; private set; }
    public int TileSize => tileSize;

    public virtual void SetMap(string? mapKey, JsonObject? mapData)
    {
        MapKey = mapKey;
        MapData = mapData;
        updateContentSize();
        InvalidateVisual();
    }

    public override void Render(DrawingContext context)
    {
        context.FillRectangle(BackgroundBrush, Bounds);
        if (!TryGetMapSize(out int width, out int height))
            return;
        Rect mapRect = GetMapRect(width, height);
        using (context.PushClip(mapRect))
        using (context.PushTransform(Matrix.CreateTranslation(mapRect.X, mapRect.Y)))
        {
            Rect localMapRect = new(0, 0, width * (double)tileSize, height * (double)tileSize);
            context.FillRectangle(BackgroundBrush, localMapRect);
            drawLayers(context, width, height);
            drawGrid(context, width, height);
            DrawOverlay(context);
        }
    }

    public void Dispose()
    {
        if (disposed)
            return;
        disposed = true;
        LayoutUpdated -= onLayoutUpdated;
        pendingMapZoomAnchor = null;
        bindHostScrollViewer(null);
        autoTileRenderer.Dispose();
        foreach (Bitmap bitmap in tilesetCache.Values)
            bitmap.Dispose();
        tilesetCache.Clear();
    }

    protected bool TryGetMapSize(out int width, out int height)
    {
        width = getInt(MapData?["width"]);
        height = getInt(MapData?["height"]);
        return width > 0 && height > 0;
    }

    protected bool IsInMap(int x, int y)
    {
        return TryGetMapSize(out int width, out int height)
            && x >= 0
            && y >= 0
            && x < width
            && y < height;
    }

    protected (int X, int Y)? GetCell(Point position)
    {
        if (!TryGetMapSize(out int width, out int height))
            return null;
        Rect mapRect = GetMapRect(width, height);
        if (!mapRect.Contains(position))
            return null;
        int x = (int)((position.X - mapRect.X) / tileSize);
        int y = (int)((position.Y - mapRect.Y) / tileSize);
        return IsInMap(x, y) ? (x, y) : null;
    }

    protected Rect GetCellRect(int x, int y)
    {
        return new Rect(x * (double)tileSize, y * (double)tileSize, tileSize, tileSize);
    }

    protected Point GetCellCenter(int x, int y)
    {
        return new Point((x + 0.5) * tileSize, (y + 0.5) * tileSize);
    }

    protected virtual void DrawOverlay(DrawingContext context)
    {
    }

    protected override void OnAttachedToVisualTree(
        VisualTreeAttachmentEventArgs args)
    {
        base.OnAttachedToVisualTree(args);
        LayoutUpdated += onLayoutUpdated;
        bindHostScrollViewer(this.FindAncestorOfType<ScrollViewer>());
    }

    protected override void OnDetachedFromVisualTree(
        VisualTreeAttachmentEventArgs args)
    {
        LayoutUpdated -= onLayoutUpdated;
        pendingMapZoomAnchor = null;
        bindHostScrollViewer(null);
        base.OnDetachedFromVisualTree(args);
    }

    protected override void OnPointerWheelChanged(PointerWheelEventArgs args)
    {
        base.OnPointerWheelChanged(args);
        if (zoomInput.ShouldSuppressWheel())
        {
            args.Handled = true;
            return;
        }
        if (!EditorZoomInput.ShouldZoomWheel(args.KeyModifiers, false))
            return;
        int direction = args.Delta.Y > 0 ? 1 : args.Delta.Y < 0 ? -1 : 0;
        if (direction == 0)
            return;
        setTileSize(
            tileSize + direction * TileSizeStep,
            args.GetPosition(this),
            getZoomViewportPoint(args));
        args.Handled = true;
    }

    private void onPointerTouchPadGestureMagnify(
        object? sender,
        PointerDeltaEventArgs args)
    {
        if (!EditorZoomInput.IsMacOS)
            return;
        zoomInput.MarkMagnify();
        double nextTileSize = EditorZoomInput.ScaleByFactor(
            continuousTileSize,
            EditorZoomInput.GetMagnifyFactor(args.Delta.Y),
            MinimumTileSize,
            MaximumTileSize);
        setTileSize(
            nextTileSize,
            args.GetPosition(this),
            getZoomViewportPoint(args));
        args.Handled = true;
    }

    private Point getZoomViewportPoint(PointerEventArgs args)
    {
        return hostScrollViewer is null
            ? args.GetPosition(this)
            : args.GetPosition(hostScrollViewer);
    }

    private void setTileSize(
        double nextContinuousTileSize,
        Point contentPoint,
        Point viewportPoint)
    {
        MapZoomAnchor? nextAnchor = null;
        if (TryGetMapSize(out int width, out int height))
        {
            Rect mapRect = GetMapRect(width, height);
            nextAnchor = new MapZoomAnchor(
                (contentPoint.X - mapRect.X) / tileSize,
                (contentPoint.Y - mapRect.Y) / tileSize,
                viewportPoint);
        }
        continuousTileSize = Math.Clamp(
            nextContinuousTileSize,
            MinimumTileSize,
            MaximumTileSize);
        int nextTileSize = Math.Clamp(
            (int)Math.Round(
                continuousTileSize,
                MidpointRounding.AwayFromZero),
            MinimumTileSize,
            MaximumTileSize);
        if (nextTileSize == tileSize)
            return;
        tileSize = nextTileSize;
        pendingMapZoomAnchor = nextAnchor;
        updateContentSize();
        InvalidateVisual();
    }

    private void onLayoutUpdated(object? sender, EventArgs args)
    {
        if (pendingMapZoomAnchor is null)
            return;
        applyMapZoomAnchor();
    }

    private void applyMapZoomAnchor()
    {
        if (pendingMapZoomAnchor is not MapZoomAnchor anchor
            || hostScrollViewer is null
            || !TryGetMapSize(out int width, out int height))
        {
            pendingMapZoomAnchor = null;
            return;
        }
        pendingMapZoomAnchor = null;
        Rect mapRect = GetMapRect(width, height);
        Point contentAnchor = new(
            mapRect.X + anchor.MapX * tileSize,
            mapRect.Y + anchor.MapY * tileSize);
        hostScrollViewer.Offset = EditorZoomInput.GetAnchoredOffset(
            contentAnchor,
            anchor.ViewportPoint,
            hostScrollViewer.Extent,
            hostScrollViewer.Viewport);
    }

    private Rect GetMapRect(int width, int height)
    {
        double mapWidth = width * (double)tileSize;
        double mapHeight = height * (double)tileSize;
        return new Rect(
            Math.Max(0, (Bounds.Width - mapWidth) / 2),
            Math.Max(0, (Bounds.Height - mapHeight) / 2),
            mapWidth,
            mapHeight);
    }

    private void drawLayers(DrawingContext context, int width, int height)
    {
        if (MapData?["layers"] is not JsonObject layers)
            return;
        foreach (KeyValuePair<string, JsonNode?> entry in layers)
        {
            if (entry.Value is JsonObject layer)
                drawLayer(context, layer, width, height);
        }
    }

    private void drawLayer(DrawingContext context, JsonObject layer, int width, int height)
    {
        Bitmap? tileset = getTileset(getString(layer["layerTileset"]));
        JsonArray? tiles = layer["tiles"] as JsonArray;
        JsonArray? autoTiles = layer["autoTiles"] as JsonArray;
        int sourceTileSize = Math.Max(1, gameData.getCellSize());
        for (int y = 0; y < height; y++)
        {
            JsonArray? tileRow = getRow(tiles, y);
            JsonArray? autoTileRow = getRow(autoTiles, y);
            for (int x = 0; x < width; x++)
            {
                Rect destination = GetCellRect(x, y);
                string? autoTileKey = getString(getValue(autoTileRow, x));
                if (!string.IsNullOrWhiteSpace(autoTileKey) && autoTiles is not null)
                {
                    autoTileRenderer.drawTile(context, autoTileKey, autoTiles, x, y, destination, 0);
                    continue;
                }
                if (tileset is null
                    || !tryGetInt(getValue(tileRow, x), out int tileNumber)
                    || tileNumber < 0)
                {
                    continue;
                }
                int columns = Math.Max(1, tileset.PixelSize.Width / sourceTileSize);
                int sourceX = tileNumber % columns * sourceTileSize;
                int sourceY = tileNumber / columns * sourceTileSize;
                Rect source = new(sourceX, sourceY, sourceTileSize, sourceTileSize);
                if (source.Right <= tileset.PixelSize.Width && source.Bottom <= tileset.PixelSize.Height)
                    context.DrawImage(tileset, source, destination);
            }
        }
    }

    private void drawGrid(DrawingContext context, int width, int height)
    {
        for (int x = 0; x <= width; x++)
        {
            double position = x * (double)tileSize;
            context.DrawLine(GridPen, new Point(position, 0), new Point(position, height * (double)tileSize));
        }
        for (int y = 0; y <= height; y++)
        {
            double position = y * (double)tileSize;
            context.DrawLine(GridPen, new Point(0, position), new Point(width * (double)tileSize, position));
        }
    }

    private Bitmap? getTileset(string? key)
    {
        if (string.IsNullOrWhiteSpace(key)
            || !gameData.TilesetData.TryGetValue(key, out JsonObject? tilesetData))
        {
            return null;
        }
        string? fileName = getString(tilesetData["fileName"]);
        if (string.IsNullOrWhiteSpace(fileName))
            return null;
        string path = Path.Combine(gameData.ProjectPath, "Assets", "Tilesets", fileName);
        if (tilesetCache.TryGetValue(path, out Bitmap? cached))
            return cached;
        if (!File.Exists(path))
            return null;
        Bitmap bitmap = new(path);
        tilesetCache[path] = bitmap;
        return bitmap;
    }

    private void updateContentSize()
    {
        if (TryGetMapSize(out int width, out int height))
        {
            MinWidth = Math.Max(540, width * (double)tileSize);
            MinHeight = Math.Max(360, height * (double)tileSize);
        }
        else
        {
            MinWidth = 540;
            MinHeight = 360;
        }
        InvalidateMeasure();
    }

    private void bindHostScrollViewer(ScrollViewer? scrollViewer)
    {
        hostScrollViewer = scrollViewer;
    }

    private static JsonArray? getRow(JsonArray? grid, int index)
    {
        return grid is not null && index >= 0 && index < grid.Count
            ? grid[index] as JsonArray
            : null;
    }

    private static JsonNode? getValue(JsonArray? row, int index)
    {
        return row is not null && index >= 0 && index < row.Count ? row[index] : null;
    }

    private static int getInt(JsonNode? value)
    {
        return tryGetInt(value, out int result) ? result : 0;
    }

    private static string? getString(JsonNode? value)
    {
        return value is JsonValue scalar && scalar.TryGetValue(out string? text) ? text : null;
    }

    private static bool tryGetInt(JsonNode? value, out int result)
    {
        if (value is JsonValue json)
        {
            if (json.TryGetValue(out int integer))
            {
                result = integer;
                return true;
            }
            if (json.TryGetValue(out long longValue))
            {
                result = (int)Math.Clamp(longValue, int.MinValue, int.MaxValue);
                return true;
            }
            if (json.TryGetValue(out double number) && double.IsFinite(number))
            {
                result = (int)Math.Clamp(number, int.MinValue, int.MaxValue);
                return true;
            }
        }
        return int.TryParse(value?.ToString(), NumberStyles.Integer, CultureInfo.InvariantCulture, out result);
    }

    private readonly record struct MapZoomAnchor(
        double MapX,
        double MapY,
        Point ViewportPoint);
}
