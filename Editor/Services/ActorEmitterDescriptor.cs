using Avalonia;

namespace Ludork.Services;

public sealed record ActorEmitterDescriptor(
    string Resource,
    Vector Anchor,
    Vector Offset,
    double Rotation,
    Vector Scale,
    bool BeforeActor,
    Size Bounds);
