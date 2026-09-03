using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using Avalonia.Platform;
using Ludork.Plugin.Avalonia;
using Ludork.Services;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Threading.Tasks;

namespace Ludork.Controls;

public sealed class UiPreviewNodeEventArgs : EventArgs
{
    public UiPreviewNodeEventArgs(string nodeName)
    {
        NodeName = nodeName;
    }

    public string NodeName { get; }
}

public sealed class UiPreviewTransformEventArgs : EventArgs
{
    public UiPreviewTransformEventArgs(
        string nodeName,
        double deltaX,
        double deltaY,
        bool resize)
    {
        NodeName = nodeName;
        DeltaX = deltaX;
        DeltaY = deltaY;
        Resize = resize;
    }

    public string NodeName { get; }
    public double DeltaX { get; }
    public double DeltaY { get; }
    public bool Resize { get; }
}

public sealed class UiPreviewSurface : UserControl
{
    private readonly Border viewport;
    private readonly Grid content;
    private readonly Image image;
    private readonly Canvas overlay;
    private readonly Border selectionBorder;
    private readonly Border resizeHandle;
    private readonly Border anchorMinVertical;
    private readonly Border anchorMaxVertical;
    private readonly Border anchorMinHorizontal;
    private readonly Border anchorMaxHorizontal;
    private readonly TextBlock statusText;
    private readonly ScaleTransform scaleTransform = new();
    private readonly TranslateTransform translateTransform = new();
    private readonly EditorZoomInput zoomInput = new();
    private readonly List<UiPreviewNodeGeometry> nodes = [];
    private WriteableBitmap? bitmap;
    private string? selectedNodeName;
    private Point pointerStart;
    private Point panStart;
    private bool transforming;
    private bool resizing;
    private bool panning;
    private double zoom = 1;
    private long frameGeneration;

    public UiPreviewSurface()
    {
        image = new Image
        {
            Stretch = Stretch.Fill,
            HorizontalAlignment = Avalonia.Layout.HorizontalAlignment.Left,
            VerticalAlignment = Avalonia.Layout.VerticalAlignment.Top,
        };
        RenderOptions.SetBitmapInterpolationMode(image, BitmapInterpolationMode.None);
        selectionBorder = new Border
        {
            BorderBrush = new SolidColorBrush(Color.Parse("#3DAEE9")),
            BorderThickness = new Thickness(1),
            IsHitTestVisible = false,
            IsVisible = false,
        };
        resizeHandle = new Border
        {
            Width = 9,
            Height = 9,
            Background = new SolidColorBrush(Color.Parse("#3DAEE9")),
            BorderBrush = Brushes.White,
            BorderThickness = new Thickness(1),
            Cursor = new Cursor(StandardCursorType.BottomRightCorner),
            IsVisible = false,
        };
        anchorMinVertical = createGuide();
        anchorMaxVertical = createGuide();
        anchorMinHorizontal = createGuide();
        anchorMaxHorizontal = createGuide();
        overlay = new Canvas
        {
            IsHitTestVisible = false,
            Children =
            {
                anchorMinVertical,
                anchorMaxVertical,
                anchorMinHorizontal,
                anchorMaxHorizontal,
                selectionBorder,
                resizeHandle,
            },
        };
        content = new Grid
        {
            HorizontalAlignment = Avalonia.Layout.HorizontalAlignment.Left,
            VerticalAlignment = Avalonia.Layout.VerticalAlignment.Top,
            RenderTransformOrigin = RelativePoint.TopLeft,
            RenderTransform = new TransformGroup
            {
                Children =
                {
                    scaleTransform,
                    translateTransform,
                },
            },
            Children =
            {
                image,
                overlay,
            },
        };
        statusText = new TextBlock
        {
            TextAlignment = TextAlignment.Center,
            TextWrapping = TextWrapping.Wrap,
            Foreground = Brushes.Gray,
            HorizontalAlignment = Avalonia.Layout.HorizontalAlignment.Center,
            VerticalAlignment = Avalonia.Layout.VerticalAlignment.Center,
            Margin = new Thickness(30),
        };
        Grid viewportContent = new()
        {
            Children =
            {
                content,
                statusText,
            },
        };
        viewport = new Border
        {
            Background = new SolidColorBrush(Color.Parse("#181818")),
            BorderBrush = new SolidColorBrush(Color.Parse("#404040")),
            BorderThickness = new Thickness(1),
            ClipToBounds = true,
            Child = viewportContent,
        };
        Content = viewport;
        viewport.PointerPressed += onPointerPressed;
        viewport.PointerMoved += onPointerMoved;
        viewport.PointerReleased += onPointerReleased;
        viewport.PointerWheelChanged += onPointerWheelChanged;
        viewport.PointerTouchPadGestureMagnify += onPointerTouchPadGestureMagnify;
        viewport.PointerCaptureLost += onPointerCaptureLost;
        SetUnavailable(string.Empty);
    }

