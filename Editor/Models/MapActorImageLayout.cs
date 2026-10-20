using Avalonia;

namespace Ludork.Models;

internal readonly record struct MapActorImageLayout(Matrix Transform, Point Pivot, Rect GridBounds);
