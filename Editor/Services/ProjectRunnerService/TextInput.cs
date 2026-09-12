using System;
using System.Globalization;
using System.Text.Json;

namespace Ludork.Services;

public sealed partial class ProjectRunnerService
{
    public event EventHandler<RuntimeTextInputMessage>? TextInputReceived;

    public bool IsCurrentConnection(long expectedRunGeneration, long expectedConnectionGeneration)
    {
        lock (processLock)
        {
            return !disposed && commandClient is not null
                && RunGeneration == expectedRunGeneration
                && ConnectionGeneration == expectedConnectionGeneration;
        }
    }

    private string? receiveRuntimeMessage(string line, long generation, long connection)
    {
        try
        {
            using JsonDocument document = JsonDocument.Parse(line);
            JsonElement root = document.RootElement;
            if (root.ValueKind == JsonValueKind.Object
                && root.TryGetProperty("type", out JsonElement messageType)
                && messageType.ValueKind == JsonValueKind.String
                && messageType.GetString() == "liveDebug")
                return receiveLiveDebugMessage(root, generation, connection);
            string? error = validateBridgeEnvelope(root, "textInput");
            if (error is not null)
                return error;
            if (!root.TryGetProperty("action", out JsonElement actionValue)
                || actionValue.ValueKind != JsonValueKind.String
                || actionValue.GetString() is not ("begin" or "update" or "end"))
            {
                return "invalid text input action";
            }
            string action = actionValue.GetString()!;
            if (!root.TryGetProperty("session", out JsonElement sessionValue)
                || sessionValue.ValueKind != JsonValueKind.String
                || !ulong.TryParse(sessionValue.GetString(), NumberStyles.None, CultureInfo.InvariantCulture, out ulong session)
                || session == 0)
            {
                return "invalid text input session";
            }
            double x = 0;
            double y = 0;
            double width = 0;
            double height = 0;
            if (action != "end"
                && (!readCoordinate(root, "x", out x)
                    || !readCoordinate(root, "y", out y)
                    || !readCoordinate(root, "width", out width)
                    || !readCoordinate(root, "height", out height)
                    || width < 0 || height < 0
                    || x + width > int.MaxValue || y + height > int.MaxValue))
            {
                return "invalid text input caret rectangle";
            }
            if (IsCurrentConnection(generation, connection))
            {
                TextInputReceived?.Invoke(this, new(action, session.ToString(CultureInfo.InvariantCulture),
                    x, y, width, height, generation, connection));
            }
            return null;
        }
        catch (JsonException exception)
        {
            return exception.Message;
        }
    }

    private static bool readCoordinate(JsonElement root, string name, out double value)
    {
        value = 0;
        return root.TryGetProperty(name, out JsonElement coordinate)
            && coordinate.ValueKind == JsonValueKind.Number
            && coordinate.TryGetDouble(out value)
            && double.IsFinite(value)
            && value >= int.MinValue && value <= int.MaxValue;
    }

    private static string? validateBridgeEnvelope(JsonElement root, string expectedType)
    {
        if (root.ValueKind != JsonValueKind.Object)
            return "bridge message must be a JSON object";
        if (!root.TryGetProperty("v", out JsonElement version)
            || version.ValueKind != JsonValueKind.Number
            || !version.TryGetInt32(out int parsedVersion)
            || parsedVersion != BridgeProtocolVersion)
        {
            return $"expected protocol {BridgeProtocolVersion}";
        }
        if (!root.TryGetProperty("type", out JsonElement type)
            || type.ValueKind != JsonValueKind.String
            || type.GetString() != expectedType)
        {
            return $"expected runtime message {expectedType}";
        }
        return null;
    }
}
