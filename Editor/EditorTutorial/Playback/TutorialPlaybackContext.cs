using Avalonia;
using Avalonia.Media.Imaging;
using Ludork.EditorTutorial.Models;
using System;
using System.Collections.Generic;
using System.Linq;

namespace Ludork.EditorTutorial.Playback;

internal sealed class TutorialPlaybackContext(
    IReadOnlyDictionary<string, Bitmap> images,
    Func<Rect, IReadOnlyDictionary<string, TutorialLayout>?>? resolveLayouts = null) : IDisposable
{
    private bool disposed;
    private IReadOnlyDictionary<string, TutorialLayout> layouts = new Dictionary<string, TutorialLayout>();

    public Bitmap GetImage(string key) => images[key];

    public TutorialLayout GetLayout(string key) => layouts[key];

    public bool UpdateLayouts(Rect anchor, IEnumerable<TutorialElement> elements)
    {
        if (disposed)
            return false;
        if (resolveLayouts is not null)
        {
            if (resolveLayouts(anchor) is not { } next)
                return false;
            layouts = next;
        }
        return hasLayoutsFor(elements);
    }

    private bool hasLayoutsFor(IEnumerable<TutorialElement> elements) => elements.All(element =>
        (element.LayoutKey is null || layouts.TryGetValue(element.LayoutKey, out TutorialLayout layout)
            && double.IsFinite(layout.Size.Width) && layout.Size.Width > 0
            && double.IsFinite(layout.Size.Height) && layout.Size.Height > 0
            && double.IsFinite(layout.Transform.M11) && double.IsFinite(layout.Transform.M12)
            && double.IsFinite(layout.Transform.M21) && double.IsFinite(layout.Transform.M22)
            && double.IsFinite(layout.Transform.M31) && double.IsFinite(layout.Transform.M32))
        && hasLayoutsFor(element.Children));

    public bool HasImagesFor(IEnumerable<TutorialElement> elements)
    {
        return !disposed && elements.All(element =>
            (element.ImageKey is null || images.TryGetValue(element.ImageKey, out Bitmap? image)
                && image.PixelSize.Width > 0 && image.PixelSize.Height > 0)
            && HasImagesFor(element.Children));
    }

    public void Dispose()
    {
        if (disposed)
            return;
        disposed = true;
        foreach (Bitmap image in images.Values.Distinct())
            image.Dispose();
    }
}
