using System;
using System.IO;
using System.Text.Json.Nodes;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Services;

public sealed class MaterialPreviewClient(PreviewRuntimeService runtime) : IAsyncDisposable
{
    private readonly PreviewHostConnection connection = new(runtime);

    public async Task<JsonObject> DescribeAsync(CancellationToken cancellationToken)
    {
        if (!await connection.StartAsync(cancellationToken))
            throw new InvalidOperationException(connection.StatusMessage);
        JsonObject response = await connection.ExchangeAsync(new JsonObject { ["type"] = "describeMaterials" }, cancellationToken);
        if (response["registry"] is not JsonObject registry)
            throw new InvalidDataException(response["message"]?.GetValue<string>() ?? "Material registry is unavailable");
        if (registry["defaultMaterial"] is null && response["defaultMaterial"] is JsonObject template)
            registry["defaultMaterial"] = template.DeepClone();
        return registry;
    }

    public async Task<(JsonObject Response, byte[]? Pixels)> RenderAsync(JsonObject request, CancellationToken cancellationToken)
    {
        if (!await connection.StartAsync(cancellationToken))
            throw new InvalidOperationException(connection.StatusMessage);
        request["type"] = "renderMaterial";
        JsonObject response = await connection.ExchangeAsync(request, cancellationToken);
        if (response["type"]?.GetValue<string>() != "materialFrame")
            return (response, null);
        if (response["pixelFormat"]?.GetValue<string>() != "Bgra8888Premultiplied")
            throw new InvalidDataException("Unsupported material preview pixel format");
        int width = response["width"]?.GetValue<int>() ?? 0;
        int height = response["height"]?.GetValue<int>() ?? 0;
        int stride = response["stride"]?.GetValue<int>() ?? 0;
        if (width <= 0 || height <= 0 || width > 4096 || height > 4096 || stride != checked(width * 4))
            throw new InvalidDataException("Invalid material preview frame size");
        return (response, connection.ReadPixels(response, checked(stride * height)));
    }

    public ValueTask DisposeAsync() => connection.DisposeAsync();
}
