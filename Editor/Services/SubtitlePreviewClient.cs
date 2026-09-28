using System;
using System.IO;
using System.Text.Json.Nodes;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Services;

public sealed class SubtitlePreviewClient : IAsyncDisposable
{
    private readonly PreviewHostConnection connection;
    private readonly string sessionId = Guid.NewGuid().ToString("N");

    public SubtitlePreviewClient(UiPreviewRuntimeService runtime) => connection = new PreviewHostConnection(runtime);

    public async Task<SubtitlePreviewFrame?> RenderAsync(string command, long generation, int width, int height,
        JsonObject? asset, string language, string videoPath, bool mute, double time, CancellationToken cancellationToken)
    {
        if (!await connection.StartAsync(cancellationToken))
            throw new InvalidOperationException(connection.StatusMessage);
        if (!connection.Capabilities.Contains("subtitle"))
            throw new InvalidOperationException(LocaleService.Get("SUBTITLE_PREVIEW_UNAVAILABLE"));
        JsonObject request = new()
        {
            ["type"] = "renderSubtitle", ["sessionId"] = sessionId, ["generation"] = generation,
            ["command"] = command, ["width"] = width, ["height"] = height,
            ["language"] = language, ["videoPath"] = videoPath, ["mute"] = mute, ["time"] = time,
        };
        if (asset is not null)
            request["asset"] = asset.DeepClone();
        JsonObject response = await connection.ExchangeAsync(request, CancellationToken.None);
        cancellationToken.ThrowIfCancellationRequested();
        if (response["type"]?.GetValue<string>() == "subtitleStale")
            return null;
        if (response["type"]?.GetValue<string>() != "subtitleFrame")
            throw new InvalidDataException(response["message"]?.GetValue<string>() ?? "Invalid subtitle preview response");
        if (response["generation"]?.GetValue<long>() != generation)
            return null;
        int frameWidth = response["width"]?.GetValue<int>() ?? 0;
        int frameHeight = response["height"]?.GetValue<int>() ?? 0;
        int stride = response["stride"]?.GetValue<int>() ?? 0;
        if (frameWidth <= 0 || frameHeight <= 0 || frameWidth > 4096 || frameHeight > 4096 || stride != checked(frameWidth * 4))
            throw new InvalidDataException("Invalid subtitle preview frame size");
        byte[] pixels = connection.ReadPixels(response, checked(stride * frameHeight));
        return new SubtitlePreviewFrame(frameWidth, frameHeight, stride, pixels,
            response["time"]?.GetValue<double>() ?? 0, response["duration"]?.GetValue<double>() ?? 0,
            response["videoDuration"]?.GetValue<double>() ?? 0, response["playing"]?.GetValue<bool>() == true);
    }

    public ValueTask DisposeAsync() => connection.DisposeAsync();
}
