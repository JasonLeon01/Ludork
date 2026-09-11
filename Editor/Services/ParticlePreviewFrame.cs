namespace Ludork.Services;

public sealed record ParticlePreviewFrame(
    int Width,
    int Height,
    int Stride,
    byte[] Pixels,
    double Time,
    bool Seeking,
    int Capacity,
    int? AliveCount,
    double? SampledTime,
    double? GpuSimulationMilliseconds,
    double? GpuDrawMilliseconds,
    double ReadbackMilliseconds,
    double TransferMilliseconds);
