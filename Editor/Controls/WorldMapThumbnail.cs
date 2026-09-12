using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Controls.Templates;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Threading;
using Avalonia.VisualTree;
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

internal sealed class WorldMapThumbnail : Control
{
    private static readonly IBrush BackgroundBrush = Ludork.Services.EditorTheme.Brush("Background");
    private static readonly Pen BorderPen = new(EditorTheme.Brush("Border"), 1);
    private readonly WorldMapPreviewRenderer? renderer;
    private readonly WorldMapChildSource child;

    public WorldMapThumbnail(WorldMapPreviewRenderer? renderer, WorldMapChildSource child)
    {
        this.renderer = renderer;
        this.child = child;
        ClipToBounds = true;
    }

    protected override void OnAttachedToVisualTree(VisualTreeAttachmentEventArgs args)
    {
        base.OnAttachedToVisualTree(args);
        child.DataChanged += onPreviewChanged;
        if (renderer is not null)
            renderer.PreviewChanged += onPreviewChanged;
    }

    protected override void OnDetachedFromVisualTree(VisualTreeAttachmentEventArgs args)
    {
        child.DataChanged -= onPreviewChanged;
        child.ReleaseData(this);
        if (renderer is not null)
            renderer.PreviewChanged -= onPreviewChanged;
        base.OnDetachedFromVisualTree(args);
    }

    public override void Render(DrawingContext context)
    {
        context.FillRectangle(BackgroundBrush, Bounds);
        int width = Math.Max(0, child.Width);
        int height = Math.Max(0, child.Height);
        if (width <= 0 || height <= 0)
            return;
        double cellSize = Math.Max(0.25, Math.Min((Bounds.Width - 8) / width, (Bounds.Height - 8) / height));
        Rect mapRect = new(
            (Bounds.Width - width * cellSize) / 2,
            (Bounds.Height - height * cellSize) / 2,
            width * cellSize,
            height * cellSize);
        if (child.RequestData(this) is JsonObject map)
            renderer?.DrawMap(context, child.Key, map, mapRect.TopLeft, cellSize, Bounds);
        context.DrawRectangle(null, BorderPen, mapRect);
    }

    private void onPreviewChanged(object? sender, EventArgs args)
    {
        if (!Dispatcher.UIThread.CheckAccess())
        {
            Dispatcher.UIThread.Post(() => onPreviewChanged(sender, args));
            return;
        }
        InvalidateVisual();
    }
}
