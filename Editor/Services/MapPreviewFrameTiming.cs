namespace Ludork.Services;

public sealed record MapPreviewFrameTiming(
    double ChangeToPublishMilliseconds,
    double SceneUpdateMilliseconds,
    double SerializationMilliseconds,
    double SceneApplyMilliseconds,
    double RenderMilliseconds,
    double ReadbackMilliseconds,
    double TransferMilliseconds,
    double PixelReadMilliseconds,
    double BitmapMilliseconds);
