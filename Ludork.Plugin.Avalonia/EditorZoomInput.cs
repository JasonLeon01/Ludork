using Avalonia;
using Avalonia.Input;
using System;
using System.Diagnostics;

namespace Ludork.Plugin.Avalonia;

public sealed class EditorZoomInput
{
    private const double ScrollDipPerDelta = 50.0;
    private static readonly TimeSpan WheelSuppressionDuration = TimeSpan.FromMilliseconds(200);
    private long lastMagnifyTimestamp;

    public static bool IsMacOS => OperatingSystem.IsMacOS();

    public static bool HasPrimaryModifier(KeyModifiers modifiers)
    {
        KeyModifiers primaryModifier = IsMacOS
            ? KeyModifiers.Meta
            : KeyModifiers.Control;
        return modifiers.HasFlag(primaryModifier);
    }

    public static bool ShouldZoomWheel(
        KeyModifiers modifiers,
        bool primaryModifierRequired)
    {
        if (IsMacOS)
            return HasPrimaryModifier(modifiers);
        return !primaryModifierRequired || HasPrimaryModifier(modifiers);
    }

    public static double GetMagnifyFactor(double delta)
    {
        return Math.Max(0.01, 1.0 + delta);
    }

    public static double ScaleByFactor(
        double value,
        double factor,
        double minimum,
        double maximum)
    {
        return Math.Clamp(value * factor, minimum, maximum);
    }

    public static Vector GetAnchoredOffset(
        Point contentAnchor,
        Point viewportPoint,
        Size extent,
        Size viewport)
    {
        double maximumX = Math.Max(0, extent.Width - viewport.Width);
        double maximumY = Math.Max(0, extent.Height - viewport.Height);
        return new Vector(
            Math.Clamp(contentAnchor.X - viewportPoint.X, 0, maximumX),
            Math.Clamp(contentAnchor.Y - viewportPoint.Y, 0, maximumY));
    }

    public static Vector GetPannedOffset(
        Vector currentOffset,
        Vector wheelDelta,
        Size extent,
        Size viewport)
    {
        Point contentAnchor = new(
            currentOffset.X - wheelDelta.X * ScrollDipPerDelta,
            currentOffset.Y - wheelDelta.Y * ScrollDipPerDelta);
        return GetAnchoredOffset(contentAnchor, default, extent, viewport);
    }

    public static Vector GetPannedTranslation(
        Vector currentTranslation,
        Vector wheelDelta)
    {
        return currentTranslation + GetPanTranslation(wheelDelta);
    }

    public static Vector GetPanTranslation(Vector wheelDelta)
    {
        return wheelDelta * ScrollDipPerDelta;
    }

    public void MarkMagnify()
    {
        lastMagnifyTimestamp = Stopwatch.GetTimestamp();
    }

    public bool ShouldSuppressWheel()
    {
        return IsMacOS
            && lastMagnifyTimestamp != 0
            && Stopwatch.GetElapsedTime(lastMagnifyTimestamp) < WheelSuppressionDuration;
    }
}
