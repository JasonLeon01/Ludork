using System;
using System.IO;
using System.Text.Json.Nodes;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Services;

public sealed class ParticlePreviewClient : IAsyncDisposable
{
    private readonly PreviewHostConnection connection;
    private readonly string sessionId = Guid.NewGuid().ToString("N");

    public ParticlePreviewClient(UiPreviewRuntimeService runtime)
    {
        connection = new PreviewHostConnection(runtime);
    }

    public async Task<ParticlePreviewFrame?> RenderAsync(
        string command, long generation, int width, int height, double zoom, double speed,
        string assetKey, JsonObject? asset, double time, double deltaTime,
        CancellationToken cancellationToken)
    {
        if (!await connection.StartAsync(cancellationToken))
            throw new InvalidOperationException(connection.StatusMessage);
        if (!connection.Capabilities.Contains("particle"))
            throw new InvalidOperationException(LocaleService.Get("PARTICLE_PREVIEW_UNAVAILABLE"));
        JsonObject request = new()
        {
            ["type"] = "renderParticle", ["sessionId"] = sessionId, ["generation"] = generation,
            ["command"] = command, ["width"] = width, ["height"] = height,
            ["zoom"] = zoom, ["speed"] = speed, ["time"] = time, ["deltaTime"] = deltaTime,
        };
        if (asset is not null)
        {
            request["assetKey"] = assetKey;
            request["asset"] = asset.DeepClone();
            request["asset"]!["type"] = "particle";
        }
        JsonObject response = await connection.ExchangeAsync(request, CancellationToken.None);
        cancellationToken.ThrowIfCancellationRequested();
        if (response["type"]?.GetValue<string>() == "particleStale")
            return null;
        if (response["type"]?.GetValue<string>() != "particleFrame")
            throw new InvalidDataException(response["message"]?.GetValue<string>() ?? "Invalid particle preview response");
        if (response["generation"]?.GetValue<long>() != generation)
            return null;
        int frameWidth = response["width"]?.GetValue<int>() ?? 0;
        int frameHeight = response["height"]?.GetValue<int>() ?? 0;
        int stride = response["stride"]?.GetValue<int>() ?? 0;
        if (frameWidth <= 0 || frameHeight <= 0 || frameWidth > 4096 || frameHeight > 4096
            || stride != checked(frameWidth * 4))
            throw new InvalidDataException("Invalid particle preview frame size");
        byte[] pixels = connection.ReadPixels(response, checked(stride * frameHeight));
        JsonObject stats = response["stats"] as JsonObject ?? new JsonObject();
        return new ParticlePreviewFrame(frameWidth, frameHeight, stride, pixels,
            response["time"]?.GetValue<double>() ?? 0, response["seeking"]?.GetValue<bool>() == true,
            stats["capacity"]?.GetValue<int>() ?? 0, stats["aliveCount"]?.GetValue<int>(),
            stats["sampledTime"]?.GetValue<double>(),
            stats["gpuSimulationMs"]?.GetValue<double>(), stats["gpuDrawMs"]?.GetValue<double>(),
            response["previewReadbackMs"]?.GetValue<double>() ?? 0,
            response["previewTransferMs"]?.GetValue<double>() ?? 0);
    }

    public ValueTask DisposeAsync() => connection.DisposeAsync();
}