    public event EventHandler<UiPreviewNodeEventArgs>? NodeSelected;
    public event EventHandler<UiPreviewTransformEventArgs>? TransformStarted;
    public event EventHandler<UiPreviewTransformEventArgs>? TransformChanged;
    public event EventHandler<UiPreviewTransformEventArgs>? TransformCompleted;
    public event EventHandler? TransformCancelled;
    public event EventHandler? ZoomChanged;

    public double Zoom => zoom;
    public double RenderScale => Math.Max(
        0.25,
        zoom * (TopLevel.GetTopLevel(this)?.RenderScaling ?? 1));
    public Func<long, double, double, Task<string?>>? HitTestResolver { get; set; }
    public bool TransformEnabled { get; set; } = true;

    public void SetFrame(UiPreviewFrame frame)
    {
        WriteableBitmap nextBitmap = createBitmap(frame);
        WriteableBitmap? previous = bitmap;
        bitmap = nextBitmap;
        image.Source = bitmap;
        content.Width = frame.DesignWidth;
        content.Height = frame.DesignHeight;
        image.Width = frame.DesignWidth;
        image.Height = frame.DesignHeight;
        overlay.Width = frame.DesignWidth;
        overlay.Height = frame.DesignHeight;
        nodes.Clear();
        nodes.AddRange(frame.Nodes);
        frameGeneration = frame.Generation;
        content.IsVisible = true;
        statusText.IsVisible = false;
        updateSelection();
        previous?.Dispose();
    }

    public void SetUnavailable(string message)
    {
        nodes.Clear();
        frameGeneration = 0;
        statusText.Text = message;
        statusText.IsVisible = true;
        content.IsVisible = false;
        selectionBorder.IsVisible = false;
        resizeHandle.IsVisible = false;
    }

    public void SetSelectedNode(string? nodeName)
    {
        selectedNodeName = nodeName;
        updateSelection();
    }

    public void ResetView()
    {
        bool zoomChanged = Math.Abs(zoom - 1) >= double.Epsilon;
        zoom = 1;
        scaleTransform.ScaleX = 1;
        scaleTransform.ScaleY = 1;
        translateTransform.X = 0;
        translateTransform.Y = 0;
        if (zoomChanged)
            ZoomChanged?.Invoke(this, EventArgs.Empty);
    }

    public void SetAnchorGuides(
        double designWidth,
        double designHeight,
        double minimumX,
        double minimumY,
        double maximumX,
        double maximumY)
    {
        setVerticalGuide(anchorMinVertical, designHeight, minimumX * designWidth);
        setVerticalGuide(anchorMaxVertical, designHeight, maximumX * designWidth);
        setHorizontalGuide(anchorMinHorizontal, designWidth, minimumY * designHeight);
        setHorizontalGuide(anchorMaxHorizontal, designWidth, maximumY * designHeight);
    }

    public void HideAnchorGuides()
    {
        anchorMinVertical.IsVisible = false;
        anchorMaxVertical.IsVisible = false;
        anchorMinHorizontal.IsVisible = false;
        anchorMaxHorizontal.IsVisible = false;
    }

    private async void onPointerPressed(object? sender, PointerPressedEventArgs args)
    {
        PointerPoint point = args.GetCurrentPoint(viewport);
        if (statusText.IsVisible)
            return;
        if (point.Properties.IsMiddleButtonPressed)
        {
            panning = true;
            pointerStart = point.Position;
            panStart = new Point(translateTransform.X, translateTransform.Y);
            args.Pointer.Capture(viewport);
            args.Handled = true;
            return;
        }
        if (!point.Properties.IsLeftButtonPressed)
            return;
        Point designPoint = toDesignPoint(point.Position);
        UiPreviewNodeGeometry? selected = nodes
            .Where(node => string.Equals(
                node.NodeName,
                selectedNodeName,
                StringComparison.Ordinal))
            .OrderByDescending(node => node.DrawOrder)
            .FirstOrDefault();
        if (TransformEnabled && selected is not null && isResizeHandleHit(designPoint))
        {
            beginTransform(args, point, selected.NodeName, designPoint);
            return;
        }
        if (HitTestResolver is null || frameGeneration == 0)
            return;
        long hitTestGeneration = frameGeneration;
        string? hitNodeName = await HitTestResolver(
            hitTestGeneration,
            designPoint.X,
            designPoint.Y);
        if (hitNodeName is null || frameGeneration != hitTestGeneration)
            return;
        if (string.Equals(hitNodeName, selectedNodeName, StringComparison.Ordinal))
        {
            if (!TransformEnabled)
                return;
            PointerPoint currentPoint = args.GetCurrentPoint(viewport);
            if (currentPoint.Properties.IsLeftButtonPressed)
            {
                beginTransform(
                    args,
                    currentPoint,
                    hitNodeName,
                    toDesignPoint(currentPoint.Position));
            }
            return;
        }
        selectedNodeName = hitNodeName;
        updateSelection();
        NodeSelected?.Invoke(this, new UiPreviewNodeEventArgs(hitNodeName));
        args.Handled = true;
    }

