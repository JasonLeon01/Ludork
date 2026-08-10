using System;
using System.Collections.Generic;

namespace Ludork.Services;

public sealed record MainFrameTiming(
    double Time,
    double Interval,
    double Active,
    double Runtime,
    double SceneOps,
    double Input,
    double UiUpdate,
    double RenderCpu,
    double PresentWait,
    double LateUpdate,
    double Audio);

public sealed record LogicTickTiming(
    double Time,
    double Iteration,
    double SceneTick,
    double Maintenance,
    double FixedTick,
    double Sleep,
    int FixedSteps);

public sealed record PerformanceSample(double Fps, double MemoryMegabytes)
{
    public int ProtocolVersion { get; init; } = 2;
    public int SampleFrames { get; init; } = 30;
    public double? TargetFps { get; init; }
    public IReadOnlyList<MainFrameTiming> MainFrames { get; init; } = Array.Empty<MainFrameTiming>();
    public IReadOnlyList<LogicTickTiming> LogicTicks { get; init; } = Array.Empty<LogicTickTiming>();
    public long DroppedLogicTicks { get; init; }
}
