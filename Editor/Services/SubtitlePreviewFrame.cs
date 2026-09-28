namespace Ludork.Services;

public sealed record SubtitlePreviewFrame(int Width, int Height, int Stride, byte[] Pixels,
    double Time, double Duration, double VideoDuration, bool Playing);
