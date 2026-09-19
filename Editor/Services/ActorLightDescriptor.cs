using Avalonia;
using Avalonia.Media;

namespace Ludork.Services;

public sealed record ActorLightDescriptor(
    Point LocalPosition,
    Vector Translation,
    Vector Scale,
    double Rotation,
    double Radius,
    Color Colour);
