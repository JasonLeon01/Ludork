using Avalonia;
using Avalonia.Media;
using System;

namespace Ludork.Services;

public sealed record ActorLightDescriptor(
    Point LocalPosition,
    Vector Translation,
    Vector Scale,
    double Rotation,
    double Radius,
    Color Colour)
{
    public Point GetWorldPosition(Point actorPosition)
    {
        double angle = Rotation * Math.PI / 180;
        double localX = LocalPosition.X * Scale.X;
        double localY = LocalPosition.Y * Scale.Y;
        return new Point(actorPosition.X + Translation.X + localX * Math.Cos(angle) - localY * Math.Sin(angle),
            actorPosition.Y + Translation.Y + localX * Math.Sin(angle) + localY * Math.Cos(angle));
    }
}
