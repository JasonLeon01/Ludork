using System;
using System.Collections.Concurrent;
using System.Globalization;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Services;

public sealed partial class ProjectRunnerService
{
    private readonly ConcurrentDictionary<string, LiveDebugRequest> liveDebugRequests = new(StringComparer.Ordinal);
    private long nextLiveDebugRequest;

    public async Task<JsonObject?> SendLiveDebugRequestAsync(
        JsonObject request, long generation, long connection, CancellationToken cancellationToken)
    {
        if (!IsCurrentConnection(generation, connection))
            return null;
        string requestId = Interlocked.Increment(ref nextLiveDebugRequest).ToString(CultureInfo.InvariantCulture);
        JsonObject message = (JsonObject)request.DeepClone();
        message["v"] = BridgeProtocolVersion;
        message["type"] = "liveDebug";
        message["run"] = generation.ToString(CultureInfo.InvariantCulture);
        message["requestId"] = requestId;
        byte[] json = JsonSerializer.SerializeToUtf8Bytes(message);
        if (json.Length > MaximumBridgeMessageSize)
            return new JsonObject { ["success"] = false, ["error"] = "Live Debug request exceeds the 64 KiB message limit." };
        LiveDebugRequest pending = new(generation, connection);
        liveDebugRequests[requestId] = pending;
        try
        {
            if (!await sendBridgeBytesAsync(json, generation, connection))
                return null;
            return await pending.Completion.Task.WaitAsync(TimeSpan.FromSeconds(10), cancellationToken);
        }
        catch (TimeoutException)
        {
            return new JsonObject { ["success"] = false, ["error"] = "Live Debug request timed out." };
        }
        catch (OperationCanceledException)
        {
            return null;
        }
        finally
        {
            liveDebugRequests.TryRemove(requestId, out _);
        }
    }

    private string? receiveLiveDebugMessage(JsonElement message, long generation, long connection)
    {
        string? error = validateBridgeEnvelope(message, "liveDebug");
        if (error is not null)
            return error;
        if (!message.TryGetProperty("requestId", out JsonElement requestId)
            || requestId.ValueKind != JsonValueKind.String
            || !message.TryGetProperty("run", out JsonElement run)
            || run.ValueKind != JsonValueKind.String)
            return "invalid Live Debug response identity";
        if (!IsCurrentConnection(generation, connection)
            || run.GetString() != generation.ToString(CultureInfo.InvariantCulture)
            || !liveDebugRequests.TryGetValue(requestId.GetString()!, out LiveDebugRequest? pending)
            || pending.Generation != generation || pending.Connection != connection)
            return null;
        if (!message.TryGetProperty("chunkIndex", out JsonElement indexValue)
            || indexValue.ValueKind != JsonValueKind.Number
            || !indexValue.TryGetInt32(out int index)
            || !message.TryGetProperty("chunkCount", out JsonElement countValue)
            || countValue.ValueKind != JsonValueKind.Number
            || !countValue.TryGetInt32(out int count)
            || count < 1 || count > 16384 || index != pending.NextChunk || index >= count
            || pending.ChunkCount is int previousCount && previousCount != count
            || !message.TryGetProperty("payload", out JsonElement payload)
            || payload.ValueKind != JsonValueKind.String)
            return "invalid Live Debug response chunk";
        pending.ChunkCount = count;
        pending.Payload.Append(payload.GetString());
        if (pending.Payload.Length > 64 * 1024 * 1024)
            return "Live Debug response exceeds the size limit";
        pending.NextChunk++;
        if (pending.NextChunk == count)
        {
            if (JsonNode.Parse(pending.Payload.ToString()) is not JsonObject response)
                return "Live Debug response must be an object";
            pending.Completion.TrySetResult(response);
        }
        return null;
    }

    private void cancelLiveDebugRequests()
    {
        foreach (LiveDebugRequest pending in liveDebugRequests.Values)
            pending.Completion.TrySetResult(null);
        liveDebugRequests.Clear();
    }

    private sealed class LiveDebugRequest(long generation, long connection)
    {
        public long Generation { get; } = generation;
        public long Connection { get; } = connection;
        public int NextChunk { get; set; }
        public int? ChunkCount { get; set; }
        public StringBuilder Payload { get; } = new();
        public TaskCompletionSource<JsonObject?> Completion { get; } = new(TaskCreationOptions.RunContinuationsAsynchronously);
    }
}
