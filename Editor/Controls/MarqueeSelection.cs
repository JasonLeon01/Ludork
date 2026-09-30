using Avalonia;
using System;
using System.Collections.Generic;

namespace Ludork.Controls;

internal static class MarqueeSelection
{
    public static Rect CreateRect(Point start, Point finish, Rect bounds)
    {
        double left = Math.Clamp(Math.Min(start.X, finish.X), bounds.Left, bounds.Right);
        double top = Math.Clamp(Math.Min(start.Y, finish.Y), bounds.Top, bounds.Bottom);
        double right = Math.Clamp(Math.Max(start.X, finish.X), bounds.Left, bounds.Right);
        double bottom = Math.Clamp(Math.Max(start.Y, finish.Y), bounds.Top, bounds.Bottom);
        return new Rect(left, top, right - left, bottom - top);
    }

    public static bool Overlaps(Rect first, Rect second)
    {
        return first.Left <= second.Right
            && first.Right >= second.Left
            && first.Top <= second.Bottom
            && first.Bottom >= second.Top;
    }

    public static HashSet<T> Build<T>(
        IEnumerable<T> initialSelection,
        IEnumerable<T> hitItems,
        bool additive,
        bool toggle
    ) where T : notnull
    {
        HashSet<T> selection = additive || toggle
            ? [.. initialSelection]
            : [];
        foreach (T item in hitItems)
        {
            if (toggle && !selection.Add(item))
                selection.Remove(item);
            else if (!toggle)
                selection.Add(item);
        }
        return selection;
    }
}
