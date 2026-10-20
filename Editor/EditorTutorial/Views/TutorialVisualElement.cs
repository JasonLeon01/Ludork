using Avalonia;
using Avalonia.Controls;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using Avalonia.Platform;
using Avalonia.Svg;
using Ludork.EditorTutorial.Models;
using Ludork.EditorTutorial.Playback;
using Ludork.Services;
using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;

namespace Ludork.EditorTutorial.Views;

internal sealed class TutorialVisualElement : IDisposable
{
    private readonly TutorialElement definition;
    private readonly Dictionary<string, TutorialTrack> tracks;
    private readonly List<TutorialVisualElement> children = [];
    private readonly IBrush? fill;
    private readonly Pen? pen;
    private readonly Typeface typeface;
    private readonly IImage? image;
    private readonly bool ownsImage;
    private readonly TutorialPlaybackContext? playbackContext;

    public TutorialVisualElement(TutorialElement definition, TutorialScene scene, TutorialPlaybackContext? playbackContext)
    {
        this.definition = definition;
        this.playbackContext = playbackContext;
        tracks = scene.Tracks.Where(track => track.Target == definition.Id)
            .ToDictionary(track => track.Property, StringComparer.Ordinal);
        fill = resolveBrush(definition.Fill);
        IBrush? stroke = resolveBrush(definition.Stroke);
        pen = stroke is null ? null : new Pen(stroke, definition.StrokeThickness);
        typeface = new Typeface(Application.Current?.FindResource("Editor.FontFamily") as FontFamily
            ?? FontFamily.Default);
        if (definition.Type == "image")
        {
            if (definition.ImageKey is { } key)
                image = playbackContext!.GetImage(key);
            else
            {
                Uri uri = TutorialCatalog.GetAssetUri(definition.Asset!);
                using Stream stream = AssetLoader.Open(uri);
                if (uri.AbsolutePath.EndsWith(".svg", StringComparison.OrdinalIgnoreCase))
                    image = new SvgImage { Source = SvgSource.Load(stream) };
                else
                    image = new Bitmap(stream);
                ownsImage = true;
            }
        }
    }

    public void AddChild(TutorialVisualElement child) => children.Add(child);

    public void Draw(DrawingContext context, Size parentSize, double time)
    {
        double x = length("x", definition.X, parentSize.Width, time);
        double y = length("y", definition.Y, parentSize.Height, time);
        double width = Math.Max(0, length("width", definition.Width, parentSize.Width, time));
        double height = Math.Max(0, length("height", definition.Height, parentSize.Height, time));
        TutorialLayout? layout = definition.LayoutKey is { } key ? playbackContext!.GetLayout(key) : null;
        if (layout is { } placement)
        {
            width = placement.Size.Width;
            height = placement.Size.Height;
        }
        double opacity = Math.Clamp(number("opacity", definition.Opacity, time), 0, 1);
        Matrix transform = Matrix.CreateScale(number("scaleX", definition.ScaleX, time),
                number("scaleY", definition.ScaleY, time))
            * Matrix.CreateRotation(number("rotation", definition.Rotation, time) * Math.PI / 180)
            * Matrix.CreateTranslation(x, y) * (layout?.Transform ?? Matrix.Identity);
        using (context.PushTransform(transform))
        using (context.PushOpacity(opacity))
        {
            Rect bounds = new(0, 0, width, height);
            switch (definition.Type)
            {
                case "rectangle":
                    context.DrawRectangle(fill, pen, bounds);
                    break;
                case "ellipse":
                    context.DrawEllipse(fill, pen, bounds);
                    break;
                case "line":
                    if (pen is not null)
                        context.DrawLine(pen, default, new Point(width, height));
                    break;
                case "image":
                    if (image is not null && width > 0 && height > 0)
                        context.DrawImage(image, imageBounds(bounds));
                    break;
                case "text":
                    if (width > 0 && height > 0)
                    {
                        FormattedText text = new(TutorialText.Format(definition, OperatingSystem.IsMacOS()),
                            CultureInfo.CurrentCulture, FlowDirection.LeftToRight, typeface,
                            definition.FontSize, fill ?? Brushes.White)
                        {
                            MaxTextWidth = width,
                            MaxTextHeight = height,
                            TextAlignment = TextAlignment.Center,
                            Trimming = TextTrimming.None
                        };
                        context.DrawText(text, new Point(0, Math.Max(0, (height - text.Height) / 2)));
                    }
                    break;
            }
            foreach (TutorialVisualElement child in children)
                child.Draw(context, bounds.Size, time);
        }
    }

    private Rect imageBounds(Rect bounds)
    {
        if (definition.Stretch != "uniform" || image is null || image.Size.Width <= 0 || image.Size.Height <= 0)
            return bounds;
        double scale = Math.Min(bounds.Width / image.Size.Width, bounds.Height / image.Size.Height);
        Size size = image.Size * scale;
        double y = definition.ImageVerticalAlignment switch
        {
            "top" => 0,
            "bottom" => bounds.Height - size.Height,
            _ => (bounds.Height - size.Height) / 2
        };
        return new Rect(new Point((bounds.Width - size.Width) / 2, y), size);
    }

    private double length(string property, TutorialValue fallback, double extent, double time)
    {
        TutorialValue value = tracks.TryGetValue(property, out TutorialTrack? track)
            ? TutorialTimeline.Evaluate(track, time) : fallback;
        return value.Value * extent + value.Offset;
    }

    private double number(string property, double fallback, double time) =>
        tracks.TryGetValue(property, out TutorialTrack? track)
            ? TutorialTimeline.Evaluate(track, time).Value : fallback;

    private static IBrush? resolveBrush(string? key)
    {
        if (string.IsNullOrEmpty(key))
            return null;
        return Application.Current?.FindResource(key) as IBrush
            ?? throw new InvalidDataException($"Tutorial brush '{key}' was not found.");
    }

    public void Dispose()
    {
        foreach (TutorialVisualElement child in children)
            child.Dispose();
        if (ownsImage && image is IDisposable resource)
            resource.Dispose();
    }
}
