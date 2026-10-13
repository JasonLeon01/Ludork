using System;
using System.Collections.Generic;
using System.IO;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace Ludork.Services;

internal sealed class MapPreviewCatalogCapture : IDisposable
{
    private MemoryStream? stream;
    private Utf8JsonWriter? writer;
    private string? property;
    private string? layerName;
    private int depth;

    public JsonObject Data { get; } = new();

    public void Append(ref Utf8JsonReader reader, string? currentLayerName)
    {
        if (writer is null && property is null)
        {
            if (reader.TokenType != JsonTokenType.PropertyName)
                return;
            string? name = reader.GetString();
            if (reader.CurrentDepth == 1 && name is "actors" or "BPClassVarChanged")
                property = name;
            else if (reader.CurrentDepth == 3 && name == "actors" && currentLayerName is not null)
            {
                property = name;
                layerName = currentLayerName;
            }
            return;
        }
        if (writer is null)
        {
            stream = new MemoryStream();
            writer = new Utf8JsonWriter(stream);
            depth = reader.CurrentDepth;
        }
        switch (reader.TokenType)
        {
            case JsonTokenType.StartObject: writer.WriteStartObject(); break;
            case JsonTokenType.EndObject: writer.WriteEndObject(); break;
            case JsonTokenType.StartArray: writer.WriteStartArray(); break;
            case JsonTokenType.EndArray: writer.WriteEndArray(); break;
            case JsonTokenType.PropertyName: writer.WritePropertyName(reader.GetString()!); break;
            case JsonTokenType.String: writer.WriteStringValue(reader.GetString()); break;
            case JsonTokenType.Number: writer.WriteRawValue(reader.ValueSpan, true); break;
            case JsonTokenType.True: writer.WriteBooleanValue(true); break;
            case JsonTokenType.False: writer.WriteBooleanValue(false); break;
            case JsonTokenType.Null: writer.WriteNullValue(); break;
        }
        if (reader.CurrentDepth != depth || reader.TokenType is JsonTokenType.StartObject or JsonTokenType.StartArray)
            return;
        writer.Flush();
        JsonNode? value = JsonNode.Parse(stream!.GetBuffer().AsSpan(0, checked((int)stream.Length)));
        if (layerName is null)
            Data[property!] = value;
        else
        {
            if (Data["layers"] is not JsonObject layers)
                Data["layers"] = layers = new JsonObject();
            layers[layerName] = new JsonObject { ["actors"] = value };
        }
        Dispose();
        property = null;
        layerName = null;
    }

    public void Dispose()
    {
        writer?.Dispose();
        stream?.Dispose();
        writer = null;
        stream = null;
    }

    public static JsonObject Extract(JsonObject map)
    {
        JsonObject result = new()
        {
            ["actors"] = map["actors"]?.DeepClone(),
            ["BPClassVarChanged"] = map["BPClassVarChanged"]?.DeepClone(),
        };
        if (map["layers"] is JsonObject layers)
        {
            JsonObject capturedLayers = new();
            foreach (KeyValuePair<string, JsonNode?> entry in layers)
                if (entry.Value is JsonObject layer && layer["actors"] is JsonArray actors)
                    capturedLayers[entry.Key] = new JsonObject { ["actors"] = actors.DeepClone() };
            result["layers"] = capturedLayers;
        }
        return result;
    }
}
