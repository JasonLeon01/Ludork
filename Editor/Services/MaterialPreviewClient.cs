using System;
using System.IO;
using System.Text.Json.Nodes;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Services;

public sealed class MaterialPreviewClient(PreviewRuntimeService runtime) : IAsyncDisposable
{
    private readonly PreviewHostConnection connection = new(runtime);
    private bool synchronized;
    private long connectionGeneration = -1;
    private long materialRevision = -1;
    private string? materialPath;
    private long diagnosticsRevision = -1;
    private JsonArray diagnostics = [];
    private string generatedShader = string.Empty;

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

    public async Task<MaterialPreviewFrame> RenderAsync(JsonObject request, long nextMaterialRevision,
        string nextMaterialPath, JsonObject asset, CancellationToken cancellationToken)
    {
        if (!await connection.StartAsync(cancellationToken))
            throw new InvalidOperationException(connection.StatusMessage);
        if (!connection.Capabilities.Contains("material") || !connection.Capabilities.Contains("materialSnapshots"))
            throw new InvalidOperationException(LocaleService.Get("UI_PREVIEW_SNAPSHOT_INVALID"));
        long nextConnectionGeneration = connection.ConnectionGeneration;
        bool sendSnapshot = !synchronized || connectionGeneration != nextConnectionGeneration
            || materialRevision != nextMaterialRevision || materialPath != nextMaterialPath;
        synchronized = false;
        request["type"] = "renderMaterial";
        request["materialRevision"] = nextMaterialRevision;
        if (sendSnapshot)
        {
            request["materialPath"] = nextMaterialPath;
            request["asset"] = asset.DeepClone();
        }
        JsonObject response = await connection.ExchangeAsync(request, cancellationToken);
        if (!connection.IsReady || connection.ConnectionGeneration != nextConnectionGeneration)
            throw new IOException("Material preview connection changed.");
        if (response["type"]?.GetValue<string>() != "materialFrame"
            || response["generation"]?.GetValue<long>() != request["generation"]!.GetValue<long>()
            || response["materialRevision"]?.GetValue<long>() != nextMaterialRevision)
            throw new InvalidDataException(response["message"]?.GetValue<string>() ?? "Invalid material preview response");
        if (response["pixelFormat"]?.GetValue<string>() != "Bgra8888Premultiplied")
            throw new InvalidDataException("Unsupported material preview pixel format");
        int width = response["width"]?.GetValue<int>() ?? 0;
        int height = response["height"]?.GetValue<int>() ?? 0;
        int stride = response["stride"]?.GetValue<int>() ?? 0;
        if (width <= 0 || height <= 0 || width > 4096 || height > 4096 || stride != checked(width * 4)
            || width != request["width"]!.GetValue<int>() || height != request["height"]!.GetValue<int>())
            throw new InvalidDataException("Invalid material preview frame size");
        long nextDiagnosticsRevision = response["diagnosticsRevision"]?.GetValue<long>() ?? -1;
        if (nextDiagnosticsRevision < 0
            || connectionGeneration == nextConnectionGeneration && nextDiagnosticsRevision < diagnosticsRevision)
            throw new InvalidDataException("Invalid material preview diagnostics revision");
        JsonArray nextDiagnostics;
        if (response["diagnostics"] is JsonArray errors)
        {
            foreach (JsonNode? item in errors)
                if (item is not JsonObject error || error["message"]?.GetValue<string>() is null
                    || error["nodeId"]?.GetValue<string>() is null || error["portId"]?.GetValue<string>() is null)
                    throw new InvalidDataException("Invalid material preview diagnostic");
            nextDiagnostics = errors;
        }
        else if (!sendSnapshot && nextDiagnosticsRevision == diagnosticsRevision)
            nextDiagnostics = diagnostics;
        else
            throw new InvalidDataException("Material preview diagnostics are missing");
        string nextShader = generatedShader;
        if (response["generatedVertex"] is JsonValue vertex && response["generatedFragment"] is JsonValue fragment)
            nextShader = vertex.GetValue<string>() + "\n\n" + fragment.GetValue<string>();
        else if (sendSnapshot || response.ContainsKey("generatedVertex") || response.ContainsKey("generatedFragment"))
            throw new InvalidDataException("Material preview shader sources are missing");
        byte[] pixels = connection.ReadPixels(response, checked(stride * height));
        response.Remove("diagnostics");
        connectionGeneration = nextConnectionGeneration;
        materialRevision = nextMaterialRevision;
        materialPath = nextMaterialPath;
        diagnosticsRevision = nextDiagnosticsRevision;
        diagnostics = nextDiagnostics;
        generatedShader = nextShader;
        synchronized = true;
        return new MaterialPreviewFrame(width, height, stride, pixels, connectionGeneration, materialRevision,
            diagnosticsRevision, diagnostics, generatedShader);
    }

    public ValueTask DisposeAsync() => connection.DisposeAsync();
}
