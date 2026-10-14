using System;

namespace Ludork.Plugin.Abstractions;

public interface IPluginActorPreview : IDisposable
{
    PluginActorPreviewFrame? Frame { get; }
    event EventHandler? Changed;
    bool IsActive { get; set; }
}

public sealed record PluginActorPreviewFrame(int Width, int Height, byte[] Pixels,
    double OffsetX, double OffsetY, string? Error);
