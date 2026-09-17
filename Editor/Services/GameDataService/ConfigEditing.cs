using System;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed partial class GameDataService
{
    internal static bool CanEditConfigField(string fileKey, string name)
    {
        return fileKey != "System" || name != "cellSize";
    }

    public bool UpdateConfigValue(string fileKey, string name, JsonNode? value)
    {
        if (!CanEditConfigField(fileKey, name)
            || !sections["Configs"].Data.TryGetValue(fileKey, out JsonObject? data)
            || data[name] is not JsonObject field || JsonNode.DeepEquals(field["value"], value))
        {
            return false;
        }
        JsonNode? next = value?.DeepClone();
        RecordDocumentSnapshot("Configs", fileKey);
        field["value"] = next;
        refreshModifiedState();
        return true;
    }

    public bool UpdateStartMap(string runtimePath, JsonArray position)
    {
        if (!sections["Configs"].Data.TryGetValue("System", out JsonObject? data)
            || data["startMap"] is not JsonObject mapField
            || data["startPos"] is not JsonObject positionField
            || (JsonNode.DeepEquals(mapField["value"], JsonValue.Create(runtimePath))
                && JsonNode.DeepEquals(positionField["value"], position)))
        {
            return false;
        }
        JsonArray nextPosition = (JsonArray)position.DeepClone();
        RecordDocumentSnapshot("Configs", "System");
        mapField["value"] = runtimePath;
        positionField["value"] = nextPosition;
        refreshModifiedState();
        return true;
    }

    public bool UpdateConfigArrayValue(string fileKey, string name, int index, JsonNode? value)
    {
        JsonArray? values = getConfigArrayForEdit(fileKey, name, out _);
        if (values is null || index < 0 || index >= values.Count || JsonNode.DeepEquals(values[index], value))
            return false;
        values[index] = value?.DeepClone();
        return UpdateConfigValue(fileKey, name, values);
    }

    public bool InsertConfigArrayValue(string fileKey, string name, int index, JsonNode? value)
    {
        JsonArray? values = getConfigArrayForEdit(fileKey, name, out bool variableLength);
        if (values is null || !variableLength || index < 0 || index > values.Count)
            return false;
        values.Insert(index, value?.DeepClone());
        return UpdateConfigValue(fileKey, name, values);
    }

    public bool RemoveConfigArrayValue(string fileKey, string name, int index)
    {
        JsonArray? values = getConfigArrayForEdit(fileKey, name, out bool variableLength);
        if (values is null || !variableLength || index < 0 || index >= values.Count)
            return false;
        values.RemoveAt(index);
        return UpdateConfigValue(fileKey, name, values);
    }

    private JsonArray? getConfigArrayForEdit(string fileKey, string name, out bool variableLength)
    {
        variableLength = false;
        if (!CanEditConfigField(fileKey, name)
            || !sections["Configs"].Data.TryGetValue(fileKey, out JsonObject? data)
            || data[name] is not JsonObject field)
        {
            return null;
        }
        string type = field["type"]?.GetValue<string>()?.Trim() ?? string.Empty;
        int open = type.IndexOf('[');
        int close = type.IndexOf(']');
        if (open < 0 || close <= open)
            return null;
        string lengthText = type[(open + 1)..close];
        variableLength = lengthText.Length == 0;
        if (!variableLength && !int.TryParse(lengthText, out _))
            return null;
        JsonArray values = field["value"]?.DeepClone() as JsonArray ?? new JsonArray();
        if (!variableLength && int.TryParse(lengthText, out int length))
        {
            length = Math.Max(0, length);
            string baseType = type[..open];
            while (values.Count < length)
            {
                values.Add(baseType switch
                {
                    "int" => JsonValue.Create(0),
                    "float" => JsonValue.Create(0.0),
                    _ => JsonValue.Create(string.Empty),
                });
            }
            while (values.Count > length)
                values.RemoveAt(values.Count - 1);
        }
        return values;
    }
}