    private void beginTransform(
        PointerPressedEventArgs args,
        PointerPoint point,
        string nodeName,
        Point designPoint)
    {
        transforming = true;
        resizing = isResizeHandleHit(designPoint);
        pointerStart = point.Position;
        args.Pointer.Capture(viewport);
        UiPreviewTransformEventArgs transformArgs = new(nodeName, 0, 0, resizing);
        TransformStarted?.Invoke(this, transformArgs);
        args.Handled = true;
    }

    private void onPointerMoved(object? sender, PointerEventArgs args)
    {
        Point current = args.GetPosition(viewport);
        if (panning)
        {
            translateTransform.X = panStart.X + current.X - pointerStart.X;
            translateTransform.Y = panStart.Y + current.Y - pointerStart.Y;
            args.Handled = true;
            return;
        }
        if (!transforming || selectedNodeName is null)
            return;
        double deltaX = (current.X - pointerStart.X) / zoom;
        double deltaY = (current.Y - pointerStart.Y) / zoom;
        TransformChanged?.Invoke(
            this,
            new UiPreviewTransformEventArgs(selectedNodeName, deltaX, deltaY, resizing));
        args.Handled = true;
    }

    private void onPointerReleased(object? sender, PointerReleasedEventArgs args)
    {
        if (panning)
        {
            panning = false;
            args.Pointer.Capture(null);
            args.Handled = true;
            return;
        }
        if (!transforming || selectedNodeName is null)
            return;
        Point current = args.GetPosition(viewport);
        double deltaX = (current.X - pointerStart.X) / zoom;
        double deltaY = (current.Y - pointerStart.Y) / zoom;
        transforming = false;
        args.Pointer.Capture(null);
        TransformCompleted?.Invoke(
            this,
            new UiPreviewTransformEventArgs(selectedNodeName, deltaX, deltaY, resizing));
        args.Handled = true;
    }

    private void onPointerCaptureLost(object? sender, PointerCaptureLostEventArgs args)
    {
        bool cancelTransform = transforming;
        panning = false;
        transforming = false;
        if (cancelTransform)
            TransformCancelled?.Invoke(this, EventArgs.Empty);
    }

    private void onPointerWheelChanged(object? sender, PointerWheelEventArgs args)
    {
        if (statusText.IsVisible)
            return;
        if (zoomInput.ShouldSuppressWheel())
        {
            args.Handled = true;
            return;
        }
        if (EditorZoomInput.IsMacOS
            && !EditorZoomInput.HasPrimaryModifier(args.KeyModifiers))
        {
            Vector translation = EditorZoomInput.GetPannedTranslation(
                new Vector(translateTransform.X, translateTransform.Y),
                args.Delta);
            translateTransform.X = translation.X;
            translateTransform.Y = translation.Y;
            args.Handled = true;
            return;
        }
        if (args.Delta.Y == 0)
            return;
        double factor = args.Delta.Y > 0 ? 1.1 : 1 / 1.1;
        Point position = args.GetPosition(viewport);
        applyZoom(position, EditorZoomInput.ScaleByFactor(zoom, factor, 0.25, 4));
        args.Handled = true;
    }

    private void onPointerTouchPadGestureMagnify(
        object? sender,
        PointerDeltaEventArgs args)
    {
        if (!EditorZoomInput.IsMacOS || statusText.IsVisible)
            return;
        zoomInput.MarkMagnify();
        Point position = args.GetPosition(viewport);
        double nextZoom = EditorZoomInput.ScaleByFactor(
            zoom,
            EditorZoomInput.GetMagnifyFactor(args.Delta.Y),
            0.25,
            4);
        applyZoom(position, nextZoom);
        args.Handled = true;
    }

    private void applyZoom(Point position, double nextZoom)
    {
        if (Math.Abs(nextZoom - zoom) < double.Epsilon)
            return;
        Point designBefore = toDesignPoint(position);
        zoom = nextZoom;
        scaleTransform.ScaleX = zoom;
        scaleTransform.ScaleY = zoom;
        Point designAfter = toDesignPoint(position);
        translateTransform.X += (designAfter.X - designBefore.X) * zoom;
        translateTransform.Y += (designAfter.Y - designBefore.Y) * zoom;
        ZoomChanged?.Invoke(this, EventArgs.Empty);
    }

    private Point toDesignPoint(Point point)
    {
        return new Point(
            (point.X - translateTransform.X) / zoom,
            (point.Y - translateTransform.Y) / zoom);
    }

    private void updateSelection()
    {
        UiPreviewNodeGeometry? geometry = nodes
            .Where(node => string.Equals(node.NodeName, selectedNodeName, StringComparison.Ordinal))
            .OrderByDescending(node => node.DrawOrder)
            .FirstOrDefault();
        if (geometry is null)
        {
            selectionBorder.IsVisible = false;
            resizeHandle.IsVisible = false;
            return;
        }
        selectionBorder.IsVisible = true;
        resizeHandle.IsVisible = TransformEnabled;
        selectionBorder.Width = Math.Max(0, geometry.Width);
        selectionBorder.Height = Math.Max(0, geometry.Height);
        Canvas.SetLeft(selectionBorder, geometry.X);
        Canvas.SetTop(selectionBorder, geometry.Y);
        Canvas.SetLeft(resizeHandle, geometry.X + geometry.Width - resizeHandle.Width / 2);
        Canvas.SetTop(resizeHandle, geometry.Y + geometry.Height - resizeHandle.Height / 2);
    }

    private bool isResizeHandleHit(Point point)
    {
        double left = Canvas.GetLeft(resizeHandle);
        double top = Canvas.GetTop(resizeHandle);
        return resizeHandle.IsVisible
            && new Rect(left - 3, top - 3, resizeHandle.Width + 6, resizeHandle.Height + 6)
                .Contains(point);
    }

    private static WriteableBitmap createBitmap(UiPreviewFrame frame)
    {
        WriteableBitmap result = new(
            new PixelSize(frame.Width, frame.Height),
            new Vector(96, 96),
            PixelFormat.Bgra8888,
            AlphaFormat.Premul);
        using ILockedFramebuffer target = result.Lock();
        if (target.Format != PixelFormat.Bgra8888
            && target.Format != PixelFormat.Rgba8888)
        {
            result.Dispose();
            throw new NotSupportedException($"Unsupported UI preview pixel format: {target.Format}");
        }
        byte[] row = new byte[target.RowBytes];
        for (int y = 0; y < frame.Height; y++)
        {
            Array.Clear(row);
            int sourceOffset = y * frame.Stride;
            int pixelBytes = Math.Min(frame.Width * 4, target.RowBytes);
            Buffer.BlockCopy(frame.Pixels, sourceOffset, row, 0, pixelBytes);
            if (target.Format == PixelFormat.Rgba8888)
                swapRedBlue(row, pixelBytes);
            Marshal.Copy(row, 0, target.Address + y * target.RowBytes, target.RowBytes);
        }
        return result;
    }

    private static void swapRedBlue(byte[] pixels, int length)
    {
        for (int index = 0; index + 3 < length; index += 4)
            (pixels[index], pixels[index + 2]) = (pixels[index + 2], pixels[index]);
    }

    private static Border createGuide()
    {
        return new Border
        {
            Background = new SolidColorBrush(Color.Parse("#3DAEE9")),
            Opacity = 0.45,
            IsHitTestVisible = false,
            IsVisible = false,
        };
    }

    private static void setVerticalGuide(
        Border guide,
        double height,
        double position)
    {
        guide.Width = 1;
        guide.Height = Math.Max(0, height);
        Canvas.SetLeft(guide, position);
        Canvas.SetTop(guide, 0);
        guide.IsVisible = true;
    }

    private static void setHorizontalGuide(
        Border guide,
        double width,
        double position)
    {
        guide.Width = Math.Max(0, width);
        guide.Height = 1;
        Canvas.SetLeft(guide, 0);
        Canvas.SetTop(guide, position);
        guide.IsVisible = true;
    }
}
